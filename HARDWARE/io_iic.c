#include "i2c.h"
#include "io_iic.h"
#include "delay_us.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "init.h"
#include "user_lib.h"

static SemaphoreHandle_t xI2C_Mutex;
static SemaphoreHandle_t xI2C_Semphr;
static volatile HAL_StatusTypeDef xI2CTransferStatus = HAL_OK;

static int iic_start(void)
{
	SDA_OUT();     
	IIC_SDA(1);
	if(!READ_SDA) return 0;	
	IIC_SCL(1);
	delay_us(1);
 	IIC_SDA(0); 
	if(READ_SDA) return 0;
	delay_us(1);
	IIC_SCL(0);
	return 1;
}

static void iic_stop(void)
{
	SDA_OUT();
	IIC_SCL(0);
	IIC_SDA(0);
 	delay_us(1);
	IIC_SCL(1); 
	IIC_SDA(1);
	delay_us(1);							   	
}

static int iic_wait_ack(void)
{
	uint8_t ucErrTime=0;
	SDA_IN();      
	IIC_SDA(1);
	delay_us(1);	   
	IIC_SCL(1);
	delay_us(1);	 
	while(READ_SDA)
	{
		ucErrTime++;
		if(ucErrTime>50)
		{
			iic_stop();
			return 0;
		}
	  delay_us(1);
	}
	IIC_SCL(0); 
	return 1;  
} 

static void iic_ack(void)
{
	IIC_SCL(0);
	SDA_OUT();
	IIC_SDA(0);
	delay_us(1);
	IIC_SCL(1);
	delay_us(1);
	IIC_SCL(0);
}

static void iic_nack(void)
{
	IIC_SCL(0);
	SDA_OUT();
	IIC_SDA(1);
	delay_us(1);
	IIC_SCL(1);
	delay_us(1);
	IIC_SCL(0);
}

/**
 * @brief 发送一个字节（IIC时序）
 * @param 
 * @return 
 * @note 
 */
static void iic_send_byte(uint8_t txd)
{                        
    uint8_t t;   
	SDA_OUT(); 	    
    IIC_SCL(0);
    for (t=0; t<8; t++) {              
		IIC_SDA((txd&0x80)>>7);
		txd<<=1; 	  
		delay_us(1);   
		IIC_SCL(1);
		delay_us(1); 
		IIC_SCL(0);	
		delay_us(1);
    }	 
} 

/**
 * @brief 读取一个字节（IIC时序）
 * @param 
 * @return 
 * @note 
 */
static uint8_t iic_read_byte(unsigned char ack)
{
	uint8_t receive=0;
	SDA_IN();
    for (int i=0;i<8;i++ ) {
		IIC_SCL(0); 
		delay_us(2);
		IIC_SCL(1);
		receive<<=1;
		if(READ_SDA)receive++;   
		delay_us(2); 
    }					 
    if (ack)
        iic_ack();  
    else
        iic_nack();
    return receive;
}


/**
 * @brief 写（多个）数据到寄存器
 * @param dev - 设备地址
 * @param reg - 寄存器地址
 * @param len - 字节数
 * @param buf - 写数据缓存
 * @return 0：成功；-1：失败
 * @note 
 */
#if 0
int iic_write(uint8_t dev, uint8_t reg, uint8_t len, uint8_t *buf)
{
	if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) != pdTRUE) 
		return 1;
	
	int ret = 0;
	if (!iic_start()) {ret = 1; goto exit;}
	iic_send_byte(dev << 1);
	if (!iic_wait_ack()) {ret = 1; goto exit;}
	iic_send_byte(reg);
	if (!iic_wait_ack()) {ret = 1; goto exit;}
	for (int i = 0; i < len; i++) {
		iic_send_byte(buf[i]);
		if (!iic_wait_ack()) {ret = 1; goto exit;}
	}
		
	exit:
	iic_stop();
	xSemaphoreGive(xI2CMutex);
	return ret;
}
#else
int iic_write(uint8_t dev, uint8_t reg, uint8_t len, uint8_t *buf)
{
#if 0
	if (xSemaphoreTake(xI2C_Mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 1;
    }
	
	xSemaphoreTake(xI2C_Semphr, 0);
	
	HAL_StatusTypeDef status = HAL_I2C_Mem_Write_IT(&hi2c1, dev<<1, reg, 
													I2C_MEMADD_SIZE_8BIT, buf, len);
	
	if (status != HAL_OK) 
	{
		xSemaphoreGive(xI2C_Mutex);
		return 1;
	}
	
	if (xSemaphoreTake(xI2C_Semphr, pdMS_TO_TICKS(100)) != pdTRUE) {
		// 关闭总线
        __HAL_I2C_DISABLE(&hi2c1);
        hi2c1.State = HAL_I2C_STATE_READY;
        __HAL_I2C_ENABLE(&hi2c1);
		return 1;
    }
	
	xSemaphoreGive(xI2C_Mutex);
    return (xI2CTransferStatus == HAL_OK) ? 0 : 1;
#endif

	if (xSemaphoreTake(xI2C_Mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return -1;
    }
	HAL_StatusTypeDef status = HAL_I2C_Mem_Write(&hi2c1, dev<<1, reg, 
													I2C_MEMADD_SIZE_8BIT, buf, len,100);
	
	xSemaphoreGive(xI2C_Mutex);
    return 0;
}
#endif

/**
 * @brief 读寄存器（多个）数据
 * @param dev - 设备地址
 * @param reg - 寄存器地址
 * @param len - 字节数
 * @param buf - 读出数据缓存
 * @return 0：成功；-1：失败
 * @note 
 */
#if 0
int iic_read(uint8_t dev, uint8_t reg, uint8_t len, uint8_t *buf)
{
	if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) != pdTRUE) 
	return 1;
	
	int ret = 0;
		if (!iic_start()) {ret = 1; goto exit;}
		iic_send_byte(dev << 1);
		if (!iic_wait_ack()) {ret = 1; goto exit;}
		iic_send_byte(reg);
		if (!iic_wait_ack()) {ret = 1; goto exit;}
		if (!iic_start()) {ret = 1; goto exit;}
		iic_send_byte((dev << 1)+1);
		if (!iic_wait_ack()) {ret = 1; goto exit;}
		while (len) {
			if (len == 1)
				*buf = iic_read_byte(0);
			else
				*buf = iic_read_byte(1);
			buf++;
			len--;
		}
		
	exit:
	iic_stop();
	xSemaphoreGive(xI2CMutex);
	return ret;
}
#else
int iic_read(uint8_t dev, uint8_t reg, uint8_t len, uint8_t *buf)
{
#if 0
	if (xSemaphoreTake(xI2C_Mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 1;
    }
	
	xSemaphoreTake(xI2C_Semphr, 0);
	
	HAL_StatusTypeDef status = HAL_I2C_Mem_Read_IT(&hi2c1, dev<<1, reg, 
													I2C_MEMADD_SIZE_8BIT, buf, len);
	
	if (HAL_OK != status)
	{
		xSemaphoreGive(xI2C_Mutex);
		return 1;
	}
	
	if (xSemaphoreTake(xI2C_Semphr, pdMS_TO_TICKS(100)) != pdTRUE) {
		// 关闭总线
        __HAL_I2C_DISABLE(&hi2c1);
        hi2c1.State = HAL_I2C_STATE_READY;
        __HAL_I2C_ENABLE(&hi2c1);
		xSemaphoreGive(xI2C_Mutex);
		return 1;
    }
	
	xSemaphoreGive(xI2C_Mutex);
    return (xI2CTransferStatus == HAL_OK) ? 0 : 1;
#else
	
	if (xSemaphoreTake(xI2C_Mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return -1;
    }
	HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1, dev<<1, reg, 
													I2C_MEMADD_SIZE_8BIT, buf, len, 100);
	
	if (HAL_OK != status)
	{
		return -1;
	}
	
	xSemaphoreGive(xI2C_Mutex);
    return 0;
#endif
}
#endif

/**
 * @brief 修改指定设备寄存器一个字节的一个位
 * @param dev - 设备地址
 * @param reg - 寄存器地址
 * @param bitNum - 目标字节位数
 * @param len - 读字节数
 * @param data - 读取缓存
 * @return 0：成功；-1：失败
 * @note 
 */
int iic_write_bit(uint8_t dev, uint8_t reg, uint8_t bitNum, uint8_t data){
	if (bitNum >= 8) return -1;
    uint8_t b;
    if (iic_read(dev, reg, 1, &b) == 1) return -1;
	
    b = (data != 0) ? (b | (1U << bitNum)) : (b & ~(1U << bitNum));
    return iic_write(dev, reg, 1, &b);
}

#if 0
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == hi2c1.Instance)  
    {
		xI2CTransferStatus = HAL_OK;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xI2C_Semphr, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == hi2c1.Instance)
    {
		xI2CTransferStatus = HAL_OK;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xI2C_Semphr, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == hi2c1.Instance)
    {
		xI2CTransferStatus = HAL_ERROR;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xI2C_Semphr, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
#endif

/**
 * @brief 修改指定设备寄存器一个字节的多个连续位
 * @param dev - 设备地址
 * @param reg - 寄存器地址
 * @param bitStart - 目标字节起始位
 * @param len - bitStart后连续几位
 * @param data - 读取缓存
 * @return 0：成功；-1：失败
 * @note 
 */
int iic_write_bits(uint8_t dev, uint8_t reg, uint8_t bitStart, uint8_t len, uint8_t data)
{
	if (len == 0 || bitStart + len > 8) return -1; // 参数错误
	uint8_t b;
	if (iic_read(dev, reg, 1, &b) == 1) return -1;
	
    uint8_t mask = ((1U << len) - 1U) << bitStart;    // 掩码
    data = (data & ((1U << len) - 1U)) << bitStart;   // 限制data位宽并移位
    b = (b & ~mask) | data;
	return iic_write(dev, reg, 1, &b);
}

void io_iic_init(void)
{
	xI2C_Mutex = xSemaphoreCreateMutex();
	ASSERT(xI2C_Mutex != NULL);

	//xI2C_Semphr = xSemaphoreCreateBinary();
}

//INIT_BOARD_EXPORT(io_iic_init);
