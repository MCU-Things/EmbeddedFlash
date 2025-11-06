// EmbeddedFlash.h - 持久化存储组件
#ifndef __EMBEDDED_FLASH_H__
#define __EMBEDDED_FLASH_H__

#include "EmbeddedFlash_def.h"
#include "EmbeddedFlash_port.h"
#include <stdint.h>
#include <stdbool.h>

// 函数接口
EF_ErrCode embedded_flash_init(const kv_data_t *defaults, uint8_t default_count);

// 获取数据类型的固定长度
uint8_t embedded_flash_get_type_size(EmbeddedFlash_data_type_e data_type);

// 固定长度数据类型的设置函数（返回EF_ErrCode）
EF_ErrCode embedded_flash_set_bool(uint8_t key, bool value);
EF_ErrCode embedded_flash_set_uint8(uint8_t key, uint8_t value);
EF_ErrCode embedded_flash_set_int8(uint8_t key, int8_t value);
EF_ErrCode embedded_flash_set_uint16(uint8_t key, uint16_t value);
EF_ErrCode embedded_flash_set_int16(uint8_t key, int16_t value);
EF_ErrCode embedded_flash_set_uint32(uint8_t key, uint32_t value);
EF_ErrCode embedded_flash_set_int32(uint8_t key, int32_t value);
EF_ErrCode embedded_flash_set_uint64(uint8_t key, uint64_t value);
EF_ErrCode embedded_flash_set_int64(uint8_t key, int64_t value);
EF_ErrCode embedded_flash_set_float(uint8_t key, float value);
// 可变长度数据类型的设置函数
EF_ErrCode embedded_flash_set_string(uint8_t key, const char *value);
EF_ErrCode embedded_flash_set_hex(uint8_t key, const uint8_t *value, uint8_t length);

// 通用获取函数
EF_ErrCode embedded_flash_get(uint8_t key, uint8_t *value, uint8_t *length, uint8_t *data_type);
// 删除函数
EF_ErrCode embedded_flash_delete(uint8_t key);

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
