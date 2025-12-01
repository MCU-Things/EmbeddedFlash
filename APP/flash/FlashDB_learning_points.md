# FlashDB 值得学习的实现点

## 1. 缓存机制（Cache Mechanism）

### 1.1 KV 缓存（KV Cache）
**位置**：`fdb_kvdb.c` 中的 `update_kv_cache` 和 `get_kv_from_cache`

**设计亮点**：
- 使用 CRC16 作为键的哈希值，减少字符串比较
- LRU-like 算法：使用 `active` 字段记录访问活跃度
- 缓存命中时更新活跃度，未命中时选择活跃度最低的项替换

**代码示例**：
```c
static void update_kv_cache(fdb_kvdb_t db, const char *name, size_t name_len, uint32_t addr)
{
    uint16_t name_crc = (uint16_t)(fdb_calc_crc32(0, name, name_len) >> 16);
    // 使用 CRC 作为哈希，避免字符串比较
    // LRU 算法：更新活跃度，选择最低活跃度的项替换
}

static bool get_kv_from_cache(fdb_kvdb_t db, const char *name, size_t name_len, uint32_t *addr)
{
    // 先通过 CRC 快速查找，再验证字符串
    // 命中时增加活跃度
}
```

**学习价值**：
- 减少 Flash 读取次数，提升性能
- 适合嵌入式场景的轻量级缓存实现

### 1.2 扇区缓存（Sector Cache）
**位置**：`fdb_kvdb.c` 中的 `update_sector_cache` 和 `get_sector_from_cache`

**设计亮点**：
- 缓存当前使用的扇区信息（empty_kv、remain 等）
- 减少扇区头读取次数

**学习价值**：
- 对于频繁访问的扇区，缓存可以显著提升性能

## 2. 状态表实现（Status Table）

### 2.1 支持多种写入粒度
**位置**：`fdb_utils.c` 中的 `_fdb_set_status` 和 `_fdb_get_status`

**设计亮点**：
- 支持 1bit、8bit、32bit、64bit、128bit 写入粒度
- 状态表大小根据写入粒度动态计算
- 状态 0（全 FF）无需写入 Flash

**代码示例**：
```c
size_t _fdb_set_status(uint8_t status_table[], size_t status_num, size_t status_index)
{
    memset(status_table, FDB_BYTE_ERASED, FDB_STATUS_TABLE_SIZE(status_num));
    if (status_index > 0) {
#if (FDB_WRITE_GRAN == 1)
        // 1bit 粒度：按位操作
        byte_index = (status_index - 1) / 8;
        status_table[byte_index] &= (0x00ff >> (status_index % 8));
#else
        // 8/32/64bit 粒度：按字节操作
        byte_index = (status_index - 1) * (FDB_WRITE_GRAN / 8);
        status_table[byte_index] = FDB_BYTE_WRITTEN;
#endif
    }
    return byte_index;  // SIZE_MAX 表示状态 0，无需写入
}
```

**学习价值**：
- 灵活支持不同 Flash 硬件的写入特性
- 状态 0 优化：全 FF 状态无需写入，节省 Flash 写入次数

## 3. 宏定义简化代码（Macro Simplification）

### 3.1 错误处理宏
**位置**：`fdb_tsdb.c` 和 `fdb_kvdb.c`

**设计亮点**：
- 使用宏简化错误检查，减少重复代码
- 失败时直接 return，避免深层嵌套

**代码示例**：
```c
#define _FDB_WRITE_STATUS(db, addr, status_table, status_num, status_index, sync)    \
    do {                                                                       \
        result = _fdb_write_status((fdb_db_t)db, addr, status_table, status_num, status_index, sync);\
        if (result != FDB_NO_ERR) return result;                               \
    } while(0);

#define FLASH_WRITE(db, addr, buf, size, sync)                                 \
    do {                                                                       \
        result = _fdb_flash_write((fdb_db_t)db, addr, buf, size, sync);        \
        if (result != FDB_NO_ERR) return result;                               \
    } while(0);

// 使用方式
FLASH_WRITE(db, addr, buf, size, true);  // 失败自动 return
```

**学习价值**：
- 减少 `if` 判断，代码更简洁
- 统一的错误处理模式

### 3.2 对齐宏
**位置**：`fdb_low_lvl.h`

**设计亮点**：
- 提供对齐和对齐下取整宏
- 支持写入粒度对齐

**代码示例**：
```c
#define FDB_ALIGN(size, align)                    (((size) + (align) - 1) - (((size) + (align) -1) % (align)))
#define FDB_WG_ALIGN(size)                        (FDB_ALIGN(size, ((FDB_WRITE_GRAN + 7)/8)))
#define FDB_ALIGN_DOWN(size, align)               (((size) / (align)) * (align))
```

**学习价值**：
- 统一的对齐处理，避免硬编码

## 4. 迭代器模式（Iterator Pattern）

### 4.1 扇区迭代器
**位置**：`fdb_kvdb.c` 中的 `sector_iterator`

**设计亮点**：
- 使用回调函数实现迭代器模式
- 支持按状态过滤扇区
- 回调返回 `true` 时中断迭代

**代码示例**：
```c
static void sector_iterator(fdb_kvdb_t db, kv_sec_info_t sector, 
                           fdb_sector_store_status_t status, 
                           void *arg1, void *arg2,
                           bool (*callback)(kv_sec_info_t sector, void *arg1, void *arg2), 
                           bool traversal_kv)
{
    uint32_t sec_addr = db_oldest_addr(db);
    do {
        read_sector_info(db, sec_addr, sector, false);
        if (status == FDB_SECTOR_STORE_UNUSED || status == sector->status.store) {
            if (traversal_kv) {
                read_sector_info(db, sec_addr, sector, true);
            }
            // 回调返回 true 时中断迭代
            if (callback && callback(sector, arg1, arg2)) {
                return;
            }
        }
    } while ((sec_addr = get_next_sector_addr(db, sector, traversed_len)) != FAILED_ADDR);
}

// 使用示例
static bool alloc_kv_cb(kv_sec_info_t sector, void *arg1, void *arg2)
{
    struct alloc_kv_cb_args *arg = arg1;
    if (sector->check_ok && sector->remain > arg->kv_size) {
        *(arg->empty_kv) = sector->empty_kv;
        return true;  // 找到合适的扇区，中断迭代
    }
    return false;  // 继续迭代
}

// 调用
sector_iterator(db, sector, FDB_SECTOR_STORE_USING, &arg, NULL, alloc_kv_cb, true);
```

**学习价值**：
- 代码复用：同一迭代器可用于不同场景（GC、分配、统计等）
- 灵活：通过回调函数实现不同的处理逻辑

## 5. GC 机制（Garbage Collection）

### 5.1 增量 GC
**位置**：`fdb_kvdb.c` 中的 `gc_collect_by_free_size`

**设计亮点**：
- 按需 GC：根据空闲扇区数量决定是否触发
- 增量执行：每次 GC 一个扇区，可中断
- 状态标记：使用 `FDB_SECTOR_DIRTY_GC` 标记正在 GC 的扇区

**代码示例**：
```c
static void gc_collect_by_free_size(fdb_kvdb_t db, size_t free_size)
{
    size_t empty_sec_num = 0;
    uint32_t empty_sec_addr = 0;
    
    // 统计空闲扇区数量
    sector_iterator(db, &sector, FDB_SECTOR_STORE_EMPTY, &empty_sec_num, &empty_sec_addr, gc_check_cb, false);
    
    // 空闲扇区不足时触发 GC
    if (empty_sec_num <= FDB_GC_EMPTY_SEC_THRESHOLD) {
        struct gc_cb_args arg = { db, free_size, empty_sec_addr };
        sector_iterator(db, &sector, FDB_SECTOR_STORE_UNUSED, &arg, NULL, do_gc, false);
    }
}
```

**学习价值**：
- 避免一次性 GC 造成的延迟
- 可中断设计，适合实时系统

## 6. CRC32 校验（查表法）

### 6.1 查表法实现
**位置**：`fdb_utils.c` 中的 `fdb_calc_crc32`

**设计亮点**：
- 使用预计算表，提升计算速度
- 适合嵌入式场景（速度 vs 代码大小权衡）

**代码示例**：
```c
static const uint32_t crc32_table[256] = { /* 预计算表 */ };

uint32_t fdb_calc_crc32(uint32_t crc, const void *buf, size_t size)
{
    const uint8_t *p = (const uint8_t *)buf;
    crc = crc ^ ~0U;
    while (size--) {
        crc = crc32_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ ~0U;
}
```

**学习价值**：
- 查表法比逐位计算快得多
- 适合对性能有要求的场景

## 7. 结构体设计（Data Structure Design）

### 7.1 扇区信息结构
**位置**：`fdb_def.h` 中的 `struct kvdb_sec_info`

**设计亮点**：
- 分离状态和角色：`store` 状态和 `dirty` 状态分开管理
- 缓存友好：常用信息放在一起
- 对齐处理：考虑写入粒度对齐

**代码示例**：
```c
struct kvdb_sec_info {
    bool check_ok;                               // 扇区头校验是否通过
    struct {
        fdb_sector_store_status_t store;         // 存储状态（EMPTY/USING/FULL）
        fdb_sector_dirty_status_t dirty;          // 脏标记（FALSE/TRUE/GC）
    } status;
    uint32_t addr;                               // 扇区地址
    uint32_t magic;                              // 魔术字
    uint32_t combined;                           // 合并的下一个扇区号
    size_t remain;                               // 剩余空间
    uint32_t empty_kv;                           // 下一个空 KV 地址
};
```

**学习价值**：
- 清晰的数据结构设计
- 状态分离：不同维度的状态分开管理

## 8. 地址查找优化（Address Finding）

### 8.1 Magic Word 查找
**位置**：`fdb_kvdb.c` 中的 `find_next_kv_addr`

**设计亮点**：
- 使用缓冲区批量读取，减少 Flash 读取次数
- 通过 Magic Word 快速定位记录

**代码示例**：
```c
static uint32_t find_next_kv_addr(fdb_kvdb_t db, uint32_t start, uint32_t end)
{
    uint8_t buf[32];  // 缓冲区
    uint32_t magic;
    
    // 批量读取，减少 Flash 访问
    for (; start < end && start + sizeof(buf) < end; start += (sizeof(buf) - sizeof(uint32_t))) {
        _fdb_flash_read((fdb_db_t)db, start, (uint32_t *) buf, sizeof(buf));
        // 在缓冲区中查找 Magic Word
        for (i = 0; i < sizeof(buf) - sizeof(uint32_t); i++) {
            magic = /* 从 buf[i] 提取 32bit magic */;
            if (magic == KV_MAGIC_WORD) {
                return start + i;
            }
        }
    }
    return FAILED_ADDR;
}
```

**学习价值**：
- 批量读取减少 Flash 访问次数
- 适合顺序扫描场景

## 9. 文件模式支持（File Mode）

### 9.1 统一的接口抽象
**位置**：`fdb_utils.c` 中的 `_fdb_flash_read/write/erase`

**设计亮点**：
- 通过 `db->file_mode` 标志统一接口
- 支持 Flash 模式和文件模式两种实现
- 文件模式使用文件缓存，减少文件打开/关闭开销

**代码示例**：
```c
fdb_err_t _fdb_flash_read(fdb_db_t db, uint32_t addr, void *buf, size_t size)
{
    if (db->file_mode) {
#ifdef FDB_USING_FILE_MODE
        return _fdb_file_read(db, addr, buf, size);
#else
        return FDB_READ_ERR;
#endif
    } else {
#ifdef FDB_USING_FAL_MODE
        if (fal_partition_read(db->storage.part, addr, (uint8_t *) buf, size) < 0) {
            return FDB_READ_ERR;
        }
#endif
    }
    return FDB_NO_ERR;
}
```

**学习价值**：
- 接口抽象：上层代码无需关心底层实现
- 便于测试：文件模式便于单元测试

## 10. 错误处理策略

### 10.1 分层错误处理
**设计亮点**：
- 底层函数返回详细错误码
- 使用宏简化错误检查
- 关键路径才进行错误处理

**学习价值**：
- 平衡错误处理的详细程度和代码简洁性

## 总结

FlashDB 值得学习的主要点：

1. **性能优化**：缓存机制、批量读取、查表法 CRC
2. **代码组织**：宏简化、迭代器模式、接口抽象
3. **可靠性**：状态表设计、GC 机制、错误处理
4. **灵活性**：支持多种写入粒度、文件/Flash 双模式

这些设计在保持代码简洁的同时，兼顾了性能和可靠性，非常适合嵌入式场景。

