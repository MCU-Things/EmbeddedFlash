/*
 * EmbeddedFlash Port for STM32F103ZET6
 * 
 * 芯片特性:
 * - Flash容量: 512KB
 * - 页大小: 2KB (0x800)
 * - 编程方式: 字(32位)编程
 * - 擦除后值为0xFFFFFFFF
 */

#ifndef __EMBEDDED_FLASH_PORT_H__
#define __EMBEDDED_FLASH_PORT_H__

#include "EmbeddedFlash_def.h"
#include "stm32f10x.h"
#include <stdint.h>
#include <stddef.h>



/**
 * @brief Flash硬件初始化
 * @return 错误码
 */
EF_ErrCode flash_port_init(void);

/**
 * @brief 从Flash读取数据
 * @note 操作单位为字节
 * 
 * @param addr Flash地址
 * @param buf 存储读取数据的缓冲区
 * @param size 读取字节数
 * @return 错误码
 */
EF_ErrCode flash_port_read(uint32_t addr, uint8_t *buf, size_t size);

/**
 * @brief 擦除Flash数据
 * @note 此操作不可逆
 * @note 操作单位为页
 * 
 * @param addr Flash地址（必须按页对齐）
 * @param size 擦除字节数
 * @return 错误码
 */
EF_ErrCode flash_port_erase(uint32_t addr, size_t size);

/**
 * @brief 写入数据到Flash
 * @note 操作单位为字节，内部按4字节对齐处理
 * @note 必须先擦除后写入
 * 
 * @param addr Flash地址（必须4字节对齐）
 * @param buf 要写入的数据缓冲区
 * @param size 写入字节数（必须4字节对齐）
 * @return 错误码
 */
EF_ErrCode flash_port_write(uint32_t addr, const uint8_t *buf, size_t size);

/**
 * @brief Flash环境锁定（关闭中断）
 */
void flash_port_lock(void);

/**
 * @brief Flash环境解锁（开启中断）
 */
void flash_port_unlock(void);

/**
 * @brief Flash接口测试函数
 * @param test_addr 测试地址
 * @return 错误码
 */
EF_ErrCode flash_port_test(uint32_t test_addr);


#endif /* __EMBEDDED_FLASH_PORT_H__ */

