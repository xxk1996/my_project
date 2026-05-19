#include <stdint.h>
#include <stdlib.h>
#include "tim.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "init.h"

#define DBG_TAG "motor_ctrl"
#define DBG_LVL DBG_LOG
//#define DBG_COLOR

#include "jsdbg.h"

#define PI 3.14159265f

#define ENCODER_MULTIPLES   4.0 		// 倍频（Encoder Mode Tl1 and Tl2），一个周期采集四个边沿
#define ENCODER_PRECISION  13.0 		// 精度（13线）
#define REDUCTION_RATIO	   30.0			// 减速比
#define Diameter_67  67.0 				// 轮子直径

// 电机PWM配置为周期72Mhz，计数器值为7199

// 右电机
#define SET_PWMA(Pulse)     __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, Pulse) 	// PA8
#define AIN1(x) HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, (GPIO_PinState)x)	// PB14
#define AIN2(x) HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, (GPIO_PinState)x)	// PB15

// 左电机
#define SET_PWMB(Pulse)     __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, Pulse) 	// PA11
#define BIN1(x) HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, (GPIO_PinState)x)	// PB13
#define BIN2(x) HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, (GPIO_PinState)x)	// PB12


typedef struct {
	TIM_HandleTypeDef *htim;
	
	uint16_t last_count;	// 上次计数
	uint32_t last_ms;		// 上次时间戳
	
	uint8_t dirc;		// 方向
	uint16_t cycle;		// 圈数（不可靠）
	float rpm;			// 转速rpm
	float velocity;		// 轮速mm/s
	
} encoder_data_t;

// 0为左，1为右
// 左右轮电机PWM为正时均向前
// 左右编码器均为负
encoder_data_t encoder_data[2];

int Read_Encoder(uint8_t TIMX)
{
	int Encoder_TIM;    
	switch(TIMX)
	{
		case 2:  Encoder_TIM = (short)TIM2->CNT;  TIM2->CNT=0;break;
		case 3:  Encoder_TIM = (short)TIM3->CNT;  TIM3->CNT=0;break;	
		case 4:  Encoder_TIM = (short)TIM4->CNT;  TIM4->CNT=0;break;	
		default: Encoder_TIM =0;
	}
	return Encoder_TIM;
}

static void encoder_velcotiy_update(encoder_data_t *p)
{
	uint16_t current_counter = __HAL_TIM_GET_COUNTER(p->htim);
	uint32_t current_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;	
	int16_t delta_count = (int16_t)(current_counter - p->last_count);
	
	p->cycle = current_counter / (ENCODER_MULTIPLES * ENCODER_PRECISION * REDUCTION_RATIO);
	uint32_t delta_ms = current_ms - p->last_ms;
	
	if (delta_ms)
	{
		p->dirc = (delta_count < 0) ? 1 : 0;   // 向下计数为1，向上为0
		
		p->rpm = (float)(delta_count) * 60.0f / (delta_ms / 1000.0f) / (ENCODER_MULTIPLES * ENCODER_PRECISION * REDUCTION_RATIO);
		p->velocity = (float)(delta_count) * PI * Diameter_67 / (delta_ms /1000.0f) / (ENCODER_MULTIPLES * ENCODER_PRECISION * REDUCTION_RATIO);
		
		p->last_count = current_counter;
		p->last_ms = current_ms;
	}
}

void encoder_update(float *left_vel, float *right_vel)
{
	encoder_velcotiy_update(&encoder_data[0]);
	encoder_velcotiy_update(&encoder_data[1]);
	
	*left_vel = encoder_data[0].velocity;
	*right_vel = encoder_data[1].velocity;
}


// 左右电机PWM，输入-7199 - 7199
void Set_Pwm(int motor_left,int motor_right)
{
	if (motor_left > 0) {
		BIN1(1);
		BIN2(0);
	}	
	else {
		BIN1(0);
		BIN2(1);
	}
	SET_PWMB(abs(motor_left));

	if (motor_right > 0) {
		AIN1(0);
		AIN2(1);
	}
	else {
		AIN1(1);
		AIN2(0);
	}
	SET_PWMA(abs(motor_right));
}

void motor_tim_init(void)
{
	// 电机PWM定时器使能
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
	
	// 编码器定时器使能
	HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL); 
	__HAL_TIM_SET_COUNTER(&htim2, 0);
	
	HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL); 
	__HAL_TIM_SET_COUNTER(&htim4, 0);
	
	encoder_data[0].htim = &htim2;
	encoder_data[1].htim = &htim4;
}

//INIT_BOARD_EXPORT(motor_tim_init);
