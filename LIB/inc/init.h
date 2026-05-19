#ifndef _INIT_H_ 
#define _INIT_H_ 

typedef void (*init_fn_t)(void);
	
/*
fn：传入函数名
__init_##fn：函数指针变量
__attribute__((used, section("_init_0"))) = fn
used：告诉编译器即使变量未被引用也不要优化掉
section("_init_0")：将变量放在名为 _init_0 的段（section）中

内存布局：
内存中有一个段叫 "_init_0"
其中包含：
[0x20000180] __init_sysinit = &sysinit
[0x20000184] __init_other_func = &other_func
...

链接器视角：
链接器收集所有目标文件中 section("_init_0") 的变量
将它们放在一起，形成连续的内存区域
生成符号信息供程序访问

展开后
static init_fn_t __init_gpio_init __attribute__((used, section("_init_0"))) = gpio_init;
创建对应函数gpio_init的函数指针变量__init_gpio_init并存入段中

段默认放在ram中
在使用const修饰后放在flash中
*/

#define DEFINE_INIT(fn, id) \
	static const init_fn_t __init_##fn##id \
	__attribute__((used, section("_init_"#id))) = fn
	
#define INIT_BOARD_EXPORT(fn)  DEFINE_INIT(fn, 0)
#define INIT_DRIVER_EXPORT(fn) DEFINE_INIT(fn, 1)
#define INIT_DEVICE_EXPORT(fn) DEFINE_INIT(fn, 2)
#define INIT_APP_EXPORT(fn)    DEFINE_INIT(fn, 3)


void func_auto_init(void);

#endif // _INIT_H_
	

