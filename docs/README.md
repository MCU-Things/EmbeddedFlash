## EmbeddedFlash 持久化组件

一个面向 单片机 的轻量级 KV 持久化组件，支持断电安全、垃圾回收（GC）、磨损均衡和自动恢复测试。

- **目标场景**：参数配置、校准数据、小体积状态数据等掉电保存
- **存储模型**：按「键值对 K‑V」组织，定长 + 变长数据混合
- **安全特性**：状态机 + 日志式写入，支持异常断电恢复
- **测试保障**：基于 Unity 的大量手动测试用例（`embedded_flash_manual_tests.*`）

---

## 目录结构

- `EmbeddedFlash.h / EmbeddedFlash.c`  
  对外主接口与内部实现。
- `EmbeddedFlash_def.h`  
  公共类型、枚举、结构体定义。
- `EmbeddedFlash_config.h`  
  所有可调参数与宏配置（Flash 布局、扇区数、日志等级等）。
- `EmbeddedFlash_port.h / EmbeddedFlash_port.c`  
  与芯片相关的 Flash 读写/擦除端口，支持 RAM 软件仿真。
- `EmbeddedFlash_log.h`  
  日志系统与编译期日志裁剪。
- `embedded_flash_manual_tests.*`  
  基于 Unity 的手动测试与压力测试。

---

## 快速使用

1. **配置 Flash 区域与参数**（位于 `EmbeddedFlash_config.h`）：

- `KV_SECTOR_START_ADDR`：KV 区起始地址  
- `KV_SECTOR_COUNT`：扇区数量（≥2）  
- `KV_MAX_VALUE_SIZE`：单条 KV 最大值长度  
- `EFLASH_LOG_LEVEL`：日志等级（默认 `WARN`）

2. **在你的工程中集成**

- 将 `APP/flash` 目录加入工程
- 保证 CMSIS / 标准外设库可用（用于 Flash 操作）

3. **初始化并使用**

```c
// 1) 准备默认 KV 表（放在只读区）
static kv_data_t g_kv_defaults[] = {
    // .key, .value, .value_length, .data_type, .data_source
    { .key = 0xA1, .value = &some_default_u8, .value_length = 1, .data_type = EFLASH_FORMAT_UINT8 },
    // ...
};

// 2) 初始化组件
embedded_flash_init(g_kv_defaults, sizeof(g_kv_defaults) / sizeof(g_kv_defaults[0]));

// 3) 读写接口示例
embedded_flash_set_uint8(0xA1, 0x10);
uint8_t v = 0;
uint8_t len = 1, type = 0;
embedded_flash_get(0xA1, &v, &len, &type);
```

更多接口见 `EmbeddedFlash.h`，包括：

- `embedded_flash_set_* / embedded_flash_get`
- `embedded_flash_delete`
- `embedded_flash_gc`

---

## 软件仿真与 0→1 严格校验

在 `EmbeddedFlash_port.c` 中，可以通过宏控制软件仿真与 0→1 写入校验：

- `EFLASH_SOFTWARE_SIMULATION`  
  - `0`：关闭软件仿真，直接使用芯片 Flash  
  - `1`：启用软件仿真，使用 RAM 模拟 KV 区

- `EFLASH_SIM_STRICT_0_TO_1_CHECK`  
  - `0`（默认）：只做 `current & write` 写入，行为与真实 Flash 一致，仅在遇到 0→1 企图时打印 **WARN** 日志  
  - `1`：检测到 0→1 时立即报错，返回 `EF_ERR_WRITE`，适合调试协议是否违反「只写 1→0」约束

推荐开源默认配置为：

```c
#define EFLASH_SOFTWARE_SIMULATION      1
#define EFLASH_SIM_STRICT_0_TO_1_CHECK  0
```

---

## 测试与验证

组件自带一套基于 Unity 的手动测试（`embedded_flash_manual_tests.c`）：

- 扇区头损坏恢复测试（包括随机破坏 magic / 状态 / 角色）
- GC 重建与断电恢复测试
- 压力读写与磨损分布观测

在你的工程中：

1. 定义宏 `EMBEDDED_FLASH_MANUAL_TESTS_ENABLE` 以启用测试入口。
2. 调用：

```c
int failed = embedded_flash_run_manual_tests();
```

测试过程中，可通过 `EFLASH_LOG_LEVEL` 控制日志详细程度。

---

## 许可证与致谢

你可以根据自己的计划在项目根目录添加 `LICENSE`，例如 MIT / Apache-2.0 等。  
如果二次开发或在其他项目中使用，欢迎在 README 中注明来源：EmbeddedFlash 持久化组件。


