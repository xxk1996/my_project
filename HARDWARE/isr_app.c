#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "main.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "cmsis_os.h"

#include "user_lib.h"
#include "init.h"
#include "mpu6050.h"
#include "isr_app.h"
#include "jspool.h"

#define DBG_TAG "isr_app"
#define DBG_LVL DBG_LOG
//#define DBG_COLOR

#include "jsdbg.h"

static SemaphoreHandle_t xBinarySem;
static jspool_handle_t wd_jspool_handle;

extern void Set_Pwm(int motor_left,int motor_right);
extern int Read_Encoder(uint8_t TIMX);
extern void mpu6050_get_euler_angle(mpu6050_data_t *data, uint8_t way);
extern void encoder_update(float *left_vel, float *right_vel);

float Turn_Kp=4200,Turn_Kd=0;

int fputc(int ch, FILE *f)
{
	HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xffff);
	return ch;
}

int fgetc(FILE * f)
{
	uint8_t ch = 0;
	HAL_UART_Receive(&huart1,&ch, 1, 0xffff);
	return ch;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	
	if (GPIO_Pin == MPU6050_EXIT_Pin)
	{
		// mpu6050中断引脚驱动
		if (xBinarySem) {
			xSemaphoreGiveFromISR(xBinarySem, &xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
}



/**
 * @brief 平衡直立环
 * @param Angle - 平衡倾角
 * @param Gyro - 平衡角速度
 * @return 输出PWM
 * @note pwm = kp*e(k)+kd*[e(k)-e(k-1)]
 */
int Balance(float Angle,float Gyro)
{  
	 float Balance_Kp = 40000,Balance_Kd=120 ;
	 float Angle_bias = - Angle;                       				
	 float Gyro_bias = - Gyro; 
	 float balance = -Balance_Kp/100.0f * Angle_bias - Gyro_bias * Balance_Kd/100.0f; 
	 return (int)balance;
}


/**
 * @brief 速度环
 * @param encoder_left - 左轮编码器计数
 * @param encoder_right - 右轮编码器计数
 * @param current_angle - 当前倾角
 * @param target_speed - 目标速度
 * @return 输出PWM
 * @note 
 */
int Velocity(int encoder_left,int encoder_right, float current_angle, int target_speed)
{  
	static float Encoder_Integral;
	static float Encoder_bias;
	float Velocity_Kp = 3000, Velocity_Ki = 20;
	
	int current_speed = -(encoder_left+encoder_right); 
	int err = target_speed - current_speed;
	
	#if 0
	Encoder_bias = (float)err;
	#else
	Encoder_bias = 0.86f * Encoder_bias + 0.14f * err;	// 一阶低通滤波
	#endif
		
	Encoder_Integral += Encoder_bias; 
	// 大角度积分清零
	if (fabsf(current_angle) > 10.0f) {
        Encoder_Integral = 0;
    }
				
	Encoder_Integral = CLAMP(Encoder_Integral, -800, 800);	// 积分限幅
	
	float velocity = Encoder_bias * (Velocity_Kp / 100.0f) + Encoder_Integral * (Velocity_Ki / 100);
	return (int)velocity;
}



void Get_Velocity_Form_Encoder(int encoder_left,int encoder_right, float *left_vel, float *right_vel)
{ 							
	float Rotation_Speed_L = (float)encoder_left*200.0f/4.0f/30.0f/13.0f;
	*left_vel = Rotation_Speed_L*3.1415f*67.0f;		
	float Rotation_Speed_R = (float)encoder_right*200.0f/4.0f/30.0f/13.0f;
	*right_vel = Rotation_Speed_R*3.1415f*67.0f;		
}



static void control_task(void *argument)
{
	static mpu6050_data_t mpu_data = {0};
	static int balance_pwm = 0, velocity_pwm = 0, turn_pwm = 0;
	int encoder_left = 0, encoder_right = 0;
	wheel_data_t wheel_data = {0};
		
	for (;;)
	{
		if (xSemaphoreTake(xBinarySem, portMAX_DELAY) == pdTRUE) {
			mpu6050_get_euler_angle(&mpu_data, 2);
			
			encoder_left = -Read_Encoder(2);
			encoder_right = -Read_Encoder(4);
			Get_Velocity_Form_Encoder(encoder_left, encoder_right, &wheel_data.vel_left, &wheel_data.vel_right);
			
			// encoder_update(&left_vel, &right_vel);
			wheel_data.angle_balance = mpu_data.angle_balance;
			wheel_data.gyro_balance = mpu_data.gyro_balance;
			
			balance_pwm = Balance(wheel_data.angle_balance, wheel_data.gyro_balance);
			//velocity_pwm = Velocity(encoder_left, encoder_right, wheel_data.angle_balance, 0.0f);
			
			wheel_data.motor_left = balance_pwm + velocity_pwm;// + turn_pwm;
			wheel_data.motor_right = balance_pwm + velocity_pwm;// - turn_pwm;

			wheel_data.motor_left = CLAMP(wheel_data.motor_left, -5000, 5000);
			wheel_data.motor_right = CLAMP(wheel_data.motor_right, -5000, 5000);
			
			Set_Pwm(wheel_data.motor_left, wheel_data.motor_right);
			
			if (wd_jspool_handle)
				jspool_write(wd_jspool_handle, 0, (const void*)&wheel_data, sizeof(wheel_data_t)); // 更新内存池

		} else {
			// 等待超时，信号量仍不可用
		}
	}
}

void isr_app_init(void)
{
	xBinarySem = xSemaphoreCreateBinary();
	ASSERT(xBinarySem != NULL);
	
	BaseType_t xReturn = xTaskCreate(control_task, "control task", 512, NULL, osPriorityNormal, NULL);
	ASSERT(xReturn == pdPASS);
	
	wd_jspool_handle = jspool_register("wheel data", sizeof(wheel_data_t));;
}


