/*
 * EmbeddedFlash Port for STM32F103ZET6
 * 
 * 芯片特性:
 * - Flash容量: 512KB
 * - 页大小: 2KB (0x800)
 * - 编程方式: 字(32位)编程
 * - 擦除后值为0xFFFFFFFF
 */

#include "EmbeddedFlash_port.h"
#include "stm32f10x_flash.h"
#include <stdio.h>

/**
 * @brief Flash硬件初始化
 * @return 错误码
 */
FlashErrCode flash_port_init(void) {
    FlashErrCode result = FLASH_NO_ERR;
    
    /* STM32 Flash不需要特殊初始化 */
    
    return result;
}

/**
 * @brief 从Flash读取数据
 * @note 操作单位为字节
 * 
 * @param addr Flash地址
 * @param buf 存储读取数据的缓冲区
 * @param size 读取字节数
 * @return 错误码
 */
FlashErrCode flash_port_read(uint32_t addr, uint32_t *buf, size_t size) {
    FlashErrCode result = FLASH_NO_ERR;
    uint8_t *buf_8 = (uint8_t *)buf;
    size_t i;

    /* 参数检查 */
    if (addr < FLASH_START_ADDR || addr > FLASH_END_ADDR || 
        size > (FLASH_END_ADDR - addr + 1) || buf == NULL) {
        return FLASH_PARAM_ERR;
    }

    /* 从Flash复制到RAM */
    for (i = 0; i < size; i++, addr++, buf_8++) {
        *buf_8 = *(uint8_t *)addr;
    }

    return result;
}

/**
 * @brief 擦除Flash数据
 * @note 此操作不可逆
 * @note 操作单位为页
 * 
 * @param addr Flash地址（必须按页对齐）
 * @param size 擦除字节数
 * @return 错误码
 */
FlashErrCode flash_port_erase(uint32_t addr, size_t size) {
    FlashErrCode result = FLASH_NO_ERR;
    FLASH_Status flash_status;
    size_t erase_pages, i;
    
    /* 参数检查 */
    if (addr < FLASH_START_ADDR || addr > FLASH_END_ADDR || 
        size == 0 || size > (FLASH_END_ADDR - addr + 1)) {
        return FLASH_PARAM_ERR;
    }
    
    /* 确保起始地址按页对齐 */
    if (addr % FLASH_PAGE_SIZE != 0) {
        printf("Flash: Address not page aligned: 0x%08X\n", addr);
        return FLASH_PARAM_ERR;
    }
    
    /* 计算需要擦除的页数 */
    erase_pages = size / FLASH_PAGE_SIZE;
    if (size % FLASH_PAGE_SIZE != 0) {
        erase_pages++;
    }

    /* 开始擦除 */
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    
    /* 开始擦除页面 */
    for (i = 0; i < erase_pages; i++) {
        uint32_t page_addr = addr + (FLASH_PAGE_SIZE * i);
        flash_status = FLASH_ErasePage(page_addr);
        
        if (flash_status != FLASH_COMPLETE) {
            printf("Flash: Erase failed at page %d, addr=0x%08X, status=%d\n", 
                   i, page_addr, flash_status);
            result = FLASH_ERASE_ERR;
            break;
        }
    }
    
    FLASH_Lock();

    return result;
}

/**
 * @brief 写入数据到Flash
 * @note 操作单位为字(32位)
 * @note 必须先擦除后写入
 * 
 * @param addr Flash地址
 * @param buf 要写入的数据缓冲区
 * @param size 写入字节数
 * @return 错误码
 */
FlashErrCode flash_port_write(uint32_t addr, const uint32_t *buf, size_t size) {
    FlashErrCode result = FLASH_NO_ERR;
    size_t i;
    uint32_t read_data;
    FLASH_Status flash_status;

    /* 参数检查 */
    if (addr < FLASH_START_ADDR || addr > FLASH_END_ADDR || 
        size > (FLASH_END_ADDR - addr + 1) || buf == NULL) {
        printf("Flash: Invalid parameters - addr=0x%08X, size=%d, buf=%p\n", addr, size, buf);
        return FLASH_PARAM_ERR;
    }

    /* 检查地址对齐 */
    if (addr % 4 != 0) {
        printf("Flash: Address not 4-byte aligned: 0x%08X\n", addr);
        return FLASH_PARAM_ERR;
    }

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    
    for (i = 0; i < size; i += 4, buf++, addr += 4) {
        /* 写入数据 */
        flash_status = FLASH_ProgramWord(addr, *buf);
        if (flash_status != FLASH_COMPLETE) {
            printf("Flash: Program failed at 0x%08X, status=%d\n", addr, flash_status);
            result = FLASH_WRITE_ERR;
            break;
        }

        /* 验证写入 */
        read_data = *(uint32_t *)addr;
        if (read_data != *buf) {
            result = FLASH_WRITE_ERR;
            printf("Flash: Write verification failed at 0x%08X, expected=0x%08X, actual=0x%08X\n",
                   addr, *buf, read_data);
            break;
        }
    }
    
    FLASH_Lock();

    return result;
}

/**
 * @brief Flash环境锁定（关闭中断）
 */
void flash_port_lock(void) {
    __disable_irq();
}

/**
 * @brief Flash环境解锁（开启中断）
 */
void flash_port_unlock(void) {
    __enable_irq();
}

/**
 * @brief Flash接口测试函数
 * @param test_addr 测试地址
 * @return 错误码
 */
FlashErrCode flash_port_test(uint32_t test_addr) {
    FlashErrCode result = FLASH_NO_ERR;
    uint8_t test_data[] = {0x5A, 0xA5, 0x66, 0x77};
    uint8_t read_data[4] = {0};
    size_t i;
    
    printf("\n========== Flash Port Test ==========\n");
    printf("Test Address: 0x%08X\n", test_addr);
    
    /* 测试1: 擦除Flash */
    printf("1. Erasing Flash...\n");
    result = flash_port_erase(test_addr, FLASH_PAGE_SIZE);
    if (result != FLASH_NO_ERR) {
        printf("❌ Flash erase failed, error=%d\n", result);
        return result;
    }
    printf("✅ Flash erase successful\n");
    
    /* 验证擦除结果 */
    printf("2. Verifying erase...\n");
    result = flash_port_read(test_addr, (uint32_t *)read_data, 4);
    if (result != FLASH_NO_ERR) {
        printf("❌ Flash read failed, error=%d\n", result);
        return result;
    }
    
    for (i = 0; i < 4; i++) {
        if (read_data[i] != 0xFF) {
            printf("❌ Erase verification failed at offset %d: expected=0xFF, actual=0x%02X\n", 
                   i, read_data[i]);
            return FLASH_ERASE_ERR;
        }
    }
    printf("✅ Erase verification successful\n");
    
    /* 测试2: 写入数据 */
    printf("3. Writing test data...\n");
    result = flash_port_write(test_addr, (const uint32_t *)test_data, 4);
    if (result != FLASH_NO_ERR) {
        printf("❌ Flash write failed, error=%d\n", result);
        return result;
    }
    printf("✅ Flash write successful\n");
    
    /* 测试3: 读取并验证数据 */
    printf("4. Reading and verifying data...\n");
    memset(read_data, 0, sizeof(read_data));
    result = flash_port_read(test_addr, (uint32_t *)read_data, 4);
    if (result != FLASH_NO_ERR) {
        printf("❌ Flash read failed, error=%d\n", result);
        return result;
    }
    
    for (i = 0; i < 4; i++) {
        if (read_data[i] != test_data[i]) {
            printf("❌ Data mismatch at offset %d: expected=0x%02X, actual=0x%02X\n", 
                   i, test_data[i], read_data[i]);
            return FLASH_WRITE_ERR;
        }
    }
    printf("✅ Data verification successful\n");
    
    printf("========== Flash Port Test Complete ==========\n");
    printf("✅ All tests passed!\n\n");
    
    return FLASH_NO_ERR;
}

