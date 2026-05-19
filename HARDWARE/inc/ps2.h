#ifndef __PS2_H_
#define __PS2_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
	int PS2_LX; // 0 - 255
	int PS2_LY; // 0 - 225
	int PS2_RX; // 0 - 255
	int PS2_RY; // 0 - 255
	
	union {
		uint16_t PS2_KEY;
		
        struct {
            uint16_t select   :1;
            uint16_t l3       :1;
            uint16_t r3       :1;
            uint16_t start    :1;
            uint16_t pad_up   :1;
            uint16_t pad_right:1;
            uint16_t pad_down :1;
            uint16_t pad_left :1;
            uint16_t l2       :1;
            uint16_t r2       :1;
            uint16_t l1       :1;
            uint16_t r1       :1;
            uint16_t green    :1;
            uint16_t red      :1;
            uint16_t blue     :1;
            uint16_t pink     :1;
        } bit;
	} key;
} ps2_data_t;
	   
	         
void PS2_SetInit(void);		    


#endif /* __PS2_H_ */                 
