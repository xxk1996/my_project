#ifndef __JSPOOL_H__
#define __JSPOOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "list.h"

// 错误码定义
#define JSPOOL_OK                   0      // 成功
#define JSPOOL_ERROR               -1      // 通用错误
#define JSPOOL_ERROR_NOMEM         -2      // 内存不足
#define JSPOOL_ERROR_BUSY          -3      // 忙状态
#define JSPOOL_ERROR_TIMEOUT       -4      // 超时
#define JSPOOL_ERROR_INVALID       -5      // 无效参数
#define JSPOOL_ERROR_NOTFOUND      -6      // 未找到
#define JSPOOL_ERROR_ADDR          -7      // 地址错误
#define JSPOOL_ERROR_PRINT(func_name, line_num, error_code) \
    printf("[JSPOOL_ERROR] %s:%d error: %d\n", func_name, line_num, error_code)
// 模块句柄（不透明指针）
typedef struct jspool_module *jspool_handle_t;

// ==================== 公开API接口 ====================

/**
 * @brief 初始化JSPool管理器
 * @return 错误码
 */
int32_t jspool_init(void);

/**
 * @brief 反初始化JSPool管理器
 * @return 错误码
 */
int32_t jspool_deinit(void);

/**
 * @brief 注册模块到JSPool
 * @param name 模块名称
 * @param mem_size 内存大小（字节）
 * @return 模块句柄，失败返回NULL
 */
jspool_handle_t jspool_register(const char *name, uint16_t mem_size);

/**
 * @brief 注销模块
 * @param handle 模块句柄
 * @return 错误码
 */
int32_t jspool_unregister(jspool_handle_t handle);

/**
 * @brief 查找模块
 * @param name 模块名称
 * @return 模块句柄，未找到返回NULL
 */
jspool_handle_t jspool_find(const char *name);

/**
 * @brief 从模块读取数据
 * @param handle 模块句柄
 * @param offset 模块内偏移量（字节）
 * @param buffer 数据缓冲区
 * @param size 数据大小（字节）
 * @return 实际读取字节数，负数为错误码
 */
int32_t jspool_read(jspool_handle_t handle, uint16_t offset, 
                    void *buffer, uint16_t size);

/**
 * @brief 向模块写入数据
 * @param handle 模块句柄
 * @param offset 模块内偏移量（字节）
 * @param buffer 数据缓冲区
 * @param size 数据大小（字节）
 * @return 实际写入字节数，负数为错误码
 */
int32_t jspool_write(jspool_handle_t handle, uint16_t offset,
                     const void *buffer, uint16_t size);

/**
 * @brief 直接获取模块内存指针（危险操作，需谨慎使用）
 * @param handle 模块句柄
 * @return 内存指针，失败返回NULL
 */
void *jspool_get_memory(jspool_handle_t handle);

/**
 * @brief 获取模块内存大小
 * @param handle 模块句柄
 * @return 内存大小，失败返回0
 */
uint16_t jspool_get_mem_size(jspool_handle_t handle);

/**
 * @brief 获取模块名称
 * @param handle 模块句柄
 * @return 模块名称，失败返回NULL
 */
const char *jspool_get_name(jspool_handle_t handle);

/**
 * @brief 遍历所有模块
 * @param iterator 迭代器函数
 * @param arg 迭代器参数
 * @return 遍历的模块数量
 */
int32_t jspool_foreach(int32_t (*iterator)(jspool_handle_t handle, void *arg), void *arg);

/**
 * @brief 设置锁超时时间
 * @param timeout 超时时间（毫秒）
 * @return 错误码
 */
int32_t jspool_set_timeout(uint32_t timeout);

/**
 * @brief 获取模块数量
 * @return 模块数量
 */
uint16_t jspool_get_module_count(void);

/**
 * @brief 清除模块内存数据
 * @param handle 模块句柄
 * @return 错误码
 */
int32_t jspool_clear(jspool_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* __JSPOOL_H__ */
