#ifndef __OLED_H_
#define __OLED_H_

#include "gpio.h"

#define OLED_RST_Clr() 	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET) //RST
#define OLED_RST_Set() 	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET)   //RST

#define OLED_RS_Clr() 	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET)   //DC
#define OLED_RS_Set() 	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET)     //DC

#define OLED_SCLK_Clr() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET)   	//SCL
#define OLED_SCLK_Set() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET)  	//SCL

#define OLED_SDIN_Clr() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET)   	//SDA
#define OLED_SDIN_Set() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET)   	//SDA

#define OLED_CMD  0	//写命令
#define OLED_DATA 1	//写数据


void OLED_ShowNumber(uint8_t x,uint8_t y,uint32_t num,uint8_t len,uint8_t size);
void OLED_ShowString(uint8_t x,uint8_t y,const uint8_t *p);	 
#endif  
	 
