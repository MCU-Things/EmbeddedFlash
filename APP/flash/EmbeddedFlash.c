// EmbeddedFlash.c - 持久化存储组件实现
/*
初始化与自检:        扫描扇区、重建索引、首启格式化、异常状态恢复。
原子性与断电安全:    预写→提交状态机、双写/日志、幂等恢复流程。
磨损均衡与循环写:    扇区角色轮转、写放大控制、写入配额/节流。
垃圾回收（GC）:      增量搬移、扇区角色切换、可中断与恢复、进度持久化。
*/

#include "EmbeddedFlash.h"
#include "crc16_x25.h"
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

// 内部函数声明
//状态表操作函数
static size_t _set_status(uint8_t status_table[], size_t status_num, size_t status_index);
static size_t _get_status(uint8_t status_table[], size_t status_num);
static FlashErrCode _write_status(uint32_t addr, uint8_t status_table[], size_t status_num, size_t status_index);
static size_t _read_status(uint32_t addr, uint8_t status_table[], size_t total_num);

//初始化有关函数
static int _startup_rebuild_sector(void);
static void _startup_restore_sector_attr(void);
static int _refresh_sector_data(int sector_idx);
static int _startup_restore_kv_data(void);
static int _startup_init_missing_defaults(void);

static int _find_sector_role(uint8_t role);
static int _find_writable_data_sector(uint16_t required_space);
static int _erase_sector(uint8_t sector_idx);

//kv数据有关函数
static kv_data_t* _find_kv_data(uint8_t key);
static bool _is_kv_record(const KV_Record *record);
static int _find_latest_record(kv_data_t *p_kv_data, KV_Record *record, uint32_t *addr);
static int _write_record(KV_Record *p, uint32_t *p_addr_abs);
static int _write_record_to_sector(uint8_t sector_idx, KV_Record *p, uint32_t *p_addr_abs);

//头信息有关函数
static bool _is_sector_header(const sector_header_t *header);
static int _sector_header_read(uint8_t sector_idx, sector_header_t *header);
static int _sector_header_write(uint8_t sector_idx, uint8_t status, uint8_t role);

//gc有关函数
// static int _gc_sector_is_empty(uint8_t sector_idx);
static int embedded_flash_gc(void);
static int _execute_gc_operation(int gc_temp_sector_idx, int data_sector_idx, uint32_t gc_temp_empty_addr_abs, uint32_t data_resume_addr_abs);

// 状态查询接口
static embedded_flash_status_t embedded_flash_get_status(void);

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
    memset(status_table, 0xFF, STATUS_TABLE_SIZE(status_num));
    
    if (status_index > 0) {
        /* 对于32位写入粒度，每个状态占用4字节 */
        byte_index = (status_index - 1) * (EFLASH_WRITE_GRAN / 8);
        status_table[byte_index] = 0x00;  /* 将对应位置设为0x00 */
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
    size_t i = 0, status_num_bak = --status_num;
    
    while (status_num--) {
        /* 从后往前查找第一个0x00的位置 */
        if (status_table[status_num * EFLASH_WRITE_GRAN / 8] == 0x00) {
            break;
        }
        i++;
    }
    
    return status_num_bak - i;
}

/**
 * @brief 写入状态到Flash
 * @param addr 写入地址
 * @param status_table 状态表缓冲区
 * @param status_num 状态总数
 * @param status_index 目标状态索引
 * @return 错误码
 */
static FlashErrCode _write_status(uint32_t addr, uint8_t status_table[], size_t status_num, size_t status_index)
{
    FlashErrCode result = FLASH_NO_ERR;
    size_t byte_index;
    
    /* 设置状态 */
    byte_index = _set_status(status_table, status_num, status_index);
    
    /* status0（全FF）无需写入Flash */
    if (byte_index == ~0UL) {
        return FLASH_NO_ERR;
    }
    
    /* 写入4字节（32位）到对应偏移 */
    result = flash_port_write(addr + byte_index, (uint32_t *)&status_table[byte_index], EFLASH_WRITE_GRAN / 8);
    
    return result;
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
    flash_port_read(addr, (uint32_t *)status_table, STATUS_TABLE_SIZE(total_num));
    return _get_status(status_table, total_num);
}

/*==============================================================================
 * KV记录状态表辅助函数（用于实现分阶段提交）
 *============================================================================*/

/**
 * @brief 设置KV记录状态到状态表（RAM）
 * @param status_table 状态表缓冲区
 * @param status_index 状态索引（0=UNUSED, 1=PRE_WRITE, 2=WRITE, 3=PRE_DELETE, 4=DELETED）
 * @return 需要写入Flash的字节索引（~0UL表示无需写入）
 */
static size_t _kv_set_status(uint8_t status_table[], size_t status_index)
{
    return _set_status(status_table, KV_STATUS_NUM, status_index);
}

/**
 * @brief 从状态表获取KV记录当前状态
 * @param status_table 状态表缓冲区
 * @return 当前状态索引
 */
static size_t _kv_get_status(uint8_t status_table[])
{
    return _get_status(status_table, KV_STATUS_NUM);
}

/**
 * @brief 写入KV记录状态到Flash
 * @param addr KV记录地址
 * @param status_table 状态表缓冲区
 * @param status_index 目标状态索引
 * @return FLASH_NO_ERR=成功, 其他=失败
 */
static FlashErrCode _kv_write_status(uint32_t addr, uint8_t status_table[], size_t status_index)
{
    return _write_status(addr, status_table, KV_STATUS_NUM, status_index);
}

/**
 * @brief 从Flash读取KV记录状态
 * @param addr KV记录地址
 * @param status_table 状态表缓冲区
 * @return 当前状态索引
 */
static size_t _kv_read_status(uint32_t addr, uint8_t status_table[])
{
    return _read_status(addr, status_table, KV_STATUS_NUM);
}



/**
 * @brief 删除KV记录（分阶段提交）
 * @param addr KV记录地址
 * @return FLASH_NO_ERR=成功, 其他=失败
 * 
 * @note 删除流程：
 *       1. 标记为PRE_DELETE（写入addr+8）
 *       2. 标记为DELETED（写入addr+12）
 */
static FlashErrCode _kv_delete_record(uint32_t addr)
{
    FlashErrCode result = FLASH_NO_ERR;
    uint8_t status_table[KV_STATUS_TABLE_SIZE];
    
    /* 阶段1：标记为"预删除" */
    result = _kv_write_status(addr, status_table, EFLASH_KV_PRE_DELETE);
    if (result != FLASH_NO_ERR) {
        return result;
    }
    
    /* 阶段2：标记为"已删除" */
    result = _kv_write_status(addr, status_table, EFLASH_KV_DELETED);
    
    return result;
}

/**
 * @brief 读取KV记录并解析状态
 * @param addr KV记录地址
 * @param kv_record 输出：KV记录数据
 * @param status 输出：记录状态
 * @return FLASH_NO_ERR=成功, 其他=失败
 */
static FlashErrCode _kv_read_record(uint32_t addr, KV_Record *kv_record, EmbeddedFlash_record_status_e *status)
{
    FlashErrCode result;
    uint8_t status_table[KV_STATUS_TABLE_SIZE];
    
    /* 读取完整记录 */
    result = flash_port_read(addr, (uint32_t *)kv_record, sizeof(KV_Record));
    if (result != FLASH_NO_ERR) {
        return result;
    }
    
    /* 解析状态 */
    *status = (EmbeddedFlash_record_status_e)_kv_get_status(kv_record->status_table);
    
    return FLASH_NO_ERR;
}

/**
 * @brief 恢复异常状态的KV记录（断电恢复）
 * @param addr KV记录地址
 * @param kv_record KV记录数据
 * @param status 当前状态
 * @return FLASH_NO_ERR=成功, 其他=失败
 * 
 * @note 恢复逻辑：
 *       - PRE_WRITE：写入未完成，可能数据不完整 → 标记为DELETED
 *       - PRE_DELETE：删除未完成 → 继续标记为DELETED
 *       - WRITE/DELETED/UNUSED：稳定状态，无需处理
 */
static FlashErrCode _kv_recovery_record(uint32_t addr, const KV_Record *kv_record, EmbeddedFlash_record_status_e status)
{
    FlashErrCode result = FLASH_NO_ERR;
    uint8_t status_table[KV_STATUS_TABLE_SIZE];
    
    if (status == EFLASH_KV_PRE_WRITE) {
        /* 预写入状态：数据可能不完整，标记为已删除 */
        printf("EmbeddedFlash: Recovery KV record at 0x%08X from PRE_WRITE to DELETED\n", addr);
        result = _kv_write_status(addr, status_table, EFLASH_KV_DELETED);
    } 
    else if (status == EFLASH_KV_PRE_DELETE) {
        /* 预删除状态：完成删除操作 */
        printf("EmbeddedFlash: Recovery KV record at 0x%08X from PRE_DELETE to DELETED\n", addr);
        result = _kv_write_status(addr, status_table, EFLASH_KV_DELETED);
    }
    /* WRITE/DELETED/UNUSED状态无需恢复 */
    
    return result;
}
/**
 * @brief 初始化持久化存储组件
 * @note 1. 初始化时，会扫描所有扇区，并重建扇区信息
 * @param defaults 默认键值对配置表
 * @param default_count 默认键值对数量
 * @return 0=成功, -1=失败
 */
int embedded_flash_init(const kv_data_t *defaults, uint8_t default_count) {
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
    }

restart_init://重新开始

    if (defaults == NULL || default_count == 0 || default_count > 255) {
        return -1;
    }
    // 移除错误的全局擦除逻辑 - 这会导致数据丢失
    // for(uint8_t g =0;g<KV_SECTOR_COUNT;g++){
    //     _erase_sector(g);
    // }
    mp_kv_list = (kv_data_t *)defaults;  // 转换为非const指针
    m_kv_data_list_count = default_count;
    
    /*工作流程
        1、扫描并重建扇区信息,检查没有扇区信息，确认是否是第一次初始化，如果是第一次初始化，则初始化扇区信息
        2、检查并处理默认值和初始化状态
    */
    //1、扫描并重建扇区信息
    embedded_flash_status_t status = embedded_flash_get_status();
    printf("EmbeddedFlash: Current status=%d\n", status);
    switch(status){
        case EFLASH_STATUS_NORMAL:{
            //正常状态
            goto normal_kv_restore_handle;
        }
        case EFLASH_STATUS_FIRST_POWER_ON:{
            //首次上电
            // _startup_rebuild_sector会擦除并写入扇区头，同时更新内存中的扇区信息
            if(_startup_rebuild_sector()!= 0){
                    return  -1;
            }
            // ✅ 已删除重复的_sector_header_write调用
            // _startup_rebuild_sector已经完成了扇区头的写入
            // 只需要设置内存中的扇区信息即可（_sector_header_write内部已更新）
            
            goto first_power_on_handle;
        }
        //
        case EFLASH_STATUS_EMPTY_SECTOR_ERROR:{
            //空白扇区异常
            goto error_handle;//全部擦除，重新处理
        }
        case EFLASH_STATUS_NEW_SECTOR_ADDED:{
            // 非第一次初始化,新增扇区 
            /* 理论上新增扇区应该继续保持数据的连续性，但是这里不处理，直接全部扇区擦掉，重新建立扇区信息，数据全部恢复成默认值 */
            //直接擦掉，后续逻辑会好做一点
            goto error_handle;//全部擦除，重新处理
        }
        //
        case EFLASH_STATUS_MULTI_ROLE_ERROR:{
            goto error_handle;//全部擦除，重新处理
        }
        case EFLASH_STATUS_UNKNOWN_ERROR:{
            goto error_handle;//全部擦除，重新处理
        }
        //GC中断
        case EFLASH_STATUS_GC_PREPARE:
        case EFLASH_STATUS_GC_MIGRATING:
        case EFLASH_STATUS_GC_AFTER_MIGRATE:
        case EFLASH_STATUS_GC_AFTER_MIGRATE_WITH_EMPTY:{
            
            goto gc_inter_handle;
        }
        default:
            goto error_handle;
    }

   
//gc中断具体操作
gc_inter_handle:
    //获取各扇区角色
    _startup_restore_sector_attr();
    embedded_flash_gc();

//正常运行，恢复kv记录
normal_kv_restore_handle:
   //获取各扇区角色
   _startup_restore_sector_attr();
   //获取各扇区、记录、剩余空间，并将数据赋值到m_sector_desc_list和m_kv_list
   _startup_restore_kv_data();

//首次上电   
first_power_on_handle: 
    //初始化缺失的默认值
    return _startup_init_missing_defaults();

//异常处理
error_handle:
    if(_startup_rebuild_sector()!= 0){
			return  -1;
	}
    goto restart_init;
}

// ==================== 类型化设置函数 ====================
static int embedded_flash_set(uint8_t key, const uint8_t *value, uint8_t length, uint8_t data_type);
int embedded_flash_set_bool(uint8_t key, bool value) {
    uint8_t temp = value ? 1 : 0;
    return embedded_flash_set(key, (uint8_t*)&temp, sizeof(uint8_t), EFLASH_FORMAT_BOOL);
}

int embedded_flash_set_uint8(uint8_t key, uint8_t value) {
    return embedded_flash_set(key, &value, sizeof(uint8_t), EFLASH_FORMAT_UINT8);
}

int embedded_flash_set_int8(uint8_t key, int8_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(int8_t), EFLASH_FORMAT_INT8);
}

int embedded_flash_set_uint16(uint8_t key, uint16_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(uint16_t), EFLASH_FORMAT_UINT16);
}

int embedded_flash_set_int16(uint8_t key, int16_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(int16_t), EFLASH_FORMAT_INT16);
}

int embedded_flash_set_uint32(uint8_t key, uint32_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(uint32_t), EFLASH_FORMAT_UINT32);
}

int embedded_flash_set_int32(uint8_t key, int32_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(int32_t), EFLASH_FORMAT_INT32);
}

int embedded_flash_set_uint64(uint8_t key, uint64_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(uint64_t), EFLASH_FORMAT_UINT64);
}

int embedded_flash_set_int64(uint8_t key, int64_t value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(int64_t), EFLASH_FORMAT_INT64);
}

int embedded_flash_set_float(uint8_t key, float value) {
    return embedded_flash_set(key, (uint8_t*)&value, sizeof(float), EFLASH_FORMAT_FLOAT);
}

int embedded_flash_set_string(uint8_t key, const char *value) {
    if (value == NULL) {
        printf("Invalid string value for key=%d\n", key);
        return -1;
    }
    uint8_t length = strlen(value);
    if (length+1 > KV_MAX_VALUE_SIZE) {
        printf("String too long for key=%d, length=%d\n", key, length);
        return -1;
    }
    // 存储字符串时包含结束符，所以长度+1
    return embedded_flash_set(key, (uint8_t*)value, length+1, EFLASH_FORMAT_STRING);
}

int embedded_flash_set_hex(uint8_t key, const uint8_t *value, uint8_t length) {
    if (value == NULL || length == 0 || length > KV_MAX_VALUE_SIZE) {
        printf("Invalid hex data for key=%d, length=%d\n", key, length);
        return -1;
    }
    return embedded_flash_set(key, value, length, EFLASH_FORMAT_HEX);
}

// ==================== 内部通用设置函数 ====================

/**
 * @brief 设置键值对（内部使用）
 * @param key 键
 * @param value 值指针
 * @param length 值长度
 * @param data_type 数据类型
 * @return 0=成功, -1=失败
 */
static int embedded_flash_set(uint8_t key, const uint8_t *value, uint8_t length, uint8_t data_type) {
    printf("[SET] key=%d, type=%d, len=%d\n", key, data_type, length);
    
    if (value == NULL || length == 0) {
		printf("Invalid input for set operation: key=%d, len:%d\n", key,length);
        return -1;
    }
    
    // 验证长度与数据类型匹配
    uint8_t expected_length = embedded_flash_get_type_size(data_type);
    if (expected_length > 0) {
        // 固定长度类型
        if (length != expected_length) {
            printf("Invalid length for key=%d: expected=%d, got=%d\n", key, expected_length, length);
            return -1;
        }
    } else {
        // 可变长度类型（STRING, HEX）
        if (length == 0 || length > KV_MAX_VALUE_SIZE) {
            printf("Invalid length for key=%d: length=%d (must be 1-%d for variable length types)\n", key, length, KV_MAX_VALUE_SIZE);
            return -1;
        }
    }
    //从flash将旧数据读出来，如果没变化就不写入
    kv_data_t *p_kv_data = _find_kv_data(key);
    if(p_kv_data == NULL){
        printf("Key not found: %d\n", key);
        return -1;
    }
    if (data_type != p_kv_data->data_type) {
        printf("Invalid data type for key=%d: expected=%d, actual=%d\n", key, p_kv_data->data_type, data_type);
        return -1;
    }
    KV_Record record = {0};

    if(_find_latest_record(p_kv_data, &record, &p_kv_data->addr_abs) != 0){
        return -1;
    }
    if(record.value_length == length && memcmp(record.value, value, length) == 0){
        return 0;
    }
    /*工作流程
        1、新 PRE_WRITE → 新 WRITE → 旧 PRE_DELETE → 旧 DELETED
        2、任意时刻至少有一条 WRITE 生效（数据存储顺序为先旧后新）
    */
    // 构造记录
    memset(&record, 0xFF, sizeof(KV_Record));  // 先全部初始化为0xFF
    record.magic = KV_HEADER_MAGIC;
    record.data_type = data_type;
    record.key = key;
    record.value_length = length;  // value_length就是值的长度
    
    // 额外的边界检查，防止数组越界
    if (length > KV_MAX_VALUE_SIZE) {
        printf("CRITICAL: length=%d exceeds KV_MAX_VALUE_SIZE=%d for key=%d\n", length, KV_MAX_VALUE_SIZE, key);
        return -1;
    }
    
    // 关键修复：先清零整个value数组，然后复制数据
    memset(record.value, 0, KV_MAX_VALUE_SIZE);
    memcpy(record.value, value, length);

    // 计算CRC - 跳过status_table(16) + magic(1) + data_type(1)，从key开始校验key + value_length + value
    // 关键修复：使用固定长度KV_MAX_VALUE_SIZE而不是实际数据长度
    record.crc = crc16_x25_calculate((uint8_t*)&record.key, 2 + KV_MAX_VALUE_SIZE);
    // 写入记录（使用状态表机制实现分阶段提交）
    uint32_t new_write_abs_addr = 0;
    if(_write_record(&record,&new_write_abs_addr) != 0){
		printf("_write_record fail\r\n");
        return -1;
    }

    // 使用状态表机制将新记录标记为WRITE状态
    uint8_t status_table[KV_STATUS_TABLE_SIZE];
    if(_kv_write_status(new_write_abs_addr, status_table, EFLASH_KV_WRITE) != FLASH_NO_ERR){
		printf("write EFLASH_KV_WRITE fail\r\n");
        EFLASH_ASSERT(0);
        return -1;
    }
	memcpy(record.status_table, status_table, KV_STATUS_TABLE_SIZE);
    //如果不相等，那么内部有大于2条数据记录，需要将旧记录设置为删除
    if(p_kv_data->addr_abs != new_write_abs_addr){
        //使用状态表机制删除旧记录
        if(_kv_delete_record(p_kv_data->addr_abs) != FLASH_NO_ERR){
            printf("delete old record fail\r\n");
            EFLASH_ASSERT(0);
            return -1;
        }
    }
    //更新最新记录的信息到ram
    p_kv_data->addr_abs = new_write_abs_addr;
    p_kv_data->value_length = record.value_length;
    p_kv_data->data_type = record.data_type;
    p_kv_data->data_source = KV_DATA_SOURCE_UPDATE_WRITE;//更新数据到掉电储存区
	memcpy(p_kv_data->value, value, length);

    //验证是否写入
    KV_Record verify_record = {0};
    if (flash_port_read(new_write_abs_addr, (uint32_t*)&verify_record, sizeof(KV_Record)) == FLASH_NO_ERR) {
        if(memcmp(&verify_record, &record, sizeof(KV_Record)) != 0){
            printf("VERIFY FAIL: key=%d, addr=0x%x\n", key,new_write_abs_addr);
            printf("Expected record:\n");
            printf("  magic=0x%02X, data_type=%d, key=%d, value_length=%d\n", 
                   record.magic, record.data_type, record.key, record.value_length);
            printf("  value: ");
            for(int i=0; i<KV_MAX_VALUE_SIZE; i++) printf("%02X ", record.value[i]);
            printf("\n  crc=0x%04X\n", record.crc);
            
            printf("Read back record:\n");
            printf("  magic=0x%02X, data_type=%d, key=%d, value_length=%d\n", 
                   verify_record.magic, verify_record.data_type, verify_record.key, verify_record.value_length);
            printf("  value: ");
            for(int i=0; i<KV_MAX_VALUE_SIZE; i++) printf("%02X ", verify_record.value[i]);
            printf("\n  crc=0x%04X\n", verify_record.crc);
            
            printf("Differences:\n");
            uint8_t *p1 = (uint8_t*)&record;
            uint8_t *p2 = (uint8_t*)&verify_record;
            for(int i=0; i<sizeof(KV_Record); i++) {
                if(p1[i] != p2[i]) {
                    printf("  offset %d: expected=0x%02X, actual=0x%02X, diff=0x%02X\n", 
                           i, p1[i], p2[i], p1[i]^p2[i]);
                }
            }
            return -1;
        }
    }
    printf("Set operation successful for key=%d,addr:0x%x\n", key,new_write_abs_addr);

    
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
int embedded_flash_get(uint8_t key, uint8_t *value, uint8_t *length, uint8_t *data_type) {
    if (value == NULL || length == NULL || data_type == NULL) {
        printf("Invalid input for get operation: key=%d\n", key);
        return -1;
    }
    /* 工作流程
        1、mp_kv_list中找到对应键值对的记录
        2、通过地址偏移量找到对应扇区和地址
        3、读取数据并跟当数据对比，如果有差异就遍历整个扇区，找到最新数据
        4、返回数据
    */
    uint32_t addr = 0;
    KV_Record record={0};
    kv_data_t *p_kv_data = _find_kv_data(key);
    if(p_kv_data == NULL){
        printf("Key not found: %d\n", key);
        //写一个默认值进去
        //todo...
        return -1;
    }
    // 、、printf("Getting data for key=%d, stored address=0x%x\n", key, p_kv_data->addr_abs);
    if(_find_latest_record(p_kv_data, &record, &addr) != 0){
        printf("Failed to find latest record for key=%d\n", key);
        return -1;
    }
    // 复制数据
    *length = record.value_length;  // value_length就是值的长度
    *data_type = record.data_type;
    
    // 检查缓冲区大小，防止数组越界
    if (*length > KV_MAX_VALUE_SIZE) {
        printf("CRITICAL: value_length=%d exceeds KV_MAX_VALUE_SIZE=%d for key=%d\n", *length, KV_MAX_VALUE_SIZE, key);
        return -1;
    }
    memcpy(value, record.value, *length);
    printf("Get operation successful for key=%d, len=%d, addr=0x%x\n", key, *length, p_kv_data->addr_abs);
    return 0;
}

/**
 * @brief 删除键值对
 * @param key 键
 * @return 0=成功, -1=失败
 */
int embedded_flash_delete(uint8_t key) {
    /* 工作流程
        1、mp_kv_list中找到对应键值对的记录
        2、将记录状态设置为删除
        3、写入成功后将数据设置为删除，并更新addr_abs_offset，方便读写
        4、写入失败返回错误码
    */
    kv_data_t *p_kv_data = _find_kv_data(key);
    if(p_kv_data == NULL){
        return -1;
    }
    
    // 使用状态表机制删除记录
    if(_kv_delete_record(p_kv_data->addr_abs) != FLASH_NO_ERR){
        EFLASH_ASSERT(0);
        return -1;
    }
    
    p_kv_data->addr_abs = 0;
    p_kv_data->data_source = KV_DATA_SOURCE_DEFAULT;
    return 0;
}

/**
 * @brief 垃圾回收（Garbage Collection）
 * @note GC是持久化存储的核心机制，用于回收无效数据，整理有效数据
 * @note 采用循环存储模式，确保断电安全的状态机设计
 * @note 支持GC过程中断后的恢复操作
 * @return 0=成功, -1=失败
 */
static int embedded_flash_gc(void) {
    /*
    ==================== 增量GC操作流程详解 ====================
    
    1. 【GC触发条件】
       - 当所有数据扇区都满了，无法写入新数据时触发GC
       - 上电时检测到有扇区是GC_TEMP或者DATA_GCING状态，说明上次GC被中断，需要完成GC操作
    完成上述功能，需要的api，迁移函数

    2. 【增量GC执行步骤】
       a) 检测GC中断状态（GC_TEMP、DATA_GCING）
       b) 如果存在中断状态，继续完成被中断的GC操作
       c) 如果是正常GC，获取当前GC扇区并开始新的GC操作
       d) 每次GC只处理一个数据扇区，不是所有扇区
       e) 将有效数据从当前数据扇区迁移到GC区
       f) 迁移完成后进行扇区角色切换
    
    3. 【增量GC特点】
       - 每次只处理一个扇区，减少GC时间
       - 避免长时间阻塞系统
       - 可以分多次完成所有扇区的整理
       - 断电安全：使用状态机确保数据一致性
       - 支持中断恢复：能够从GC_TEMP状态继续完成GC
    */
    int gc_sector_pos, data_sector_pos;
    uint32_t gc_temp_resume_empty_addr_abs = 0;
    uint32_t data_gcing_resume_addr_abs = 0;
    
    embedded_flash_status_t status = embedded_flash_get_status();
    printf("Starting GC operation, status=%d\n", status);
    switch(status){
        case EFLASH_STATUS_NORMAL:{
            // 获取当前GC扇区
            gc_sector_pos = _find_sector_role(EFLASH_SECTOR_ROLE_GC);
            // 获取第一个数据区
            data_sector_pos = (gc_sector_pos + 1) % KV_SECTOR_COUNT;
						printf("EFLASH_STATUS_NORMAL\n");
            break;
        }
        //GC中断恢复
        case EFLASH_STATUS_GC_PREPARE:{
            gc_sector_pos = _find_sector_role(EFLASH_SECTOR_ROLE_GC_TEMP);
            // 获取第一个数据区
            data_sector_pos = (gc_sector_pos + 1) % KV_SECTOR_COUNT;
						printf("EFLASH_STATUS_GC_PREPARE\n");
            break;
        }
        case EFLASH_STATUS_GC_MIGRATING:{
						printf("EFLASH_STATUS_GC_MIGRATING\n");
            //这里要遍历cg临时区和被gc数据区，知道上次遍历的位置并还原
            gc_sector_pos = _find_sector_role(EFLASH_SECTOR_ROLE_GC_TEMP);
            data_sector_pos = _find_sector_role(EFLASH_SECTOR_ROLE_DATA_GCING);
            
            //遍历gc临时扇区
            uint32_t last_gc_temp_scan_addr = m_sector_desc_list[gc_sector_pos].sector_addr + sizeof(sector_header_t);
            uint32_t last_gc_temp_end_addr = m_sector_desc_list[gc_sector_pos].sector_addr + KV_SECTOR_SIZE;
            
            //获取当前迁移的位置 - 找到GC临时扇区中最后一条记录
            KV_Record temp_record = {0};
            KV_Record last_gc_temp_record = {0}; 
            KV_Record last_data_gcing_record = {0};
            
            uint16_t record_count = 0;
            // 遍历GC临时扇区，找到最后一条记录
            while (last_gc_temp_scan_addr + sizeof(KV_Record) < last_gc_temp_end_addr) {//使用《=还是《
                
                // 读取记录
                if (flash_port_read(last_gc_temp_scan_addr, (uint32_t*)&temp_record, sizeof(KV_Record)) != FLASH_NO_ERR) {
                    //todo...
        
                    // data_sector_scan_addr += sizeof(KV_Record);
                    continue;
                }
                
                // 检查是否为有效的KV记录，gc临时区只有pre_write和write状态
                if (_is_kv_record(&temp_record)) {
                    last_gc_temp_record = temp_record;//更新最新有效记录
                     // 记录最后一条记录的地址
                    uint8_t record_status = _kv_get_status(last_gc_temp_record.status_table);
                    if(record_status == EFLASH_KV_PRE_WRITE){
                        gc_temp_resume_empty_addr_abs = last_gc_temp_scan_addr;
                    }else{
                        gc_temp_resume_empty_addr_abs = last_gc_temp_scan_addr;
                    }
                    record_count++;
                }else{
                    //要检查这片区域是不是0xff
                    if(temp_record.magic != 0xFF){
                        //这片区域有问题，可能上次掉电没完全写入，跳过这片区域重新开始
                        last_gc_temp_scan_addr += sizeof(KV_Record);
                        gc_temp_resume_empty_addr_abs = last_gc_temp_scan_addr;
                        break;
                    }else{
                        //这片区域机器后面区域都是空的,可以停止遍历了
                        gc_temp_resume_empty_addr_abs = last_gc_temp_scan_addr;
                        break;
                    }
                }
                last_gc_temp_scan_addr += sizeof(KV_Record);
            }	
            //更新扇区信息
            m_sector_desc_list[gc_sector_pos].free_space = last_gc_temp_end_addr - last_gc_temp_scan_addr;
            m_sector_desc_list[gc_sector_pos].record_count = record_count;//统计记录
            
            record_count = 0;
            //遍历被gc扇区 - 找到最后一条已处理的记录位置
            uint32_t last_data_gcing_scan_addr = m_sector_desc_list[data_sector_pos].sector_addr + sizeof(sector_header_t);
            uint32_t last_data_gcing_end_addr = m_sector_desc_list[data_sector_pos].sector_addr + KV_SECTOR_SIZE;
        
            while (last_data_gcing_scan_addr + sizeof(KV_Record) < last_data_gcing_end_addr) {//使用<=还是<
                // 读取记录
                if (flash_port_read(last_data_gcing_scan_addr, (uint32_t*)&temp_record, sizeof(KV_Record)) != FLASH_NO_ERR) {
                    //todo...
        
                    // data_sector_scan_addr += sizeof(KV_Record);
                    continue;
                }
                
                // 只处理有效的记录（EFLASH_KV_WRITE状态）
                if (_is_kv_record(&temp_record)) {
                    
                    last_data_gcing_record = temp_record;//更新最新有效记录
                    //互为副本，所以数据值一定一样
                    if(last_data_gcing_record.key == last_gc_temp_record.key
                    && last_data_gcing_record.value_length == last_gc_temp_record.value_length
                    && memcmp(last_data_gcing_record.value,
                              last_gc_temp_record.value,
                              last_data_gcing_record.value_length) == 0
                    ){
                        //如果gc临时区最后一条数据是EFLASH_KV_PRE_WRITE，那么被gc数据区的当前记录一定是EFLASH_KV_WRITE，因为pre_write和write是成对出现的
                        uint8_t gc_temp_status = _kv_get_status(last_gc_temp_record.status_table);
                        uint8_t data_gcing_status = _kv_get_status(last_data_gcing_record.status_table);
                        
                        if(gc_temp_status == EFLASH_KV_PRE_WRITE
                           && data_gcing_status == EFLASH_KV_WRITE){
                            //将gc临时区的数据置为有效（使用状态表机制）
                            uint8_t status_table[KV_STATUS_TABLE_SIZE];
                            if (_kv_write_status(gc_temp_resume_empty_addr_abs, status_table, EFLASH_KV_WRITE) != FLASH_NO_ERR) {
                                EFLASH_ASSERT(0);
                                return -1;
                            }
                            
                            //将原记录标记为DELETED状态（使用状态表机制）
                            if (_kv_delete_record(last_data_gcing_scan_addr) != FLASH_NO_ERR) {
                                EFLASH_ASSERT(0);
                                return -1;
                            }
                            data_gcing_resume_addr_abs = last_data_gcing_scan_addr;
                            break;//跳出while循环
                        }
                        //如果gc临时区最后一条记录是EFLASH_KV_WRITE，那么被gc数据区的当前记录可能也是EFLASH_KV_WRITE或者是delete
                        else if(gc_temp_status == EFLASH_KV_WRITE){
                            if(data_gcing_status == EFLASH_KV_WRITE){
                                //将原记录标记为DELETED状态（使用状态表机制）
                                if (_kv_delete_record(last_data_gcing_scan_addr) != FLASH_NO_ERR) {
                                    EFLASH_ASSERT(0);
                                    return -1;
                                }
                                data_gcing_resume_addr_abs = last_data_gcing_scan_addr;
                                break;//跳出while循环
                            }else if(data_gcing_status == EFLASH_KV_DELETED){
                                data_gcing_resume_addr_abs = last_data_gcing_scan_addr;
                                break;//跳出while循环
                            }
                        }
                    }    
										record_count++;
                }
                last_data_gcing_scan_addr += sizeof(KV_Record);
            }	
            //更新扇区信息
            m_sector_desc_list[data_sector_pos].free_space = last_data_gcing_end_addr - last_data_gcing_scan_addr;
            m_sector_desc_list[data_sector_pos].record_count = record_count;
            goto gc_operation_handle;
        }
        case EFLASH_STATUS_GC_AFTER_MIGRATE:
        case EFLASH_STATUS_GC_AFTER_MIGRATE_WITH_EMPTY:{
						printf("EFLASH_STATUS_GC_AFTER_MIGRATE，EFLASH_STATUS_GC_AFTER_MIGRATE_WITH_EMPTY\n");
            if(status == EFLASH_STATUS_GC_AFTER_MIGRATE_WITH_EMPTY){
                gc_sector_pos = _find_sector_role(EFLASH_SECTOR_ROLE_UNASSIGNED);
            }else{
                gc_sector_pos = _find_sector_role(EFLASH_SECTOR_ROLE_DATA_GCING);
            }
            if (gc_sector_pos < 0) {
                return -1;
            }
            _erase_sector(gc_sector_pos);
            _sector_header_write(gc_sector_pos, EFLASH_SECTOR_STATUS_FREE, EFLASH_SECTOR_ROLE_GC);
            m_sector_desc_list[gc_sector_pos].free_space = KV_SECTOR_SIZE - sizeof(sector_header_t);
            m_sector_desc_list[gc_sector_pos].record_count = 0;
            //再触发正常gc流程
            // 获取当前GC扇区
            gc_sector_pos = _find_sector_role(EFLASH_SECTOR_ROLE_GC);
            if (gc_sector_pos < 0) {
                return -1;
            }
            // 获取第一个数据区
            data_sector_pos = (gc_sector_pos + 1) % KV_SECTOR_COUNT;
            break;
        }
        default:
            return -1;
    }
    //获取地址
    gc_temp_resume_empty_addr_abs = m_sector_desc_list[gc_sector_pos].sector_addr+sizeof(sector_header_t);
    data_gcing_resume_addr_abs = m_sector_desc_list[data_sector_pos].sector_addr+sizeof(sector_header_t);
    
    // 将GC区设置为临时GC区，确保原子性操作
    _sector_header_write(gc_sector_pos, m_sector_desc_list[gc_sector_pos].attr.status, EFLASH_SECTOR_ROLE_GC_TEMP);
    
    // 设置为GC进行时状态，标记正在被GC的数据区
    _sector_header_write(data_sector_pos, m_sector_desc_list[data_sector_pos].attr.status, EFLASH_SECTOR_ROLE_DATA_GCING);
    //执行gc操作
gc_operation_handle:
    if(_execute_gc_operation(gc_sector_pos, data_sector_pos, gc_temp_resume_empty_addr_abs, data_gcing_resume_addr_abs) != 0){
        return -1;
    }
    printf("GC operation completed successfully\n");
    return _refresh_sector_data(gc_sector_pos);
}

// ==================== 内部函数实现 ====================
// /**
//  * @brief 检查GC扇区是否为空
//  * @param sector_idx 扇区索引
//  * @return 1=空, 0=非空, -1=错误
//  */
// static int _gc_sector_is_empty(uint8_t sector_idx)
// {
//     if (sector_idx >= KV_SECTOR_COUNT) {
//         return -1;
//     }
    
//     // 遍历整个扇区，看是不是都是FF，扇区开头4字节的头信息除外
//     uint32_t start_addr = m_sector_desc_list[sector_idx].sector_addr + sizeof(sector_header_t);
//     uint32_t end_addr = m_sector_desc_list[sector_idx].sector_addr + KV_SECTOR_SIZE;
    
//     // 先按4字节对齐读取
//     while (start_addr <= end_addr && start_addr + 4 <= end_addr) {
//         uint32_t data;
//         if (drv_flash_read(start_addr, (uint8_t*)&data, 4) != 4) {
//             return -1;
//         }
        
//         // 检查是否都是0xFF
//         if (data != 0xFFFFFFFF) {
//             return 0;  // 非空
//         }
        
//         start_addr += 4;
//     }
    
//     // 处理剩余不足4字节的部分
//     while (start_addr < end_addr) {
//         uint8_t byte;
//         if (drv_flash_read(start_addr, &byte, 1) != 1) {
//             return -1;
//         }
        
//         if (byte != 0xFF) {
//             return 0;  // 非空
//         }
        
//         start_addr++;
//     }
    
//     return 1;  // 空
// }

static int _startup_rebuild_sector(void)
{
	for(uint8_t i = 0; i < (KV_SECTOR_COUNT-1); i++){
        //擦除扇区
        int ret = _erase_sector(i);
        //重建扇区，写入新的头信息
        ret |= _sector_header_write(i, EFLASH_SECTOR_STATUS_FREE, EFLASH_SECTOR_ROLE_DATA);
        if(ret != 0){
                return -1;
        }
	}
	return _sector_header_write(KV_SECTOR_COUNT-1, EFLASH_SECTOR_STATUS_FREE, EFLASH_SECTOR_ROLE_GC);
		
}
static void _startup_restore_sector_attr(void) {
    
    for (int i = 0; i < KV_SECTOR_COUNT; i++) {
        sector_header_t header = {0};
        if(_sector_header_read(i, &header) != 0) {
            m_sector_desc_list[i].attr.status = EFLASH_SECTOR_STATUS_FREE;
            m_sector_desc_list[i].attr.role = EFLASH_SECTOR_ROLE_UNASSIGNED;
            continue;
        }
        // 状态表机制已经通过_sector_header_read更新了m_sector_desc_list[i].attr
        // 无需额外操作
    }
}
/**
 * @brief 刷新指定扇区的数据与状态
 * @param sector_idx 扇区索引
 * @return 0=成功, -1=失败
 */
static int _refresh_sector_data(int sector_idx) {
	// 跳过GC扇区，只处理数据扇区
	if (m_sector_desc_list[sector_idx].attr.role == EFLASH_SECTOR_ROLE_GC) {
		// GC扇区应该是空的，设置相应状态
		m_sector_desc_list[sector_idx].free_space = KV_SECTOR_SIZE - sizeof(sector_header_t);
		m_sector_desc_list[sector_idx].record_count = 0;
		m_sector_desc_list[sector_idx].attr.status = EFLASH_SECTOR_STATUS_FREE;
		return 0;
	}

	uint32_t scan_addr = m_sector_desc_list[sector_idx].sector_addr + sizeof(sector_header_t);
	uint32_t sector_end_addr = m_sector_desc_list[sector_idx].sector_addr + KV_SECTOR_SIZE;
	uint16_t record_count = 0;

    // 扫描当前扇区的所有记录
    while (scan_addr + sizeof(KV_Record) < sector_end_addr) {//使用<=还是<
		KV_Record record ={0};
		// 读取完整记录
		if (flash_port_read(scan_addr, (uint32_t*)&record, sizeof(KV_Record)) == FLASH_NO_ERR) {
            // 检查记录基本有效性（magic、CRC等）
            if (_is_kv_record(&record)) {
                // 查找对应的kv_data_t
                kv_data_t *p_kv_data = _find_kv_data(record.key);
                if (p_kv_data != NULL) {
                    // 跳过无效记录，但仍需要统计record_count
                    uint8_t record_status = _kv_get_status(record.status_table);
                    if (record_status == EFLASH_KV_DELETED
                    || record_status == EFLASH_KV_PRE_DELETE
                    || record_status == EFLASH_KV_PRE_WRITE) {
                        record_count++;
                        scan_addr += sizeof(KV_Record);
                        continue;
                    }

                    //gc过程和set能保证数据区存在0<有效数据<=2条,所以这里从第一个数据区取数据一定能取到最新的
                    p_kv_data->addr_abs = scan_addr;
                    p_kv_data->value_length = record.value_length;
                    p_kv_data->data_type = record.data_type;
                    p_kv_data->data_source = KV_DATA_SOURCE_FLASH_READ;
                    memcpy(p_kv_data->value, record.value, record.value_length);
                }
                record_count++;
            } else {
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
        }
        //成功失败都继续推进，避免死循环
		scan_addr += sizeof(KV_Record);
	}

	// 更新扇区状态信息
	m_sector_desc_list[sector_idx].free_space = sector_end_addr - scan_addr;
	m_sector_desc_list[sector_idx].record_count = record_count;

	// 设置扇区状态
	if (m_sector_desc_list[sector_idx].free_space < sizeof(KV_Record)) {
		m_sector_desc_list[sector_idx].attr.status = EFLASH_SECTOR_STATUS_FULL;
	} else if (record_count > 0) {
		m_sector_desc_list[sector_idx].attr.status = EFLASH_SECTOR_STATUS_USING;
	} else {
		m_sector_desc_list[sector_idx].attr.status = EFLASH_SECTOR_STATUS_FREE;
	}

	return 0;
}
/**
 * @brief 上电时恢复键值对数据
 * @note 扫描所有扇区，从Flash读取数据到RAM，重建扇区状态信息
 * @return 0=成功, -1=失败
 */
static int _startup_restore_kv_data(void) {

    //一定要从第一个数据区开始，因为数据是按顺序储存的,在写入过程中掉电会导致数据区有两条有效数据
    
    //先看有没有gc区，如果没有gc区就看有没有gc临时区，如果没有gc临时区就看有没有被gc区
    uint8_t first_data_sector_pos = 0;
    for (int j = 0; j < KV_SECTOR_COUNT; j++) {
        //读头信息
        sector_header_t header = {0};
        if(_sector_header_read(j, &header) != 0) {
            continue;
        }
        uint8_t role = _get_status(header.role_table, SECTOR_ROLE_NUM);
        if(role == EFLASH_SECTOR_ROLE_GC
        || role == EFLASH_SECTOR_ROLE_GC_TEMP) {
            first_data_sector_pos = (j + 1) % KV_SECTOR_COUNT;
            break;
        }
        if(role == EFLASH_SECTOR_ROLE_DATA_GCING) {
            first_data_sector_pos = j;
            break;
        }
    }

	//读取所有扇区。不管是gc临时、数据区、被gc数据区,GC区就没必要了，因为GC区应该都是空的
	for (int i = 0; i < KV_SECTOR_COUNT; i++) {
		int sector_idx = (first_data_sector_pos + i) % KV_SECTOR_COUNT;
		_refresh_sector_data(sector_idx);
	}
    
    return 0;
}

/**
 * @brief 上电时初始化缺失的默认值
 * @note 将Flash中不存在的键值对写入默认值
 * @return 0=成功, -1=失败
 */
static int _startup_init_missing_defaults(void) {
    for (int i = 0; i < m_kv_data_list_count; i++) {
        if (mp_kv_list[i].data_source == KV_DATA_SOURCE_DEFAULT) {
            // 构造默认值记录
            KV_Record record;
            memset(&record, 0xFF, sizeof(KV_Record));  // 先初始化为0xFF
            record.magic = KV_HEADER_MAGIC;
            record.data_type = mp_kv_list[i].data_type;
            record.key = mp_kv_list[i].key;
            record.value_length = mp_kv_list[i].value_length;
            
            // 检查边界，防止数组越界
            if (mp_kv_list[i].value_length > KV_MAX_VALUE_SIZE) {
                printf("CRITICAL: default value_length=%d exceeds KV_MAX_VALUE_SIZE=%d for key=%d\n", 
                       mp_kv_list[i].value_length, KV_MAX_VALUE_SIZE, mp_kv_list[i].key);
                return -1;
            }
            
            // 关键修复：先清零整个value数组，然后复制数据
            memset(record.value, 0, KV_MAX_VALUE_SIZE);
            memcpy(record.value, mp_kv_list[i].value, mp_kv_list[i].value_length);
            // 计算CRC - 跳过status_table(16) + magic(1) + data_type(1)，从key开始
            record.crc = crc16_x25_calculate((uint8_t*)&record.key, 2 + KV_MAX_VALUE_SIZE);
            // 写入记录（使用状态表机制）
            uint32_t write_addr_abs = 0;
            if (_write_record(&record, &write_addr_abs) != 0) {
                return -1;
            }
            // 使用状态表机制标记为WRITE
            uint8_t status_table[KV_STATUS_TABLE_SIZE];
            if(_kv_write_status(write_addr_abs, status_table, EFLASH_KV_WRITE) != FLASH_NO_ERR){
                EFLASH_ASSERT(0);
                return -1;
            }
            // 更新RAM中的状态
            mp_kv_list[i].addr_abs = write_addr_abs;
            mp_kv_list[i].data_source = KV_DATA_SOURCE_FIRST_WRITE;
        }
    }
    
    return 0;
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
    if(flash_port_read(read_addr, (uint32_t*)record, sizeof(KV_Record)) != FLASH_NO_ERR){
        printf("Failed to read record at stored address=0x%x for key=%d\n", read_addr, p_kv_data->key);
        return -1;
    }
    if (!_is_kv_record(record)){
        printf("Invalid record at stored address=0x%x for key=%d\n", read_addr, p_kv_data->key);
        // 遍历所有扇区查找最新记录
        for (int i = 0; i < KV_SECTOR_COUNT; i++) {
            if (m_sector_desc_list[i].attr.role == EFLASH_SECTOR_ROLE_DATA || m_sector_desc_list[i].attr.role == EFLASH_SECTOR_ROLE_DATA_GCING) {
                uint32_t scan_addr = m_sector_desc_list[i].sector_addr + sizeof(sector_header_t);
                uint32_t end_addr = m_sector_desc_list[i].sector_addr + KV_SECTOR_SIZE;
                printf("Scanning sector %d for key=%d\n", i, p_kv_data->key);
                while (scan_addr + sizeof(KV_Record) < end_addr) {//使用<=还是<
                    KV_Record temp_record = {0};
                    if (flash_port_read(scan_addr, (uint32_t*)&temp_record, sizeof(KV_Record)) == FLASH_NO_ERR) {
                        uint8_t temp_status = _kv_get_status(temp_record.status_table);
                        if (_is_kv_record(&temp_record) && temp_record.key == p_kv_data->key && temp_status == EFLASH_KV_WRITE) {
                            *addr = scan_addr;
                            *record = temp_record;
                            printf("Found latest record for key=%d at address=%ld in sector=%d\n", p_kv_data->key, scan_addr, i);
                            p_kv_data->addr_abs = scan_addr;
                            if(record->value_length > KV_MAX_VALUE_SIZE){
                                EFLASH_ASSERT(0);
                                return -1;
                            }
                            p_kv_data->value_length = record->value_length;
                            p_kv_data->data_type = record->data_type;
                            p_kv_data->data_source = KV_DATA_SOURCE_FLASH_OVERRIDE;
                            memcpy(p_kv_data->value, record->value, record->value_length);
                            return 0;
                        }
                    }
                    scan_addr += sizeof(KV_Record);
                }
            }
        }
        printf("No valid record found for key=%d after scanning all sectors\n", p_kv_data->key);
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
 * @brief 写入记录到指定扇区
 * @param sector_idx 目标扇区索引
 * @param p KV记录指针
 * @param p_addr_abs_offset 返回写入的绝对偏移地址
 * @return 0=成功, -1=失败
 */
static int _write_record_to_sector(uint8_t sector_idx, KV_Record *p, uint32_t *p_addr_abs) {
    if (sector_idx >= KV_SECTOR_COUNT || p == NULL || p_addr_abs == NULL) {
        return -1;
    }
    
    // 检查扇区是否有足够空间
    if (m_sector_desc_list[sector_idx].free_space < sizeof(KV_Record)) {
        return -1;  // 空间不足
    }
    
    // 计算写入地址 = 扇区起始地址 + 扇区头大小 + 已使用空间
    uint32_t write_addr = m_sector_desc_list[sector_idx].sector_addr + sizeof(sector_header_t) + 
                         (KV_SECTOR_SIZE - sizeof(sector_header_t) - m_sector_desc_list[sector_idx].free_space);
    
    // 写入Flash
    if (flash_port_write(write_addr, (uint32_t*)p, sizeof(KV_Record)) != FLASH_NO_ERR){
        EFLASH_ASSERT(0);
        return -1;
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
    
    // 只有状态改变时才写入Flash
    if (m_sector_desc_list[sector_idx].attr.status != original_status) {
        printf("original_status:%d, new_status:%d\n", original_status, m_sector_desc_list[sector_idx].attr.status);
        if(_sector_header_write(sector_idx, m_sector_desc_list[sector_idx].attr.status, m_sector_desc_list[sector_idx].attr.role) != 0) {
            return -1;
        }
    }
		
		// 返回绝对地址 ，必须是最后，不然中途失败了，外部又在用新地址就麻烦了
    *p_addr_abs = write_addr;
    return 0;
}

/**
 * @brief 写入记录到Flash（自动选择扇区）
 */
static int _write_record(KV_Record *p, uint32_t *p_addr_abs) {
    int sector_idx = 0;
    
restart_write:   
    // 寻找有足够空间的可写入扇区
    sector_idx = _find_writable_data_sector(sizeof(KV_Record));
    if (sector_idx < 0) {
        // 所有数据扇区都满了，需要GC
        if (embedded_flash_gc() != 0) {
            return -1;
        }
        goto restart_write;  // GC后重新寻找可写入扇区
    }
    
    // 使用新的写入函数
    return _write_record_to_sector(sector_idx, p, p_addr_abs);
}

static int _find_sector_role(uint8_t role) {
    // 从最后一个扇区开始逆序查找
    for (int i = KV_SECTOR_COUNT - 1; i >= 0; i--) {
        if (m_sector_desc_list[i].attr.role == role) {  
            return i;
        }
    }
    return -1;  // 未找到
}




/**
 * @brief 检查记录是否有效
 */
static bool _is_kv_record(const KV_Record *record) {
    // 空指针检查
    if (record == NULL) {
        printf("_is_kv_record: NULL record pointer\n");
        return false;
    }
    
    if (record->magic != KV_HEADER_MAGIC) {
        printf("_is_kv_record: Invalid magic=0x%02X, expected=0x%02X\n", record->magic, KV_HEADER_MAGIC);
        return false;
    }

    if (record->value_length == 0 || record->value_length > KV_MAX_VALUE_SIZE) {
        printf("_is_kv_record: Invalid value_length=%d, key=%d\n", record->value_length, record->key);
        return false;
    }

    // 检查CRC - 跳过status_table(16) + magic(1) + data_type(1)，从key开始校验key + value_length + value
    // 关键修复：使用固定长度KV_MAX_VALUE_SIZE而不是实际数据长度
    uint16_t calc_crc = crc16_x25_calculate((uint8_t*)&record->key, 2 + KV_MAX_VALUE_SIZE);
    if (calc_crc != record->crc) {
        uint8_t record_status = _kv_get_status((uint8_t * )record->status_table);
        printf("_is_kv_record: CRC mismatch for key=%d, status:%d, value_length:%d, data_type:%d, calc_crc=0x%04X, stored_crc=0x%04X\n", 
               record->key, record_status, record->value_length, record->data_type, calc_crc, record->crc);
        return false;
    }

    return true;
}



/**
 * @brief 擦除扇区
 */
static int _erase_sector(uint8_t sector_idx) {
    if (sector_idx < KV_SECTOR_COUNT) {
        printf("EmbeddedFlash: Erasing sector %d at address 0x%08X, size=%d\n", 
               sector_idx, m_sector_desc_list[sector_idx].sector_addr, KV_SECTOR_SIZE);
        if (flash_port_erase(m_sector_desc_list[sector_idx].sector_addr, KV_SECTOR_SIZE) == FLASH_NO_ERR) {
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
                
                return 0;  
        }
    }else{
        printf("erase sector failed, sector_idx=%d\n", sector_idx);
        EFLASH_ASSERT(0);
    }
    EFLASH_ASSERT(0);
    return -1;
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

/**
 * @brief 执行GC操作（统一的数据迁移和角色切换逻辑）
 * @param gc_sector_pos GC扇区索引（GC_TEMP状态）
 * @param data_sector_pos 要处理的数据扇区索引（DATA_GCING状态）
 * @note 这是GC的核心实现，负责数据迁移和扇区角色切换
 * @note 采用断电安全的状态机设计，确保数据一致性
 * @note 支持中断恢复和正常GC两种模式
 * @return 0=成功, -1=失败
 */
static int _execute_gc_operation(int gc_temp_sector_idx, int data_gcing_sector_idx, uint32_t gc_temp_empty_addr_abs, uint32_t data_gcing_resume_addr_abs) {
    printf("Executing GC operation: GC Temp Sector=%d, Data GCing Sector=%d\n", gc_temp_sector_idx, data_gcing_sector_idx);
    printf("GC Temp Empty Address=0x%x, Data GCing Resume Address=0x%x\n", gc_temp_empty_addr_abs, data_gcing_resume_addr_abs);
    // 计算扫描地址和结束地址
    uint32_t data_sector_scan_addr = data_gcing_resume_addr_abs;
    uint32_t data_sector_end_addr = m_sector_desc_list[data_gcing_sector_idx].sector_addr + KV_SECTOR_SIZE;
    
    uint32_t gc_sector_scan_addr = gc_temp_empty_addr_abs;
    uint32_t gc_sector_end_addr = m_sector_desc_list[gc_temp_sector_idx].sector_addr + KV_SECTOR_SIZE;
    uint16_t gc_sector_status = EFLASH_SECTOR_STATUS_FREE;
    // ==================== 数据迁移阶段 ====================
    KV_Record record;  
    printf("Starting data migration for GC\n");
    while (data_sector_scan_addr + sizeof(KV_Record) < data_sector_end_addr) {//使用<=还是<
        // 读取记录
        memset(&record, 0, sizeof(KV_Record));
        printf("GC: Reading record at address=0x%08X, size=%d\n", data_sector_scan_addr, sizeof(KV_Record));
        if (flash_port_read(data_sector_scan_addr, (uint32_t*)&record, sizeof(KV_Record)) == FLASH_NO_ERR) {
            // 调试：打印读取的记录信息
            uint8_t record_status = _kv_get_status(record.status_table);
            printf("GC: Read record - magic=0x%02X, key=%d, status=%d, value_len=%d, data_type=%d, crc=0x%04X\n", 
                   record.magic, record.key, record_status, record.value_length, record.data_type, record.crc);
            
            // 只处理有效的记录（EFLASH_KV_WRITE状态）
            if (_is_kv_record(&record) && (record_status == EFLASH_KV_WRITE)) {
                printf("Migrating record: Key=%d, Length=%d, Address=%lu\n", record.key, record.value_length, data_sector_scan_addr);
                // 检查GC临时区空间是否足够
                if (gc_sector_scan_addr + sizeof(KV_Record) > gc_sector_end_addr) {
                    // GC临时区空间不足，退出循环
                    printf("GC Temp sector full, stopping migration\n");
                    gc_sector_status = EFLASH_SECTOR_STATUS_FULL;
                    break;
                } else {
                    // GC临时区空间足够，执行原子迁移操作（使用状态表机制）
                    printf("Writing to GC Temp sector at address=%lu\n", gc_sector_scan_addr);
                    // 步骤1: 在GC临时区写入新副本（使用_kv_write_record分阶段提交）
                    KV_Record new_rec = record;
                    uint32_t new_rec_addr = 0; 
                    if (_write_record_to_sector(gc_temp_sector_idx, &new_rec, &new_rec_addr) != 0) {
                        return -1;
                    }
                    
                    // 步骤2: 将新副本提交为WRITE状态（使用状态表机制）
                    uint8_t status_table[KV_STATUS_TABLE_SIZE];
                    if (_kv_write_status(new_rec_addr, status_table, EFLASH_KV_WRITE) != FLASH_NO_ERR) {
                        EFLASH_ASSERT(0);
                        return -1;
                    }
                    
                    // 步骤3: 将原记录标记为DELETED状态（使用状态表机制）
                    if (_kv_delete_record(data_sector_scan_addr) != FLASH_NO_ERR) {
                        EFLASH_ASSERT(0);
                        return -1;
                    }
                    
                    // 更新GC临时区扫描地址
                    gc_sector_scan_addr += sizeof(KV_Record);
                    gc_sector_status = EFLASH_SECTOR_STATUS_USING;
                } 
            }else{
							printf("gc invalid record,key:%d,status:%d,len:%d\n",record.key, record_status, record.value_length);
						}
           
        } else {
            printf("Failed to read record at address=%lu\n", data_sector_scan_addr);
        }
        // 读失败也推进，避免死循环
        data_sector_scan_addr += sizeof(KV_Record);
    }
    // 检查扇区是否已满
    if (m_sector_desc_list[gc_temp_sector_idx].free_space < sizeof(KV_Record)) {
        gc_sector_status = EFLASH_SECTOR_STATUS_FULL;
    }
    // ==================== 扇区角色切换阶段 ====================
    // 步骤1: 将原GC_TEMP扇区转为DATA扇区
    _sector_header_write(gc_temp_sector_idx, gc_sector_status, EFLASH_SECTOR_ROLE_DATA);

    // 步骤2: 将被GC的数据扇区直接擦除并设为GC扇区
    if (_erase_sector(data_gcing_sector_idx) != 0) {
        return -1;
    }
    _sector_header_write(data_gcing_sector_idx, EFLASH_SECTOR_STATUS_FREE, EFLASH_SECTOR_ROLE_GC);
    
    // 重置可用空间
    m_sector_desc_list[data_gcing_sector_idx].free_space = KV_SECTOR_SIZE - sizeof(sector_header_t);
    m_sector_desc_list[data_gcing_sector_idx].record_count = 0;

    // 关键：同步更新内存中的“当前GC扇区”和下一个数据扇区顺序，避免下一次GC立刻处理刚转换的DATA扇区
    // 将新产生的GC扇区视为“尾部”，写入顺序应从它的下一个扇区开始
    // 这里无需立即触发再次GC，返回由上层写流程按需触发

    return 0;
}

/**
 * @brief 寻找可写入的数据扇区（有足够空间的）
 * @param required_space 需要的空间大小
 * @note 从GC区+1开始，按循环顺序查找可用的数据扇区
 * @note 这是循环存储模式的关键实现
 * @return 扇区索引，-1表示所有数据扇区都满了，需要GC
 */
static int _find_writable_data_sector(uint16_t required_space) {
    int gc_sector = _find_sector_role(EFLASH_SECTOR_ROLE_GC);
    if (gc_sector >= 0) {
        // 遍历所有数据扇区，寻找有足够空间的扇区
        // 从GC区+1开始，按循环顺序查找（循环存储模式）
        for (int i = 0; i < (KV_SECTOR_COUNT - 1); i++) {
            uint8_t pos = (gc_sector + 1 + i) % KV_SECTOR_COUNT; // 从第一个数据区开始查找
            
            // 检查扇区是否满足写入条件：
            // 1. 有足够的空闲空间
            // 2. 状态为USING或FREE（可以写入）
            // 3. 角色为DATA（数据扇区）
            if (m_sector_desc_list[pos].free_space >= required_space 
                && (m_sector_desc_list[pos].attr.status == EFLASH_SECTOR_STATUS_USING
                || m_sector_desc_list[pos].attr.status == EFLASH_SECTOR_STATUS_FREE)
                && m_sector_desc_list[pos].attr.role == EFLASH_SECTOR_ROLE_DATA) {
                return pos;  // 找到有足够空间的数据扇区
            }
        }
    }
    return -1;  // 所有数据扇区都满了，需要GC
}

/**
 * @brief 检查扇区头信息是否有效
 * @param header 扇区头信息指针
 * @return true=有效, false=无效
 */
static bool _is_sector_header(const sector_header_t *header) {
    if (header->magic != SECTOR_HEADER_MAGIC_WORD) {
        return false;
    }
    return true;
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
        if (flash_port_read(addr, (uint32_t*)header, sizeof(sector_header_t)) == FLASH_NO_ERR) {
            if (!_is_sector_header(header)) {
                return -1;
            }
            
            /* 解析状态表，更新RAM中的attr */
            m_sector_desc_list[sector_idx].attr.status = _get_status(header->status_table, SECTOR_STATUS_NUM);
            m_sector_desc_list[sector_idx].attr.role = _get_status(header->role_table, SECTOR_ROLE_NUM);
            
            return 0;
        }
    }
    return -1;
}

/**
 * @brief 写入扇区头信息
 * @param sector_idx 扇区索引
 * @param status 扇区状态
 * @param role 扇区角色
 * @return 0=成功, -1=失败
 */
static int _sector_header_write(uint8_t sector_idx, uint8_t status, uint8_t role) {
    FlashErrCode result = FLASH_NO_ERR;
    
    if (sector_idx >= KV_SECTOR_COUNT) {
        return -1;
    }
    
    uint32_t addr = m_sector_desc_list[sector_idx].sector_addr;
    sector_header_t current_header = {0};
    
    /* 尝试读取当前扇区头 */
    if (_sector_header_read(sector_idx, &current_header) == 0) {
        /* 扇区头已存在，使用状态表机制更新状态 */
        uint8_t status_table[SECTOR_STATUS_TABLE_SIZE];
        uint8_t role_table[SECTOR_ROLE_TABLE_SIZE];
        
        /* 更新状态（如果需要） */
        if (m_sector_desc_list[sector_idx].attr.status != status) {
            result = _write_status(addr, status_table, SECTOR_STATUS_NUM, status);
            if (result != FLASH_NO_ERR) {
                EFLASH_ASSERT(0);
                return -1;
            }
            m_sector_desc_list[sector_idx].attr.status = status;
        }
        
        /* 更新角色（如果需要） */
        if (m_sector_desc_list[sector_idx].attr.role != role) {
            result = _write_status(addr + SECTOR_STATUS_TABLE_SIZE, role_table, SECTOR_ROLE_NUM, role);
            if (result != FLASH_NO_ERR) {
                EFLASH_ASSERT(0);
                return -1;
            }
            m_sector_desc_list[sector_idx].attr.role = role;
        }
        
        return 0;
    } else {
        /* 扇区头不存在（擦除后第一次写入），创建新的扇区头 */
        sector_header_t header;
        memset(&header, 0xFF, sizeof(sector_header_t));
        
        /* 设置状态表 */
        _set_status(header.status_table, SECTOR_STATUS_NUM, status);
        _set_status(header.role_table, SECTOR_ROLE_NUM, role);
        
        /* 设置魔术字 */
        header.magic = SECTOR_HEADER_MAGIC_WORD;
        header.reserved = 0xFFFFFFFF;
        
        /* 写入完整扇区头 */
        result = flash_port_write(addr, (uint32_t*)&header, sizeof(sector_header_t));
        if (result == FLASH_NO_ERR) {
            m_sector_desc_list[sector_idx].attr.status = status;
            m_sector_desc_list[sector_idx].attr.role = role;
            return 0;
        }
    }
    
    EFLASH_ASSERT(0);
    return -1;
}


static embedded_flash_status_t embedded_flash_get_status(void)
{
	uint8_t gc_cnt = 0;
	uint8_t gc_temp_cnt = 0;
	uint8_t data_cnt = 0;
	uint8_t data_gcing_cnt = 0;
	uint8_t empty_cnt = 0;
    sector_header_t header={0};
	for (int i = 0; i < KV_SECTOR_COUNT; i++) {
		if (_sector_header_read(i, &header) != 0) {
			empty_cnt++;
		}else{
            uint8_t role = _get_status(header.role_table, SECTOR_ROLE_NUM);
            switch (role) {
                case EFLASH_SECTOR_ROLE_GC:         gc_cnt++;           break;
                case EFLASH_SECTOR_ROLE_GC_TEMP:    gc_temp_cnt++;      break;
                case EFLASH_SECTOR_ROLE_DATA:       data_cnt++;         break;
                case EFLASH_SECTOR_ROLE_DATA_GCING: data_gcing_cnt++;   break;
            }
        }
	}
    uint8_t status = EFLASH_STATUS_UNKNOWN_ERROR;
	if (gc_temp_cnt > 1 || gc_cnt > 1 || data_gcing_cnt > 1) {
		status = EFLASH_STATUS_MULTI_ROLE_ERROR; // GC_TEMP>1 或 GC>1 或 DATA_GCING>1
	}
    
    else if (gc_cnt == 0 && gc_temp_cnt == 0 && data_cnt == 0 && data_gcing_cnt == 0 && empty_cnt == KV_SECTOR_COUNT) {
		status = EFLASH_STATUS_FIRST_POWER_ON; // 首次上电：全空白
	}

	else if (gc_cnt == 1 && gc_temp_cnt == 0 && data_cnt >= 1 && data_gcing_cnt == 0 && empty_cnt == 0) {
		status =  EFLASH_STATUS_NORMAL; // 正常：1 GC，无 GC_TEMP，有 DATA，无 DATA_GCING，无空白
	}

	else if (gc_cnt == 0 && gc_temp_cnt == 1 && data_cnt >= 1 && data_gcing_cnt == 0 && empty_cnt == 0) {
		status =  EFLASH_STATUS_GC_PREPARE; // 异常1：无 GC，有 GC_TEMP，有 DATA，无 DATA_GCING
	}

	else if (gc_cnt == 0 && gc_temp_cnt == 1 && data_cnt >= 1 && data_gcing_cnt == 1 && empty_cnt == 0) {
		status =  EFLASH_STATUS_GC_MIGRATING; // 异常2：无 GC，有 GC_TEMP，有 DATA，有 DATA_GCING
	}

	else if (gc_cnt == 0 && gc_temp_cnt == 0 && data_cnt >= 1 && data_gcing_cnt == 1 && empty_cnt == 0) {
		status =  EFLASH_STATUS_GC_AFTER_MIGRATE; // 异常3：无 GC，无 GC_TEMP，有 DATA，有 DATA_GCING
	}

	else if (gc_cnt == 0 && gc_temp_cnt == 0 && data_cnt >= 1 && data_gcing_cnt == 0 && empty_cnt >= 1) {
		status =  EFLASH_STATUS_GC_AFTER_MIGRATE_WITH_EMPTY; // 异常4：无 GC，无 GC_TEMP，有 DATA，无 DATA_GCING，有空白
	}

	else if (empty_cnt > 0 && (data_cnt > 0 || data_gcing_cnt > 0 || gc_cnt > 0 || gc_temp_cnt > 0)) {
		status =  EFLASH_STATUS_EMPTY_SECTOR_ERROR; // 存在空白但不符合上面场景
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




