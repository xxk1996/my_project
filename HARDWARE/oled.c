#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "init.h"
#include "oled.h"
#include "oledfont.h"  
#include "cmsis_os.h" 

#include "FreeRTOS.h"
#include "semphr.h"

#include "isr_app.h"
#include "user_lib.h"
#include "jspool.h"

static SemaphoreHandle_t oledMutex;

static uint8_t OLED_GRAM[128][8];	 


/**
 * @brief 写入一个字节
 * @param 
 * @return 
 * @note 
 */
static void OLED_WR_Byte(uint8_t dat, uint8_t cmd)
{	
	taskENTER_CRITICAL(); // 原子操作
			  
	if(cmd)
		OLED_RS_Set();
	else 
		OLED_RS_Clr();		  
	for (uint8_t i=0; i<8;i++)
	{			  
		OLED_SCLK_Clr();
		if (dat & 0x80)
			OLED_SDIN_Set();
		else 
			OLED_SDIN_Clr();
		OLED_SCLK_Set();
		dat <<= 1;   
	}				 		  
	OLED_RS_Set(); 
	
	taskEXIT_CRITICAL();	
} 

/**
 * @brief 刷新OLED屏
 * @param 
 * @return 
 * @note 
 */
static void OLED_Refresh_Gram(void)
{		
	uint8_t i,n;		    
	for(i=0;i<8;i++)  
	{  
		OLED_WR_Byte (0xb0+i,OLED_CMD);    
		OLED_WR_Byte (0x00,OLED_CMD);     
		OLED_WR_Byte (0x10,OLED_CMD);        
		for(n=0;n<128;n++) OLED_WR_Byte(OLED_GRAM[n][i],OLED_DATA); 
	} 	
}

/**
 * @brief 开启OLED显示
 * @param 
 * @return 
 * @note 
 */
static void OLED_Display_On(void)
{
	OLED_WR_Byte(0X8D,OLED_CMD);  
	OLED_WR_Byte(0X14,OLED_CMD);  
	OLED_WR_Byte(0XAF,OLED_CMD);  
}
 
/**
 * @brief 关闭OLED显示
 * @param 
 * @return 
 * @note 
 */
static void OLED_Display_Off(void)
{
	OLED_WR_Byte(0X8D,OLED_CMD);  
	OLED_WR_Byte(0X10,OLED_CMD);  
	OLED_WR_Byte(0XAE,OLED_CMD);  
}		   			 

/**
 * @brief 清屏函数
 * @param 
 * @return 
 * @note 
 */
static void OLED_Clear(void)  
{  
	uint8_t i,n;  
	for(i=0;i<8;i++)for(n=0;n<128;n++)OLED_GRAM[n][i]=0X00;  
	OLED_Refresh_Gram();
}

/**
 * @brief 画点
 * @param x,y - 坐标
 * @param t - 0：清空，1：填充
 * @return 
 * @note 
 */
static void OLED_DrawPoint(uint8_t x,uint8_t y,uint8_t t)
{
	uint8_t pos,bx,temp=0;
	if(x>127||y>63)return;
	pos=7-y/8;
	bx=y%8;
	temp=1<<(7-bx);
	if(t)  OLED_GRAM[x][pos]|=temp;
	else   OLED_GRAM[x][pos]&=~temp;	    
}


/**
 * @brief 指定位置显示字符
 * @param x,y - 坐标
 * @param 
 * @return 
 * @note 
 */
static void OLED_ShowChar(uint8_t x,uint8_t y,uint8_t chr,uint8_t size,uint8_t mode)
{  	
	uint8_t temp,t,t1;
	uint8_t y0=y;
	chr=chr-' ';																			   
    for(t=0;t<size;t++)
    {   
		if(size==12)  temp=oled_asc2_1206[chr][t];  
		else          temp=oled_asc2_1608[chr][t];		                          
		for (t1=0;t1<8;t1++)
		{
			if(temp&0x80)  OLED_DrawPoint(x,y,mode);
			else           OLED_DrawPoint(x,y,!mode);
			temp<<=1;
			y++;
			if((y-y0)==size)
			{
				y=y0;
				x++;
				break;
			}
		}  	 
    }  
}

/**
 * @brief 求m的n次方
 * @param 
 * @return 
 * @note 
 */
static uint32_t oled_pow(uint8_t m,uint8_t n)
{
	uint32_t result=1;	 
	while(n--)result*=m;    
	return result;
}				  

/**
 * @brief 显示两个数字
 * @param 
 * @return 
 * @note 
 */
static void OLED_ShowNumber(uint8_t x,uint8_t y,uint32_t num,uint8_t len,uint8_t size)
{ 	
	uint8_t t,temp;
	uint8_t enshow=0;						   
	for(t=0;t<len;t++)
	{
		temp=(num/oled_pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				OLED_ShowChar(x+(size/2)*t,y,' ',size,1);
				continue;
			}else enshow=1; 
		 	 
		}
	 	OLED_ShowChar(x+(size/2)*t,y,temp+'0',size,1); 
	}
} 

/**
 * @brief 显示字符串
 * @param 
 * @return 
 * @note 
 */
static void OLED_ShowString(uint8_t x,uint8_t y,const uint8_t *p)
{	
#define MAX_CHAR_POSX 122
#define MAX_CHAR_POSY 58          
    while(*p!='\0')
    {       
      if(x>MAX_CHAR_POSX){x=0;y+=16;}
      if(y>MAX_CHAR_POSY){y=x=0;OLED_Clear();}
      OLED_ShowChar(x,y,*p,12,1);	 
      x+=8;
      p++;
    }  
}	 

void OLED_ShowString_Safe(uint8_t x, uint8_t y, const uint8_t *p)
{
    if (oledMutex == NULL) return;
    xSemaphoreTake(oledMutex, pdMS_TO_TICKS(50));
    OLED_ShowString(x, y, p);      
    xSemaphoreGive(oledMutex);
}

void OLED_ShowNumber_Safe(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size)
{
    if (oledMutex == NULL) return;
    xSemaphoreTake(oledMutex, pdMS_TO_TICKS(50));
    OLED_ShowNumber(x, y, num, len, size);
    xSemaphoreGive(oledMutex);
}

void OLED_Refresh_Gram_Safe(void)
{
    if (oledMutex == NULL) return;
    xSemaphoreTake(oledMutex, pdMS_TO_TICKS(50));
    OLED_Refresh_Gram();          
    xSemaphoreGive(oledMutex);
}

void OLED_Clear_Safe(void)
{
    if (oledMutex == NULL) return;
    xSemaphoreTake(oledMutex, pdMS_TO_TICKS(50));
    OLED_Clear();          
    xSemaphoreGive(oledMutex);
}
 
/**
 * @brief OLED初始化
 * @param 
 * @return 
 * @note 
 */
void oled_driver_Init(void)
{ 	
	OLED_RST_Clr();
	HAL_Delay(100);
	OLED_RST_Set(); 
					  
	OLED_WR_Byte(0xAE, OLED_CMD); 
	OLED_WR_Byte(0xD5, OLED_CMD); 
	OLED_WR_Byte(80,   OLED_CMD);   
	OLED_WR_Byte(0xA8, OLED_CMD); 
	OLED_WR_Byte(0X3F, OLED_CMD);  
	OLED_WR_Byte(0xD3, OLED_CMD); 
	OLED_WR_Byte(0X00, OLED_CMD); 

	OLED_WR_Byte(0x40, OLED_CMD); 
													    
	OLED_WR_Byte(0x8D, OLED_CMD); 
	OLED_WR_Byte(0x14, OLED_CMD); 
	OLED_WR_Byte(0x20, OLED_CMD); 
	OLED_WR_Byte(0x02, OLED_CMD); 
	OLED_WR_Byte(0xA1, OLED_CMD); 
	OLED_WR_Byte(0xC0, OLED_CMD); 
	OLED_WR_Byte(0xDA, OLED_CMD); 
	OLED_WR_Byte(0x12, OLED_CMD); 
		 
	OLED_WR_Byte(0x81, OLED_CMD); 
	OLED_WR_Byte(0xEF, OLED_CMD); 
	OLED_WR_Byte(0xD9, OLED_CMD); 
	OLED_WR_Byte(0xf1, OLED_CMD);
	OLED_WR_Byte(0xDB, OLED_CMD); 
	OLED_WR_Byte(0x30, OLED_CMD); 

	OLED_WR_Byte(0xA4, OLED_CMD); 
	OLED_WR_Byte(0xA6, OLED_CMD);     						   
	OLED_WR_Byte(0xAF, OLED_CMD); 
	OLED_Clear();
}  

static void oled_show(void)
{
	wheel_data_t wheel_data = {0};
	
	jspool_handle_t handle = jspool_find("wheel data");

	if (handle)
	{
		jspool_read(handle, 0, (void*)&wheel_data, sizeof(wheel_data_t));
		
		//=============第一行显示小车模式=======================//	
		if (wheel_data.way_angle == 1)			OLED_ShowString_Safe(0, 0, (const uint8_t*)"DMP");
		else if (wheel_data.way_angle == 2)		OLED_ShowString_Safe(0, 0, (const uint8_t*)"Kalman");
		else if (wheel_data.way_angle == 3)		OLED_ShowString_Safe(0, 0, (const uint8_t*)"C F");
		
		//=============第二行显示角度=======================//
		int angle_balance = (int)wheel_data.angle_balance;
		OLED_ShowString_Safe(00, 10, (const uint8_t*)"Angle");
		if (angle_balance < 0)	OLED_ShowString_Safe(48, 10, (const uint8_t*)"-");
		if (angle_balance >= 0)	OLED_ShowString_Safe(48, 10, (const uint8_t*)"+");
		OLED_ShowNumber_Safe(56, 10, abs(angle_balance), 3, 12);
		
		//=============第三行显示角速度与距离===============//	
		int gyro_balance = (int)wheel_data.gyro_balance;
		OLED_ShowString_Safe(0, 20, (const uint8_t*)"Gyrox");
		if (gyro_balance < 0)	OLED_ShowString_Safe(42, 20, (const uint8_t*)"-");
		if (gyro_balance >= 0)	OLED_ShowString_Safe(42, 20, (const uint8_t*)"+");
		OLED_ShowNumber_Safe(50, 20, abs(gyro_balance), 4, 12);
														
		OLED_ShowNumber_Safe(82, 20, wheel_data.distance, 5, 12);
		OLED_ShowString_Safe(114, 20, (const uint8_t*)"mm");
		
		//=============第四行显示左轮PWM与编码器读数=======================//	
		int motor_left = (int)wheel_data.motor_left;
		int vel_left = (int)wheel_data.vel_left;
		OLED_ShowString_Safe(00, 30, (const uint8_t*)"L");
		if (motor_left < 0)		OLED_ShowString_Safe(16, 30, (const uint8_t*)"-"),	OLED_ShowNumber_Safe(26, 30, abs(motor_left), 4, 12);
		if (motor_left >= 0)	OLED_ShowString_Safe(16, 30, (const uint8_t*)"+"),	OLED_ShowNumber_Safe(26, 30, abs(motor_left), 4, 12);
														
		if (vel_left < 0)		OLED_ShowString_Safe(60, 30, (const uint8_t*)"-");
		if (vel_left >= 0)		OLED_ShowString_Safe(60, 30, (const uint8_t*)"+");
		OLED_ShowNumber_Safe(68, 30, abs(vel_left), 4, 12);
		OLED_ShowString_Safe(96, 30, (const uint8_t*)"mm/s");
		
		//=============第五行显示右轮PWM与编码器读数=======================//	
		int motor_right = (int)wheel_data.motor_right;
		int vel_right = (int)wheel_data.vel_right;	
		OLED_ShowString_Safe(00, 40, (const uint8_t*)"R");
		if (motor_right < 0)	OLED_ShowString_Safe(16, 40, (const uint8_t*)"-"),	OLED_ShowNumber_Safe(26, 40, abs(motor_right), 4, 12);
		if (motor_right >= 0)	OLED_ShowString_Safe(16, 40, (const uint8_t*)"+"),	OLED_ShowNumber_Safe(26, 40, abs(motor_right), 4, 12);
														
		if (vel_right <0 )		OLED_ShowString_Safe(60, 40, (const uint8_t*)"-");
		if (vel_right >= 0)		OLED_ShowString_Safe(60, 40, (const uint8_t*)"+");
		OLED_ShowNumber_Safe(68, 40, abs(vel_right), 4, 12);
		OLED_ShowString_Safe(96, 40, (const uint8_t*)"mm/s");
		
		OLED_Refresh_Gram_Safe();
	}
}

static void oled_task(void *argument)
{
	for (;;)
	{
		oled_show();
		osDelay(100);
		//printf("new2!\r\n");
	}
}

void oled_module_init(void)
{
	oledMutex = xSemaphoreCreateMutex();
	ASSERT(oledMutex != NULL);

	BaseType_t xReturn = xTaskCreate(oled_task, "oled task", 512, NULL, osPriorityNormal, NULL);
	ASSERT(xReturn == pdPASS);
}

//INIT_APP_EXPORT(oled_module_init);


