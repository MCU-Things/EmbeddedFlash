// EmbeddedFlash.c - 持久化存储组件实现
/*
初始化与自检:        扫描扇区、重建索引、首启格式化、异常状态恢复。
原子性与断电安全:    预写→提交状态机、双写/日志、幂等恢复流程。
磨损均衡与循环写:    扇区角色轮转、写放大控制、写入配额/节流。
垃圾回收（GC）:      增量搬移、扇区角色切换、可中断与恢复、进度持久化。
*/

#include "EmbeddedFlash.h"
#include "crc16_x25.h"
#include <stdint.h>
#include <string.h>

// 全局变量
static sector_desc_t m_sector_desc_list[KV_SECTOR_COUNT] = {
    {.sector_addr = KV_SECTOR_START_ADDR+0*KV_SECTOR_SIZE, .sector_idx = 0, .attr = {EFLASH_SECTOR_STATUS_FREE, EFLASH_SECTOR_ROLE_UNASSIGNED}, .free_space = KV_SECTOR_SIZE - sizeof(sector_header_t), .record_count = 0},
    {.sector_addr = KV_SECTOR_START_ADDR+1*KV_SECTOR_SIZE, .sector_idx = 1, .attr = {EFLASH_SECTOR_STATUS_FREE, EFLASH_SECTOR_ROLE_UNASSIGNED}, .free_space = KV_SECTOR_SIZE - sizeof(sector_header_t), .record_count = 0},
    {.sector_addr = KV_SECTOR_START_ADDR+2*KV_SECTOR_SIZE, .sector_idx = 2, .attr = {EFLASH_SECTOR_STATUS_FREE, EFLASH_SECTOR_ROLE_UNASSIGNED}, .free_space = KV_SECTOR_SIZE - sizeof(sector_header_t), .record_count = 0},
    {.sector_addr = KV_SECTOR_START_ADDR+3*KV_SECTOR_SIZE, .sector_idx = 3, .attr = {EFLASH_SECTOR_STATUS_FREE, EFLASH_SECTOR_ROLE_UNASSIGNED}, .free_space = KV_SECTOR_SIZE - sizeof(sector_header_t), .record_count = 0}
};
static kv_data_t *mp_kv_list = NULL;  // 指向外部默认配置
static uint8_t m_kv_data_list_count = 0;

#if EFLASH_ENABLE_ERASE_COUNTER
// 扇区擦除统计信息
static eflash_erase_stats_t m_erase_stats = {
    .sector_erase_count = {0},
    .total_erase_count = 0,
    .max_erase_count = 0,
    .max_erase_sector_idx = 0
};
#endif


/* ==================== 静态函数前向声明 ==================== */

/* --- 1. 状态表底层操作（最底层） --- */
static size_t _set_status(uint8_t status_table[], size_t status_num, size_t status_index);
static size_t _get_status(uint8_t status_table[], size_t status_num);
static size_t _read_status(uint32_t addr, uint8_t status_table[], size_t total_num);
static EF_ErrCode _write_status(uint32_t addr, uint8_t status_table[], size_t status_num, size_t status_index);

/* --- 2. 类型安全的状态表操作（状态表的封装） --- */
static size_t _set_kv_status_table(uint8_t status_table[], EmbeddedFlash_record_status_e status);
static size_t _set_sector_status_table(uint8_t status_table[], EmbeddedFlash_sector_status_e status);
static size_t _set_sector_role_table(uint8_t role_table[], EmbeddedFlash_sector_role_e role);
static EmbeddedFlash_record_status_e _get_kv_status_from_table(uint8_t status_table[]);
static EmbeddedFlash_sector_status_e _get_sector_status_from_table(uint8_t status_table[]);
static EmbeddedFlash_sector_role_e _get_sector_role_from_table(uint8_t role_table[]);

/* ---  类型安全的状态操作（Flash读写封装） --- */
static EF_ErrCode _write_kv_status(uint32_t addr, EmbeddedFlash_record_status_e status);
static EF_ErrCode _write_sector_status(uint32_t addr, EmbeddedFlash_sector_status_e status);
static EF_ErrCode _write_sector_role(uint32_t addr, EmbeddedFlash_sector_role_e role);
static EmbeddedFlash_record_status_e _read_kv_status(uint32_t addr);
static EmbeddedFlash_sector_status_e _read_sector_status(uint32_t addr);
static EmbeddedFlash_sector_role_e _read_sector_role(uint32_t addr);

/* ---  扇区管理 --- */
static EF_ErrCode _sector_magic_write(uint8_t sector_idx);
static int _sector_header_read(uint8_t sector_idx, sector_header_t *header);
static bool _is_sector_header(const sector_header_t *header);
static int _find_sector_role(uint8_t role);
static EF_ErrCode _erase_sector(uint8_t sector_idx);
static EF_ErrCode _init_all_sector_attr(void);
static embedded_flash_att_status_t embedded_flash_get_attr_status(void);

/* ---  KV记录操作 --- */
static EmbeddedFlash_record_status_e _is_kv_record(const KV_Record *record);
static EF_ErrCode _kv_delete_record(uint32_t addr);
static kv_data_t* _find_kv_data(uint8_t key);
static int _find_latest_record(kv_data_t *p_kv_data, KV_Record *record, uint32_t *addr);

/* ---  KV记录写入 --- */
static int _find_writable_data_sector(uint16_t required_space);
static uint32_t _write_kv_record_to_sector(uint8_t sector_idx, KV_Record *p);
static uint32_t _write_kv_record(KV_Record *p);

/* ---  数据迁移和垃圾回收（GC） --- */
static EF_ErrCode _migrate_sector_data(uint8_t source_sector_idx, uint8_t target_sector_idx);
static EF_ErrCode ef_embedded_flash_gc(void);

/* ---  初始化和加载 --- */
static EF_ErrCode _init_all_kv_record(void);
static EF_ErrCode _iteration(EF_ErrCode (*func)(KV_Record *record, uint32_t abs_addr));
static EF_ErrCode _load_kv_record_callback(KV_Record *record, uint32_t abs_addr);

/* ---  核心API实现 --- */
static EF_ErrCode embedded_flash_set(uint8_t key, const uint8_t *value, uint8_t length, uint8_t data_type);


/**
 * @brief 写入扇区魔术字
 * @param sector_idx 扇区索引
 * @return 
 */
static EF_ErrCode _sector_magic_write(uint8_t sector_idx)
{
    // 获取扇区地址
    uint32_t sector_addr = m_sector_desc_list[sector_idx].sector_addr;
    
    // 魔术字在扇区头中的偏移量 = 状态表大小 + 角色表大小
    uint32_t magic_offset = SECTOR_STATUS_TABLE_SIZE + SECTOR_ROLE_TABLE_SIZE;
    uint32_t magic_addr = sector_addr + magic_offset;
    
    // 写入魔术字
	  uint32_t data = SECTOR_HEADER_MAGIC_WORD;
    if (flash_port_write(magic_addr, (uint8_t*)&data, sizeof(uint32_t)) != EF_OK) {
        printf("Failed to write sector magic word at addr=0x%08X\n", magic_addr);
        return EF_ERR_WRITE;
    }
    
    printf("Sector %d magic word written successfully at 0x%08X\n", sector_idx, magic_addr);
    return EF_OK;
}
/**
 * @brief 读取扇区头信息并解析状态
 * @param sector_idx 扇区索引
 * @param header 扇区头信息指针
 * @return 0=成功, -1=失败
 */
static int _sector_header_read(uint8_t sector_idx, sector_header_t *header) {
    if (sector_idx < KV_SECTOR_COUNT && header != NULL) {
        uint32_t addr = m_sector_desc_list[sector_idx].sector_addr;
        
        /* 读取扇区头 */
        if (flash_port_read(addr, (uint8_t*)header, sizeof(sector_header_t)) == EF_OK) {
            if (!_is_sector_header(header)) {
                return -1;
            }
            
            /* 解析状态表，更新RAM中的attr */
            m_sector_desc_list[sector_idx].attr.status = (uint8_t)_get_sector_status_from_table(header->status_table);
            m_sector_desc_list[sector_idx].attr.role = (uint8_t)_get_sector_role_from_table(header->role_table);
            
            return 0;
        }
    }
    return -1;
}
static embedded_flash_att_status_t embedded_flash_get_attr_status(void)
{
	uint8_t gc_cnt = 0;
	uint8_t gc_temp_cnt = 0;
	uint8_t data_cnt = 0;
	uint8_t data_gcing_cnt = 0;
	uint8_t empty_cnt = 0;
    sector_header_t header={0};
    
    printf("Checking system status...\n");
	for (int i = 0; i < KV_SECTOR_COUNT; i++) {
		if (_sector_header_read(i, &header) != 0) {
			empty_cnt++;
			printf("Sector %d: empty\n", i);
		}else{
            uint8_t role = (uint8_t)_get_sector_role_from_table(header.role_table);
            switch (role) {
                case EFLASH_SECTOR_ROLE_GC:         gc_cnt++;           break;
                case EFLASH_SECTOR_ROLE_GC_TEMP:    gc_temp_cnt++;      break;
                case EFLASH_SECTOR_ROLE_DATA:       data_cnt++;         break;
                case EFLASH_SECTOR_ROLE_DATA_GCING: data_gcing_cnt++;   break;
            }
            printf("Sector %d: role=%d, status=%d\n", i, role, (uint8_t)_get_sector_status_from_table(header.status_table));
        }
	}
	
	printf("Status summary: gc=%d, gc_temp=%d, data=%d, data_gcing=%d, empty=%d\n", 
	       gc_cnt, gc_temp_cnt, data_cnt, data_gcing_cnt, empty_cnt);
	
    uint8_t status = EFLASH_STATUS_UNKNOWN_ERROR;
	if (gc_temp_cnt > 1 || gc_cnt > 1 || data_gcing_cnt > 1) {
		status = EFLASH_STATUS_MULTI_ROLE_ERROR; // GC_TEMP>1 或 GC>1 或 DATA_GCING>1
		printf("Status: MULTI_ROLE_ERROR\n");
	}
    
    else if (gc_cnt == 0 && gc_temp_cnt == 0 && data_cnt == 0 && data_gcing_cnt == 0 && empty_cnt == KV_SECTOR_COUNT) {
		status = EFLASH_STATUS_FIRST_POWER_ON; // 首次上电：全空白
		printf("Status: FIRST_POWER_ON\n");
	}

	else if (gc_cnt == 1 && gc_temp_cnt == 0 && data_cnt >= 1 && data_gcing_cnt == 0 && empty_cnt == 0) {
		status =  EFLASH_STATUS_NORMAL; // 正常：1 GC，无 GC_TEMP，有 DATA，无 DATA_GCING，无空白
		printf("Status: NORMAL\n");
	}

	else if (gc_cnt == 0 && gc_temp_cnt == 1 && data_cnt >= 1 && data_gcing_cnt == 0 && empty_cnt == 0) {
		status =  EFLASH_STATUS_GC_PREPARE; // 异常1：无 GC，有 GC_TEMP，有 DATA，无 DATA_GCING
		printf("Status: GC_PREPARE\n");
	}

	else if (gc_cnt == 0 && gc_temp_cnt == 1 && data_cnt >= 1 && data_gcing_cnt == 1 && empty_cnt == 0) {
		status =  EFLASH_STATUS_GC_MIGRATING; // 异常2：无 GC，有 GC_TEMP，有 DATA，有 DATA_GCING
		printf("Status: GC_MIGRATING\n");
	}

	else if (gc_cnt == 0 && gc_temp_cnt == 0 && data_cnt >= 1 && data_gcing_cnt == 1 && empty_cnt == 0) {
		status =  EFLASH_STATUS_GC_AFTER_MIGRATE; // 异常3：无 GC，无 GC_TEMP，有 DATA，有 DATA_GCING
		printf("Status: GC_AFTER_MIGRATE\n");
	}

	else if (gc_cnt == 0 && gc_temp_cnt == 0 && data_cnt >= 1 && data_gcing_cnt == 0 && empty_cnt >= 1) {
		status =  EFLASH_STATUS_GC_AFTER_MIGRATE_WITH_EMPTY; // 异常4：无 GC，无 GC_TEMP，有 DATA，无 DATA_GCING，有空白
		printf("Status: GC_AFTER_MIGRATE_WITH_EMPTY\n");
	}

	else if (empty_cnt > 0 && (data_cnt > 0 || data_gcing_cnt > 0 || gc_cnt > 0 || gc_temp_cnt > 0)) {
		status =  EFLASH_STATUS_EMPTY_SECTOR_ERROR; // 存在空白但不符合上面场景
		printf("Status: EMPTY_SECTOR_ERROR\n");
	}

	else {
		printf("Status: UNKNOWN_ERROR\n");
	}

	return status; // 兜底
}



// ==================== 扇区擦除统计API实现 ====================
#if EFLASH_ENABLE_ERASE_COUNTER

/**
 * @brief 获取擦除统计信息
 * @param stats 输出参数，存储统计信息
 */
void embedded_flash_get_erase_stats(eflash_erase_stats_t *stats) {
    if (stats == NULL) {
        return;
    }
    
    // 复制统计信息
    memcpy(stats, &m_erase_stats, sizeof(eflash_erase_stats_t));
}

/**
 * @brief 重置擦除统计信息
 */
void embedded_flash_reset_erase_stats(void) {
    memset(&m_erase_stats, 0, sizeof(eflash_erase_stats_t));
}

/**
 * @brief 打印擦除统计信息
 */
void embedded_flash_print_erase_stats(void) {
    printf("\n========== Flash Erase Statistics ==========\n");
    printf("Total erase operations: %lu\n", (unsigned long)m_erase_stats.total_erase_count);
    printf("Max erase count:        %lu (Sector %d at 0x%08X)\n", 
           (unsigned long)m_erase_stats.max_erase_count,
           m_erase_stats.max_erase_sector_idx,
           m_sector_desc_list[m_erase_stats.max_erase_sector_idx].sector_addr);
    
    printf("\nPer-sector erase counts:\n");
    for (int i = 0; i < KV_SECTOR_COUNT; i++) {
        uint32_t erase_count = m_erase_stats.sector_erase_count[i];
        float wear_percent = (float)erase_count * 100.0f / CHIP_FLASH_ERASE_MAXTIMES;
        
        printf("  Sector %d (0x%08X): %5lu erases (%.2f%% wear)", 
               i,
               m_sector_desc_list[i].sector_addr,
               (unsigned long)erase_count,
               wear_percent);
        
        // 显示磨损等级
        if (wear_percent < 25.0f) {
            printf(" [LOW]");
        } else if (wear_percent < 50.0f) {
            printf(" [MEDIUM]");
        } else if (wear_percent < 75.0f) {
            printf(" [HIGH]");
        } else if (wear_percent < 90.0f) {
            printf(" [CRITICAL]");
        } else {
            printf(" [DANGER!]");
        }
        
        // 显示最大磨损扇区标记
        if (i == m_erase_stats.max_erase_sector_idx && m_erase_stats.max_erase_count > 0) {
            printf(" <-- Max wear");
        }
        
        printf("\n");
    }
    
    // 计算平均擦除次数
    float avg_erase = (float)m_erase_stats.total_erase_count / KV_SECTOR_COUNT;
    printf("\nAverage erase count:    %.1f\n", avg_erase);
    
    // 计算磨损均衡度（最大值与平均值的比率）
    if (avg_erase > 0) {
        float wear_balance = (float)m_erase_stats.max_erase_count / avg_erase;
        printf("Wear balance ratio:     %.2f ", wear_balance);
        if (wear_balance < 1.5f) {
            printf("(Excellent)\n");
        } else if (wear_balance < 2.0f) {
            printf("(Good)\n");
        } else if (wear_balance < 3.0f) {
            printf("(Fair)\n");
        } else {
            printf("(Poor - Consider improving wear leveling)\n");
        }
    }
    
    // 估算剩余寿命
    if (m_erase_stats.max_erase_count > 0) {
        uint32_t remaining = CHIP_FLASH_ERASE_MAXTIMES - m_erase_stats.max_erase_count;
        printf("Estimated remaining:    %lu erases on most worn sector\n", (unsigned long)remaining);
        
        if (m_erase_stats.max_erase_count >= CHIP_FLASH_ERASE_MAXTIMES) {
            printf("\n[WARNING] Flash has exceeded rated endurance!\n");
        } else if (m_erase_stats.max_erase_count >= CHIP_FLASH_ERASE_MAXTIMES * 0.9f) {
            printf("\n[WARNING] Flash is approaching end of life!\n");
        }
    }
    
    printf("============================================\n\n");
}

#endif



// ==================== 状态表操作函数实现 ====================
/**
 * @brief 设置状态表
 * @param status_table 状态表缓冲区
 * @param status_num 状态总数
 * @param status_index 当前状态索引
 * @return 写入的字节索引
 */
 static size_t _set_status(uint8_t status_table[], size_t status_num, size_t status_index)
 {
     size_t byte_index = ~0UL;
     
     /* 初始化状态表为全FF */
    //  memset(status_table, 0xFF, STATUS_TABLE_SIZE(status_num));
     
     if (status_index > 0) {
         /* 对于32位写入粒度，每个状态占用4字节 */
         byte_index = (status_index - 1) * (EFLASH_WRITE_GRAN / 8);
         /* 将4字节的第一个设为0x00，其他设置为0xFF，因为Flash写入粒度是4字节 */
         for (size_t i = 0; i < EFLASH_WRITE_GRAN / 8; i++) {
             status_table[byte_index + i] = (i == 0) ? 0x00 : 0xFF;
         }
     }
     
     return byte_index;
 }
 
 /**
  * @brief 获取当前状态
  * @param status_table 状态表缓冲区
  * @param status_num 状态总数
  * @return 当前状态索引
  */
 static size_t _get_status(uint8_t status_table[], size_t status_num)
 {
     size_t i;
     size_t highest_status = 0;
     
     /* 从后往前查找所有0x00的位置，返回最高的状态 */
     for (i = status_num; i > 0; i--) {
         if (status_table[(i - 1) * EFLASH_WRITE_GRAN / 8] == 0x00) {
             highest_status = i;  /* 找到最高状态，立即返回 */
             break;
         }
     }
     
     return highest_status;  /* 返回最高状态，如果全FF则返回0 */
 }
 /**
* @brief 从Flash读取状态
* @param addr 读取地址
* @param status_table 状态表缓冲区
* @param total_num 状态总数
* @return 当前状态索引
*/
static size_t _read_status(uint32_t addr, uint8_t status_table[], size_t total_num)
{
		flash_port_read(addr, (uint8_t *)status_table, STATUS_TABLE_SIZE(total_num));
		return _get_status(status_table, total_num);
}

/**
* @brief 写入状态到Flash（优化版：只写入需要改变的区域）
* @param addr 写入地址
* @param status_table 状态表缓冲区
* @param status_num 状态总数
* @param status_index 目标状态索引
* @return 错误码
*/
static EF_ErrCode _write_status(uint32_t addr, uint8_t status_table[], size_t status_num, size_t status_index)
{
	 EF_ErrCode result = EF_OK;
	 size_t byte_index;
	 
	 /* 设置状态 */
	 byte_index = _set_status(status_table, status_num, status_index);
	 
	 /* status0（全FF）无需写入Flash */
	 if (byte_index == ~0UL) {
        printf("status0 (full FF) does not need to be written.\r\n");
			return EF_OK;
	 }
	 
	 /* 写入4字节（32位）到对应偏移 */
	 result = flash_port_write(addr + byte_index, (uint8_t *)&status_table[byte_index], EFLASH_WRITE_GRAN / 8);
	 printf("addr:0x%x size:%d, status_index:%d\r\n", addr + byte_index, EFLASH_WRITE_GRAN / 8, status_index);
	 for(uint8_t i=0; i<EFLASH_WRITE_GRAN / 8 ; i++){
		 printf("data[%d]:0x%02X\r\n", i, status_table[byte_index + i]);
	 }
	 return result;
}



// ==================== 类型安全的状态操作函数实现 ====================

/**
 * @brief 写入KV记录状态（类型安全）
 * @param addr KV记录地址
 * @param status 记录状态
 * @return 错误码
 */
 static EF_ErrCode _write_kv_status(uint32_t addr, EmbeddedFlash_record_status_e status) {
    printf("write kv status: %d\r\n", status);
    uint8_t status_table[KV_STATUS_TABLE_SIZE];
    memset(status_table, 0xFF, KV_STATUS_TABLE_SIZE);
    return _write_status(addr, status_table, KV_STATUS_NUM, (size_t)status);
}

/**
 * @brief 写入扇区状态（类型安全）
 * @param addr 扇区头地址
 * @param status 扇区状态
 * @return 错误码
 */
static EF_ErrCode _write_sector_status(uint32_t addr, EmbeddedFlash_sector_status_e status) {
    printf("write sector status: %d\r\n", status);
    uint8_t status_table[SECTOR_STATUS_TABLE_SIZE];
    memset(status_table, 0xFF, SECTOR_STATUS_TABLE_SIZE);
    return _write_status(addr, status_table, SECTOR_STATUS_NUM, (size_t)status);
}

/**
 * @brief 写入扇区角色（类型安全）
 * @param addr 扇区头地址 + 状态表偏移
 * @param role 扇区角色
 * @return 错误码
 */
static EF_ErrCode _write_sector_role(uint32_t addr, EmbeddedFlash_sector_role_e role) {
    printf("write sector role: %d\r\n", role);
    uint8_t role_table[SECTOR_ROLE_TABLE_SIZE];
    memset(role_table, 0xFF, SECTOR_ROLE_TABLE_SIZE);
    return _write_status(addr + SECTOR_STATUS_TABLE_SIZE, role_table, SECTOR_ROLE_NUM, (size_t)role);
}

/**
 * @brief 读取KV记录状态（类型安全）
 * @param addr KV记录地址
 * @return 记录状态
 */
static EmbeddedFlash_record_status_e _read_kv_status(uint32_t addr) {
    uint8_t status_table[KV_STATUS_TABLE_SIZE];
    size_t status = _read_status(addr, status_table, KV_STATUS_NUM);
    return (EmbeddedFlash_record_status_e)status;
}

/**
 * @brief 读取扇区状态（类型安全）
 * @param addr 扇区头地址
 * @return 扇区状态
 */
static EmbeddedFlash_sector_status_e _read_sector_status(uint32_t addr) {
    uint8_t status_table[SECTOR_STATUS_TABLE_SIZE];
    size_t status = _read_status(addr, status_table, SECTOR_STATUS_NUM);
    return (EmbeddedFlash_sector_status_e)status;
}

/**
 * @brief 读取扇区角色（类型安全）
 * @param addr 扇区头地址 + 状态表偏移
 * @return 扇区角色
 */
static EmbeddedFlash_sector_role_e _read_sector_role(uint32_t addr) {
    uint8_t role_table[SECTOR_ROLE_TABLE_SIZE];
    size_t role = _read_status(addr + SECTOR_STATUS_TABLE_SIZE, role_table, SECTOR_ROLE_NUM);
    return (EmbeddedFlash_sector_role_e)role;
}

// ==================== 类型安全的状态表操作函数实现 ====================

/**
 * @brief 设置KV记录状态表（类型安全）
 * @param status_table 状态表缓冲区
 * @param status 记录状态
 * @return 写入的字节索引
 */
static size_t _set_kv_status_table(uint8_t status_table[], EmbeddedFlash_record_status_e status) {
    return _set_status(status_table, KV_STATUS_NUM, (size_t)status);
}

/**
 * @brief 设置扇区状态表（类型安全）
 * @param status_table 状态表缓冲区
 * @param status 扇区状态
 * @return 写入的字节索引
 */
static size_t _set_sector_status_table(uint8_t status_table[], EmbeddedFlash_sector_status_e status) {
    return _set_status(status_table, SECTOR_STATUS_NUM, (size_t)status);
}

/**
 * @brief 设置扇区角色表（类型安全）
 * @param role_table 角色表缓冲区
 * @param role 扇区角色
 * @return 写入的字节索引
 */
static size_t _set_sector_role_table(uint8_t role_table[], EmbeddedFlash_sector_role_e role) {
    return _set_status(role_table, SECTOR_ROLE_NUM, (size_t)role);
}

/**
 * @brief 从状态表获取KV记录状态（类型安全）
 * @param status_table 状态表缓冲区
 * @return 记录状态
 */
static EmbeddedFlash_record_status_e _get_kv_status_from_table(uint8_t status_table[]) {
    return (EmbeddedFlash_record_status_e)_get_status(status_table, KV_STATUS_NUM);
}

/**
 * @brief 从状态表获取扇区状态（类型安全）
 * @param status_table 状态表缓冲区
 * @return 扇区状态
 */
static EmbeddedFlash_sector_status_e _get_sector_status_from_table(uint8_t status_table[]) {
    return (EmbeddedFlash_sector_status_e)_get_status(status_table, SECTOR_STATUS_NUM);
}

/**
 * @brief 从角色表获取扇区角色（类型安全）
 * @param role_table 角色表缓冲区
 * @return 扇区角色
 */
static EmbeddedFlash_sector_role_e _get_sector_role_from_table(uint8_t role_table[]) {
    return (EmbeddedFlash_sector_role_e)_get_status(role_table, SECTOR_ROLE_NUM);
}




/***** v2 开始 *****/

// 获取数据类型的固定长度
uint8_t embedded_flash_get_type_size(EmbeddedFlash_data_type_e data_type) 
{
    switch (data_type) {
        case EFLASH_FORMAT_BOOL:
        case EFLASH_FORMAT_UINT8:
        case EFLASH_FORMAT_INT8:
            return 1;
        case EFLASH_FORMAT_UINT16:
        case EFLASH_FORMAT_INT16:
            return 2;
        case EFLASH_FORMAT_FLOAT:
        case EFLASH_FORMAT_UINT32:
        case EFLASH_FORMAT_INT32:
            return 4;
        case EFLASH_FORMAT_UINT64:
        case EFLASH_FORMAT_INT64:
            return 8;
        case EFLASH_FORMAT_STRING:
        case EFLASH_FORMAT_HEX:
            return 0; // 可变长度，需要用户指定
        default:
            return 0;
    }
}



/**
 * @brief 写入记录到指定扇区
 * @param sector_idx 目标扇区索引
 * @param p KV记录指针
 * @return 写入地址(成功)，0(失败)
 */
static uint32_t _write_kv_record_to_sector(uint8_t sector_idx, KV_Record *p) 
{
    // 计算写入地址 = 扇区起始地址 + 扇区头大小 + 已使用空间
    uint32_t write_addr = m_sector_desc_list[sector_idx].sector_addr + sizeof(sector_header_t) + 
                         (KV_SECTOR_SIZE - sizeof(sector_header_t) - m_sector_desc_list[sector_idx].free_space);
    
    // 写入Flash
    if (flash_port_write(write_addr, (uint8_t*)p, sizeof(KV_Record)) != EF_OK){
		printf("Failed to write record to sector %d, addr=0x%08X\n", sector_idx, write_addr);
        EFLASH_ASSERT(0);
        return 0;  // 返回0表示失败
    }
    
    // 保存原始状态
    uint8_t original_status = m_sector_desc_list[sector_idx].attr.status;
    
    // 更新扇区信息
    m_sector_desc_list[sector_idx].attr.status = EFLASH_SECTOR_STATUS_USING;
    m_sector_desc_list[sector_idx].record_count++;
    m_sector_desc_list[sector_idx].free_space -= sizeof(KV_Record);

    // 检查扇区是否已满
    if (m_sector_desc_list[sector_idx].free_space < sizeof(KV_Record)) {
        m_sector_desc_list[sector_idx].attr.status = EFLASH_SECTOR_STATUS_FULL;
    }
    
    // 只有状态改变时才写入扇区头（状态/角色）
    if (m_sector_desc_list[sector_idx].attr.status != original_status) {
        printf("original_sector_status:%d, new_sector_status:%d\n", original_status, m_sector_desc_list[sector_idx].attr.status);
        if (_write_sector_status(m_sector_desc_list[sector_idx].sector_addr, (EmbeddedFlash_sector_status_e)m_sector_desc_list[sector_idx].attr.status) != EF_OK) {
			printf("Failed to write sector status to sector %d, addr=0x%08X\n", sector_idx, m_sector_desc_list[sector_idx].sector_addr);
            return 0;
        }
        //扇区角色不需要在这里管理
        // if (_write_sector_role(m_sector_desc_list[sector_idx].sector_addr, (EmbeddedFlash_sector_role_e)m_sector_desc_list[sector_idx].attr.role) != EF_OK) {
		// 	printf("Failed to write sector role to sector %d, addr=0x%08X\n", sector_idx, m_sector_desc_list[sector_idx].sector_addr);
        //     return 0;
        // }
    }
    return write_addr;
}



/**
 * @brief 寻找可写入的数据扇区（有足够空间的）
 * @param required_space 需要的空间大小
 * @note 从GC区+1开始，按循环顺序查找可用的数据扇区
 * @note 这是循环存储模式的关键实现
 * @return 扇区索引，-1表示所有数据扇区都满了，需要GC
 */
static int _find_writable_data_sector(uint16_t required_space) 
{
    int gc_sector = _find_sector_role(EFLASH_SECTOR_ROLE_GC);
    if (gc_sector >= 0) {
        printf("Found GC sector at index %d\n", gc_sector);
        // 遍历所有数据扇区，寻找有足够空间的扇区
        // 从GC区+1开始，按循环顺序查找（循环存储模式）
        for (int i = 0; i < (KV_SECTOR_COUNT - 1); i++) {
            uint8_t pos = (gc_sector + 1 + i) % KV_SECTOR_COUNT; // 从第一个数据区开始查找
            
            // 检查扇区是否满足写入条件：
            // 1. 有足够的空闲空间
            // 2. 状态为USING或FREE（可以写入）
            // 3. 角色为DATA（数据扇区）
            printf("Checking sector %d: free_space=%d, status=%d, role=%d\n", 
                   pos, m_sector_desc_list[pos].free_space, 
                   m_sector_desc_list[pos].attr.status, 
                   m_sector_desc_list[pos].attr.role);
                   
            if (m_sector_desc_list[pos].free_space >= required_space 
                && (m_sector_desc_list[pos].attr.status == EFLASH_SECTOR_STATUS_USING
                || m_sector_desc_list[pos].attr.status == EFLASH_SECTOR_STATUS_FREE)
                && m_sector_desc_list[pos].attr.role == EFLASH_SECTOR_ROLE_DATA) {
                printf("Found writable data sector %d\n", pos);
                return pos;  // 找到有足够空间的数据扇区
            }
        }
        printf("No writable data sector found with required_space=%d\n", required_space);
    } else {
        printf("Failed to find GC sector - all sectors:\n");
        for (int i = 0; i < KV_SECTOR_COUNT; i++) {
            printf("  Sector %d: addr=0x%08X, status=%d, role=%d\n", 
                   i, m_sector_desc_list[i].sector_addr,
                   m_sector_desc_list[i].attr.status, 
                   m_sector_desc_list[i].attr.role);
        }
    }
    return -1;  // 所有数据扇区都满了，需要GC
}
static uint32_t _write_kv_record(KV_Record *p) 
{
    int sector_idx = 0;
restart_write:   
    // 寻找有足够空间的可写入扇区
    sector_idx = _find_writable_data_sector(sizeof(KV_Record));
    if (sector_idx < 0) {
        // 所有数据扇区都满了，需要GC
        if (ef_embedded_flash_gc() != EF_OK) {
            printf("Failed to perform GC for writing record\n");
            return 0;  // 返回0表示失败
        }
        goto restart_write;  // GC后重新寻找可写入扇区
    }
    
    // 使用新的写入函数
    return _write_kv_record_to_sector(sector_idx, p);
}


/**
 * @brief 擦除扇区
 */
static EF_ErrCode _erase_sector(uint8_t sector_idx) {

    printf("EmbeddedFlash: Erasing sector %d at address 0x%08X, size=%d\n", 
            sector_idx, m_sector_desc_list[sector_idx].sector_addr, KV_SECTOR_SIZE);
    if (flash_port_erase(m_sector_desc_list[sector_idx].sector_addr, KV_SECTOR_SIZE) == EF_OK) {
            // 重置扇区信息
            m_sector_desc_list[sector_idx].attr.status = EFLASH_SECTOR_STATUS_FREE;
            m_sector_desc_list[sector_idx].free_space = KV_SECTOR_SIZE - sizeof(sector_header_t);  // 减去扇区头大小
            m_sector_desc_list[sector_idx].record_count = 0;
            
            #if EFLASH_ENABLE_ERASE_COUNTER
            // 更新擦除统计信息
            m_erase_stats.sector_erase_count[sector_idx]++;
            m_erase_stats.total_erase_count++;
            
            // 更新最大擦除次数信息
            if (m_erase_stats.sector_erase_count[sector_idx] > m_erase_stats.max_erase_count) {
                m_erase_stats.max_erase_count = m_erase_stats.sector_erase_count[sector_idx];
                m_erase_stats.max_erase_sector_idx = sector_idx;
            }
            #endif
            return EF_OK;  
    }else{
        printf("erase sector failed, sector_idx=%d\n", sector_idx);
        EFLASH_ASSERT(0);
        return EF_ERR_ERASE;
    }
}

/* 初始化扇区头 */
static EF_ErrCode _init_all_sector_attr(void)
{
    printf("EmbeddedFlash: Initializing all sector attributes...\n");
	for(uint8_t i = 0; i < KV_SECTOR_COUNT; i++){
        //擦除扇区
        EF_ErrCode ret = _erase_sector(i);
        //写入新的头信息
        if(i == KV_SECTOR_COUNT-1) {
            //GC区
            ret |= _write_sector_status(m_sector_desc_list[i].sector_addr, EFLASH_SECTOR_STATUS_FREE);
            m_sector_desc_list[i].attr.role = EFLASH_SECTOR_ROLE_GC;
            ret |= _write_sector_role(m_sector_desc_list[i].sector_addr, EFLASH_SECTOR_ROLE_GC);
            m_sector_desc_list[i].attr.status = EFLASH_SECTOR_STATUS_FREE;
        }else{
            //数据区
            ret |= _write_sector_status(m_sector_desc_list[i].sector_addr, EFLASH_SECTOR_STATUS_FREE);
            m_sector_desc_list[i].attr.role = EFLASH_SECTOR_ROLE_DATA;
            ret |= _write_sector_role(m_sector_desc_list[i].sector_addr, EFLASH_SECTOR_ROLE_DATA);
            m_sector_desc_list[i].attr.status = EFLASH_SECTOR_STATUS_FREE;
        }
				ret |= _sector_magic_write(i);
        if(ret != EF_OK){return ret;}
	}	
    return EF_OK;
}

/* 初始化KV记录 */
static EF_ErrCode _init_all_kv_record(void)
{
    printf("EmbeddedFlash: Initializing all KV records...\n");
    for (int i = 0; i < m_kv_data_list_count; i++) {
        if (mp_kv_list[i].data_source == KV_DATA_SOURCE_DEFAULT) {
            // 构造默认值记录
            KV_Record record={0};
            record.magic = KV_HEADER_MAGIC;
            memset(record.status_table, 0xFF, sizeof(record.status_table));
            _set_kv_status_table(record.status_table, EFLASH_KV_PRE_WRITE);
            EFLASH_PRINT_HEX("status_table", record.status_table, sizeof(record.status_table));
            
            record.data_type = mp_kv_list[i].data_type;
            record.key = mp_kv_list[i].key;
            record.value_length = mp_kv_list[i].value_length;
            
            // 检查边界，防止数组越界
            if (mp_kv_list[i].value_length > KV_MAX_VALUE_SIZE) {
                printf("CRITICAL: default value_length=%d exceeds KV_MAX_VALUE_SIZE=%d for key=%d\n", 
                       mp_kv_list[i].value_length, KV_MAX_VALUE_SIZE, mp_kv_list[i].key);
                return EF_ERR_PARAM;
            }
            
            memset(record.value, 0, KV_MAX_VALUE_SIZE);
            memcpy(record.value, mp_kv_list[i].value, mp_kv_list[i].value_length);
            // 计算CRC - 跳过status_table(16) + magic(1) + data_type(1)，从key开始
            record.crc = crc16_x25_calculate((uint8_t*)&record.key, 2 + KV_MAX_VALUE_SIZE);
            
            // 写入记录（使用状态表机制）
            uint32_t write_addr_abs = _write_kv_record(&record);
            if(write_addr_abs == 0 ){
				printf("Failed to write default value for key=%d\n", mp_kv_list[i].key);
                return EF_ERR_WRITE;
            }
            
            // 使用类型安全函数标记为WRITE状态
            if(_write_kv_status(write_addr_abs, EFLASH_KV_WRITE) != EF_OK){
                printf("Failed to set WRITE status for key=%d at addr=0x%08X\n", mp_kv_list[i].key, write_addr_abs);
                EFLASH_ASSERT(0);
                return EF_ERR_WRITE;
            }
            
            // 更新RAM中的状态
            mp_kv_list[i].addr_abs = write_addr_abs;
            mp_kv_list[i].data_source = KV_DATA_SOURCE_FIRST_WRITE;
            
            printf("Initialized default value for key=%d at addr=0x%08X\n", mp_kv_list[i].key, write_addr_abs);
        }
    }

    return EF_OK;
}


/**
 * @brief 检查记录是否有效
 */
static EmbeddedFlash_record_status_e _is_kv_record(const KV_Record *record) {
    // 空指针检查
    if (record == NULL) {
        printf("_is_kv_record: NULL record pointer\n");
        return EFLASH_KV_UNUSED;
    }
    
    if (record->magic != KV_HEADER_MAGIC) {
        printf("_is_kv_record: Invalid magic=0x%02X, expected=0x%02X\n", record->magic, KV_HEADER_MAGIC);
        return EFLASH_KV_UNUSED;
    }

    if (record->value_length == 0 || record->value_length > KV_MAX_VALUE_SIZE) {
        printf("_is_kv_record: Invalid value_length=%d, key=%d\n", record->value_length, record->key);
        return EFLASH_KV_UNUSED;
    }

    // 检查CRC - 跳过status_table(16) + magic(1) + data_type(1)，从key开始校验key + value_length + value
    // 关键修复：使用固定长度KV_MAX_VALUE_SIZE而不是实际数据长度
    uint16_t calc_crc = crc16_x25_calculate((uint8_t*)&record->key, 2 + KV_MAX_VALUE_SIZE);
    if (calc_crc != record->crc) {
        //crc不匹配，记录无效
        printf("_is_kv_record: CRC mismatch for key=%d, calc_crc=0x%04X, stored_crc=0x%04X\n", record->key, calc_crc, record->crc);
        return EFLASH_KV_UNUSED;
    }
    uint8_t record_status = (uint8_t)_get_kv_status_from_table((uint8_t*)record->status_table);

    return record_status;
}

/**
 * @brief 检查扇区头信息是否有效
 * @param header 扇区头信息指针
 * @return true=有效, false=无效
 */
 static bool _is_sector_header(const sector_header_t *header)
{
    if (header->magic != SECTOR_HEADER_MAGIC_WORD) {
        return false;
    }
    return true;
}
/* 迭代 */
static EF_ErrCode _iteration(EF_ErrCode (*func)(KV_Record *record, uint32_t abs_addr))
{
    for(uint8_t sector_idx = 0; sector_idx < KV_SECTOR_COUNT; sector_idx++){
        //初始化扇区信息
        uint16_t record_count = 0;
        uint16_t free_space = KV_SECTOR_SIZE - sizeof(sector_header_t);
        
        //读取扇区头
        sector_header_t header = {0};
        if(flash_port_read(m_sector_desc_list[sector_idx].sector_addr, (uint8_t*)&header, sizeof(sector_header_t)) != EF_OK){
            printf("Failed to read sector header at addr=0x%08X\n", m_sector_desc_list[sector_idx].sector_addr);
            return EF_ERR_READ;
        }
        if(_is_sector_header(&header) == false){
            printf("Invalid sector header at addr=0x%08X\n", m_sector_desc_list[sector_idx].sector_addr);
            return EF_ERR_INVALID;
        }
        uint8_t sector_status = (uint8_t)_get_sector_status_from_table(header.status_table);
        uint8_t sector_role = (uint8_t)_get_sector_role_from_table(header.role_table);
        
        // 扫描当前扇区的所有记录
        uint32_t scan_addr = m_sector_desc_list[sector_idx].sector_addr + sizeof(sector_header_t);
        uint32_t sector_end_addr = m_sector_desc_list[sector_idx].sector_addr + KV_SECTOR_SIZE;
        
        while (scan_addr + sizeof(KV_Record) < sector_end_addr) {//使用<=还是<
            KV_Record record ={0};
            // 读取完整记录
            if (flash_port_read(scan_addr, (uint8_t*)&record, sizeof(KV_Record)) == EF_OK) {
                // 检查记录基本有效性（magic、CRC等）
                if (_is_kv_record(&record) != EFLASH_KV_UNUSED) {
                    if(func != NULL){
                        func(&record, scan_addr);
                    }
                    record_count++;
                } else {
                    printf("Invalid record at addr=0x%08X\n", scan_addr);
                    EFLASH_PRINT_HEX("record", (uint8_t*)&record, sizeof(KV_Record));
                    // 检查是否是全0xFF区域（空白区域）
                    uint8_t all_0xff = 1;
                    uint8_t *record_bytes = (uint8_t*)&record;
                    for (int k = 0; k < sizeof(KV_Record); k++) {
                        if (record_bytes[k] != 0xFF) {
                            all_0xff = 0;
                            break;//跳出for循环
                        }
                    }
                    if (all_0xff) {
                        // 遇到全0xFF区域，说明后面都是空白区域
                        break;
                    } /*else {
                        // 无效数据但不是全0xFF，标记为无效（写入全0）
                        memset(&record, 0, sizeof(KV_Record));
                        drv_flash_write(scan_addr, (uint8_t*)&record, sizeof(KV_Record));
                    }*///不清0
                }
            }else{
                printf("Failed to read record at addr=0x%08X\n", scan_addr);
            }
            //成功失败都继续推进，避免死循环
            scan_addr += sizeof(KV_Record);
        }

        // 更新扇区状态信息
        m_sector_desc_list[sector_idx].free_space = sector_end_addr - scan_addr;
        m_sector_desc_list[sector_idx].record_count = record_count;

        m_sector_desc_list[sector_idx].attr.status = sector_status;//这个状态会根据记录数和剩余空间更新
        m_sector_desc_list[sector_idx].attr.role = sector_role;//这个角色不会改变
        // 设置扇区状态
        if (m_sector_desc_list[sector_idx].free_space < sizeof(KV_Record)) {
            m_sector_desc_list[sector_idx].attr.status = EFLASH_SECTOR_STATUS_FULL;
        } else if (record_count > 0) {
            m_sector_desc_list[sector_idx].attr.status = EFLASH_SECTOR_STATUS_USING;
        } else {
            m_sector_desc_list[sector_idx].attr.status = EFLASH_SECTOR_STATUS_FREE;
        }
        //更新扇区头
        if(m_sector_desc_list[sector_idx].attr.status != sector_status){
            _write_sector_status(m_sector_desc_list[sector_idx].sector_addr, m_sector_desc_list[sector_idx].attr.status);
        }
    }
    return EF_OK;
}



/**
 * @brief 在kv_data_t列表中查找指定key的记录
 * @param key 要查找的键
 * @return 找到返回kv_data_t指针，未找到返回NULL
 */
static kv_data_t* _find_kv_data(uint8_t key) {
    if (mp_kv_list == NULL || m_kv_data_list_count == 0) {
        return NULL;
    }
    
    // 遍历kv_data_t列表查找匹配的key
    for (int i = 0; i < m_kv_data_list_count; i++) {
        if (mp_kv_list[i].key == key) {
            return &mp_kv_list[i];  // 返回找到的记录的指针
        }
    }
    
    return NULL;  // 未找到
}

static EF_ErrCode _load_kv_record_callback(KV_Record *record, uint32_t abs_addr)
{
    uint8_t record_status = (uint8_t)_get_kv_status_from_table((uint8_t*)record->status_table);
    if(record_status == EFLASH_KV_DELETED
    || record_status == EFLASH_KV_PRE_DELETE) {
        //无效记录
        printf("Invalid record status for key=0x%02X, status=%d, addr=0x%08X\n", record->key, record_status, abs_addr);
        return EF_ERR_INVALID;
    }

    //查找对应的kv_data_t
    kv_data_t *p_kv_data = _find_kv_data(record->key);
    if(p_kv_data == NULL){
        printf("No kv_data_t found for key=0x%02X, addr=0x%08X\n", record->key, abs_addr);
        return EF_ERR_INVALID;
    }
    
    p_kv_data->addr_abs = abs_addr;
    p_kv_data->data_type = record->data_type;
    p_kv_data->value_length = record->value_length;
    if(record->value_length > KV_MAX_VALUE_SIZE){
        printf("Value length %d for key=0x%02X exceeds max size %d, addr=0x%08X\n", 
               record->value_length, record->key, KV_MAX_VALUE_SIZE, abs_addr);
        return EF_ERR_SIZE_TOO_LONG;
    }
    memcpy(p_kv_data->value, record->value, record->value_length);
    p_kv_data->data_source = KV_DATA_SOURCE_FLASH_READ;
    return EF_OK;
}
/* 加载kv记录*/
EF_ErrCode _load_kv_record(void)
{
    printf("EmbeddedFlash: Loading all KV records...\n");
    return _iteration(_load_kv_record_callback);
}

/**
 * @brief 将源扇区的PRE_WRITE和WRITE数据搬运到目标扇区，搬运完成后将源扇区的数据标记为删除
 * @param source_sector_idx 源扇区索引
 * @param target_sector_idx 目标扇区索引
 * @return EF_ErrCode 错误码，EF_OK表示成功
 * @note 此函数会：
 *       1. 遍历源扇区的所有记录
 *       2. 查找状态为PRE_WRITE或WRITE的记录
 *       3. 将这些记录写入目标扇区
 *       4. 将源扇区中的原记录标记为DELETED
 */
static EF_ErrCode _migrate_sector_data(uint8_t source_sector_idx, uint8_t target_sector_idx)
{
    if (source_sector_idx >= KV_SECTOR_COUNT || target_sector_idx >= KV_SECTOR_COUNT) {
        printf("Invalid sector index: source=%d, target=%d\n", source_sector_idx, target_sector_idx);
        return EF_ERR_PARAM;
    }
    
    printf("Migrating data from source sector %d to target sector %d\n", source_sector_idx, target_sector_idx);
    
    // 计算源扇区的扫描地址范围
    uint32_t source_sector_start_addr = m_sector_desc_list[source_sector_idx].sector_addr + sizeof(sector_header_t);
    uint32_t source_sector_end_addr = m_sector_desc_list[source_sector_idx].sector_addr + KV_SECTOR_SIZE;
    
    // 检查目标扇区是否有足够空间（至少需要能写入一个记录的空间）
    if (m_sector_desc_list[target_sector_idx].free_space < sizeof(KV_Record)) {
        printf("Target sector %d has insufficient space: %d bytes\n", 
               target_sector_idx, m_sector_desc_list[target_sector_idx].free_space);
        return EF_ERR_NO_SPACE;
    }
    
    // 遍历源扇区的所有记录
    uint16_t migrated_count = 0;
    uint16_t deleted_count = 0;
    
    while (source_sector_start_addr + sizeof(KV_Record) < source_sector_end_addr) {
        KV_Record record = {0};
        
        // 读取记录
        if (flash_port_read(source_sector_start_addr, (uint8_t*)&record, sizeof(KV_Record)) != EF_OK) {
            printf("Failed to read record at address 0x%08X\n", source_sector_start_addr);
            source_sector_start_addr += sizeof(KV_Record);
            continue;
        }
        
        // 检查是否是有效的KV记录
        EmbeddedFlash_record_status_e record_status = _is_kv_record(&record);
        if (record_status == EFLASH_KV_UNUSED) {
            // 检查是否是全0xFF区域（空白区域）
            uint8_t all_0xff = 1;
            uint8_t *record_bytes = (uint8_t*)&record;
            for (int k = 0; k < sizeof(KV_Record); k++) {
                if (record_bytes[k] != 0xFF) {
                    all_0xff = 0;
                    break;
                }
            }
            if (all_0xff) {
                // 遇到全0xFF区域，说明后面都是空白区域，可以停止遍历
                break;
            }
            // 无效记录但不是全0xFF，跳过
            source_sector_start_addr += sizeof(KV_Record);
            continue;
        }
        
        // 只处理PRE_WRITE和WRITE状态的记录
        if (record_status == EFLASH_KV_PRE_WRITE || record_status == EFLASH_KV_WRITE) {
            printf("Found record to migrate: key=%d, status=%d, addr=0x%08X\n", 
                   record.key, record_status, source_sector_start_addr);
            
            // 检查目标扇区是否有足够空间
            if (m_sector_desc_list[target_sector_idx].free_space < sizeof(KV_Record)) {
                printf("Target sector %d is full, cannot migrate more records\n", target_sector_idx);
                return EF_ERR_NO_SPACE;
            }
            
            // 准备写入到目标扇区的记录（保持原状态）
            KV_Record new_record = record;
            // 重置状态表，准备写入PRE_WRITE状态
            memset(new_record.status_table, 0xFF, KV_STATUS_TABLE_SIZE);
            _set_kv_status_table(new_record.status_table, EFLASH_KV_PRE_WRITE);
            
            // 写入到目标扇区
            uint32_t new_record_addr = _write_kv_record_to_sector(target_sector_idx, &new_record);
            if (new_record_addr == 0) {
                printf("Failed to write record to target sector %d\n", target_sector_idx);
                return EF_ERR_WRITE;
            }
            
            // 将新记录标记为WRITE状态
            if (_write_kv_status(new_record_addr, EFLASH_KV_WRITE) != EF_OK) {
                printf("Failed to set WRITE status for migrated record at 0x%08X\n", new_record_addr);
                EFLASH_ASSERT(0);
                return EF_ERR_WRITE;
            }
            
            // 将源扇区中的原记录标记为删除
            if (_kv_delete_record(source_sector_start_addr) != EF_OK) {
                printf("Failed to delete original record at 0x%08X\n", source_sector_start_addr);
                EFLASH_ASSERT(0);
                return EF_ERR_WRITE;
            }
            
            printf("Record migrated: key=%d, from 0x%08X to 0x%08X\n", 
                   record.key, source_sector_start_addr, new_record_addr);
            migrated_count++;
            deleted_count++;
            printf("Original record at 0x%08X marked as DELETED\n", source_sector_start_addr);
        }
        
        // 继续扫描下一个记录
        source_sector_start_addr += sizeof(KV_Record);
    }
    printf("Migration completed: %d records migrated, %d records deleted\n", 
           migrated_count, deleted_count);
    
    return EF_OK;
}

/* 重建扇区信息 */ 
EF_ErrCode _rebuild_sector(void)
{
    /* 重建扇区的目的就是让整个持久化存储组件中只有一个gc扇区其余全是数据区，这个过程可能涉及到数据的搬运 */
    

    /*1、找到一个剩余空间最多的扇区*/
    uint16_t max_free_space = 0;
    uint8_t start_sector_idx = 0;
    for(uint8_t i = 0; i < KV_SECTOR_COUNT; i++){
        if(m_sector_desc_list[i].free_space > max_free_space){
            max_free_space = m_sector_desc_list[i].free_space;
            start_sector_idx = i;
        }
    }
    /*2、将下一个扇区的数据挪到剩余空间最多的扇区，重复KV_SECTOR_COUNT次 */
    for(uint8_t i = 0; i < KV_SECTOR_COUNT; i++){
        uint8_t next_sector_idx = (start_sector_idx + i + 1) % KV_SECTOR_COUNT;
        //开始搬运
        // 将源扇区(next_sector_idx)的数据搬运到目标扇区(start_sector_idx)
        if (_migrate_sector_data(next_sector_idx, start_sector_idx) != EF_OK) {
            printf("Failed to migrate data from source sector %d to target sector %d\n", 
                   next_sector_idx, start_sector_idx);
            return EF_ERR;
        }
        start_sector_idx = next_sector_idx;
        //搬运完擦除扇区
        EF_ErrCode ret = _erase_sector(next_sector_idx);
        //写入新的头信息
        if(i == KV_SECTOR_COUNT-1) {
            //GC区
            ret |= _write_sector_status(m_sector_desc_list[next_sector_idx].sector_addr, EFLASH_SECTOR_STATUS_FREE);
            ret |= _write_sector_role(m_sector_desc_list[next_sector_idx].sector_addr, EFLASH_SECTOR_ROLE_GC);
        }else{
            //数据区
            ret |= _write_sector_status(m_sector_desc_list[next_sector_idx].sector_addr, EFLASH_SECTOR_STATUS_USING);
            ret |= _write_sector_role(m_sector_desc_list[next_sector_idx].sector_addr, EFLASH_SECTOR_ROLE_DATA);
        }
        if(ret != EF_OK){return ret;}
    }
    return EF_OK;
}

//查找指定扇区角色
static int _find_sector_role(uint8_t role)
{
    printf("Searching for sector with role %d\n", role);
    for(uint8_t i = 0; i < KV_SECTOR_COUNT; i++){
        printf("Sector %d: role=%d\n", i, m_sector_desc_list[i].attr.role);
        if(m_sector_desc_list[i].attr.role == role){
            printf("Found sector %d with role %d\n", i, role);
            return i;
        }
    }
    printf("No sector found with role %d\n", role);
    return -1;
}
/* GC操作 */
EF_ErrCode ef_embedded_flash_gc(void)
{
    printf("GC operation started\n");
    embedded_flash_att_status_t status = embedded_flash_get_attr_status();
    if(status != EFLASH_STATUS_NORMAL){
        printf("GC operation not allowed in status %d\n", status);
        return EF_ERR_NOT_INIT;
    }
    /* 将gc区的数据搬运到第一个数据区 */
   
    //1、查找gc区位置和第一个数据区位置
    int gc_sector_idx = _find_sector_role(EFLASH_SECTOR_ROLE_GC);
    if(gc_sector_idx < 0){
        printf("GC sector not found\n");
        return EF_ERR_NOT_FOUND;
    }
    int data_sector_idx = (gc_sector_idx + 1) % KV_SECTOR_COUNT;

    //2、进入gc状态
    if(_write_sector_role(m_sector_desc_list[gc_sector_idx].sector_addr, EFLASH_SECTOR_ROLE_GC_TEMP) != EF_OK){
        return EF_ERR;
    }
    m_sector_desc_list[gc_sector_idx].attr.role = EFLASH_SECTOR_ROLE_GC_TEMP;
    
    if(_write_sector_role(m_sector_desc_list[data_sector_idx].sector_addr, EFLASH_SECTOR_ROLE_DATA_GCING) != EF_OK){
        printf("Failed to set DATA_GCING role for sector %d\n", data_sector_idx);
        return EF_ERR;
    }
    m_sector_desc_list[data_sector_idx].attr.role = EFLASH_SECTOR_ROLE_DATA_GCING;
    
    //3、将第一个数据区的数据搬运到gc区
    if(_migrate_sector_data(data_sector_idx, gc_sector_idx) != EF_OK){
        printf("Failed to migrate data from sector %d to sector %d\n", data_sector_idx, gc_sector_idx);
        return EF_ERR;
    }
    //4、擦除第一个数据区
    if(_erase_sector(data_sector_idx) != EF_OK){
        printf("Failed to erase sector %d\n", data_sector_idx);
        return EF_ERR;
    }
    //5、写入新的头信息

    //第一个数据区 -> gc区
    if(_write_sector_status(m_sector_desc_list[data_sector_idx].sector_addr, EFLASH_SECTOR_STATUS_FREE) != EF_OK){
        printf("Failed to set FREE status for sector %d\n", data_sector_idx);
        return EF_ERR;
    }
    m_sector_desc_list[data_sector_idx].attr.status = EFLASH_SECTOR_STATUS_FREE;

    if(_write_sector_role(m_sector_desc_list[data_sector_idx].sector_addr, EFLASH_SECTOR_ROLE_GC) != EF_OK){
        printf("Failed to set GC role for sector %d\n", data_sector_idx);
        return EF_ERR;
    }
    m_sector_desc_list[data_sector_idx].attr.role = EFLASH_SECTOR_ROLE_GC;

    //gc区 -> 数据区
    if(_write_sector_status(m_sector_desc_list[gc_sector_idx].sector_addr, EFLASH_SECTOR_STATUS_USING) != EF_OK){
        printf("Failed to set USING status for sector %d\n", gc_sector_idx);
        return EF_ERR;
    }
    m_sector_desc_list[gc_sector_idx].attr.status = EFLASH_SECTOR_STATUS_USING;
    if(_write_sector_role(m_sector_desc_list[gc_sector_idx].sector_addr, EFLASH_SECTOR_ROLE_DATA) != EF_OK){
        printf("Failed to set DATA role for sector %d\n", gc_sector_idx);
        return EF_ERR;
    }
    m_sector_desc_list[gc_sector_idx].attr.role = EFLASH_SECTOR_ROLE_DATA;
    //6、更新内存中的扇区信息
    m_sector_desc_list[data_sector_idx].free_space = KV_SECTOR_SIZE - sizeof(sector_header_t);
    m_sector_desc_list[data_sector_idx].record_count = 0;
    return EF_OK;
}


EF_ErrCode embedded_flash_init(const kv_data_t *defaults, uint8_t default_count) {
    printf("EmbeddedFlash: Starting initialization...\n");
    printf("EmbeddedFlash: KV_SECTOR_START_ADDR=0x%08X\n", KV_SECTOR_START_ADDR);
    printf("EmbeddedFlash: KV_SECTOR_SIZE=%d\n", KV_SECTOR_SIZE);
    printf("EmbeddedFlash: KV_SECTOR_COUNT=%d\n", KV_SECTOR_COUNT);
    
    // 调试：打印扇区头和KV记录结构体大小
    printf("EmbeddedFlash: sizeof(sector_header_t)=%d bytes\n", sizeof(sector_header_t));
    printf("EmbeddedFlash: SECTOR_STATUS_TABLE_SIZE=%d bytes\n", SECTOR_STATUS_TABLE_SIZE);
    printf("EmbeddedFlash: SECTOR_ROLE_TABLE_SIZE=%d bytes\n", SECTOR_ROLE_TABLE_SIZE);
    printf("EmbeddedFlash: sizeof(KV_Record)=%d bytes\n", sizeof(KV_Record));
    printf("EmbeddedFlash: KV_STATUS_TABLE_SIZE=%d bytes\n", KV_STATUS_TABLE_SIZE);
    printf("EmbeddedFlash: KV_MAGIC_OFFSET=%d bytes\n", KV_MAGIC_OFFSET);
    printf("EmbeddedFlash: EFLASH_WRITE_GRAN=%d bits\n", EFLASH_WRITE_GRAN);
    for (int i = 0; i < KV_SECTOR_COUNT; i++) {
        printf("EmbeddedFlash: Sector[%d] addr=0x%08X, status=%d, role=%d\n", 
               i, m_sector_desc_list[i].sector_addr, 
               m_sector_desc_list[i].attr.status, m_sector_desc_list[i].attr.role);
               printf("EmbeddedFlash: Sector[%d] free_space=%d\n", i, m_sector_desc_list[i].free_space);
               printf("EmbeddedFlash: Sector[%d] record_count=%d\n", i, m_sector_desc_list[i].record_count);
    }

    if (defaults == NULL || default_count == 0 || default_count > 255) {
        return EF_ERR_PARAM;
    }
    mp_kv_list = (kv_data_t *)defaults;  // 转换为非const指针
    m_kv_data_list_count = default_count;
    
	EF_ErrCode ret = EF_OK;
    embedded_flash_att_status_t status = embedded_flash_get_attr_status();
    switch(status){
        case EFLASH_STATUS_NORMAL:
            // 确保扇区属性已正确加载
            for (int i = 0; i < KV_SECTOR_COUNT; i++) {
                sector_header_t header;
                if (_sector_header_read(i, &header) == 0) {
                    printf("Sector %d: status=%d, role=%d\n", i, 
                           m_sector_desc_list[i].attr.status, 
                           m_sector_desc_list[i].attr.role);
                } else {
                    printf("Failed to read header for sector %d\n", i);
                }
            }
            ret = _load_kv_record();
            if(ret != EF_OK){
                printf("Failed to load KV records\n");
                return ret;
            }
            ret = _init_all_kv_record();
            if(ret != EF_OK){
                printf("Failed to initialize all KV records\n");
                return ret;
            }
            break;
        case EFLASH_STATUS_FIRST_POWER_ON:
            ret = _init_all_sector_attr();
            if(ret != EF_OK){
                printf("Failed to initialize all sector attributes\n");
                return ret;
            }
            ret = _init_all_kv_record();
            if(ret != EF_OK){
                printf("Failed to initialize all KV records\n");
                return ret;
            }
            ret = _load_kv_record();
            if(ret != EF_OK){
                printf("Failed to load KV records\n");
                return ret;
            }
            break;
       default:
            ret = _load_kv_record();
            if(ret != EF_OK){
                printf("Failed to load KV records\n");
                return ret;
            }
            ret = _rebuild_sector();
            if(ret != EF_OK){
                printf("Failed to rebuild sector\n");
                return ret;
            }
            ret = _load_kv_record();
            if(ret != EF_OK){
                printf("Failed to load KV records\n");
                return ret;
            }
            ret = _init_all_kv_record();
            if(ret != EF_OK){
                printf("Failed to initialize all KV records\n");
                return ret;
            }   
            break;
    }
    return ret;
}

// ==================== 类型化设置函数 ====================
static EF_ErrCode embedded_flash_set(uint8_t key, const uint8_t *value, uint8_t length, uint8_t data_type);
EF_ErrCode embedded_flash_set_bool(uint8_t key, bool value) {
    uint8_t temp = value ? 1 : 0;
    return embedded_flash_set(key, (uint8_t*)&temp, sizeof(uint8_t), EFLASH_FORMAT_BOOL);
}

EF_ErrCode embedded_flash_set_uint8(uint8_t key, uint8_t value) {
    return embedded_flash_set(key, &value, sizeof(uint8_t), EFLASH_FORMAT_UINT8);
}

EF_ErrCode embedded_flash_set_int8(uint8_t key, int8_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(int8_t), EFLASH_FORMAT_INT8);
}

EF_ErrCode embedded_flash_set_uint16(uint8_t key, uint16_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(uint16_t), EFLASH_FORMAT_UINT16);
}

EF_ErrCode embedded_flash_set_int16(uint8_t key, int16_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(int16_t), EFLASH_FORMAT_INT16);
}

EF_ErrCode embedded_flash_set_uint32(uint8_t key, uint32_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(uint32_t), EFLASH_FORMAT_UINT32);
}

EF_ErrCode embedded_flash_set_int32(uint8_t key, int32_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(int32_t), EFLASH_FORMAT_INT32);
}

EF_ErrCode embedded_flash_set_uint64(uint8_t key, uint64_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(uint64_t), EFLASH_FORMAT_UINT64);
}

EF_ErrCode embedded_flash_set_int64(uint8_t key, int64_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(int64_t), EFLASH_FORMAT_INT64);
}

EF_ErrCode embedded_flash_set_float(uint8_t key, float value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(float), EFLASH_FORMAT_FLOAT);
}

EF_ErrCode embedded_flash_set_string(uint8_t key, const char *value) {
    if (value == NULL) {
        printf("Invalid string value for key=%d\n", key);
        return EF_ERR_PARAM;
    }
    uint8_t length = strlen(value);
    if (length+1 > KV_MAX_VALUE_SIZE || length == 0) {
        printf("String too long or empty for key=%d, length=%d\n", key, length);
        return EF_ERR_PARAM;
    }
    // 存储字符串时包含结束符，所以长度+1
    return embedded_flash_set(key, (uint8_t*)value, length/*+1*/, EFLASH_FORMAT_STRING);
}

EF_ErrCode embedded_flash_set_hex(uint8_t key, const uint8_t *value, uint8_t length) {
    if (value == NULL || length == 0 || length > KV_MAX_VALUE_SIZE) {
        printf("Invalid hex data for key=%d, length=%d\n", key, length);
        return EF_ERR_PARAM;
    }
    return embedded_flash_set(key, value, length, EFLASH_FORMAT_HEX);
}

// ==================== 内部通用设置函数 ====================
/**
 * @brief 删除KV记录（分阶段提交）
 * @param addr KV记录地址
 * @return FLASH_NO_ERR=成功, 其他=失败
 * 
 * @note 删除流程：
 *       1. 标记为PRE_DELETE（写入addr+8）
 *       2. 标记为DELETED（写入addr+12）
 */
static EF_ErrCode _kv_delete_record(uint32_t addr)
{
    EF_ErrCode result = EF_OK;
    uint8_t status_table[KV_STATUS_TABLE_SIZE];
    
    /* 阶段1：标记为"预删除" */
    result = _write_kv_status(addr, EFLASH_KV_PRE_DELETE);
    if (result != EF_OK) {
        printf("Failed to mark as PRE_DELETE for addr=0x%x, result=%d\n", addr, result);
        return result;
    }
    
    /* 阶段2：标记为"已删除" */
    result = _write_kv_status(addr, EFLASH_KV_DELETED);
    if (result != EF_OK) {
        printf("Failed to mark as DELETED for addr=0x%x, result=%d\n", addr, result);
        return result;
    }
    
    return EF_OK;
}

/**
 * @brief 设置键值对（内部使用）
 * @param key 键
 * @param value 值指针
 * @param length 值长度
 * @param data_type 数据类型
 * @return 0=成功, -1=失败
 */
static EF_ErrCode embedded_flash_set(uint8_t key, const uint8_t *value, uint8_t length, uint8_t data_type) {
    printf("[SET] key=%d, type=%d, len=%d\n", key, data_type, length);
    
    if (value == NULL) {
		printf("Invalid input for set operation: key=%d, len:%d\n", key,length);
        return EF_ERR_PARAM;
    }
    
    // 验证长度与数据类型匹配
    uint8_t expected_length = embedded_flash_get_type_size(data_type);
    if (expected_length > 0) {
        // 固定长度类型
        if (length != expected_length) {
            printf("Invalid length for key=%d: expected=%d, got=%d\n", key, expected_length, length);
            EFLASH_ASSERT(0);
            return EF_ERR_PARAM;
        }
    } else {
        // 可变长度类型（STRING, HEX）
        if (length == 0 || length > KV_MAX_VALUE_SIZE) {
            printf("Invalid length for key=%d: length=%d (must be 1-%d for variable length types)\n", key, length, KV_MAX_VALUE_SIZE);
            EFLASH_ASSERT(0);
            return EF_ERR_PARAM;
        }
    }
    
    uint8_t get_value[KV_MAX_VALUE_SIZE] = {0};
    uint8_t get_length = 0;
    uint8_t get_data_type = 0;
    if(embedded_flash_get(key, get_value, &get_length, &get_data_type) != EF_OK){
        printf("Failed to get data for key=%d\n", key);
        return EF_ERR_READ;
    }
    if(get_length == length && memcmp(get_value, value, length) == 0 && get_data_type == data_type){
        //重复数据，直接返回
        printf("Duplicate data for key=%d, length=%d, data_type=%d\n", key, length, data_type);
        return EF_OK;
    }
    /*工作流程
        1、新 PRE_WRITE → 新 WRITE → 旧 PRE_DELETE → 旧 DELETED
        2、任意时刻至少有一条 WRITE 生效（数据存储顺序为先旧后新）
    */
    KV_Record record = {0};
    // 构造记录
    memset(&record, 0xFF, sizeof(KV_Record));  // 先全部初始化为0xFF
	_set_kv_status_table(record.status_table, EFLASH_KV_PRE_WRITE);
    record.magic = KV_HEADER_MAGIC;
    record.data_type = data_type;
    record.key = key;
    record.value_length = length;  // value_length就是值的长度
    
    // 关键修复：先清零整个value数组，然后复制数据
    memset(record.value, 0, KV_MAX_VALUE_SIZE);
    memcpy(record.value, value, length);

    // 计算CRC - 跳过status_table(16) + magic(1) + data_type(1)，从key开始校验key + value_length + value
    // 关键修复：使用固定长度KV_MAX_VALUE_SIZE而不是实际数据长度
    record.crc = crc16_x25_calculate((uint8_t*)&record.key, 2 + KV_MAX_VALUE_SIZE);
    
    // 查找对应的kv_data_t
    kv_data_t *p_kv_data = _find_kv_data(key);
    if(p_kv_data == NULL){
        printf("Key not found: %d\n", key);
        return EF_ERR_PARAM;
    }
    
    // 写入记录（使用状态表机制实现分阶段提交）
    uint32_t new_write_abs_addr = _write_kv_record(&record);
    if(new_write_abs_addr == 0){
		printf("_write_kv_record fail\r\n");
        return EF_ERR_WRITE;
    }

    // 使用类型安全函数将新记录标记为WRITE状态
    if(_write_kv_status(new_write_abs_addr, EFLASH_KV_WRITE) != EF_OK){
		printf("write EFLASH_KV_WRITE fail\r\n");
        EFLASH_ASSERT(0);
        return EF_ERR_WRITE;
    }
    //如果不相等，那么内部有大于2条数据记录，需要将旧记录设置为删除
    if(p_kv_data->addr_abs != 0 && p_kv_data->addr_abs != new_write_abs_addr){
        //使用状态表机制删除旧记录
        if(_kv_delete_record(p_kv_data->addr_abs) != EF_OK){
            printf("Failed to delete old record at 0x%08X\n", p_kv_data->addr_abs);
            return EF_ERR_WRITE;
        }
    }
    //更新最新记录的信息到ram
    p_kv_data->addr_abs = new_write_abs_addr;
    p_kv_data->value_length = record.value_length;
    p_kv_data->data_type = record.data_type;
    p_kv_data->data_source = KV_DATA_SOURCE_UPDATE_WRITE;//更新数据到掉电储存区
	memcpy(p_kv_data->value, value, length);

    printf("Set operation successful for key=%d,addr:0x%x\n", key,new_write_abs_addr);

    
    return EF_OK;
}


/**
 * @brief 查找指定键的最新记录
 * @param key 键
 * @param record 记录
 * @param addr 记录地址 
 * @return 0=成功, -1=失败
 */
static int _find_latest_record(kv_data_t *p_kv_data, KV_Record *record, uint32_t *addr) {
    uint32_t read_addr = p_kv_data->addr_abs;
    if(flash_port_read(read_addr, (uint8_t*)record, sizeof(KV_Record)) != EF_OK){
        printf("Failed to read record at stored address=0x%x for key=%d\n", read_addr, p_kv_data->key);
        return -1;
    }
    EmbeddedFlash_record_status_e record_status = _is_kv_record(record);
    if (record_status == EFLASH_KV_UNUSED){
        printf("Invalid record at stored address=0x%x for key=%d\n", read_addr, p_kv_data->key);
        // _foreach_sector_record()
        // printf("No valid record found for key=%d after scanning all sectors\n", p_kv_data->key);
        return -1;
    }
    *addr = read_addr;
    if(memcmp(record->value, p_kv_data->value, p_kv_data->value_length) != 0){
        goto data_diff_proce;
    }
    if(record->data_type != p_kv_data->data_type){
        goto data_diff_proce;
    }

    
    //有没有可能长度不一样？？？？
    //todo...

    //数据没有差异
    return 0;

data_diff_proce:
    if(record->value_length > KV_MAX_VALUE_SIZE){
        EFLASH_ASSERT(0);
        return -1;
    }
    p_kv_data->value_length = record->value_length;  // value_length就是值的长度
	p_kv_data->data_type = record->data_type;
    p_kv_data->data_source = KV_DATA_SOURCE_FLASH_OVERRIDE;//从掉电储存区更新数据
    memcpy(p_kv_data->value, record->value, record->value_length);
    return 0;
}
/**
 * @brief 获取键值对
 * @param key 键
 * @param value 值缓冲区
 * @param length 返回值长度
 * @param data_type 返回数据类型
 * @return 0=成功, -1=失败
 */
EF_ErrCode embedded_flash_get(uint8_t key, uint8_t *value, uint8_t *length, uint8_t *data_type) {
    if (value == NULL || length == NULL || data_type == NULL) {
        printf("Invalid input for get operation: key=%d\n", key);
        return EF_ERR_PARAM;
    }
    /* 工作流程
        1、根据地址获取flash数据记录，如果没找到就遍历整个扇区看有没有数据记录
        2、对比读取出来的数据记录并跟当前数据对比，如果有差异就断言失败
        3、返回数据
    */
    
    kv_data_t *p_kv_data = _find_kv_data(key);
    if(p_kv_data == NULL){
        printf("Key not found: %d\n", key);    
        return EF_ERR;
    }
    // printf("Getting data for key=%d, stored address=0x%x\n", key, p_kv_data->addr_abs);
    uint32_t addr = 0;
    KV_Record record={0};
    if(_find_latest_record(p_kv_data, &record, &addr) != 0){
        printf("Failed to find latest record for key=%d\n", key);
        return EF_ERR;
    }
    // 复制数据
    *length = record.value_length;  // value_length就是值的长度
    *data_type = record.data_type;
    
    // 检查缓冲区大小，防止数组越界
    if (*length+1 > KV_MAX_VALUE_SIZE || *length == 0) {
        printf("CRITICAL: value_length=%d exceeds KV_MAX_VALUE_SIZE=%d for key=%d\n", *length, KV_MAX_VALUE_SIZE, key);
        return EF_ERR_PARAM;
    }
    memcpy(value, record.value, *length+1);
    printf("Get operation successful for key=%d, len=%d, addr=0x%x\n", key, *length, p_kv_data->addr_abs);
    return EF_OK;
}


/**
 * @brief 删除键值对
 * @param key 键
 * @return 0=成功, -1=失败
 */
EF_ErrCode embedded_flash_delete(uint8_t key) 
{
    /* 工作流程
        1、mp_kv_list中找到对应键值对的记录
        2、将记录状态设置为删除
        3、写入成功后将数据设置为删除，并更新addr_abs_offset，方便读写
        4、写入失败返回错误码
    */
    kv_data_t *p_kv_data = _find_kv_data(key);
    if(p_kv_data == NULL){
        return EF_ERR;
    }
    
    // 使用状态表机制删除记录
    if(_kv_delete_record(p_kv_data->addr_abs) != EF_OK){
        EFLASH_ASSERT(0);
        return EF_ERR;
    }
    
    p_kv_data->addr_abs = 0;
    p_kv_data->data_source = KV_DATA_SOURCE_DEFAULT;
    return EF_OK;
}


