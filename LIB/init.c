#include "init.h"
#include <stdio.h>

/* 
这里使用指针数组的原因是在遍历时只有数组名作为指针才能累加
这里声明是由编译器生成的段边界
*/
extern __attribute__((weak)) init_fn_t _init_0$$Base[];
extern __attribute__((weak)) init_fn_t _init_0$$Limit[];
extern __attribute__((weak)) init_fn_t _init_1$$Base[];
extern __attribute__((weak)) init_fn_t _init_1$$Limit[];
extern __attribute__((weak)) init_fn_t _init_2$$Base[];
extern __attribute__((weak)) init_fn_t _init_2$$Limit[];
extern __attribute__((weak)) init_fn_t _init_3$$Base[];
extern __attribute__((weak)) init_fn_t _init_3$$Limit[];

typedef struct {
    init_fn_t *start;   // 指向段起始的指针
    init_fn_t *end;     // 指向段结束的指针
} init_section_t;

static const init_section_t sections[] = {
    {_init_0$$Base, _init_0$$Limit},
    {_init_1$$Base, _init_1$$Limit},
    {_init_2$$Base, _init_2$$Limit},
    {_init_3$$Base, _init_3$$Limit},
};

	
void func_auto_init(void)
{
	for (int i = 0; i < sizeof(sections)/sizeof(sections[0]); i++) 
	{
        init_fn_t *start = sections[i].start;
        init_fn_t *end   = sections[i].end;
        
        // 检查指针有效性（弱引用时可能为 0）
        if (start && end) {
            for (init_fn_t *fn = start; fn < end; fn++) {
                if (*fn) {
                    (*fn)();
                }
            }
        }
    }
}

