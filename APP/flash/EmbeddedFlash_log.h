/*
 * EmbeddedFlash Log - 日志系统头文件
 * 
 * 此文件提供编译期日志消除功能，根据 EFLASH_LOG_LEVEL 在编译时移除不需要的日志字符串
 * 从而减少 Flash 占用。
 * 
 * 使用方法：
 * 1. 在 EmbeddedFlash_config.h 中设置 EFLASH_LOG_LEVEL
 * 2. 使用 EFLASH_LOGD/I/W/E 等宏进行日志输出
 * 3. 被禁用的日志级别对应的字符串字面量不会被编译进 Flash
 */

#ifndef __EMBEDDED_FLASH_LOG_H__
#define __EMBEDDED_FLASH_LOG_H__

#include "EmbeddedFlash_config.h"
#include <stdio.h>
#include <stdint.h>

/* ==================== 日志标签配置 ==================== */
#ifndef EFLASH_LOG_TAG
#define EFLASH_LOG_TAG "EF"
#endif

/* ==================== 编译期日志级别判断 ==================== */
// 根据 EFLASH_LOG_LEVEL 在编译时判断哪些日志级别需要启用
// 逻辑：如果 level >= EFLASH_LOG_LEVEL，则启用该级别
// 例如：EFLASH_LOG_LEVEL=INFO(2) 时，启用 ERROR(4), WARN(3), INFO(2)，禁用 DEBUG(1), ALL(0)

#if EFLASH_LOG_LEVEL <= EFLASH_LOG_LEVEL_ALL
    #define _EFLASH_LOG_ENABLE_ALL   1
#else
    #define _EFLASH_LOG_ENABLE_ALL   0
#endif

#if EFLASH_LOG_LEVEL <= EFLASH_LOG_LEVEL_DEBUG
    #define _EFLASH_LOG_ENABLE_DEBUG 1
#else
    #define _EFLASH_LOG_ENABLE_DEBUG 0
#endif

#if EFLASH_LOG_LEVEL <= EFLASH_LOG_LEVEL_INFO
    #define _EFLASH_LOG_ENABLE_INFO  1
#else
    #define _EFLASH_LOG_ENABLE_INFO  0
#endif

#if EFLASH_LOG_LEVEL <= EFLASH_LOG_LEVEL_WARN
    #define _EFLASH_LOG_ENABLE_WARN  1
#else
    #define _EFLASH_LOG_ENABLE_WARN  0
#endif

#if EFLASH_LOG_LEVEL <= EFLASH_LOG_LEVEL_ERROR
    #define _EFLASH_LOG_ENABLE_ERROR 1
#else
    #define _EFLASH_LOG_ENABLE_ERROR 0
#endif

/* ==================== 基础日志宏 ==================== */
// 基础日志宏始终定义，内部通过运行时判断决定是否打印
// 编译期消除通过便捷宏（EFLASH_LOGD/I/W/E）来实现
#ifndef EFLASH_LOG
    #define EFLASH_LOG(level, fmt, ...)                                                       \
        do {                                                                                  \
            if ((level) >= EFLASH_LOG_LEVEL && (level) < EFLASH_LOG_LEVEL_NONE) {             \
                printf("[" EFLASH_LOG_TAG "][%s] " fmt,                            \
                       (level)==EFLASH_LOG_LEVEL_DEBUG ? "DEBUG" :                            \
                       (level)==EFLASH_LOG_LEVEL_INFO  ? "INFO " :                            \
                       (level)==EFLASH_LOG_LEVEL_WARN  ? "WARN " :                            \
                       (level)==EFLASH_LOG_LEVEL_ERROR ? "ERROR" : "ALL  ",                   \
                       ##__VA_ARGS__);                                                        \
                printf("\r\n");                                                                      \
                fflush(stdout);                                                                     \
            }                                                                                         \
        } while(0)
#endif

/* ==================== 便捷日志宏（编译期消除） ==================== */
// 根据日志级别在编译时决定是否编译日志代码和字符串字面量

#if _EFLASH_LOG_ENABLE_ALL
    #define EFLASH_LOGA(fmt, ...) EFLASH_LOG(EFLASH_LOG_LEVEL_ALL,   fmt, ##__VA_ARGS__)
#else
    #define EFLASH_LOGA(fmt, ...) ((void)0)
#endif

#if _EFLASH_LOG_ENABLE_DEBUG
    #define EFLASH_LOGD(fmt, ...) EFLASH_LOG(EFLASH_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#else
    #define EFLASH_LOGD(fmt, ...) ((void)0)
#endif

#if _EFLASH_LOG_ENABLE_INFO
    #define EFLASH_LOGI(fmt, ...) EFLASH_LOG(EFLASH_LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#else
    #define EFLASH_LOGI(fmt, ...) ((void)0)
#endif

#if _EFLASH_LOG_ENABLE_WARN
    #define EFLASH_LOGW(fmt, ...) EFLASH_LOG(EFLASH_LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#else
    #define EFLASH_LOGW(fmt, ...) ((void)0)
#endif

#if _EFLASH_LOG_ENABLE_ERROR
    #define EFLASH_LOGE(fmt, ...) EFLASH_LOG(EFLASH_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#else
    #define EFLASH_LOGE(fmt, ...) ((void)0)
#endif

/* ==================== HEX 数据打印宏（编译期消除） ==================== */
// 基础 HEX 打印宏始终定义，编译期消除通过便捷宏来实现
#ifndef EFLASH_PRINT_HEX
    #define EFLASH_PRINT_HEX(level, desc, buf, len) \
        do { \
            if ((level) >= EFLASH_LOG_LEVEL && (level) < EFLASH_LOG_LEVEL_NONE) {             \
                printf("HEX[%s]: [", (desc)); \
                for (uint32_t _i = 0; _i < (len); ++_i) { \
                    printf("%02X ", (uint8_t)((buf)[_i])); \
                } \
                printf("]\n"); \
                fflush(stdout); \
            } \
        } while(0)
#endif

/* 便捷宏：不同等级的 HEX 打印（编译期消除） */
#if _EFLASH_LOG_ENABLE_DEBUG
    #define EFLASH_LOGD_PRINT_HEX(desc, buf, len) EFLASH_PRINT_HEX(EFLASH_LOG_LEVEL_DEBUG, desc, buf, len)
#else
    #define EFLASH_LOGD_PRINT_HEX(desc, buf, len) ((void)0)
#endif

#if _EFLASH_LOG_ENABLE_INFO
    #define EFLASH_LOGI_PRINT_HEX(desc, buf, len)  EFLASH_PRINT_HEX(EFLASH_LOG_LEVEL_INFO,  desc, buf, len)
#else
    #define EFLASH_LOGI_PRINT_HEX(desc, buf, len)  ((void)0)
#endif

#if _EFLASH_LOG_ENABLE_WARN
    #define EFLASH_LOGW_PRINT_HEX(desc, buf, len)  EFLASH_PRINT_HEX(EFLASH_LOG_LEVEL_WARN,  desc, buf, len)
#else
    #define EFLASH_LOGW_PRINT_HEX(desc, buf, len)  ((void)0)
#endif

#if _EFLASH_LOG_ENABLE_ERROR
    #define EFLASH_LOGE_PRINT_HEX(desc, buf, len)  EFLASH_PRINT_HEX(EFLASH_LOG_LEVEL_ERROR, desc, buf, len)
#else
    #define EFLASH_LOGE_PRINT_HEX(desc, buf, len)  ((void)0)
#endif

/* ==================== 断言宏定义 ==================== */
// 断言始终启用（用于调试），但断言消息会根据日志级别被编译期消除
#define EFLASH_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            EFLASH_LOGE("ASSERT FAILED: %s, file %s, line %d\n", #expr, __FILE__, __LINE__); \
            while(1); /* 进入死循环，等待复位或调试器介入 */ \
        } \
    } while(0)

#endif /* __EMBEDDED_FLASH_LOG_H__ */

