#ifndef __ISR_APP_H_
#define __ISR_APP_H_

#include <stdint.h>

typedef struct {
	uint8_t way_angle;
	float angle_balance;			// 平衡角度
	float gyro_balance;			// 平衡角速度
	uint16_t distance;			// 测距
	float motor_left, motor_right;	// 电机输出PWM
	float vel_left, vel_right;		// 编码器速度
	
} wheel_data_t;

#endif  
	 
