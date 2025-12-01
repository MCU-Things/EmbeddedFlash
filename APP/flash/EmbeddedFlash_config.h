/*
 * EmbeddedFlash Config - 配置参数文件
 * 
 * 此文件包含所有可配置的参数，用户可以根据实际需求修改这些参数
 */

#ifndef __EMBEDDED_FLASH_CONFIG_H__
#define __EMBEDDED_FLASH_CONFIG_H__

/* ==================== Flash硬件配置参数 ==================== */
/* 芯片特性: STM32F103ZET6
 * - Flash容量: 512KB
 * - 页大小: 2KB (0x800)
 * - 编程方式: 字(32位)编程
 * - 擦除后值为0xFFFFFFFF
 */
#define FLASH_PAGE_SIZE             (0x800U)                  /* 2KB per page */
#define FLASH_START_ADDR            (0x08000000U)             /* Flash起始地址 */
#define FLASH_END_ADDR              (0x0807FFFFU)             /* Flash结束地址 (512KB) */
#define FLASH_SIZE                  (512 * 1024)              /* 512KB */

/* ==================== 功能开关配置 ==================== */
#define EFLASH_ENABLE_ERASE_COUNTER  (0)  // 启用扇区擦除次数统计功能（0=关闭，1=开启）
#define CHIP_FLASH_ERASE_MAXTIMES   (10000)  // Flash最大擦除次数限制

/* ==================== Flash写入粒度配置 ==================== */
// 单位：位
// 1=NOR Flash, 8=STM32F4, 32=STM32F1
#define EFLASH_WRITE_GRAN           32

/* ==================== KV记录配置 ==================== */
#define KV_HEADER_MAGIC             (0xA5)                    // KV记录魔术字（固定值，不可修改）
#define KV_MAX_VALUE_SIZE           (10)                      // KV记录最大数据长度（字节）

/* ==================== 扇区配置 ==================== */
#define KV_SECTOR_START_ADDR        (0x0801E000)              // KV存储区起始地址（调整到Flash末尾区域，避免与APP冲突）
#define KV_SECTOR_SIZE              (FLASH_PAGE_SIZE)         // 扇区大小（等于Flash页大小）
#define KV_SECTOR_COUNT             (4)                       // 扇区总数（必须大于等于2且你的掉电数据小于数据区大小）
#define KV_GC_SECTOR_COUNT          (1)                       // GC区数量（固定值不可更改）
#define KV_DATA_SECTOR_COUNT        (KV_SECTOR_COUNT - KV_GC_SECTOR_COUNT)  // 数据区数量

/* ==================== 扇区头配置 ==================== */
#define SECTOR_HEADER_MAGIC_WORD    (0x4321A55A)             // 扇区头魔术字

/* ==================== 状态表配置 ==================== */
// 状态表大小计算（基于写入粒度）
#if (EFLASH_WRITE_GRAN == 1)
#define STATUS_TABLE_SIZE(status_num)   ((status_num * EFLASH_WRITE_GRAN + 7)/8)
#else
#define STATUS_TABLE_SIZE(status_num)   (((status_num - 1) * EFLASH_WRITE_GRAN + 7)/8)
#endif

// 扇区状态数量
#define SECTOR_STATUS_NUM           3   // FREE, USING, FULL

// 扇区角色数量
#define SECTOR_ROLE_NUM             5   // UNASSIGNED, DATA, GC_TEMP, GC, DATA_GCING

// KV记录状态数量
#define KV_STATUS_NUM               5   // UNUSED, PRE_WRITE, WRITE, PRE_DELETE, DELETED

// 状态表实际大小
#define SECTOR_STATUS_TABLE_SIZE    STATUS_TABLE_SIZE(SECTOR_STATUS_NUM)
#define SECTOR_ROLE_TABLE_SIZE      STATUS_TABLE_SIZE(SECTOR_ROLE_NUM)
#define KV_STATUS_TABLE_SIZE        STATUS_TABLE_SIZE(KV_STATUS_NUM)

/* ==================== 日志等级配置 ==================== */
// 等级从低到高：ALL(全部) < DEBUG(调试) < INFO(信息) < WARN(危险/告警) < ERROR(错误) < NONE(关闭)
#define EFLASH_LOG_LEVEL_ALL    0
#define EFLASH_LOG_LEVEL_DEBUG  1
#define EFLASH_LOG_LEVEL_INFO   2
#define EFLASH_LOG_LEVEL_WARN   3
#define EFLASH_LOG_LEVEL_ERROR  4
#define EFLASH_LOG_LEVEL_NONE   5

// 默认日志等级（可根据需要修改）：INFO 更加精简，DEBUG 更详细
#ifndef EFLASH_LOG_LEVEL
#define EFLASH_LOG_LEVEL EFLASH_LOG_LEVEL_ALL
#endif

#endif /* __EMBEDDED_FLASH_CONFIG_H__ */

