#ifndef __USER_LIB_H
#define __USER_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

//#include "stdint.h"
#include <stdio.h>

#define ARR_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
	
#define BIT(n)  (1UL << (n))
#define WRITE_BIT(var, bit, set) ((var) = (set) ? ((var) | BIT(bit)) : ((var) & ~BIT(bit)))
#define GET_BIT(x,y) ((x>>y)&1)

#define CLAMP(value, min, max) \
    ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))

/* 断言宏，调试时启用，Release时禁用 */
#define ASSERT_DEUG

#ifdef ASSERT_DEUG
    #define ASSERT(expr) \
        do { \
            if (!(expr)) { \
                printf("Assertion failed: (%s), file %s, line %d, function: %s\n", \
                       #expr, __FILE__, __LINE__, __FUNCTION__); \
                while (1) {} \
            } \
        } while (0)
#else
    #define ASSERT(expr)          ((void)0)
#endif


#ifdef __cplusplus
}
#endif

#endif /* __USER_LIB_H */
