// EmbeddedFlash.h - 持久化存储组件
#ifndef __EMBEDDED_FLASH_H__
#define __EMBEDDED_FLASH_H__

#include "EmbeddedFlash_port.h"
#include <stdint.h>
#include <stdbool.h>

// 功能开关宏定义
#define EFLASH_ENABLE_ERASE_COUNTER  1  // 启用扇区擦除次数统计功能（0=关闭，1=开启）
#define CHIP_FLASH_ERASE_MAXTIMES   (10000)

// Flash写入粒度配置（单位：位）
// 1=NOR Flash, 8=STM32F4, 32=STM32F1
#define EFLASH_WRITE_GRAN           32
// 断言宏定义
// #ifdef EMBEDDED_FLASH_ENABLE_ASSERT
    #define EFLASH_ASSERT(expr) \
        do { \
            if (!(expr)) { \
                printf("EFLASH ASSERT FAILED: %s, file %s, line %d\n", #expr, __FILE__, __LINE__); \
                while(1); /* 进入死循环，等待复位或调试器介入 */ \
            } \
        } while(0)
// #else
//     #define EFLASH_ASSERT(expr) ((void)0)
// #endif

// kv记录常量定义
#define KV_HEADER_MAGIC    (0xA5)  //这个不需要动
#define KV_MAX_VALUE_SIZE  (10)
// #define sizeof(KV_Record) (KV_MAX_VALUE_SIZE + 6)// 4字节对齐，sizeof(KV_Record): magic(1) + flags(1) + key(1) + value_length(1) + value(n) + crc(2)
// 扇区信息常量定义
#define KV_SECTOR_START_ADDR   (0x0801E000)// 调整到Flash末尾区域，避免与APP冲突
#define KV_SECTOR_SIZE     (FLASH_PAGE_SIZE)
#define KV_GC_SECTOR_COUNT    (1) //GC区数量(固定值不可更改)
#define KV_DATA_SECTOR_COUNT    (KV_SECTOR_COUNT-KV_GC_SECTOR_COUNT) //数据区计数
#define KV_SECTOR_COUNT    (4) //必须大于等于2且你的掉电数据小于数据区大小
// 扇区头信息常量定义
#define SECTOR_HEADER_MAGIC_WORD    (0xA55A1234)  // 扇区头魔术字 

// 状态表大小计算（基于写入粒度）
#if (EFLASH_WRITE_GRAN == 1)
#define STATUS_TABLE_SIZE(status_num)   ((status_num * EFLASH_WRITE_GRAN + 7)/8)
#else
#define STATUS_TABLE_SIZE(status_num)   (((status_num - 1) * EFLASH_WRITE_GRAN + 7)/8)
#endif

// 扇区状态和角色的状态数量
#define SECTOR_STATUS_NUM   3  // FREE, USING, FULL
#define SECTOR_ROLE_NUM     5  // UNASSIGNED, DATA, GC_TEMP, GC, DATA_GCING

// 状态表实际大小
#define SECTOR_STATUS_TABLE_SIZE    STATUS_TABLE_SIZE(SECTOR_STATUS_NUM)
#define SECTOR_ROLE_TABLE_SIZE      STATUS_TABLE_SIZE(SECTOR_ROLE_NUM)

// KV记录状态数量（5种状态）
#define KV_STATUS_NUM       5  // UNUSED, PRE_WRITE, WRITE, PRE_DELETE, DELETED

// KV记录状态表大小
#define KV_STATUS_TABLE_SIZE    STATUS_TABLE_SIZE(KV_STATUS_NUM)

//数据类型
typedef enum {
    EFLASH_DATA_FORMAT_NULL = 0,
    EFLASH_FORMAT_BOOL = 0x01,           //占用1字节
    EFLASH_FORMAT_FLOAT = 0x02,          //占用4字节
    EFLASH_FORMAT_UINT8 = 0x03,           //占用1字节
    EFLASH_FORMAT_INT8 = 0x04,            //占用1字节
    EFLASH_FORMAT_UINT16 = 0x05,          //占用2字节
    EFLASH_FORMAT_INT16 = 0x06,           //占用2字节
    EFLASH_FORMAT_UINT32 = 0x07,          //占用4字节
    EFLASH_FORMAT_INT32 = 0x08,           //占用4字节
    EFLASH_FORMAT_UINT64 = 0x09,          //占用8字节
    EFLASH_FORMAT_INT64 = 0x0A,           //占用8字节
    EFLASH_FORMAT_STRING = 0x0B,                 //占用0-255字节
    EFLASH_FORMAT_HEX = 0x0C,                //占用0-255字节
    EFLASH_FORMAT_UNDEFINED = 0x0F,
} EmbeddedFlash_data_type_e;

//KV记录状态（使用索引值，对应状态表位置）
typedef enum {
    EFLASH_KV_UNUSED     = 0,  // 未使用     - 擦除后初始状态（全FF）
    EFLASH_KV_PRE_WRITE  = 1,  // 预写入     - 开始写入，数据可能不完整
    EFLASH_KV_WRITE      = 2,  // 已写入     - 写入完成，数据有效
    EFLASH_KV_PRE_DELETE = 3,  // 预删除     - 准备删除，中间状态
    EFLASH_KV_DELETED    = 4,  // 已删除     - 删除完成，数据无效
} EmbeddedFlash_record_status_e;

// 错误类型枚举
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

// 错误记录结构体
typedef struct {
    uint32_t timestamp;                 // 时间戳
    uint8_t error_type;                 // 错误类型
    uint8_t error_code;                 // 错误代码
    uint16_t line_number;               // 错误行号
    uint32_t address;                   // 相关地址
    uint8_t key;                        // 相关键值
    char description[32];               // 错误描述
} embedded_flash_error_record_t;

// 错误统计结构体
typedef struct {
    uint32_t total_errors;              // 总错误数
    uint32_t error_counts[16];          // 各类型错误计数
    embedded_flash_error_record_t last_error; // 最后一次错误
    embedded_flash_error_record_t recent_errors[10]; // 最近10次错误
    uint8_t recent_error_index;         // 最近错误索引
} embedded_flash_error_stats_t;

// 键值对数据来源状态枚举
typedef enum {
    KV_DATA_SOURCE_DEFAULT = 0,        // 当前使用默认值，Flash中无此键值对数据
    KV_DATA_SOURCE_FIRST_WRITE = 1,    // 已首次写入Flash，从默认值转为持久化存储
    KV_DATA_SOURCE_FLASH_READ = 2,     // 已从Flash读取，当前RAM数据来自掉电存储区
    KV_DATA_SOURCE_UPDATE_WRITE = 3,   // 已更新写入Flash，当前RAM数据已同步到掉电存储区
    KV_DATA_SOURCE_FLASH_OVERRIDE = 4, // RAM数据已修改但未写入Flash，使用掉电区数据覆盖RAM数据
} kv_data_source_e;

//扇区状态（使用索引值，对应状态表位置）
typedef enum {
    EFLASH_SECTOR_STATUS_FREE = 0,      // 空闲（初始状态，全FF）
    EFLASH_SECTOR_STATUS_USING = 1,     // 使用中
    EFLASH_SECTOR_STATUS_FULL = 2,      // 已满
} EmbeddedFlash_sector_status_e;

//扇区角色（使用索引值，对应状态表位置）
typedef enum {  
    EFLASH_SECTOR_ROLE_UNASSIGNED = 0,   // 未分配（初始状态，全FF）
    EFLASH_SECTOR_ROLE_GC = 1,           // GC区
    EFLASH_SECTOR_ROLE_GC_TEMP = 2,      // 临时GC区（迁移过程中）
    EFLASH_SECTOR_ROLE_DATA = 3,         // 数据区
    EFLASH_SECTOR_ROLE_DATA_GCING = 4,   // 被gc的数据区
} EmbeddedFlash_sector_role_e;

// 系统状态枚举定义
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
} embedded_flash_status_t;


#pragma pack(1)
/* 扇区属性信息，用于管理扇区的状态和角色（仅用于RAM）*/
typedef struct {
    uint8_t status;  // 参考枚举EFLASH_SECTOR_STATUS_e
    uint8_t role;    // 参考枚举EFLASH_SECTOR_ROLE_e
} sector_attr_t;

// KV记录偏移量定义（用于状态表和数据分离）
#define KV_MAGIC_OFFSET      ((uint32_t)(&((KV_Record *)0)->magic))

// KV记录结构（状态表在最前面！）
typedef struct {
    uint8_t status_table[KV_STATUS_TABLE_SIZE];  // 状态表（16字节，支持5种状态转换）
    uint8_t magic;                                // 0xA5（协议头）
    uint8_t data_type;                            // 数据类型（1字节）
    uint8_t key;                                  // 键 (1B)
    uint8_t value_length;                         // 值的长度 (1B),仅指value字段的实际数据长度
    uint8_t value[KV_MAX_VALUE_SIZE];             // 数据值
    uint16_t crc;                                 // CRC16 校验码，只校验key、value_length、value
}KV_Record;

// 扇区头信息结构（Flash中存储的格式）
typedef struct {
    uint8_t status_table[SECTOR_STATUS_TABLE_SIZE];  // 状态表（12字节）
    uint8_t role_table[SECTOR_ROLE_TABLE_SIZE];      // 角色表（16字节）
    uint32_t magic;                                   // 魔术字 0x30344645
    uint32_t reserved;                                // 保留字段
}sector_header_t;
#pragma pack()

// 默认键值对结构
typedef struct {
    uint32_t addr_abs; // 记录在Flash中的绝对地址，用于快速定位当前的读写地址
    uint8_t key;                    // 键
    void *value;   // 默认值指针
    uint8_t value_length;                 // 默认值长度
    uint8_t data_type;              // 数据类型
    uint8_t data_source; // 数据来源状态，参考枚举kv_data_source_e
} kv_data_t;

// 扇区描述符，用于运行时跟踪和管理扇区
typedef struct {
    const uint32_t  sector_addr;    // 扇区地址
    const uint8_t   sector_idx;     // 扇区索引(固定值) 0-3，按地址顺序排列
    sector_attr_t   attr;           // 扇区属性信息
    uint16_t        free_space;     // 剩余空间
    uint16_t        record_count;   // 记录数
} sector_desc_t;



// 扇区擦除统计结构体
#if EFLASH_ENABLE_ERASE_COUNTER
typedef struct {
    uint32_t sector_erase_count[KV_SECTOR_COUNT];  // 每个扇区的擦除次数
    uint32_t total_erase_count;                     // 总擦除次数
    uint32_t max_erase_count;                       // 最大擦除次数（磨损最严重的扇区）
    uint8_t  max_erase_sector_idx;                  // 擦除次数最多的扇区索引
} eflash_erase_stats_t;
#endif

// 函数接口
int embedded_flash_init(const kv_data_t *defaults, uint8_t default_count);

// 获取数据类型的固定长度
uint8_t embedded_flash_get_type_size(uint8_t data_type);

// 固定长度数据类型的设置函数
int embedded_flash_set_bool(uint8_t key, bool value);
int embedded_flash_set_uint8(uint8_t key, uint8_t value);
int embedded_flash_set_int8(uint8_t key, int8_t value);
int embedded_flash_set_uint16(uint8_t key, uint16_t value);
int embedded_flash_set_int16(uint8_t key, int16_t value);
int embedded_flash_set_uint32(uint8_t key, uint32_t value);
int embedded_flash_set_int32(uint8_t key, int32_t value);
int embedded_flash_set_uint64(uint8_t key, uint64_t value);
int embedded_flash_set_int64(uint8_t key, int64_t value);
int embedded_flash_set_float(uint8_t key, float value);
// 可变长度数据类型的设置函数
int embedded_flash_set_string(uint8_t key, const char *value);
int embedded_flash_set_hex(uint8_t key, const uint8_t *value, uint8_t length);

// 通用获取函数
int embedded_flash_get(uint8_t key, uint8_t *value, uint8_t *length, uint8_t *data_type);
// 删除函数
int embedded_flash_delete(uint8_t key);

// 扇区擦除统计相关API
#if EFLASH_ENABLE_ERASE_COUNTER
// 获取擦除统计信息
void embedded_flash_get_erase_stats(eflash_erase_stats_t *stats);
// 重置擦除统计信息
void embedded_flash_reset_erase_stats(void);
// 打印擦除统计信息
void embedded_flash_print_erase_stats(void);
#endif

#endif
