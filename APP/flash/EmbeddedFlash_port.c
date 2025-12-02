/*
 * EmbeddedFlash Port for STM32F103ZET6
 * 
 * 芯片特性:
 * - Flash容量: 512KB
 * - 页大小: 2KB (0x800)
 * - 编程方式: 字(32位)编程
 * - 擦除后值为0xFFFFFFFF
 */

// 项目内部头文件
#include "EmbeddedFlash_port.h"
#include "EmbeddedFlash_log.h"
#include "stm32f10x_flash.h"

// 系统头文件
#include <string.h>


#define EFLASH_SOFTWARE_SIMULATION      1 // 软件仿真模式（0=关闭，1=开启，开启时使用RAM模拟Flash）

/* 
 * 软件仿真下是否对 0->1 写入进行严格校验：
 * - 0（默认）：只做按位与(current & write)，与真实 Flash 行为一致，不因为 0->1 企图返回错误
 * - 1        ：检测到 0->1 时立即报错并返回 EF_ERR_WRITE（用于调试协议是否违反“只写 1->0”约束）
 */
#define EFLASH_SIM_STRICT_0_TO_1_CHECK  0

#if (EFLASH_SOFTWARE_SIMULATION == 1)
// RAM模拟Flash存储（只模拟KV使用的区域：0x0801E000-0x0801FFFF，8KB）
#define SIM_FLASH_START_ADDR        (0x0801E000U)  // KV区域起始地址
#define SIM_FLASH_SIZE              (8 * 1024)     // 8KB（4个扇区，每个2KB）
static uint8_t s_sim_flash[SIM_FLASH_SIZE];        // RAM模拟Flash缓冲区
static uint8_t s_sim_flash_initialized = 0;        // 初始化标志

/**
 * @brief 初始化RAM模拟Flash（全部填充0xFF，模拟擦除后的状态）
 */
static void _sim_flash_init(void) {
    if (!s_sim_flash_initialized) {
        memset(s_sim_flash, 0xFF, SIM_FLASH_SIZE);
        s_sim_flash_initialized = 1;
        EFLASH_LOGD("RAM Flash sim init OK (size=%d)\n", SIM_FLASH_SIZE);
    }
}

/**
 * @brief 将Flash地址转换为RAM缓冲区偏移
 */
static uint32_t _addr_to_offset(uint32_t addr) {
    if (addr >= SIM_FLASH_START_ADDR && 
        addr < (SIM_FLASH_START_ADDR + SIM_FLASH_SIZE)) {
        return addr - SIM_FLASH_START_ADDR;
    }
    return 0xFFFFFFFF; // 无效地址
}
#endif

/**
 * @brief Flash硬件初始化
 * @return 错误码
 */
EF_ErrCode flash_port_init(void) {
    EF_ErrCode result = EF_OK;
    
    #if (EFLASH_SOFTWARE_SIMULATION == 1)
    // 软件仿真模式：初始化RAM模拟Flash
    _sim_flash_init();
    #else
    /* STM32 Flash不需要特殊初始化 */
    #endif
    
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
        EFLASH_LOGE("INV read, addr=0x%08X size=%d buf=%p\n", 
               addr, (int)size, buf);
        return EF_ERR_PARAM;
    }

    #if (EFLASH_SOFTWARE_SIMULATION == 1)
    // 软件仿真模式：从RAM缓冲区读取
    _sim_flash_init();
    uint32_t offset = _addr_to_offset(addr);
    if (offset != 0xFFFFFFFF && (offset + size) <= SIM_FLASH_SIZE) {
        memcpy(buf, &s_sim_flash[offset], size);
    } else {
        // 地址超出模拟范围，返回0xFF（擦除后的值）
        memset(buf, 0xFF, size);
    }
    #else
    /* 从Flash复制到RAM - 直接按字节操作 */
    for (i = 0; i < size; i++) {
        buf[i] = *(uint8_t *)(addr + i);
    }
    #endif

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
        EFLASH_LOGE("INV align(page) 0x%08X\n", addr);
        return EF_ERR_ADDR_ALIGN;
    }
    
    /* 计算需要擦除的页数 */
    erase_pages = size / FLASH_PAGE_SIZE;
    if (size % FLASH_PAGE_SIZE != 0) {
        erase_pages++;
    }

    #if (EFLASH_SOFTWARE_SIMULATION == 1)
    // 软件仿真模式：在RAM缓冲区中擦除（填充0xFF）
    _sim_flash_init();
    uint32_t offset = _addr_to_offset(addr);
    if (offset != 0xFFFFFFFF && (offset + size) <= SIM_FLASH_SIZE) {
        memset(&s_sim_flash[offset], 0xFF, size);
        flash_status = FLASH_COMPLETE;
        EFLASH_LOGD("ERASE sim OK @0x%08X sz=%d\n", addr, (int)size);
    } else {
        // 地址超出模拟范围，仍然返回成功（因为可能访问其他Flash区域）
        flash_status = FLASH_COMPLETE;
        EFLASH_LOGD("ERASE sim skip @0x%08X (out of sim range)\n", addr);
    }
    #else
    /* 开始擦除 */
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    
    /* 开始擦除页面 */
    for (i = 0; i < erase_pages; i++) {
        uint32_t page_addr = addr + (FLASH_PAGE_SIZE * i);
        flash_status = FLASH_ErasePage(page_addr);
        
        if (flash_status != FLASH_COMPLETE) {
            EFLASH_LOGE("ERASE fail p=%d @0x%08X st=%d\n", 
                   (int)i, page_addr, (int)flash_status);
            result = EF_ERR_ERASE;
            break;
        }
    }
    
    FLASH_Lock();
    #endif

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
        EFLASH_LOGE("INV param addr=0x%08X size=%d buf=%p\n", addr, (int)size, buf);
        return EF_ERR_PARAM;
    }

    /* 检查地址和大小对齐 */
    if (addr % 4 != 0) {
        EFLASH_LOGE("INV align(4B) 0x%08X\n", addr);
        return EF_ERR_ADDR_ALIGN;
    }
    
    #if (EFLASH_SOFTWARE_SIMULATION == 1)
    // 软件仿真模式：在RAM缓冲区中写入
    _sim_flash_init();
    uint32_t offset = _addr_to_offset(addr);
    if (offset != 0xFFFFFFFF && (offset + size) <= SIM_FLASH_SIZE) {
        // 按4字节为单位写入（模拟Flash的字编程特性）
        for (i = 0; i < size; i += 4, buf_32++) {
            uint32_t write_addr   = addr + i;
            uint32_t write_offset = offset + i;
            uint32_t write_value  = *buf_32;

            // 当前存储值
            uint32_t current_value = *(uint32_t *)&s_sim_flash[write_offset];
            if (current_value == write_value) {
                // 数据重复，跳过
                EFLASH_LOGD("SKIP same @0x%08X val=0x%08X\n", write_addr, write_value);
                continue;
            }

            /*
             * 真实 Flash 只能 1->0，0 不能再写回 1；硬件在出现 0->1 企图时不会报错，
             * 而是“尽力而为”，结果等价于 current_value & write_value。
             *
             * 为了兼顾：
             * - 默认行为：与硬件一致，只做按位与写入；
             * - 调试模式：可以强制在发现 0->1 时报错，帮助检查协议是否违规；
             * 这里通过宏 EFLASH_SIM_STRICT_0_TO_1_CHECK 进行控制。
             */
            uint32_t illegal_bits = write_value & ~current_value;

#if (EFLASH_SIM_STRICT_0_TO_1_CHECK == 1)
            if (illegal_bits != 0) {
                // 尝试将0写为1，严格模式下直接报错
                EFLASH_LOGE("WRITE sim fail @0x%08X: cannot write 0->1 (cur=0x%08X new=0x%08X, ill=0x%08X)\n",
                            write_addr, current_value, write_value, illegal_bits);
                result = EF_ERR_WRITE;
                break;
            }
#else
            if (illegal_bits != 0) {
                // 仅告警，不打断流程（与真实Flash行为更接近）
                EFLASH_LOGW("SIM 0->1 WARN @0x%08X ill=0x%08X cur=0x%08X new=0x%08X\n",
                            write_addr, illegal_bits, current_value, write_value);
            }
#endif

            // 执行写入（按位与操作，模拟Flash特性：只能将1写为0）
            *(uint32_t *)&s_sim_flash[write_offset] = current_value & write_value;
            flash_status = FLASH_COMPLETE;
            EFLASH_LOGD("WRITE sim OK @0x%08X val=0x%08X (cur=0x%08X -> new=0x%08X)\n",
                        write_addr, write_value, current_value,
                        *(uint32_t *)&s_sim_flash[write_offset]);
        }
    } else {
        // 地址超出模拟范围，仍然返回成功（因为可能访问其他Flash区域）
        flash_status = FLASH_COMPLETE;
        EFLASH_LOGD("WRITE sim skip @0x%08X (out of sim range)\n", addr);
    }
    #else
    FLASH_Unlock();
    /* 按4字节为单位写入 */
    for (i = 0; i < size; i += 4, buf_32++, addr += 4) {
        FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
        /* 写入数据 */
        flash_status = FLASH_ProgramWord(addr, *buf_32);
        
        if (flash_status == FLASH_ERROR_PG) {
            //数据重复，跳过
            EFLASH_LOGD("SKIP same @0x%08X val=0x%08X\n", addr, *buf_32);
            continue;
        }
        
        if (flash_status == FLASH_COMPLETE) {
            /* 验证写入 */
            read_data = *(uint32_t *)addr;
            if (read_data != *buf_32) {
                result = EF_ERR_WRITE;
                EFLASH_LOGE("VERIFY fail @0x%08X exp=0x%08X got=0x%08X\n",
                       addr, *buf_32, read_data);
                break;
            }
        } else {
            result = EF_ERR_WRITE;
            EFLASH_LOGE("PROG fail @0x%08X st=%d\n", addr, (int)flash_status);
            break;
        }
    }
    
    FLASH_Lock();
    #endif
    
    /* 如果部分写入成功，返回部分成功错误码 */
    if (result != EF_OK && i > 0 && i < size) {
        EFLASH_LOGW("PART write %d/%d\n", (int)i, (int)size);
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
    
    EFLASH_LOGI("PORT TEST\n");
    EFLASH_LOGI("Addr: 0x%08X\n", test_addr);
    
    /* 测试1: 擦除Flash */
    EFLASH_LOGI("1) Erase...\n");
    result = flash_port_erase(test_addr, FLASH_PAGE_SIZE);
    if (result != EF_OK) {
        EFLASH_LOGE("Erase fail err=%d\n", (int)result);
        return result;
    }
    EFLASH_LOGI("Erase OK\n");
    
    /* 验证擦除结果 */
    EFLASH_LOGI("2) Verify erase...\n");
    result = flash_port_read(test_addr, read_data, 4);
    if (result != EF_OK) {
        EFLASH_LOGE("Read fail err=%d\n", (int)result);
        return result;
    }
    
    for (i = 0; i < 4; i++) {
        if (read_data[i] != 0xFF) {
            EFLASH_LOGE("Erase verify fail off=%d exp=0xFF got=0x%02X\n", 
                   (int)i, read_data[i]);
            return EF_ERR_ERASE;
        }
    }
    EFLASH_LOGI("Erase verify OK\n");
    
    /* 测试2: 写入数据 */
    EFLASH_LOGI("3) Write...\n");
    result = flash_port_write(test_addr, test_data, 4);
    if (result != EF_OK) {
        EFLASH_LOGE("Write fail err=%d\n", (int)result);
        return result;
    }
    EFLASH_LOGI("Write OK\n");
    
    /* 测试3: 读取并验证数据 */
    EFLASH_LOGI("4) Read & verify...\n");
    memset(read_data, 0, sizeof(read_data));
    result = flash_port_read(test_addr, read_data, 4);
    if (result != EF_OK) {
        EFLASH_LOGE("Read fail err=%d\n", (int)result);
        return result;
    }
    
    for (i = 0; i < 4; i++) {
        if (read_data[i] != test_data[i]) {
            EFLASH_LOGE("Mismatch off=%d exp=0x%02X got=0x%02X\n", 
                   (int)i, test_data[i], read_data[i]);
            return EF_ERR_VERIFY;
        }
    }
    EFLASH_LOGI("Verify OK\n");
    
    EFLASH_LOGI("PORT TEST OK\n");
    
    return EF_OK;
}

