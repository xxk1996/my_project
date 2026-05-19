#include <math.h>
#include "cmsis_os.h"
#include "mpu6050.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h"
#include "io_iic.h"
#include "kf.h"
#include "init.h"
#include "user_lib.h"

#define DBG_TAG "mpu6050"
#define DBG_LVL DBG_LOG
//#define DBG_COLOR

#include "jsdbg.h"

#define devAddr  0x68 // 7位地址

// real（°/s） = raw / factor
#define MPU6050_GYRO_FS_250_FACTOR 	131.07f
#define MPU6050_GYRO_FS_500_FACTOR 	65.54f
#define MPU6050_GYRO_FS_1000_FACTOR 32.77f
#define MPU6050_GYRO_FS_2000_FACTOR 16.38f // 32768/2000

// real（g）= raw / factor
#define MPU6050_ACCEL_FS_2_FACTOR   16384.0f     
#define MPU6050_ACCEL_FS_4_FACTOR	8192.0f        
#define MPU6050_ACCEL_FS_8_FACTOR  	4096.0f     
#define MPU6050_ACCEL_FS_16_FACTOR  2048.0f     

#define PRINT_ACCEL     (0x01)
#define PRINT_GYRO      (0x02)
#define PRINT_QUAT      (0x04)
#define ACCEL_ON        (0x01)
#define GYRO_ON         (0x02)

#define MOTION          (0)
#define NO_MOTION       (1)
#define DEFAULT_MPU_HZ  (200)
#define FLASH_SIZE      (512)
#define FLASH_MEM_START ((void*)0x1800)
	
#define q30  1073741824.0f
#define PI 3.14159265f
	
static signed char gyro_orientation[9] = {-1, 0, 0,
                                           0,-1, 0,
                                           0, 0, 1};

typedef struct {
	float q0;
	float q1;
	float q2;
	float q3;
	float Roll,Pitch,Yaw;
	short gyro[3], accel[3], sensors;
} mpu6050_dmp_data_t;


static  unsigned short inv_row_2_scale(const signed char *row)
{
    unsigned short b;

    if (row[0] > 0)
        b = 0;
    else if (row[0] < 0)
        b = 4;
    else if (row[1] > 0)
        b = 1;
    else if (row[1] < 0)
        b = 5;
    else if (row[2] > 0)
        b = 2;
    else if (row[2] < 0)
        b = 6;
    else
        b = 7;      // error
    return b;
}
										   
static  unsigned short inv_orientation_matrix_to_scalar(
    const signed char *mtx)
{
    unsigned short scalar;
    scalar = inv_row_2_scale(mtx);
    scalar |= inv_row_2_scale(mtx + 3) << 3;
    scalar |= inv_row_2_scale(mtx + 6) << 6;


    return scalar;
}

static void run_self_test(void)
{
    int result;
	long gyro[3], accel[3];

    result = mpu_run_self_test(gyro, accel);
    if (result == 0x7) {
        /* Test passed. We can trust the gyro data here, so let's push it down
         * to the DMP.
         */
        float sens;
        unsigned short accel_sens;
        mpu_get_gyro_sens(&sens);
        gyro[0] = (long)(gyro[0] * sens);
        gyro[1] = (long)(gyro[1] * sens);
        gyro[2] = (long)(gyro[2] * sens);
        dmp_set_gyro_bias(gyro);
        mpu_get_accel_sens(&accel_sens);
        accel[0] *= accel_sens;
        accel[1] *= accel_sens;
        accel[2] *= accel_sens;
        dmp_set_accel_bias(accel);
		LOG_D("setting bias succesfully ......\r\n");
    }
}

/**
 * @brief 读指设备标识
 * @param 
 * @return 返回0x68表示已连接
 * @note 
 */
static uint8_t mpu6050_get_device_id(void) 
{
	uint8_t data = 0;
    iic_read(devAddr, MPU6050_RA_WHO_AM_I, 1, &data);
    return data;
}

static void DMP_init(void)
{
	if (mpu6050_get_device_id() != 0x68)
	{
		LOG_E("mpu6050 get wrong device id ......\r\n");
		return;
	}
	
	LOG_D("mpu6050 get right device id ......\r\n");
	
	if (!mpu_init())
	{
		if (!mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL)) {
			LOG_D("mpu_set_sensor complete ......\r\n");
		}

		if(!mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL)) {
			LOG_D("mpu_configure_fifo complete ......\r\n");
		}

		if(!mpu_set_sample_rate(DEFAULT_MPU_HZ)) {
			LOG_D("mpu_set_sample_rate complete ......\r\n");
		}
			
		if(!dmp_load_motion_driver_firmware()) {
			LOG_D("dmp_load_motion_driver_firmware complete ......\r\n");
		}
			
		if(!dmp_set_orientation(inv_orientation_matrix_to_scalar(gyro_orientation))) {				
			LOG_D("dmp_set_orientation complete ......\r\n");
		}
		
		if(!dmp_enable_feature(DMP_FEATURE_6X_LP_QUAT | DMP_FEATURE_TAP |
	      DMP_FEATURE_ANDROID_ORIENT | DMP_FEATURE_SEND_RAW_ACCEL | DMP_FEATURE_SEND_CAL_GYRO |
	      DMP_FEATURE_GYRO_CAL)) {
			LOG_D("dmp_enable_feature complete ......\r\n");
		}
		  
		if(!dmp_set_fifo_rate(DEFAULT_MPU_HZ)) {
			LOG_D("dmp_set_fifo_rate complete ......\r\n");
		}
		
		run_self_test();
		
		if(!mpu_set_dmp_state(1)) {
			LOG_D("mpu_set_dmp_state complete ......\r\n");
		}
	}
}

static void Read_DMP(void)
{	
	unsigned long sensor_timestamp;
	unsigned char more;
	long quat[4]; // 四元数
	float q0,q1,q2,q3;
	short gyro[3], accel[3];
	short sensors;
	float roll,pitch,yaw;

	dmp_read_fifo(gyro, accel, quat, &sensor_timestamp, &sensors, &more);		
	if (sensors & INV_WXYZ_QUAT )
	{    
		 q0 = quat[0] / q30;
		 q1 = quat[1] / q30;
		 q2 = quat[2] / q30;
		 q3 = quat[3] / q30; 		
		 roll = asin(-2 * q1 * q3 + 2 * q0 * q2)* 57.3f; 	
		 pitch = atan2(2 * q2 * q3 + 2 * q0 * q1, -2 * q1 * q1 - 2 * q2 * q2 + 1)* 57.3f; 
		 yaw = atan2(2*(q1 * q2 + q0 * q3), q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3) * 57.3f;	 
	}

}
	

/**
 * @brief 设置设备时钟源
 * @param source - 时钟源编号
 * @return 
 * @note 
 * CLK_SEL | Clock Source
 * --------+--------------------------------------
 * 0       | Internal oscillator
 * 1       | PLL with X Gyro reference
 * 2       | PLL with Y Gyro reference
 * 3       | PLL with Z Gyro reference
 * 4       | PLL with external 32.768kHz reference
 * 5       | PLL with external 19.2MHz reference
 * 6       | Reserved
 * 7       | Stops the clock and keeps the timing generator in reset
 */
static uint8_t mpu6050_set_clock_source(uint8_t source){
    return iic_write_bits(devAddr, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_CLKSEL_BIT, MPU6050_PWR1_CLKSEL_LENGTH, source);
}

/**
 * @brief 设置陀螺仪量程（精度）
 * @param 
 * @return 
 * @note 
 *	MPU6050_GYRO_FS_250 → ±250 °/s
 *	MPU6050_GYRO_FS_500 → ±500 °/s
 *	MPU6050_GYRO_FS_1000 → ±1000 °/s
 *	MPU6050_GYRO_FS_2000 → ±2000 °/s
 */
static uint8_t mpu6050_set_full_scale_gyro_range(uint8_t range) {
    return iic_write_bits(devAddr, MPU6050_RA_GYRO_CONFIG, MPU6050_GCONFIG_FS_SEL_BIT, MPU6050_GCONFIG_FS_SEL_LENGTH, range);
}

/**
 * @brief 设置加速度计量程（精度）
 * @param 
 * @return 
 * @note 
 *	MPU6050_ACCEL_FS_2 → ±2G
 *	MPU6050_ACCEL_FS_4 → ±4G
 *	MPU6050_ACCEL_FS_8 → ±8G
 *	MPU6050_ACCEL_FS_16 → ±16G
 */
static uint8_t mpu6050_set_full_scale_accel_range(uint8_t range) {
    return iic_write_bits(devAddr, MPU6050_RA_ACCEL_CONFIG, MPU6050_ACONFIG_AFS_SEL_BIT, MPU6050_ACONFIG_AFS_SEL_LENGTH, range);
}

/**
 * @brief 设置是否进入睡眠模式
 * @param 
 * @return 1：睡眠；0：工作
 * @note 
 */
static uint8_t mpu6050_set_sleep_enabled(uint8_t enabled) {
    return iic_write_bit(devAddr, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_SLEEP_BIT, enabled);
}

/**
* @brief 设置mpu6050为AUX I2C主模式
 * @param 
 * @return 1：是；0：否
 * @note 
 */
static uint8_t mpu6050_set_iic_master_enabled(uint8_t enabled) {
    return iic_write_bit(devAddr, MPU6050_RA_USER_CTRL, MPU6050_USERCTRL_I2C_MST_EN_BIT, enabled);
}

/**
* @brief 设置mpu6050为AUX I2C旁路模式
 * @param 
 * @return 1：是；0：否
 * @note 
 */
static uint8_t mpu6050_set_iic_bypass_enabled(uint8_t enabled) {
    return iic_write_bit(devAddr, MPU6050_RA_INT_PIN_CFG, MPU6050_INTCFG_I2C_BYPASS_EN_BIT, enabled);
}


static int mpu6050_read_temperature(void)
{	   
	uint8_t temp_h = 0, temp_l = 0;
	iic_read(devAddr, MPU6050_RA_TEMP_OUT_H, 1, &temp_h);
	iic_read(devAddr, MPU6050_RA_TEMP_OUT_L, 1, &temp_l);
	int16_t raw = (temp_h<<8) + temp_l;
	float temp = (36.53f + (float)raw / 340.0f) * 10.0f;	 
	return (int)temp;
}


void mpu6050_get_euler_angle(mpu6050_data_t* data, uint8_t way) 
{ 
	float gyro_x, gyro_y, gyro_z; 
	float accel_x, accel_y, accel_z;	
	//float Accel_Angle_x,Accel_Angle_y;
	int16_t ax_raw, ay_raw, az_raw;
	int16_t gx_raw, gy_raw, gz_raw; 
	
	if (way == DMP)                           
	{	
//		mpu6050_dmp_data_t dmp = {0};
//		Read_DMP(&dmp);                      	 
//		data->angle_balance = dmp.Pitch;             	 	// 更新平衡倾角，前倾为正，后倾为负
//		data->gyro_balance = dmp.gyro[0];              		// 更新平衡角速度，前倾为正，后倾为负
//		data->gyro_turn = dmp.gyro[2];                 		// 更新转向角速度
//		data->accel_z = dmp.accel[2];           		 	// 更新z轴加速度
//		
//		data->Pitch = dmp.Pitch;
//		data->Roll = dmp.Roll;
//		data->Yaw = dmp.Yaw;
	}			
	else
	{
		#if 0
		// 读取陀螺仪
		uint8_t tmp_h = 0, tmp_l = 0;
		iic_read(devAddr, MPU6050_RA_GYRO_XOUT_H, 1, &tmp_h);		
		iic_read(devAddr, MPU6050_RA_GYRO_XOUT_L, 1, &tmp_l);
		
		gx_raw = (int16_t)((tmp_h<<8) + tmp_l);
		
		iic_read(devAddr, MPU6050_RA_GYRO_YOUT_H, 1, &tmp_h);		
		iic_read(devAddr, MPU6050_RA_GYRO_YOUT_L, 1, &tmp_l);
		
		gy_raw = (int16_t)((tmp_h<<8) + tmp_l);
		
		iic_read(devAddr, MPU6050_RA_GYRO_ZOUT_H, 1, &tmp_h);	
		iic_read(devAddr, MPU6050_RA_GYRO_ZOUT_L, 1, &tmp_l);
		
		gz_raw = (int16_t)((tmp_h<<8) + tmp_l);

		// 读取加速度计
		iic_read(devAddr, MPU6050_RA_ACCEL_XOUT_H, 1, &tmp_h);
		iic_read(devAddr, MPU6050_RA_ACCEL_XOUT_L, 1, &tmp_l);
		
		ax_raw = (int16_t)((tmp_h<<8) + tmp_l);
		
		iic_read(devAddr, MPU6050_RA_ACCEL_YOUT_H, 1, &tmp_h);
		iic_read(devAddr, MPU6050_RA_ACCEL_YOUT_L, 1, &tmp_l);
		
		ay_raw = (int16_t)((tmp_h<<8) + tmp_l);
		
		iic_read(devAddr, MPU6050_RA_ACCEL_ZOUT_H, 1, &tmp_h);
		iic_read(devAddr, MPU6050_RA_ACCEL_ZOUT_L, 1, &tmp_l);
		
		az_raw = (int16_t)((tmp_h<<8) + tmp_l);
		#else
		uint8_t tmp[14]; // MPU6050_RA_ACCEL_XOUT_H - MPU6050_RA_GYRO_ZOUT_L
		iic_read(devAddr, MPU6050_RA_ACCEL_XOUT_H, 14, tmp);
		ax_raw = (int16_t)((tmp[0]<<8) + tmp[1]);
		ay_raw = (int16_t)((tmp[2]<<8) + tmp[3]);
		az_raw = (int16_t)((tmp[4]<<8) + tmp[5]);
		
		int16_t raw = (int16_t)((tmp[6]<<8) + tmp[7]);
		
		// MPU6050
		data->temperature = (int16_t)((36.53f + (float)raw / 340.0f) * 10.0f); 
		// MPU6500
		// (((float)raw - 0) / 333.87f) + 21
		
		gx_raw = (int16_t)((tmp[8]<<8) + tmp[9]);		
		gy_raw = (int16_t)((tmp[10]<<8) + tmp[11]);		
		gz_raw = (int16_t)((tmp[12]<<8) + tmp[13]);
		#endif
                           
		accel_x = (float)ax_raw / 1671.84f;
		accel_y = (float)ay_raw / 1671.84f;
		accel_z = (float)az_raw / 1671.84f;
		
		gyro_x = (float)gx_raw / 939.8;                              
		gyro_y = (float)gy_raw / 939.8; 
		gyro_z = (float)gz_raw / 939.8;		
		
		if (way == KALMAN)		  	
		{
			 data->pitch = KF_X(accel_y ,accel_z, -gyro_x) / PI * 180.0f;
			 data->roll = KF_Y(accel_x, accel_z, gyro_y) / PI * 180.0f;
		}
		else if (way == COMPLEMENTARY) 
		{  
			 //Pitch = -Complementary_Filter_x(Accel_Angle_x,gx_raw);//
			 //Roll = -Complementary_Filter_y(Accel_Angle_y,gy_raw);
		}
		data->angle_balance = data->pitch; 
		data->gyro_balance = -gx_raw; 		
		data->gyro_turn = gz_raw;                             
		data->accel_z = az_raw;  
		//LOG_D("angle_balance = %f ......\r\n", data->angle_balance);		
	}
}



void mpu6050_init(void)
{
	// 时钟复位
	mpu6050_set_clock_source(MPU6050_CLOCK_PLL_YGYRO);
	
	// 设置陀螺仪量程
	mpu6050_set_full_scale_gyro_range(MPU6050_GYRO_FS_2000);
	
	// 设置加速度计量程
	mpu6050_set_full_scale_accel_range(MPU6050_ACCEL_FS_2);
	
	// 关闭睡眠模式
	mpu6050_set_sleep_enabled(0);
	
	// 关闭AUX功能
	mpu6050_set_iic_master_enabled(0);
	mpu6050_set_iic_bypass_enabled(0);
	
	DMP_init(); // 里面有包含触发中断的设置，不能屏蔽
	
}


