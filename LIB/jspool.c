#include "jspool.h"
#include <stdlib.h>
#include <stdio.h>

// ==================== 内部结构定义 ====================

// 内部模块结构
struct jspool_module {
    char name[32];                       // 模块名称
    uint16_t mem_size;                   // 模块内存大小（字节）
    void *memory;                        // 模块内存指针
    
    // FreeRTOS链表节点
    struct {
        void *pvOwner;                   // 指向模块自身
        struct xLIST_ITEM *pxContainer;  // 容器指针（内部使用）
    } node;
    
    // 同步机制
    SemaphoreHandle_t lock;              // 模块互斥锁
    TickType_t lock_timeout;             // 锁超时时间
    
    // 状态标志
    uint8_t flag;                        // 模块标志
#define JSPOOL_FLAG_ACTIVE        (1 << 0) // 模块激活
#define JSPOOL_FLAG_INIT          (1 << 1) // 模块初始化
};

// JSPool管理器
typedef struct {
    List_t modules_list;                 // 模块链表（使用FreeRTOS List）
    uint16_t module_count;               // 模块数量
    SemaphoreHandle_t lock;              // 全局互斥锁
    char name[16];                       // 管理器名称
} jspool_manager_t;

// ==================== 静态全局变量 ====================

static jspool_manager_t g_jspool_manager = {
    .modules_list = {0},
    .module_count = 0,
    .lock = NULL,
    .name = "JSPOOL_MANAGER"
};

static TickType_t g_lock_timeout = pdMS_TO_TICKS(1000); // 默认1秒超时

// ==================== 错误打印宏定义 ====================



#define JSPOOL_CHECK_RETURN(condition, ret_code) \
    do { \
        if (!(condition)) { \
            JSPOOL_ERROR_PRINT(__func__, __LINE__, ret_code); \
            return ret_code; \
        } \
    } while(0)

#define JSPOOL_CHECK_RETURN_NULL(condition) \
    do { \
        if (!(condition)) { \
            JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_INVALID); \
            return NULL; \
        } \
    } while(0)

// ==================== 静态函数声明 ====================

static jspool_handle_t jspool_find_module_locked(const char *name);
static int32_t jspool_take_lock(SemaphoreHandle_t lock);
static void jspool_release_lock(SemaphoreHandle_t lock);
static void jspool_destroy_module(jspool_handle_t handle);
static int32_t jspool_check_handle(jspool_handle_t handle);
static int32_t jspool_check_offset(jspool_handle_t handle, uint16_t offset, uint16_t size);

// ==================== 公共API实现 ====================

int32_t jspool_init(void)
{
    // 检查是否已初始化
    if (g_jspool_manager.lock != NULL) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_BUSY);
        return JSPOOL_ERROR_BUSY;
    }
    
    // 创建全局互斥锁
    g_jspool_manager.lock = xSemaphoreCreateMutex();
    JSPOOL_CHECK_RETURN(g_jspool_manager.lock != NULL, JSPOOL_ERROR_NOMEM);
    
    // 初始化FreeRTOS链表
    vListInitialise(&g_jspool_manager.modules_list);
    
    // 初始化管理器
    g_jspool_manager.module_count = 0;
    
    printf("[JSPOOL_INFO] %s: Initialized successfully \r\n", __func__);
    return JSPOOL_OK;
}

int32_t jspool_deinit(void)
{
    jspool_handle_t handle = NULL;
    ListItem_t *item = NULL;
    const ListItem_t *list_end = NULL;
    
    // 获取全局锁
    if (jspool_take_lock(g_jspool_manager.lock) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_BUSY);
        return JSPOOL_ERROR_BUSY;
    }
    
    // 获取链表结束标记
    list_end = listGET_END_MARKER(&g_jspool_manager.modules_list);
    
    // 遍历并销毁所有模块
    while (listLIST_IS_EMPTY(&g_jspool_manager.modules_list) == pdFALSE) {
        // 获取链表第一个节点
        item = listGET_HEAD_ENTRY(&g_jspool_manager.modules_list);
        if (item != NULL && item != list_end) {
            handle = (jspool_handle_t)listGET_LIST_ITEM_OWNER(item);
            
            // 从链表中移除
            uxListRemove(item);
            vPortFree(item);
            
            // 销毁模块
            if (handle != NULL) {
                jspool_destroy_module(handle);
            }
            
            g_jspool_manager.module_count--;
        } else {
            break; // 到达链表结束标记
        }
    }
    
    // 释放全局锁
    if (g_jspool_manager.lock != NULL) {
        vSemaphoreDelete(g_jspool_manager.lock);
        g_jspool_manager.lock = NULL;
    }
    
    // 重新初始化链表
    vListInitialise(&g_jspool_manager.modules_list);
    g_jspool_manager.module_count = 0;
    
    printf("[JSPOOL_INFO] %s: Deinitialized successfully \r\n", __func__);
    return JSPOOL_OK;
}

jspool_handle_t jspool_register(const char *name, uint16_t mem_size)
{
    jspool_handle_t handle = NULL;
    ListItem_t *list_item = NULL;
    
    // 参数检查
    JSPOOL_CHECK_RETURN_NULL(name != NULL && mem_size > 0 && mem_size < 1024);
    
    // 获取全局锁
    if (jspool_take_lock(g_jspool_manager.lock) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_BUSY);
        return NULL;
    }
    
    // 检查模块是否已存在
    if (jspool_find_module_locked(name) != NULL) {
        jspool_release_lock(g_jspool_manager.lock);
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_BUSY);
        return NULL;
    }
    
    // 分配模块内存
    handle = (jspool_handle_t)pvPortMalloc(sizeof(struct jspool_module));
    JSPOOL_CHECK_RETURN_NULL(handle != NULL);
    memset(handle, 0, sizeof(struct jspool_module));
    
    // 分配模块数据内存
    handle->memory = pvPortMalloc(mem_size);
    if (handle->memory == NULL) {
        vPortFree(handle);
        jspool_release_lock(g_jspool_manager.lock);
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_NOMEM);
        return NULL;
    }
    memset(handle->memory, 0, mem_size);
    
    // 分配链表节点内存
    list_item = (ListItem_t *)pvPortMalloc(sizeof(ListItem_t));
    if (list_item == NULL) {
        vPortFree(handle->memory);
        vPortFree(handle);
        jspool_release_lock(g_jspool_manager.lock);
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_NOMEM);
        return NULL;
    }
    
    // 初始化模块
    strncpy(handle->name, name, sizeof(handle->name) - 1);
    handle->mem_size = mem_size;
    
    // 创建模块锁
    handle->lock = xSemaphoreCreateMutex();
    if (handle->lock == NULL) {
        vPortFree(list_item);
        vPortFree(handle->memory);
        vPortFree(handle);
        jspool_release_lock(g_jspool_manager.lock);
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_NOMEM);
        return NULL;
    }
    
    handle->lock_timeout = g_lock_timeout;
    
    // 初始化链表节点
    vListInitialiseItem(list_item);
    list_item->pvOwner = handle;
    listSET_LIST_ITEM_VALUE(list_item, (TickType_t)handle);
    
    // 初始化模块节点
    handle->node.pvOwner = handle;
    handle->node.pxContainer = list_item;
    
    // 初始化模块其他字段
    handle->flag = JSPOOL_FLAG_ACTIVE | JSPOOL_FLAG_INIT;
    
    // 添加到链表中
    vListInsertEnd(&g_jspool_manager.modules_list, list_item);
    g_jspool_manager.module_count++;
    
    jspool_release_lock(g_jspool_manager.lock);
    
    printf("[JSPOOL_INFO] %s: Registered module '%s' (%d bytes) \r\n", 
           __func__, name, mem_size);
    return handle;
}

int32_t jspool_unregister(jspool_handle_t handle)
{
    ListItem_t *list_item = NULL;
    const ListItem_t *list_end = NULL;
    
    // 检查句柄有效性
    if (jspool_check_handle(handle) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_INVALID);
        return JSPOOL_ERROR_INVALID;
    }
    
    // 获取全局锁
    if (jspool_take_lock(g_jspool_manager.lock) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_BUSY);
        return JSPOOL_ERROR_BUSY;
    }
    
    // 获取链表结束标记
    list_end = listGET_END_MARKER(&g_jspool_manager.modules_list);
    
    // 查找链表节点（从第一个有效节点开始）
    list_item = listGET_HEAD_ENTRY(&g_jspool_manager.modules_list);
    while (list_item != list_end) {
        if ((jspool_handle_t)listGET_LIST_ITEM_OWNER(list_item) == handle) {
            // 从链表中移除
            uxListRemove(list_item);
            
            // 释放链表节点内存
            vPortFree(list_item);
            
            // 销毁模块
            jspool_destroy_module(handle);
            
            g_jspool_manager.module_count--;
            
            jspool_release_lock(g_jspool_manager.lock);
            
            printf("[JSPOOL_INFO] %s: Unregistered module '%s' \r\n", 
                   __func__, handle->name);
            return JSPOOL_OK;
        }
        list_item = listGET_NEXT(list_item);
    }
    
    jspool_release_lock(g_jspool_manager.lock);
    JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_NOTFOUND);
    return JSPOOL_ERROR_NOTFOUND;
}

jspool_handle_t jspool_find(const char *name)
{
    jspool_handle_t handle = NULL;
    
    JSPOOL_CHECK_RETURN_NULL(name != NULL);
    
    // 获取全局锁
    if (jspool_take_lock(g_jspool_manager.lock) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_BUSY);
        return NULL;
    }
    
    handle = jspool_find_module_locked(name);
    
    jspool_release_lock(g_jspool_manager.lock);
    
    return handle;
}

int32_t jspool_read(jspool_handle_t handle, uint16_t offset, 
                    void *buffer, uint16_t size)
{
    // 参数检查
    JSPOOL_CHECK_RETURN(buffer != NULL && size > 0, JSPOOL_ERROR_INVALID);
    
    // 检查句柄有效性
    if (jspool_check_handle(handle) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_INVALID);
        return JSPOOL_ERROR_INVALID;
    }
    
    // 检查偏移量范围
    if (jspool_check_offset(handle, offset, size) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_ADDR);
        return JSPOOL_ERROR_ADDR;
    }
    
    // 获取模块锁
    if (jspool_take_lock(handle->lock) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_BUSY);
        return JSPOOL_ERROR_BUSY;
    }
    
    // 读取数据
    memcpy(buffer, (uint8_t *)handle->memory + offset, size);
    
    jspool_release_lock(handle->lock);
    
    return size;
}

int32_t jspool_write(jspool_handle_t handle, uint16_t offset,
                     const void *buffer, uint16_t size)
{
    // 参数检查
    JSPOOL_CHECK_RETURN(buffer != NULL && size > 0, JSPOOL_ERROR_INVALID);
    
    // 检查句柄有效性
    if (jspool_check_handle(handle) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_INVALID);
        return JSPOOL_ERROR_INVALID;
    }
    
    // 检查偏移量范围
    if (jspool_check_offset(handle, offset, size) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_ADDR);
        return JSPOOL_ERROR_ADDR;
    }
    
    // 获取模块锁
    if (jspool_take_lock(handle->lock) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_BUSY);
        return JSPOOL_ERROR_BUSY;
    }
    
    // 写入数据
    memcpy((uint8_t *)handle->memory + offset, buffer, size);
    
    jspool_release_lock(handle->lock);
    
    return size;
}

void *jspool_get_memory(jspool_handle_t handle)
{
    // 检查句柄有效性
    if (jspool_check_handle(handle) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_INVALID);
        return NULL;
    }
    
    // 注意：返回的内存指针需要用户自己保证线程安全
    return handle->memory;
}

uint16_t jspool_get_mem_size(jspool_handle_t handle)
{
    // 检查句柄有效性
    if (jspool_check_handle(handle) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_INVALID);
        return 0;
    }
    
    return handle->mem_size;
}

const char *jspool_get_name(jspool_handle_t handle)
{
    // 检查句柄有效性
    if (jspool_check_handle(handle) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_INVALID);
        return NULL;
    }
    
    return handle->name;
}

int32_t jspool_foreach(int32_t (*iterator)(jspool_handle_t handle, void *arg), void *arg)
{
    ListItem_t *list_item = NULL;
    const ListItem_t *list_end = NULL;
    jspool_handle_t handle = NULL;
    int32_t count = 0;
    
    JSPOOL_CHECK_RETURN(iterator != NULL, JSPOOL_ERROR_INVALID);
    
    // 获取全局锁
    if (jspool_take_lock(g_jspool_manager.lock) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_BUSY);
        return JSPOOL_ERROR_BUSY;
    }
    
    // 获取链表结束标记
    list_end = listGET_END_MARKER(&g_jspool_manager.modules_list);
    
    // 遍历链表
    list_item = listGET_HEAD_ENTRY(&g_jspool_manager.modules_list);
    while (list_item != list_end) {
        handle = (jspool_handle_t)listGET_LIST_ITEM_OWNER(list_item);
        if (handle != NULL) {
            if (iterator(handle, arg) == JSPOOL_OK) {
                count++;
            }
        }
        list_item = listGET_NEXT(list_item);
    }
    
    jspool_release_lock(g_jspool_manager.lock);
    
    return count;
}

int32_t jspool_set_timeout(uint32_t timeout)
{
    JSPOOL_CHECK_RETURN(timeout > 0, JSPOOL_ERROR_INVALID);
    
    g_lock_timeout = pdMS_TO_TICKS(timeout);
    
    printf("[JSPOOL_INFO] %s: Set lock timeout to %lu ms \r\n", 
           __func__, timeout);
    return JSPOOL_OK;
}

uint16_t jspool_get_module_count(void)
{
    uint16_t count = 0;
    
    // 获取全局锁
    if (jspool_take_lock(g_jspool_manager.lock) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_BUSY);
        return 0;
    }
    
    count = g_jspool_manager.module_count;
    
    jspool_release_lock(g_jspool_manager.lock);
    
    return count;
}

int32_t jspool_clear(jspool_handle_t handle)
{
    // 检查句柄有效性
    if (jspool_check_handle(handle) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_INVALID);
        return JSPOOL_ERROR_INVALID;
    }
    
    // 获取模块锁
    if (jspool_take_lock(handle->lock) != JSPOOL_OK) {
        JSPOOL_ERROR_PRINT(__func__, __LINE__, JSPOOL_ERROR_BUSY);
        return JSPOOL_ERROR_BUSY;
    }
    
    // 清除内存
    if (handle->memory != NULL) {
        memset(handle->memory, 0, handle->mem_size);
    }
    
    jspool_release_lock(handle->lock);
    
    printf("[JSPOOL_INFO] %s: Cleared module '%s' memory \r\n", 
           __func__, handle->name);
    return JSPOOL_OK;
}

// ==================== 静态函数实现 ====================

static jspool_handle_t jspool_find_module_locked(const char *name)
{
    ListItem_t *list_item = NULL;
    const ListItem_t *list_end = NULL;
    jspool_handle_t handle = NULL;
    
    // 获取链表结束标记
    list_end = listGET_END_MARKER(&g_jspool_manager.modules_list);
    
    // 遍历链表
    list_item = listGET_HEAD_ENTRY(&g_jspool_manager.modules_list);
    while (list_item != list_end) {
        handle = (jspool_handle_t)listGET_LIST_ITEM_OWNER(list_item);
        if (handle != NULL && strcmp(handle->name, name) == 0) {
            return handle;
        }
        list_item = listGET_NEXT(list_item);
    }
    
    return NULL;
}

static int32_t jspool_take_lock(SemaphoreHandle_t lock)
{
    if (lock == NULL) {
        printf("[JSPOOL_ERROR] %s:%d: Lock is NULL \r\n", __func__, __LINE__);
        return JSPOOL_ERROR_INVALID;
    }
    
    if (xSemaphoreTake(lock, g_lock_timeout) == pdTRUE) {
        return JSPOOL_OK;
    }
    
    printf("[JSPOOL_ERROR] %s:%d: Lock timeout \r\n", __func__, __LINE__);
    return JSPOOL_ERROR_TIMEOUT;
}

static void jspool_release_lock(SemaphoreHandle_t lock)
{
    if (lock != NULL) {
        xSemaphoreGive(lock);
    }
}

static void jspool_destroy_module(jspool_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    
    // 释放模块内存
    if (handle->memory != NULL) {
        vPortFree(handle->memory);
    }
    
    // 释放互斥锁
    if (handle->lock != NULL) {
        vSemaphoreDelete(handle->lock);
    }
    
    // 释放模块结构
    vPortFree(handle);
}

static int32_t jspool_check_handle(jspool_handle_t handle)
{
    if (handle == NULL || !(handle->flag & JSPOOL_FLAG_ACTIVE)) {
        return JSPOOL_ERROR_INVALID;
    }
    return JSPOOL_OK;
}

static int32_t jspool_check_offset(jspool_handle_t handle, uint16_t offset, uint16_t size)
{
    if (offset + size > handle->mem_size) {
        printf("[JSPOOL_ERROR] %s:%d: Offset %u + Size %u > Memory Size %u \r\n", 
               __func__, __LINE__, offset, size, handle->mem_size);
        return JSPOOL_ERROR_ADDR;
    }
    return JSPOOL_OK;
}
