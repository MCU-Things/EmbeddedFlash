/*
 * EmbeddedFlash Def - 公共定义文件
 * 
 * 此文件包含所有公共的类型定义、枚举、结构体等
 */

#ifndef __EMBEDDED_FLASH_DEF_H__
#define __EMBEDDED_FLASH_DEF_H__

#include "EmbeddedFlash_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>






/* ==================== 日志输出宏 ==================== */
// 统一的日志入口，带有等级筛选与模块名前缀
#ifndef EFLASH_LOG_TAG
#define EFLASH_LOG_TAG "EmbeddedFlash"
#endif

// #ifndef EFLASH_PRINTF_REAL
// static inline int EFLASH_PRINTF_REAL(const char *fmt, ...) {
//     va_list ap;
//     va_start(ap, fmt);
//     int n = vprintf(fmt, ap);
//     va_end(ap);
//     return n;
// }
// #endif

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

#define EFLASH_LOGA(fmt, ...) EFLASH_LOG(EFLASH_LOG_LEVEL_ALL,   fmt, ##__VA_ARGS__)
#define EFLASH_LOGD(fmt, ...) EFLASH_LOG(EFLASH_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define EFLASH_LOGI(fmt, ...) EFLASH_LOG(EFLASH_LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define EFLASH_LOGW(fmt, ...) EFLASH_LOG(EFLASH_LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define EFLASH_LOGE(fmt, ...) EFLASH_LOG(EFLASH_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

/* ==================== 断言宏定义 ==================== */
// #ifdef EMBEDDED_FLASH_ENABLE_ASSERT
#define EFLASH_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            EFLASH_LOGE("ASSERT FAILED: %s, file %s, line %d\n", #expr, __FILE__, __LINE__); \
            while(1); /* 进入死循环，等待复位或调试器介入 */ \
        } \
    } while(0)
// #else
//     #define EFLASH_ASSERT(expr) ((void)0)
// #endif

/* ==================== 打印 HEX 数据宏 ==================== */
#define EFLASH_PRINT_HEX(desc, buf, len) \
    do { \
        printf("HEX[%s]: [", (desc)); \
        for (uint32_t _i = 0; _i < (len); ++_i) { \
            printf("%02X ", (uint8_t)((buf)[_i])); \
        } \
        printf("]\n"); \
    } while(0)

/* ==================== 错误码定义 ==================== */
/* 统一以 EF_ 前缀命名 */
typedef enum {
    EF_OK = 0,                 /* 成功 */
    EF_ERR_PARAM=1,              /* 参数非法 */
    EF_ERR_ADDR_RANGE=2,         /* 地址越界 */
    EF_ERR_ADDR_ALIGN=3,         /* 地址未按要求对齐 */
    EF_ERR_SIZE_ZERO=4,          /* 大小为0 */
    EF_ERR_SIZE_TOO_LONG=5,      /* 大小超出最大限制 */
    EF_ERR_SIZE_ALIGN=6,         /* 大小未按要求对齐 */
    EF_ERR_LOCKED=7,             /* Flash处于锁定状态 */
    EF_ERR_BUSY=8,               /* Flash忙 */
    EF_ERR_TIMEOUT=9,            /* 操作超时 */
    EF_ERR_PROTECTION=10,         /* 写保护/选项字限制 */
    EF_ERR_ERASE=11,              /* 擦除失败 */
    EF_ERR_WRITE=12,              /* 写入失败 */
    EF_ERR_READ=13,               /* 读取失败 */
    EF_ERR_INVALID=14,            /* 无效数据 */
    EF_ERR_VERIFY=15,             /* 写后校验失败 */
    EF_ERR_CRC=16,                /* CRC校验失败 */
    EF_ERR_STATE=17,              /* 状态机/状态表非法 */
    EF_ERR_NOT_INIT=18,           /* 组件未初始化 */
    EF_ERR_NOT_FOUND=19,          /* 目标未找到（如键不存在） */
    EF_ERR_NO_SPACE=20,           /* 空间不足 */
    EF_ERR_FORMAT=21,             /* 结构/格式错误（头损坏等） */
    EF_ERR_IO=22,                 /* 底层IO错误（未知端口错误泛化） */
    EF_ERR_ALREADY=23,            /* 状态已满足/重复操作 */
    EF_ERR=24,                    /* 通用错误（未归类时返回） */
    EF_ERR_UNKNOWN=25             /* 未知错误 */
} EF_ErrCode;

/* ==================== 数据类型枚举 ==================== */
typedef enum {
    EFLASH_DATA_FORMAT_NULL = 0,
    EFLASH_FORMAT_BOOL = 0x01,           // 占用1字节
    EFLASH_FORMAT_FLOAT = 0x02,          // 占用4字节
    EFLASH_FORMAT_UINT8 = 0x03,          // 占用1字节
    EFLASH_FORMAT_INT8 = 0x04,           // 占用1字节
    EFLASH_FORMAT_UINT16 = 0x05,         // 占用2字节
    EFLASH_FORMAT_INT16 = 0x06,          // 占用2字节
    EFLASH_FORMAT_UINT32 = 0x07,         // 占用4字节
    EFLASH_FORMAT_INT32 = 0x08,          // 占用4字节
    EFLASH_FORMAT_UINT64 = 0x09,         // 占用8字节
    EFLASH_FORMAT_INT64 = 0x0A,          // 占用8字节
    EFLASH_FORMAT_STRING = 0x0B,         // 占用0-255字节
    EFLASH_FORMAT_HEX = 0x0C,            // 占用0-255字节
    EFLASH_FORMAT_UNDEFINED = 0x0F,
} EmbeddedFlash_data_type_e;

/* ==================== KV记录状态枚举 ==================== */
/* 使用索引值，对应状态表位置 */
typedef enum {
    EFLASH_KV_UNUSED     = 0,  // 未使用     - 擦除后初始状态（全FF）
    EFLASH_KV_PRE_WRITE  = 1,  // 预写入     - 开始写入，数据可能不完整
    EFLASH_KV_WRITE      = 2,  // 已写入     - 写入完成，数据有效
    EFLASH_KV_PRE_DELETE = 3,  // 预删除     - 准备删除，中间状态
    EFLASH_KV_DELETED    = 4,  // 已删除     - 删除完成，数据无效
} EmbeddedFlash_record_status_e;

/* ==================== 扇区状态枚举 ==================== */
/* 使用索引值，对应状态表位置 */
typedef enum {
    EFLASH_SECTOR_STATUS_FREE = 0,      // 空闲（初始状态，全FF）
    EFLASH_SECTOR_STATUS_USING = 1,     // 使用中
    EFLASH_SECTOR_STATUS_FULL = 2,      // 已满
} EmbeddedFlash_sector_status_e;

/* ==================== 扇区角色枚举 ==================== */
/* 使用索引值，对应状态表位置 */
typedef enum {  
    EFLASH_SECTOR_ROLE_UNASSIGNED = 0,   // 未分配（初始状态，全FF）
    EFLASH_SECTOR_ROLE_GC = 1,           // GC区
    EFLASH_SECTOR_ROLE_GC_TEMP = 2,      // 临时GC区（迁移过程中）
    EFLASH_SECTOR_ROLE_DATA = 3,         // 数据区
    EFLASH_SECTOR_ROLE_DATA_GCING = 4,   // 被gc的数据区
} EmbeddedFlash_sector_role_e;

/* ==================== 系统状态枚举 ==================== */
typedef enum {
    EFLASH_STATUS_NORMAL = 0,          // 正常状态：        有GC区，无GC临时区，有数据区，无被GC数据区，无空白扇区
    //GC中断有关
    EFLASH_STATUS_GC_PREPARE = 1,      // GC准备阶段异常：  无GC区，有GC临时区，有数据区，无被GC数据区，无空白扇区
    EFLASH_STATUS_GC_MIGRATING = 2,    // GC数据迁移中异常：无GC区，有GC临时区，有数据区，有被GC数据区，无空白扇区
    EFLASH_STATUS_GC_AFTER_MIGRATE = 3, // GC迁移后异常：   无GC区，无GC临时区，有数据区，有被GC数据区，无空白扇区
    EFLASH_STATUS_GC_AFTER_MIGRATE_WITH_EMPTY = 4, // GC迁移后有空白扇区异常：无GC区，无GC临时区，有数据区，无被GC数据区，有1个空白扇区
    //首次上电
    EFLASH_STATUS_FIRST_POWER_ON = 5, // 首次上电：    无GC区，无GC临时区，无数据区，无被GC数据区，全是空白扇区
    //新增扇区有关
    EFLASH_STATUS_EMPTY_SECTOR_ERROR = 6, // 空白扇区异常：有数据区or被gc数据区orGC区orGC临时区、有空白扇区，需全扇区擦除
    EFLASH_STATUS_NEW_SECTOR_ADDED =7,//新增扇区
    //未知异常有关
    EFLASH_STATUS_MULTI_ROLE_ERROR = 8, // 多重角色异常：GC临时区>1或GC区>1或被GC数据区>1，需全扇区擦除
    EFLASH_STATUS_UNKNOWN_ERROR = 9    // 未知异常：不符合上述任何状态，需全扇区擦除
} embedded_flash_att_status_t;

/* ==================== 错误类型枚举 ==================== */
typedef enum {
    EFLASH_ERROR_NONE = 0,
    EFLASH_ERROR_INIT_FAILED,           // 初始化失败
    EFLASH_ERROR_SECTOR_HEADER_INVALID, // 扇区头信息无效
    EFLASH_ERROR_RECORD_INVALID,        // 记录无效
    EFLASH_ERROR_CRC_MISMATCH,          // CRC校验失败
    EFLASH_ERROR_WRITE_FAILED,          // 写入失败
    EFLASH_ERROR_READ_FAILED,           // 读取失败
    EFLASH_ERROR_ERASE_FAILED,          // 擦除失败
    EFLASH_ERROR_GC_FAILED,             // 垃圾回收失败
    EFLASH_ERROR_INVALID_KEY,           // 无效键值
    EFLASH_ERROR_INVALID_PARAM,         // 无效参数
    EFLASH_ERROR_NO_SPACE,              // 空间不足
    EFLASH_ERROR_DATA_CORRUPTED,        // 数据损坏
    EFLASH_ERROR_POWER_LOSS,            // 断电异常
    EFLASH_ERROR_UNKNOWN                // 未知错误
} EmbeddedFlash_error_type_e;

/* ==================== 键值对数据来源状态枚举 ==================== */
typedef enum {
    KV_DATA_SOURCE_DEFAULT = 0,        // 当前使用默认值，Flash中无此键值对数据
    KV_DATA_SOURCE_FIRST_WRITE = 1,    // 已首次写入Flash，从默认值转为持久化存储
    KV_DATA_SOURCE_FLASH_READ = 2,     // 已从Flash读取，当前RAM数据来自掉电存储区
    KV_DATA_SOURCE_UPDATE_WRITE = 3,   // 已更新写入Flash，当前RAM数据已同步到掉电存储区
    KV_DATA_SOURCE_FLASH_OVERRIDE = 4, // RAM数据已修改但未写入Flash，使用掉电区数据覆盖RAM数据
} kv_data_source_e;

/* ==================== 结构体定义 ==================== */
#pragma pack(1)

/* 扇区属性信息，用于管理扇区的状态和角色（仅用于RAM）*/
typedef struct {
    uint8_t status;  // 参考枚举EmbeddedFlash_sector_status_e
    uint8_t role;    // 参考枚举EmbeddedFlash_sector_role_e
} sector_attr_t;

/* KV记录结构（状态表在最前面！）*/
typedef struct {
    uint8_t status_table[KV_STATUS_TABLE_SIZE];  // 状态表（16字节，支持5种状态转换）
    uint8_t magic;                                // 0xA5（协议头）
    uint8_t key;                                  // 键 (1B)
    uint8_t data_type;                            // 数据类型（1字节）
    uint8_t value_length;                         // 值的长度 (1B),仅指value字段的实际数据长度
    uint8_t value[KV_MAX_VALUE_SIZE];             // 数据值
    uint16_t crc;                                 // CRC16 校验码，只校验key、value_length、value
} KV_Record;

/* 扇区头信息结构（Flash中存储的格式）*/
typedef struct {
    uint8_t status_table[SECTOR_STATUS_TABLE_SIZE];  // 状态表（12字节）
    uint8_t role_table[SECTOR_ROLE_TABLE_SIZE];      // 角色表（16字节）
    uint32_t magic;                                   // 魔术字 0x4321A55A
    uint32_t reserved;                                // 保留字段
} sector_header_t;

#pragma pack()

/* 默认键值对结构 */
typedef struct {
    uint32_t addr_abs;      // 记录在Flash中的绝对地址，用于快速定位当前的读写地址
    uint8_t key;            // 键
    //使用指针有个不好的地方，如果外部定义的长度过小，会导致溢出，
    void *value;            // 默认值指针
    uint8_t value_length;   // 默认值长度
    uint8_t data_type;      // 数据类型
    uint8_t data_source;    // 数据来源状态，参考枚举kv_data_source_e
} kv_data_t;

/* 扇区描述符，用于运行时跟踪和管理扇区 */
typedef struct {
    uint32_t  sector_addr;    // 扇区地址
    uint8_t   sector_idx;     // 扇区索引(固定值) 0-3，按地址顺序排列
    sector_attr_t   attr;           // 扇区属性信息
    uint16_t        free_space;     // 剩余空间
    uint16_t        record_count;   // 记录数
} sector_desc_t;

/* 错误记录结构体 */
typedef struct {
    uint32_t timestamp;                 // 时间戳
    uint8_t error_type;                 // 错误类型
    uint8_t error_code;                 // 错误代码
    uint16_t line_number;               // 错误行号
    uint32_t address;                   // 相关地址
    uint8_t key;                        // 相关键值
    char description[32];               // 错误描述
} embedded_flash_error_record_t;

/* 错误统计结构体 */
typedef struct {
    uint32_t total_errors;              // 总错误数
    uint32_t error_counts[16];          // 各类型错误计数
    embedded_flash_error_record_t last_error; // 最后一次错误
    embedded_flash_error_record_t recent_errors[10]; // 最近10次错误
    uint8_t recent_error_index;         // 最近错误索引
} embedded_flash_error_stats_t;

/* 扇区擦除统计结构体 */
#if EFLASH_ENABLE_ERASE_COUNTER
typedef struct {
    uint32_t sector_erase_count[KV_SECTOR_COUNT];  // 每个扇区的擦除次数
    uint32_t total_erase_count;                     // 总擦除次数
    uint32_t max_erase_count;                       // 最大擦除次数（磨损最严重的扇区）
    uint8_t  max_erase_sector_idx;                  // 擦除次数最多的扇区索引
} eflash_erase_stats_t;
#endif

/* ==================== 宏定义 ==================== */
// KV记录偏移量定义（用于状态表和数据分离）
#define KV_MAGIC_OFFSET      ((uint32_t)(&((KV_Record *)0)->magic))

#endif /* __EMBEDDED_FLASH_DEF_H__ */

