#ifndef __IO_IIC_H
#define __IO_IIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gpio.h"

#define SDA_IN()  {GPIOB->CRH&=0XFFFFFF0F; GPIOB->CRH|=8<<4;}
#define SDA_OUT() {GPIOB->CRH&=0XFFFFFF0F; GPIOB->CRH|=3<<4;}

#define IIC_SCL(x)	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, (GPIO_PinState)x)
#define IIC_SDA(x)  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, (GPIO_PinState)x)
	
#define READ_SDA HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9)

int iic_read(uint8_t dev, uint8_t reg, uint8_t len, uint8_t *buf);
int iic_write(uint8_t dev, uint8_t reg, uint8_t len, uint8_t *buf);
int iic_write_bits(uint8_t dev, uint8_t reg, uint8_t bitStart, uint8_t len, uint8_t data);
int iic_write_bit(uint8_t dev, uint8_t reg, uint8_t bitNum, uint8_t data);

#endif /* __IO_IIC_H */                 
