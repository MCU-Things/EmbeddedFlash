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
#include "EmbeddedFlash_def.h"
#include "stm32f10x_flash.h"
#include <string.h>

/**
 * @brief Flash硬件初始化
 * @return 错误码
 */
EF_ErrCode flash_port_init(void) {
    EF_ErrCode result = EF_OK;
    
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
EF_ErrCode flash_port_read(uint32_t addr, uint8_t *buf, size_t size) {
    EF_ErrCode result = EF_OK;
    size_t i;

    /* 参数检查 */
    if (addr < FLASH_START_ADDR || addr > FLASH_END_ADDR || 
        size > (FLASH_END_ADDR - addr + 1) || buf == NULL) {
        EFLASH_LOGE("Flash: Invalid read parameters, addr=0x%08X, size=%d, buf=%p\n", 
               addr, (int)size, buf);
        return EF_ERR_PARAM;
    }

    /* 从Flash复制到RAM - 直接按字节操作 */
    for (i = 0; i < size; i++) {
        buf[i] = *(uint8_t *)(addr + i);
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
EF_ErrCode flash_port_erase(uint32_t addr, size_t size) {
    EF_ErrCode result = EF_OK;
    FLASH_Status flash_status;
    size_t erase_pages, i;
    
    /* 参数检查 */
    if (addr < FLASH_START_ADDR || addr > FLASH_END_ADDR || 
        size == 0 || size > (FLASH_END_ADDR - addr + 1)) {
        return EF_ERR_PARAM;
    }
    
    /* 确保起始地址按页对齐 */
    if (addr % FLASH_PAGE_SIZE != 0) {
        EFLASH_LOGE("Flash: Address not page aligned: 0x%08X\n", addr);
        return EF_ERR_ADDR_ALIGN;
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
            EFLASH_LOGE("Flash: Erase failed at page %d, addr=0x%08X, status=%d\n", 
                   (int)i, page_addr, (int)flash_status);
            result = EF_ERR_ERASE;
            break;
        }
    }
    
    FLASH_Lock();

    return result;
}

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
EF_ErrCode flash_port_write(uint32_t addr, const uint8_t *buf, size_t size) {
    EF_ErrCode result = EF_OK;
    size_t i;
    uint32_t read_data;
    FLASH_Status flash_status;
    const uint32_t *buf_32 = (const uint32_t *)buf;

    /* 参数检查 */
    if (addr < FLASH_START_ADDR || addr > FLASH_END_ADDR || 
        size > (FLASH_END_ADDR - addr + 1) || buf == NULL) {
        EFLASH_LOGE("Flash: Invalid parameters - addr=0x%08X, size=%d, buf=%p\n", addr, (int)size, buf);
        return EF_ERR_PARAM;
    }

    /* 检查地址和大小对齐 */
    if (addr % 4 != 0) {
        EFLASH_LOGE("Flash: Address not 4-byte aligned: 0x%08X\n", addr);
        return EF_ERR_ADDR_ALIGN;
    }
    
    /* 检查大小对齐 */
    if (size % 4 != 0) {
        EFLASH_LOGE("Flash: Size not 4-byte aligned: %d\n", (int)size);
        return EF_ERR_SIZE_ALIGN;
    }

    FLASH_Unlock();
    /* 按4字节为单位写入 */
    for (i = 0; i < size; i += 4, buf_32++, addr += 4) {
        FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
        /* 写入数据 */
        flash_status = FLASH_ProgramWord(addr, *buf_32);
        if (flash_status == FLASH_ERROR_PG)
        {
            //数据重复，跳过
            EFLASH_LOGD("Flash: Data already exists at 0x%08X, value=0x%08X\n", addr, *buf_32);
            continue;
        }
        
        if (flash_status == FLASH_COMPLETE) {
            /* 验证写入 */
            read_data = *(uint32_t *)addr;
            if (read_data != *buf_32) {
                result = EF_ERR_WRITE;
                EFLASH_LOGE("Flash: Verification failed at 0x%08X, expected=0x%08X, actual=0x%08X\n",
                       addr, *buf_32, read_data);
                break;
            }
        } else {
            result = EF_ERR_WRITE;
            EFLASH_LOGE("Flash: Program failed at 0x%08X, status=%d\n", addr, (int)flash_status);
            break;
        }
    }
    
    FLASH_Lock();
    
    /* 如果部分写入成功，返回部分成功错误码 */
    if (result != EF_OK && i > 0 && i < size) {
        EFLASH_LOGW("Flash: Partial write completed - %d/%d bytes written successfully\n", (int)i, (int)size);
        result = EF_ERR_WRITE; // 仍然返回错误，但已尝试最大努力写入
    }

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
EF_ErrCode flash_port_test(uint32_t test_addr) {
    EF_ErrCode result = EF_OK;
    uint8_t test_data[] = {0x5A, 0xA5, 0x66, 0x77};
    uint8_t read_data[4] = {0};
    size_t i;
    
    EFLASH_LOGI("========== Flash Port Test ==========\n");
    EFLASH_LOGI("Test Address: 0x%08X\n", test_addr);
    
    /* 测试1: 擦除Flash */
    EFLASH_LOGI("1. Erasing Flash...\n");
    result = flash_port_erase(test_addr, FLASH_PAGE_SIZE);
    if (result != EF_OK) {
        EFLASH_LOGE("Flash erase failed, error=%d\n", (int)result);
        return result;
    }
    EFLASH_LOGI("Flash erase successful\n");
    
    /* 验证擦除结果 */
    EFLASH_LOGI("2. Verifying erase...\n");
    result = flash_port_read(test_addr, read_data, 4);
    if (result != EF_OK) {
        EFLASH_LOGE("Flash read failed, error=%d\n", (int)result);
        return result;
    }
    
    for (i = 0; i < 4; i++) {
        if (read_data[i] != 0xFF) {
            EFLASH_LOGE("Erase verification failed at offset %d: expected=0xFF, actual=0x%02X\n", 
                   (int)i, read_data[i]);
            return EF_ERR_ERASE;
        }
    }
    EFLASH_LOGI("Erase verification successful\n");
    
    /* 测试2: 写入数据 */
    EFLASH_LOGI("3. Writing test data...\n");
    result = flash_port_write(test_addr, test_data, 4);
    if (result != EF_OK) {
        EFLASH_LOGE("Flash write failed, error=%d\n", (int)result);
        return result;
    }
    EFLASH_LOGI("Flash write successful\n");
    
    /* 测试3: 读取并验证数据 */
    EFLASH_LOGI("4. Reading and verifying data...\n");
    memset(read_data, 0, sizeof(read_data));
    result = flash_port_read(test_addr, read_data, 4);
    if (result != EF_OK) {
        EFLASH_LOGE("Flash read failed, error=%d\n", (int)result);
        return result;
    }
    
    for (i = 0; i < 4; i++) {
        if (read_data[i] != test_data[i]) {
            EFLASH_LOGE("Data mismatch at offset %d: expected=0x%02X, actual=0x%02X\n", 
                   (int)i, test_data[i], read_data[i]);
            return EF_ERR_VERIFY;
        }
    }
    EFLASH_LOGI("Data verification successful\n");
    
    EFLASH_LOGI("========== Flash Port Test Complete ==========\n");
    EFLASH_LOGI("All tests passed!\n");
    
    return EF_OK;
}

