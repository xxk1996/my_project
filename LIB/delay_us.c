#include "stm32f1xx.h"
#include "delay_us.h"

// 需要在时钟初始化后调用
void delay_us(uint32_t us)
{
    uint32_t total_ticks = 0;         
    // 需要等待的tick
    uint32_t target_ticks = (HAL_RCC_GetSysClockFreq() / 1000000U) * us;

    int32_t last_val = SysTick->VAL;    // 上次读取的计数值（递减）
    int32_t current_val;                // 当前计数值
    int32_t diff_ticks;                 

    while(1) {
        current_val = SysTick->VAL;      
        diff_ticks = last_val - current_val; 

        if(diff_ticks > 0) {
            total_ticks += diff_ticks;
        } else {
            total_ticks += diff_ticks + SysTick->LOAD;
        }

        if(total_ticks >= target_ticks) {
            return;  // 延时到时
        }
        last_val = current_val;
    }
}

