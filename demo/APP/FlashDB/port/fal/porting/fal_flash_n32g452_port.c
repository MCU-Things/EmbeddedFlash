/*
 * Copyright (c) 2020, Armink, <armink.ztl@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fal.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "n32g45x.h"
#include <string.h>

static SemaphoreHandle_t m_mutex_for_flash = NULL;

static void feed_dog(void)
{

}


static int init(void)
{
    if (m_mutex_for_flash == NULL)
    {
        m_mutex_for_flash = xSemaphoreCreateRecursiveMutex();
    }
    ELOG_ASSERT(m_mutex_for_flash);
    return 0;
}

static int read(long offset, uint8_t *buf, size_t size)
{
    size_t i;
    uint32_t addr = n32g452_onchip_flash.addr + offset;
    for (i = 0; i < size; i++, addr++, buf++)
    {
        *buf = *(uint8_t *) addr;
    }

    return size;
}

static int write(long offset, const uint8_t *buf, size_t size)
{
    size_t i;
    volatile uint32_t w_addr = n32g452_onchip_flash.addr + offset;

    const uint8_t *p_buf = (const uint8_t*)buf;
    
        // 判断是否为ARM Compiler
#if defined(__ARMCC_VERSION) || (__ARMCC_VERSION > 0) 
    #define ALIGN_ATTRIBUTE(n) __align(n)

// 判断是否为GCC或兼容GCC的编译器（如Clang）
#elif defined(__GNUC__) || defined(__GNUG__)
    #define ALIGN_ATTRIBUTE(n) __attribute__((aligned(n)))

// 其他未知编译器时，提供一个默认的“不支持”情况
#else
    // 可以选择定义一个空操作的宏，避免编译错误，但对齐属性可能无效
    #define ALIGN_ATTRIBUTE(n)

    // 或者可以选择触发编译错误，明确指出不支持该编译器
    #error "Unsupported compiler. Alignment attribute not defined."
#endif
    ALIGN_ATTRIBUTE(4) uint32_t write_data;
    ALIGN_ATTRIBUTE(4) uint32_t read_data;  

    ELOG_ASSERT(w_addr % 4 == 0);

    xSemaphoreTakeRecursive(m_mutex_for_flash, portMAX_DELAY);

    /* Unlocks the FLASH Program Erase Controller */
    FLASH_Unlock();
    for (i = 0; i < size; i += 4, p_buf+=4, w_addr += 4)
    {
        memcpy(&write_data, p_buf, 4); //用以保证fmc_word_program的第2个参数是内存首地址对齐

        FLASH_STS flash_status;
        uint32_t timeout_cnt = 100000;
        do  // XXX: 目前先简单死等到执行完成，后面要改成用mutex，以便在擦除期间释放CPU资源
        {  
            flash_status = FLASH_ProgramWord(w_addr, write_data);
            timeout_cnt--;
        } while ((flash_status != FLASH_COMPL) && (timeout_cnt != 0));

        read_data = *(uint32_t *)w_addr;
        /* You can add your code under here. */
        if ((flash_status != FLASH_COMPL) || (read_data != write_data)) {
            // log_e("write err:%x cmd:%x %x | pg:%x (w:%x,r:%x)", flash_status, addr, size, w_addr, write_data, read_data);
            FLASH_ClearFlag(FLASH_FLAG_PGERR);
            /* Locks the FLASH Program Erase Controller */
            FLASH_Lock(); 
            return -1;
        }
        else{
            // log_d("w addr:%x", w_addr);
			//FLash操作可能非常耗时，如果有看门狗需要喂狗，以下代码由用户实现
            feed_dog();
        }
    }
    /* Locks the FLASH Program Erase Controller */
    FLASH_Lock();

    xSemaphoreGiveRecursive(m_mutex_for_flash);

    return size;
}

static int erase(long offset, size_t size)
{
    uint32_t addr = n32g452_onchip_flash.addr + offset;

    ELOG_ASSERT((addr % n32g452_onchip_flash.blk_size == 0));
    ELOG_ASSERT((size % n32g452_onchip_flash.blk_size == 0));

    size_t erase_pages;

    erase_pages = size / n32g452_onchip_flash.blk_size;

    xSemaphoreTakeRecursive(m_mutex_for_flash, portMAX_DELAY);

    /* Unlocks the FLASH Program Erase Controller */
    FLASH_Unlock();
    for (size_t i = 0; i < erase_pages; i++)
    {
        uint32_t page_addr = addr + (n32g452_onchip_flash.blk_size * i);
        FLASH_STS flash_status;
        uint32_t timeout_cnt = 100000;
        do  // XXX: 目前先简单死等到执行完成，后面要改成用mutex，以便在擦除期间释放CPU资源
        {   
            flash_status = FLASH_EraseOnePage(page_addr);   
            timeout_cnt--;
        } while ((flash_status != FLASH_COMPL) && (timeout_cnt != 0));
        
        if (flash_status != FLASH_COMPL) {
            // log_e("erase err:%x cmd:%x %x | pg:%x", flash_status, addr, size, page_addr);

            /* Locks the FLASH Program Erase Controller */
            FLASH_Lock();
            return -1;
        }
        else{
			// log_d("e addr:%x", page_addr);
            //FLash操作可能非常耗时，如果有看门狗需要喂狗，以下代码由用户实现
            feed_dog();
        }
    }
    /* Locks the FLASH Program Erase Controller */
    FLASH_Lock();

    xSemaphoreGiveRecursive(m_mutex_for_flash);
    
    return size;
}



#include "boot_app_interface.h"
const struct fal_flash_dev n32g452_onchip_flash =
{
    .name       = "n32g452_onchip",
    .addr       = PARAM_ZOOM_ADDR,
    .len        = PARAM_ZOOM_SZ,
    .blk_size   = FLASH_PAGE_SZ,
    .ops        = {init, read, write, erase},
    .write_gran = 32
};

