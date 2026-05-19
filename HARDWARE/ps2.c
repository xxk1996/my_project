#include <string.h>
#include "gpio.h"
#include "ps2.h"
#include "delay_us.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_os.h"
#include "jspool.h"
#include "ps2.h"
#include "user_lib.h"

#define DBG_TAG "ps2"
#define DBG_LVL DBG_LOG
//#define DBG_COLOR

#include "jsdbg.h"

/*********************************************************     
**********************************************************/	 
void delay_us_1(uint32_t us) {
    uint32_t count = us * (72000000 / 1000000) / 5;  // 粗略循环计数值
    while(count--);
}
#define DELAY_TIME  delay_us_1(5); 
static uint8_t Comd[2]={0x01,0x42};	//开始命令
static uint8_t Data[9]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}; 
static SemaphoreHandle_t ps2Mutex;

static jspool_handle_t ps2_pool_handle;

//These are our button constants
typedef enum {
	PSB_SELECT = 0,
	PSB_L3,
	PSB_R3,
	PSB_START,
	PSB_PAD_UP,
	PSB_PAD_RIGHT,
	PSB_PAD_DOWN,
	PSB_PAD_LEFT,
	PSB_L2,
	PSB_R2,
	PSB_L1,
	PSB_R1,
	PSB_GREEN,
	PSB_RED,
	PSB_BLUE,
	PSB_PINK,	
} ps2_key_index_e;


#define PSB_TRIANGLE    13
#define PSB_CIRCLE      14
#define PSB_CROSS       15
#define PSB_SQUARE      16

//#define WHAMMY_BAR		8

//These are stick values
#define PSS_RX 5                
#define PSS_RY 6
#define PSS_LX 7
#define PSS_LY 8

#define DI   HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3)      

#define DO_H HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET)        
#define DO_L HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET)         

#define CS_H HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET)  
#define CS_L HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET) 

#define CLK_H HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_SET) 
#define CLK_L HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_RESET) 

/**
 * @brief 向手柄发送命令
 * @param 
 * @return 
 * @note 
 */
static void PS2_Cmd(uint8_t CMD)
{
	taskENTER_CRITICAL();
	volatile uint16_t ref = 0x01;
	Data[1] = 0;
	for(ref=0x01;ref<0x0100;ref<<=1)
	{
		if(ref&CMD)
		{
			DO_H;                   
		}
		else DO_L;

		CLK_H;                        
		DELAY_TIME;
		CLK_L;
		DELAY_TIME;
		CLK_H;
		if(DI)
			Data[1] = ref|Data[1];
	}
	delay_us(16);
	taskEXIT_CRITICAL();
}

/**
 * @brief 判断是否为红灯模式
 * @param 
 * @return 
 * @note 0x41=模拟绿灯，0x73模拟红灯
 */
static uint8_t PS2_RedLight(void)
{
	CS_L;
	PS2_Cmd(Comd[0]);  
	PS2_Cmd(Comd[1]); 
	CS_H;
	if(Data[1] == 0X73)   return 0 ;
	else return 1;

}

/**
 * @brief 读取手柄数据
 * @param 
 * @return 
 * @note 
 */
static void PS2_ReadData(void)
{
	volatile uint8_t byte=0;
	volatile uint16_t ref=0x01;
	CS_L;
	PS2_Cmd(Comd[0]);  
	PS2_Cmd(Comd[1]);  
	for(byte=2;byte<9;byte++)         
	{
		for(ref=0x01;ref<0x100;ref<<=1)
		{
			taskENTER_CRITICAL();
			CLK_H;
			DELAY_TIME;
			CLK_L;
			DELAY_TIME;
			CLK_H;
		    if(DI)
		    Data[byte] = ref|Data[byte];
			taskEXIT_CRITICAL();
		}
        delay_us(16);
	}
	CS_H;
}

static void PS2_ClearData()
{
	memset(Data, 0, sizeof(Data));
}


/**
 * @brief 振动函数
 * @param 
 * @return 
 * @note 
 */
static void PS2_Vibration(uint8_t motor1, uint8_t motor2)
{
	CS_L;
	delay_us(16);
    PS2_Cmd(0x01);  
	PS2_Cmd(0x42); 
	PS2_Cmd(0X00);
	PS2_Cmd(motor1);
	PS2_Cmd(motor2);
	PS2_Cmd(0X00);
	PS2_Cmd(0X00);
	PS2_Cmd(0X00);
	PS2_Cmd(0X00);
	CS_H;
	delay_us(16);  
}


static void PS2_ShortPoll(void)
{
	CS_L;
	delay_us(16);
	PS2_Cmd(0x01);  
	PS2_Cmd(0x42);  
	PS2_Cmd(0X00);
	PS2_Cmd(0x00);
	PS2_Cmd(0x00);
	CS_H;
	delay_us(16);	
}

static void PS2_EnterConfing(void)
{
    CS_L;
	delay_us(16);
	PS2_Cmd(0x01);  
	PS2_Cmd(0x43);  
	PS2_Cmd(0X00);
	PS2_Cmd(0x01);
	PS2_Cmd(0x00);
	PS2_Cmd(0X00);
	PS2_Cmd(0X00);
	PS2_Cmd(0X00);
	PS2_Cmd(0X00);
	CS_H;
	delay_us(16);
}

static void PS2_TurnOnAnalogMode(void)
{
	CS_L;
	PS2_Cmd(0x01);  
	PS2_Cmd(0x44);  
	PS2_Cmd(0X00);
	PS2_Cmd(0x01); 
	PS2_Cmd(0x03); 
				   
	PS2_Cmd(0X00);
	PS2_Cmd(0X00);
	PS2_Cmd(0X00);
	PS2_Cmd(0X00);
	CS_H;
	delay_us(16);
}

static void PS2_VibrationMode(void)
{
	CS_L;
	delay_us(16);
	PS2_Cmd(0x01);  
	PS2_Cmd(0x4D);  
	PS2_Cmd(0X00);
	PS2_Cmd(0x00);
	PS2_Cmd(0X01);
	CS_H;
	delay_us(16);	
}

static void PS2_ExitConfing(void)
{
    CS_L;
	delay_us(16);
	PS2_Cmd(0x01);  
	PS2_Cmd(0x43);  
	PS2_Cmd(0X00);
	PS2_Cmd(0x00);
	PS2_Cmd(0x5A);
	PS2_Cmd(0x5A);
	PS2_Cmd(0x5A);
	PS2_Cmd(0x5A);
	PS2_Cmd(0x5A);
	CS_H;
	delay_us(16);
}


static void vPS2_Task(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(20);  // 50Hz 读取频率
	ps2_data_t ps2_data = {0};

	uint16_t Handkey = 0;

    while(1)
    {
        //xSemaphoreTake(ps2Mutex, portMAX_DELAY);
        PS2_ClearData();
        PS2_ReadData();
		
        Handkey = (Data[4]<<8) | Data[3];
        // 可将 Data 和 Handkey 复制到静态全局变量或发送至队列
		
		ps2_data.PS2_LX = Data[PSS_LX];      
		ps2_data.PS2_LY = Data[PSS_LY];
		ps2_data.PS2_RX = Data[PSS_RX];
		ps2_data.PS2_RY = Data[PSS_RY];
		ps2_data.key.PS2_KEY = (~Handkey) & 0xffff; // 0有效改为1有效
		
		if (ps2_pool_handle)
			jspool_write(ps2_pool_handle, 0, (const void *)&ps2_data, sizeof(ps2_data_t));	
		
		LOG_D("LX = %d, LY = %d, RX = %d, RY = %d, KEY = %d", ps2_data.PS2_LX, ps2_data.PS2_LY, ps2_data.PS2_RX, ps2_data.PS2_RY, ps2_data.key.PS2_KEY);
		
        //xSemaphoreGive(ps2Mutex);

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

void PS2_SetInit(void)
{	
	PS2_ShortPoll();
	PS2_ShortPoll();
	PS2_ShortPoll();
	PS2_EnterConfing();		
	PS2_TurnOnAnalogMode();	
	//PS2_VibrationMode();	
	PS2_ExitConfing();	

	ps2Mutex = xSemaphoreCreateMutex();
	ASSERT(ps2Mutex != NULL);	
	
	BaseType_t xReturn = xTaskCreate(vPS2_Task, "ps2 task", 256, NULL, osPriorityNormal, NULL);
	ASSERT(xReturn == pdPASS);
	
	ps2_pool_handle = jspool_register("ps2", sizeof(ps2_data_t));
	ASSERT(ps2_pool_handle != NULL);	
}
















