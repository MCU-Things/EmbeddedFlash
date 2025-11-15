/**
 * @file embedded_flash_manual_tests.c
 * @brief 嵌入式Flash存储模块的手动测试用例
 * 
 * 这个文件包含了对EmbeddedFlash模块的手动编写测试用例
 * 使用Unity测试框架进行单元测试
 * 
 * @author 手动编写
 * @version 1.0
 * @date 2024
 */

#include "embedded_flash_manual_tests.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "stm32f10x.h"

/* ==================== Unity配置 ==================== */
// 启用Unity配置文件支持，Unity会自动包含 Unity/unity_config.h
// 所有Unity配置都在 unity_config.h 中统一管理
#define UNITY_INCLUDE_CONFIG_H

// 包含EmbeddedFlash相关头文件（在Unity之前包含，确保类型定义可用）
#include "EmbeddedFlash.h"
#include "EmbeddedFlash_def.h"
#include "EmbeddedFlash_config.h"
#include "EmbeddedFlash_port.h"

// 包含Unity测试框架
#include "Unity/unity.h"


/* ==================== 测试运行器 ==================== */
/**
 * @brief 启用/禁用测试失败时暂停功能
 * 设置为1启用：检测到测试失败时会暂停程序（进入无限循环）
 * 设置为0禁用：检测到测试失败时正常结束测试
 */
#ifndef ENABLE_TEST_FAIL_PAUSE
#define ENABLE_TEST_FAIL_PAUSE  1  // 默认启用暂停功能
#endif

/**
 * @brief 检查测试失败并提前停止的辅助宏
 * 如果检测到测试失败，立即结束测试并返回失败数量
 * 如果启用了暂停功能，会在失败时进入无限循环暂停程序
 * 注意：此宏只能在函数中使用，因为它包含return语句
 * 注意：此宏需要在RUN_TEST之后调用
 */
#define CHECK_FAIL_AND_STOP() \
    do { \
        /* 检查是否有测试失败：TestFailures不为0 或 CurrentTestFailed不为0 */ \
        if (Unity.TestFailures != 0 || Unity.CurrentTestFailed != 0) { \
            printf("Unity.TestFile: %s\r\n", (Unity.TestFile != NULL) ? Unity.TestFile : "(NULL)");\
            printf("Unity.CurrentTestName: %s\r\n", (Unity.CurrentTestName != NULL) ? Unity.CurrentTestName : "(NULL)");\
            printf("Unity.CurrentTestLineNumber: %u\r\n", (unsigned int)Unity.CurrentTestLineNumber);\
            printf("Unity.TestFailures: %u\r\n", (unsigned int)Unity.TestFailures);\
            printf("Unity.CurrentTestFailed: %u\r\n", (unsigned int)Unity.CurrentTestFailed);\
            printf("Unity.NumberOfTests: %u\r\n", (unsigned int)Unity.NumberOfTests);\
            printf("Unity.TestIgnores: %u\r\n", (unsigned int)Unity.TestIgnores);\
            printf("Unity.CurrentTestIgnored: %u\r\n", (unsigned int)Unity.CurrentTestIgnored);\
            int _result = UnityEnd(); \
            if (ENABLE_TEST_FAIL_PAUSE) { \
                printf("!!! Program paused due to test failure. Check debugger or reset device. !!!\r\n"); \
                while(1) { /* 暂停程序，方便调试 */ } \
            }else{ \
                return _result; \
            }\
        } \
    } while(0) 


// 简单的毫秒计时器实现，替代HAL_GetTick()
static volatile uint32_t ms_tick = 0;

// 初始化SysTick为1ms中断
void test_sysTick_init(void) {
    // 假设系统时钟为72MHz，配置SysTick为1ms中断
    SysTick_Config(SystemCoreClock / 1000); // 1ms中断
}

// 获取当前毫秒数
uint32_t test_SysTick_GetTick(void) {
    return ms_tick;
}

// SysTick中断处理函数（需要在中断向量表中配置）
void SysTick_Handler(void) {
    ms_tick++;
}

/* ==================== 测试键值宏定义 ==================== */
// 定义测试用的键值对ID - 命名规则: TEST_数据类型_长度_16进制键值_10进制键值
#define TEST_UINT8_1_A1_161   0xA1
#define TEST_UINT8_1_A2_162   0xA2
#define TEST_UINT16_2_A3_163  0xA3
#define TEST_INT32_4_A4_164   0xA4
#define TEST_STRING_8_A5_165  0xA5
#define TEST_HEX_4_A6_166     0xA6
#define TEST_UINT16_2_A7_167  0xA7
#define TEST_INT32_4_A8_168   0xA8
#define TEST_FLOAT_4_A9_169   0xA9
#define TEST_BOOL_1_AA_170    0xAA
#define TEST_INT8_1_AB_171    0xAB
#define TEST_INT16_2_AC_172   0xAC
#define TEST_UINT32_4_AD_173  0xAD
#define TEST_UINT64_8_AE_174  0xAE
#define TEST_INT64_8_AF_175   0xAF

/* ==================== 测试配置和辅助数据 ==================== */
// 测试用的默认键值对数据 - 覆盖所有支持的数据类型
// 注意：linter可能报类型错误，但实际编译时应正常工作（stdint.h已包含）
static uint8_t test_default_uint8_1 = 5;
static uint8_t test_default_uint8_2 = 10;
static int8_t test_default_int8_1 = -50;
static uint16_t test_default_uint16_1 = 500;
static uint16_t test_default_uint16_2 = 750;
static int16_t test_default_int16_1 = -1500;
static uint32_t test_default_uint32_1 = 1000000;
static int32_t test_default_int32_1 = -1000;
static int32_t test_default_int32_2 = -2000;
static uint64_t test_default_uint64_1 = 0x123456789ABCDEF0ULL;
static int64_t test_default_int64_1 = -0x123456789ABCDEF0LL;
static float test_default_float_1 = 3.14159f;
static bool test_default_bool_1 = true;
static char test_default_string_1[KV_MAX_VALUE_SIZE];
static uint8_t test_default_hex_1[KV_MAX_VALUE_SIZE];

// 测试用的键值对配置 - 覆盖所有数据类型
// 使用宏定义键值，提高代码可维护性
static kv_data_t test_kvs[] = {
    {0, TEST_UINT8_1_A1_161, (uint8_t*)&test_default_uint8_1, sizeof(test_default_uint8_1), EFLASH_FORMAT_UINT8, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_UINT8_1_A2_162, (uint8_t*)&test_default_uint8_2, sizeof(test_default_uint8_2), EFLASH_FORMAT_UINT8, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_UINT16_2_A3_163, (uint8_t*)&test_default_uint16_1, sizeof(test_default_uint16_1), EFLASH_FORMAT_UINT16, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_INT32_4_A4_164, (uint8_t*)&test_default_int32_1, sizeof(test_default_int32_1), EFLASH_FORMAT_INT32, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_STRING_8_A5_165, (uint8_t*)test_default_string_1, 7 + 1, EFLASH_FORMAT_STRING, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_HEX_4_A6_166, test_default_hex_1, sizeof(test_default_hex_1), EFLASH_FORMAT_HEX, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_UINT16_2_A7_167, (uint8_t*)&test_default_uint16_2, sizeof(test_default_uint16_2), EFLASH_FORMAT_UINT16, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_INT32_4_A8_168, (uint8_t*)&test_default_int32_2, sizeof(test_default_int32_2), EFLASH_FORMAT_INT32, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_FLOAT_4_A9_169, (uint8_t*)&test_default_float_1, sizeof(test_default_float_1), EFLASH_FORMAT_FLOAT, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_BOOL_1_AA_170, (uint8_t*)&test_default_bool_1, sizeof(test_default_bool_1), EFLASH_FORMAT_BOOL, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_INT8_1_AB_171, (uint8_t*)&test_default_int8_1, sizeof(test_default_int8_1), EFLASH_FORMAT_INT8, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_INT16_2_AC_172, (uint8_t*)&test_default_int16_1, sizeof(test_default_int16_1), EFLASH_FORMAT_INT16, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_UINT32_4_AD_173, (uint8_t*)&test_default_uint32_1, sizeof(test_default_uint32_1), EFLASH_FORMAT_UINT32, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_UINT64_8_AE_174, (uint8_t*)&test_default_uint64_1, sizeof(test_default_uint64_1), EFLASH_FORMAT_UINT64, KV_DATA_SOURCE_DEFAULT},
    {0, TEST_INT64_8_AF_175, (uint8_t*)&test_default_int64_1, sizeof(test_default_int64_1), EFLASH_FORMAT_INT64, KV_DATA_SOURCE_DEFAULT},
};

/* ==================== Unity测试框架回调函数 ==================== */
/**
 * @brief 每个测试用例运行前的设置函数
 * 在每个测试用例执行前调用，用于初始化测试环境
 */
void setUp(void) {
    // 可以在这里添加每个测试用例运行前的初始化代码
    // 例如：清理Flash、重置状态等
}

/**
 * @brief 每个测试用例运行后的清理函数
 * 在每个测试用例执行后调用，用于清理测试环境
 */
void tearDown(void) {
    // 可以在这里添加每个测试用例运行后的清理代码
    // 例如：释放资源、重置变量等
}

/**
 * @brief 测试套件开始前的设置函数
 * 在整个测试套件运行前调用一次
 */
 /* ==================== 辅助函数 ==================== */
/**
 * @brief 初始化测试用的Flash
 */
static EF_ErrCode test_embedded_flash_init_helper(void) {
    printf("test_embedded_flash_init_helper\n");
    uint8_t count = sizeof(test_kvs) / sizeof(kv_data_t);
    return embedded_flash_init(test_kvs, count);
}
void suiteSetUp(void) {
    // 测试套件开始前的初始化
    // 例如：初始化Flash端口、擦除测试区域等
    printf("\n========== Unity Test Suite Setup ==========\n");
    
    // 擦除Flash测试区域
    EF_ErrCode err = flash_port_erase(KV_SECTOR_START_ADDR, KV_SECTOR_SIZE * KV_SECTOR_COUNT);
    if (err != EF_OK) {
        printf("Warning: Flash erase failed with error code: %d\n", err);
    }
    
    // 初始化EmbeddedFlash模块
    err = test_embedded_flash_init_helper();
    if (err != EF_OK) {
        printf("Warning: EmbeddedFlash init failed with error code: %d\n", err);
    }
    
    printf("Test suite setup complete.\n");
}

/**
 * @brief 测试套件结束后的清理函数
 * @param num_failures 测试失败的用例数量
 * @return 返回退出码，0表示成功，非0表示失败
 */
int suiteTearDown(int num_failures) {
    printf("\n========== Unity Test Suite Teardown ==========\n");
    printf("Total test failures: %d\n", num_failures);
    printf("Test suite teardown complete.\n");
    
    // 返回失败数量作为退出码
    return num_failures;
}



/* ==================== 测试用例函数 ==================== */
/**
 * @brief 测试用例：初始化测试
 * 测试EmbeddedFlash模块的初始化功能
 */
void test_embedded_flash_init(void) {
    EF_ErrCode err = test_embedded_flash_init_helper();
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
}

/**
 * @brief 测试用例：数据类型大小获取测试
 * 测试获取所有数据类型的大小
 */
void test_embedded_flash_get_type_size(void) {
    // 先测试BOOL类型，添加调试信息
    uint8_t bool_size = embedded_flash_get_type_size(EFLASH_FORMAT_BOOL);
    UnityPrint("DEBUG: EFLASH_FORMAT_BOOL enum value = ");
    UnityPrintNumber(EFLASH_FORMAT_BOOL);
    UnityPrint(", returned size = ");
    UnityPrintNumber(bool_size);
    UnityPrint("\n");
    UNITY_OUTPUT_FLUSH();
    
    TEST_ASSERT_EQUAL_UINT8(1, bool_size);
    TEST_ASSERT_EQUAL_UINT8(1, embedded_flash_get_type_size(EFLASH_FORMAT_UINT8));
    TEST_ASSERT_EQUAL_UINT8(1, embedded_flash_get_type_size(EFLASH_FORMAT_INT8));
    TEST_ASSERT_EQUAL_UINT8(2, embedded_flash_get_type_size(EFLASH_FORMAT_UINT16));
    TEST_ASSERT_EQUAL_UINT8(2, embedded_flash_get_type_size(EFLASH_FORMAT_INT16));
    TEST_ASSERT_EQUAL_UINT8(4, embedded_flash_get_type_size(EFLASH_FORMAT_UINT32));
    TEST_ASSERT_EQUAL_UINT8(4, embedded_flash_get_type_size(EFLASH_FORMAT_INT32));
    TEST_ASSERT_EQUAL_UINT8(8, embedded_flash_get_type_size(EFLASH_FORMAT_UINT64));
    TEST_ASSERT_EQUAL_UINT8(8, embedded_flash_get_type_size(EFLASH_FORMAT_INT64));
    TEST_ASSERT_EQUAL_UINT8(4, embedded_flash_get_type_size(EFLASH_FORMAT_FLOAT));
    // 可变长度类型应返回0
    TEST_ASSERT_EQUAL_UINT8(0, embedded_flash_get_type_size(EFLASH_FORMAT_STRING));
    TEST_ASSERT_EQUAL_UINT8(0, embedded_flash_get_type_size(EFLASH_FORMAT_HEX));
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：UINT8类型读写测试
 */
void test_embedded_flash_uint8_read_write(void) {
    uint8_t key = TEST_UINT8_1_A1_161;
    uint8_t write_value = 42;
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    EF_ErrCode err = embedded_flash_set_uint8(key, write_value);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(EFLASH_FORMAT_UINT8, data_type);
    TEST_ASSERT_EQUAL_UINT8(1, len);
    TEST_ASSERT_EQUAL_UINT8(write_value, buf[0]);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：INT8类型读写测试
 */
void test_embedded_flash_int8_read_write(void) {
    uint8_t key = TEST_INT8_1_AB_171;
    int8_t write_value = -120;
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    EF_ErrCode err = embedded_flash_set_int8(key, write_value);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(EFLASH_FORMAT_INT8, data_type);
    TEST_ASSERT_EQUAL_UINT8(1, len);
    TEST_ASSERT_EQUAL_INT8(write_value, *(int8_t*)buf);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：UINT16类型读写测试
 */
void test_embedded_flash_uint16_read_write(void) {
    uint8_t key = TEST_UINT16_2_A3_163;
    uint16_t write_value = 1234;
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    EF_ErrCode err = embedded_flash_set_uint16(key, write_value);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(EFLASH_FORMAT_UINT16, data_type);
    TEST_ASSERT_EQUAL_UINT8(2, len);
    TEST_ASSERT_EQUAL_UINT16(write_value, *(uint16_t*)buf);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：INT16类型读写测试
 */
void test_embedded_flash_int16_read_write(void) {
    uint8_t key = TEST_INT16_2_AC_172;
    int16_t write_value = -1234;
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    EF_ErrCode err = embedded_flash_set_int16(key, write_value);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(EFLASH_FORMAT_INT16, data_type);
    TEST_ASSERT_EQUAL_UINT8(2, len);
    TEST_ASSERT_EQUAL_INT16(write_value, *(int16_t*)buf);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：UINT32类型读写测试
 */
void test_embedded_flash_uint32_read_write(void) {
    uint8_t key = TEST_UINT32_4_AD_173;
    uint32_t write_value = 1234000;
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    EF_ErrCode err = embedded_flash_set_uint32(key, write_value);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(EFLASH_FORMAT_UINT32, data_type);
    TEST_ASSERT_EQUAL_UINT8(4, len);
    TEST_ASSERT_EQUAL_UINT32(write_value, *(uint32_t*)buf);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：INT32类型读写测试
 */
void test_embedded_flash_int32_read_write(void) {
    uint8_t key = TEST_INT32_4_A4_164;
    int32_t write_value = -1234000;
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    EF_ErrCode err = embedded_flash_set_int32(key, write_value);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(EFLASH_FORMAT_INT32, data_type);
    TEST_ASSERT_EQUAL_UINT8(4, len);
    TEST_ASSERT_EQUAL_INT32(write_value, *(int32_t*)buf);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：UINT64类型读写测试
 */
void test_embedded_flash_uint64_read_write(void) {
    uint8_t key = TEST_UINT64_8_AE_174;
    uint64_t write_value = 1234003;
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    EF_ErrCode err = embedded_flash_set_uint64(key, write_value);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(EFLASH_FORMAT_UINT64, data_type);
    TEST_ASSERT_EQUAL_UINT8(8, len);
    TEST_ASSERT_EQUAL_UINT64(write_value, *(uint64_t*)buf);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：INT64类型读写测试
 */
void test_embedded_flash_int64_read_write(void) {
    uint8_t key = TEST_INT64_8_AF_175;
    int64_t write_value = -1234001;
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    EF_ErrCode err = embedded_flash_set_int64(key, write_value);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(EFLASH_FORMAT_INT64, data_type);
    TEST_ASSERT_EQUAL_UINT8(8, len);
    TEST_ASSERT_EQUAL_INT64(write_value, *(int64_t*)buf);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：FLOAT类型读写测试
 */
void test_embedded_flash_float_read_write(void) {
    uint8_t key = TEST_FLOAT_4_A9_169;
    float write_value = 3.14159f;
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    EF_ErrCode err = embedded_flash_set_float(key, write_value);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(EFLASH_FORMAT_FLOAT, data_type);
    TEST_ASSERT_EQUAL_UINT8(4, len);
    TEST_ASSERT_EQUAL_FLOAT(write_value, *(float*)buf);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：BOOL类型读写测试
 */
void test_embedded_flash_bool_read_write(void) {
    uint8_t key = TEST_BOOL_1_AA_170;
    bool write_value = true;
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    EF_ErrCode err = embedded_flash_set_bool(key, write_value);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(EFLASH_FORMAT_BOOL, data_type);
    TEST_ASSERT_EQUAL_UINT8(1, len);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)write_value, buf[0]);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：STRING类型读写测试
 */
void test_embedded_flash_string_read_write(void) {
    uint8_t key = TEST_STRING_8_A5_165;
    const char* write_value = "Hello";
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    EF_ErrCode err = embedded_flash_set_string(key, write_value);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(EFLASH_FORMAT_STRING, data_type);
    TEST_ASSERT_EQUAL_UINT8(strlen(write_value) + 1, len);  // 包含null终止符
    TEST_ASSERT_EQUAL_STRING(write_value, (char*)buf);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：HEX类型读写测试
 */
void test_embedded_flash_hex_read_write(void) {
    uint8_t key = TEST_HEX_4_A6_166;
    uint8_t write_value[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    EF_ErrCode err = embedded_flash_set_hex(key, write_value, sizeof(write_value));
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(EFLASH_FORMAT_HEX, data_type);
    TEST_ASSERT_EQUAL_UINT8(sizeof(write_value), len);
    TEST_ASSERT_EQUAL_MEMORY(write_value, buf, sizeof(write_value));
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：边界值测试 - 最大值
 */
void test_embedded_flash_boundary_max_values(void) {
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    // 测试UINT8最大值
    EF_ErrCode err = embedded_flash_set_uint8(TEST_UINT8_1_A1_161, 0xFF);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    err = embedded_flash_get(TEST_UINT8_1_A1_161, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(0xFF, buf[0]);
    
    // 测试UINT16最大值
    err = embedded_flash_set_uint16(TEST_UINT16_2_A3_163, 0xFFFF);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    err = embedded_flash_get(TEST_UINT16_2_A3_163, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, *(uint16_t*)buf);
    
    // 测试UINT32最大值
    err = embedded_flash_set_uint32(TEST_UINT32_4_AD_173, 0xFFFFFFFF);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    err = embedded_flash_get(TEST_UINT32_4_AD_173, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFF, *(uint32_t*)buf);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：边界值测试 - 最小值
 */
void test_embedded_flash_boundary_min_values(void) {
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    // 测试INT8最小值
    EF_ErrCode err = embedded_flash_set_int8(TEST_INT8_1_AB_171, -128);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    err = embedded_flash_get(TEST_INT8_1_AB_171, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_INT8(-128, *(int8_t*)buf);
    
    // 测试INT16最小值
    err = embedded_flash_set_int16(TEST_INT16_2_AC_172, -32768);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    err = embedded_flash_get(TEST_INT16_2_AC_172, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_INT16(-32768, *(int16_t*)buf);
    
    // 测试INT32最小值
    err = embedded_flash_set_int32(TEST_INT32_4_A4_164, -2147483648);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    err = embedded_flash_get(TEST_INT32_4_A4_164, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_INT32(-2147483648, *(int32_t*)buf);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：错误处理测试 - 无效键
 */
void test_embedded_flash_error_invalid_key(void) {
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    // 测试读取不存在的键
    EF_ErrCode err = embedded_flash_get(0xFF, buf, &len, &data_type);
    TEST_ASSERT_NOT_EQUAL_INT(EF_OK, err);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：错误处理测试 - NULL指针
 */
void test_embedded_flash_error_null_pointer(void) {
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    // 测试NULL缓冲区 - 应该返回EF_ERR_PARAM
    EF_ErrCode err = embedded_flash_get(TEST_UINT8_1_A1_161, NULL, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_ERR_PARAM, err);
    
    // 测试NULL字符串 - 应该返回EF_ERR_PARAM
    err = embedded_flash_set_string(TEST_STRING_8_A5_165, NULL);
    TEST_ASSERT_EQUAL_INT(EF_ERR_PARAM, err);
    
    // 测试NULL HEX数据 - 应该返回EF_ERR_PARAM
    err = embedded_flash_set_hex(TEST_HEX_4_A6_166, NULL, 5);
    TEST_ASSERT_EQUAL_INT(EF_ERR_PARAM, err);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：错误处理测试 - 超长数据
 */
void test_embedded_flash_error_oversize_data(void) {
    // 测试超长字符串
    char long_string[20] = "This is too long";
    EF_ErrCode err = embedded_flash_set_string(TEST_STRING_8_A5_165, long_string);
    TEST_ASSERT_EQUAL_INT(EF_ERR_SIZE_TOO_LONG, err);
    
    // 测试超长HEX数据
    uint8_t long_data[20] = {0};
    err = embedded_flash_set_hex(TEST_HEX_4_A6_166, long_data, 20);
    TEST_ASSERT_NOT_EQUAL_INT(EF_OK, err);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：错误处理测试 - 零长度数据
 */
void test_embedded_flash_error_zero_length(void) {
    uint8_t data[5] = {1, 2, 3, 4, 5};
    // 测试零长度HEX数据
    EF_ErrCode err = embedded_flash_set_hex(TEST_HEX_4_A6_166, data, 0);
    TEST_ASSERT_NOT_EQUAL_INT(EF_OK, err);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：批量读写测试
 */
void test_embedded_flash_batch_read_write(void) {
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    // 批量写入多种数据类型
    EF_ErrCode err;
    err = embedded_flash_set_uint8(TEST_UINT8_1_A1_161, 10);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_set_uint16(TEST_UINT16_2_A3_163, 1000);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_set_int32(TEST_INT32_4_A4_164, -5000);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_set_string(TEST_STRING_8_A5_165, "TestStr");
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    uint8_t hex_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    err = embedded_flash_set_hex(TEST_HEX_4_A6_166, hex_data, sizeof(hex_data));
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    // 验证所有数据
    err = embedded_flash_get(TEST_UINT8_1_A1_161, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(10, buf[0]);
    
    err = embedded_flash_get(TEST_UINT16_2_A3_163, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT16(1000, *(uint16_t*)buf);
    
    err = embedded_flash_get(TEST_INT32_4_A4_164, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_INT32(-5000, *(int32_t*)buf);
    
    err = embedded_flash_get(TEST_STRING_8_A5_165, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_STRING("TestStr", (char*)buf);
    
    err = embedded_flash_get(TEST_HEX_4_A6_166, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_MEMORY(hex_data, buf, sizeof(hex_data));
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：数据覆盖测试
 */
void test_embedded_flash_data_overwrite(void) {
    uint8_t key = TEST_UINT8_1_A1_161;
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    // 第一次写入
    EF_ErrCode err = embedded_flash_set_uint8(key, 100);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(100, buf[0]);
    
    // 第二次覆盖写入
    err = embedded_flash_set_uint8(key, 200);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(200, buf[0]);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：最大长度数据测试
 */
void test_embedded_flash_max_length_data(void) {
    uint8_t key = TEST_HEX_4_A6_166;
    uint8_t max_data[KV_MAX_VALUE_SIZE];
    uint8_t buf[KV_MAX_VALUE_SIZE];
    uint8_t len = 0;
    uint8_t data_type = 0;
    
    // 填充最大长度数据
    for (int i = 0; i < KV_MAX_VALUE_SIZE; i++) {
        max_data[i] = (uint8_t)(0xAA + i);
    }
    
    EF_ErrCode err = embedded_flash_set_hex(key, max_data, KV_MAX_VALUE_SIZE);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    
    err = embedded_flash_get(key, buf, &len, &data_type);
    TEST_ASSERT_EQUAL_INT(EF_OK, err);
    TEST_ASSERT_EQUAL_UINT8(KV_MAX_VALUE_SIZE, len);
    TEST_ASSERT_EQUAL_MEMORY(max_data, buf, KV_MAX_VALUE_SIZE);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
}

/**
 * @brief 测试用例：压力测试 - 大量写入操作
 * 
 * 测试高强度的读写操作，验证系统在压力下的稳定性
 * 
 * 测试内容：
 * - 进行100次连续的读写操作
 * - 覆盖所有12种数据类型（bool, uint8, int8, uint16, int16, uint32, int32, uint64, int64, float, string, hex）
 * - 每次写入后立即读取验证
 * - 验证数据类型、数据长度、数据值的正确性
 * - 统计各种失败情况（写入失败、读取失败、类型不匹配、长度不匹配、数据验证失败）
 * - 要求所有100次操作都必须成功
 * 
 * 参考：embedded_flash_demo_tests.c 中的 embedded_flash_demo_stress_test()
 */
void test_embedded_flash_stress_test(void) {
    // 定义压力测试参数
    #define STRESS_TEST_OPERATIONS 2000  // 压力测试操作次数
    // 直接使用 test_kvs 做轮询，去除中间 test_keys 数组，进一步降低耦合与复杂度
    uint8_t num_test_keys = (uint8_t)(sizeof(test_kvs) / sizeof(test_kvs[0]));
    if (num_test_keys == 0) {
        TEST_FAIL_MESSAGE("Stress test: test_kvs is empty");
        return;
    }
    
    // 统计数据
    int success_count = 0;
    int write_fail_count = 0;
    int read_fail_count = 0;
    int type_fail_count = 0;
    int len_fail_count = 0;
    int verify_fail_count = 0;
    
    // 数据缓冲区
    union {
        bool bool_val;
        uint8_t uint8_val;
        int8_t int8_val;
        uint16_t uint16_val;
        int16_t int16_val;
        uint32_t uint32_val;
        int32_t int32_val;
        uint64_t uint64_val;
        int64_t int64_val;
        float float_val;
        char string_val[KV_MAX_VALUE_SIZE];
        uint8_t hex_val[KV_MAX_VALUE_SIZE];
        uint8_t raw[KV_MAX_VALUE_SIZE];
    } write_data, read_data;
    
    // 压力测试循环
    for (int i = 0; i < STRESS_TEST_OPERATIONS; i++) {
        // 循环选择键
        uint8_t key_idx = i % num_test_keys;
        uint8_t key = test_kvs[key_idx].key;
        uint8_t data_type = test_kvs[key_idx].data_type;
        
        // 清空缓冲区
        memset(&write_data, 0, sizeof(write_data));
        memset(&read_data, 0, sizeof(read_data));
        
        // 根据数据类型生成测试数据并写入
        EF_ErrCode err = EF_ERR;
        uint8_t expected_len = 0;
        
        switch (data_type) {
            case EFLASH_FORMAT_BOOL:
                write_data.bool_val = (i % 2) ? true : false;
                err = embedded_flash_set_bool(key, write_data.bool_val);
                expected_len = 1;
                break;
                
            case EFLASH_FORMAT_UINT8:
                write_data.uint8_val = (uint8_t)(i & 0xFF);
                err = embedded_flash_set_uint8(key, write_data.uint8_val);
                expected_len = 1;
                break;
                
            case EFLASH_FORMAT_INT8:
                write_data.int8_val = (int8_t)((i % 256) - 128);
                err = embedded_flash_set_int8(key, write_data.int8_val);
                expected_len = 1;
                break;
                
            case EFLASH_FORMAT_UINT16:
                write_data.uint16_val = (uint16_t)(i & 0xFFFF);
                err = embedded_flash_set_uint16(key, write_data.uint16_val);
                expected_len = 2;
                break;
                
            case EFLASH_FORMAT_INT16:
                write_data.int16_val = (int16_t)((i % 65536) - 32768);
                err = embedded_flash_set_int16(key, write_data.int16_val);
                expected_len = 2;
                break;
                
            case EFLASH_FORMAT_UINT32:
                write_data.uint32_val = (uint32_t)(i * 1000);
                err = embedded_flash_set_uint32(key, write_data.uint32_val);
                expected_len = 4;
                break;
                
            case EFLASH_FORMAT_INT32:
                write_data.int32_val = (int32_t)(i * 100 - 50000);
                err = embedded_flash_set_int32(key, write_data.int32_val);
                expected_len = 4;
                break;
                
            case EFLASH_FORMAT_UINT64:
                write_data.uint64_val = (uint64_t)i * 1000000ULL;
                err = embedded_flash_set_uint64(key, write_data.uint64_val);
                expected_len = 8;
                break;
                
            case EFLASH_FORMAT_INT64:
                write_data.int64_val = (int64_t)i * 1000000LL - 500000000LL;
                err = embedded_flash_set_int64(key, write_data.int64_val);
                expected_len = 8;
                break;
                
            case EFLASH_FORMAT_FLOAT:
                write_data.float_val = (float)(i * 0.123f + 3.14159f);
                err = embedded_flash_set_float(key, write_data.float_val);
                expected_len = 4;
                break;
                
            case EFLASH_FORMAT_STRING: {
                // 生成变化的字符串，确保不超过KV_MAX_VALUE_SIZE-1字节
                // 使用简单的字符串生成方法，避免依赖snprintf
                uint16_t val = i % 1000;
                if (val < 10) {
                    write_data.string_val[0] = 'T';
                    write_data.string_val[1] = '0' + val;
                    write_data.string_val[2] = '\0';
                } else if (val < 100) {
                    write_data.string_val[0] = 'T';
                    write_data.string_val[1] = '0' + (val / 10);
                    write_data.string_val[2] = '0' + (val % 10);
                    write_data.string_val[3] = '\0';
                } else {
                    write_data.string_val[0] = 'T';
                    write_data.string_val[1] = '0' + (val / 100);
                    write_data.string_val[2] = '0' + ((val / 10) % 10);
                    write_data.string_val[3] = '0' + (val % 10);
                    write_data.string_val[4] = '\0';
                }
                err = embedded_flash_set_string(key, write_data.string_val);
                expected_len = strlen(write_data.string_val) + 1;
                break;
            }
                
            case EFLASH_FORMAT_HEX: {
                // 生成4字节的HEX数据
                write_data.hex_val[0] = (uint8_t)(i & 0xFF);
                write_data.hex_val[1] = (uint8_t)((i >> 8) & 0xFF);
                write_data.hex_val[2] = (uint8_t)((i >> 16) & 0xFF);
                write_data.hex_val[3] = (uint8_t)((i >> 24) & 0xFF);
                err = embedded_flash_set_hex(key, write_data.hex_val, 4);
                expected_len = 4;
                break;
            }
                
            default:
                TEST_FAIL_MESSAGE("Unsupported data type in stress test");
                return;
        }
        
        // 检查写入结果
        if (err != EF_OK) {
            write_fail_count++;
            continue;
        }
        
        // 立即读取验证
        uint8_t read_len = 0;
        uint8_t read_type = 0;
        err = embedded_flash_get(key, read_data.raw, &read_len, &read_type);
        
        if (err != EF_OK) {
            read_fail_count++;
            continue;
        }
        
        // 验证类型
        if (read_type != data_type) {
            type_fail_count++;
            continue;
        }
        
        // 验证长度
        if (read_len != expected_len) {
            len_fail_count++;
            continue;
        }
        
        // 验证数据值
        int data_match = 0;
        switch (data_type) {
            case EFLASH_FORMAT_BOOL:
                data_match = (read_data.bool_val == write_data.bool_val);
                break;
            case EFLASH_FORMAT_UINT8:
                data_match = (read_data.uint8_val == write_data.uint8_val);
                break;
            case EFLASH_FORMAT_INT8:
                data_match = (read_data.int8_val == write_data.int8_val);
                break;
            case EFLASH_FORMAT_UINT16:
                data_match = (read_data.uint16_val == write_data.uint16_val);
                break;
            case EFLASH_FORMAT_INT16:
                data_match = (read_data.int16_val == write_data.int16_val);
                break;
            case EFLASH_FORMAT_UINT32:
                data_match = (read_data.uint32_val == write_data.uint32_val);
                break;
            case EFLASH_FORMAT_INT32:
                data_match = (read_data.int32_val == write_data.int32_val);
                break;
            case EFLASH_FORMAT_UINT64:
                data_match = (read_data.uint64_val == write_data.uint64_val);
                break;
            case EFLASH_FORMAT_INT64:
                data_match = (read_data.int64_val == write_data.int64_val);
                break;
            case EFLASH_FORMAT_FLOAT:
                // 浮点数比较，允许小的误差
                data_match = (fabsf(write_data.float_val - read_data.float_val) < 0.0001f);
                break;
            case EFLASH_FORMAT_STRING:
                data_match = (strcmp(read_data.string_val, write_data.string_val) == 0);
                break;
            case EFLASH_FORMAT_HEX:
                data_match = (memcmp(read_data.hex_val, write_data.hex_val, expected_len) == 0);
                break;
            default:
                data_match = 0;
                break;
        }
        
        if (data_match) {
            success_count++;
        } else {
            verify_fail_count++;
            // 输出详细的错误信息
            printf("Stress test data mismatch at iteration %d, key=0x%02X, type=%d\n", i, key, data_type);
            printf("  Expected: ");
            switch (data_type) {
                case EFLASH_FORMAT_BOOL: printf("%s", write_data.bool_val ? "true" : "false"); break;
                case EFLASH_FORMAT_UINT8: printf("%u", write_data.uint8_val); break;
                case EFLASH_FORMAT_INT8: printf("%d", write_data.int8_val); break;
                case EFLASH_FORMAT_UINT16: printf("%u", write_data.uint16_val); break;
                case EFLASH_FORMAT_INT16: printf("%d", write_data.int16_val); break;
                case EFLASH_FORMAT_UINT32: printf("%d", write_data.uint32_val); break;
                case EFLASH_FORMAT_INT32: printf("%d", write_data.int32_val); break;
                case EFLASH_FORMAT_UINT64: printf("%llu", write_data.uint64_val); break;
                case EFLASH_FORMAT_INT64: printf("%lld", write_data.int64_val); break;
                case EFLASH_FORMAT_FLOAT: printf("%f", write_data.float_val); break;
                case EFLASH_FORMAT_STRING: printf("%s", write_data.string_val); break;
                case EFLASH_FORMAT_HEX: 
                    for (int j = 0; j < expected_len; j++) printf("%02X ", write_data.hex_val[j]); 
                    break;
            }
            printf("\n  Actual: ");
            switch (data_type) {
                case EFLASH_FORMAT_BOOL: printf("%s", read_data.bool_val ? "true" : "false"); break;
                case EFLASH_FORMAT_UINT8: printf("%u", read_data.uint8_val); break;
                case EFLASH_FORMAT_INT8: printf("%d", read_data.int8_val); break;
                case EFLASH_FORMAT_UINT16: printf("%u", read_data.uint16_val); break;
                case EFLASH_FORMAT_INT16: printf("%d", read_data.int16_val); break;
                case EFLASH_FORMAT_UINT32: printf("%d", read_data.uint32_val); break;
                case EFLASH_FORMAT_INT32: printf("%d", read_data.int32_val); break;
                case EFLASH_FORMAT_UINT64: printf("%llu", read_data.uint64_val); break;
                case EFLASH_FORMAT_INT64: printf("%lld", read_data.int64_val); break;
                case EFLASH_FORMAT_FLOAT: printf("%f", read_data.float_val); break;
                case EFLASH_FORMAT_STRING: printf("%s", read_data.string_val); break;
                case EFLASH_FORMAT_HEX: 
                    for (int j = 0; j < expected_len; j++) printf("%02X ", read_data.hex_val[j]); 
                    break;
            }
            printf("\n");
        }
    }
    
    // 验证压力测试结果
    // 压力测试要求所有操作都成功（与demo_tests.c保持一致）
    // 按优先级验证：先验证具体的失败类型，最后验证总体成功次数
    
    // 验证没有写入失败（最严重的错误）
    if (write_fail_count > 0) {
        TEST_FAIL_MESSAGE("Stress test: Write operations failed");
        return;
    }
    
    // 验证没有读取失败
    if (read_fail_count > 0) {
        TEST_FAIL_MESSAGE("Stress test: Read operations failed");
        return;
    }
    
    // 验证没有类型不匹配
    if (type_fail_count > 0) {
        TEST_FAIL_MESSAGE("Stress test: Data type mismatches detected");
        return;
    }
    
    // 验证没有长度不匹配
    if (len_fail_count > 0) {
        TEST_FAIL_MESSAGE("Stress test: Data length mismatches detected");
        return;
    }
    
    // 验证没有数据验证失败
    if (verify_fail_count > 0) {
        TEST_FAIL_MESSAGE("Stress test: Data verification failed");
        return;
    }
    
    // 最后验证总体成功次数（应该等于总操作数）
    TEST_ASSERT_EQUAL_INT(STRESS_TEST_OPERATIONS, success_count);
    //CHECK_FAIL_AND_STOP(); // 检查失败并停止
    // 如果所有断言都通过，测试通过
    // Unity测试框架中，如果函数正常返回且所有断言通过，则测试通过
}
/**
 * @brief 错误显示验证测试
 * 专门用于验证Unity错误位置显示功能的测试
 */
void test_error_display_verification_1(void) {
	printf("test_error_display_verification_1\r\n");
    // 这个测试会故意失败，用于验证错误位置显示
    TEST_ASSERT_EQUAL_INT(2, 0);  // 这个断言会失败
}
void test_error_display_verification_2(void) {
	printf("test_error_display_verification_2\r\n");
    // 这个测试会故意失败，用于验证错误位置显示
    TEST_ASSERT_EQUAL_INT8(1, 0);  // 这个断言会失败
}
/* ==================== 测试用例分组 ==================== */
/**
 * @brief 初始化相关测试组
 */
int test_group_init(void) {
    RUN_TEST(test_embedded_flash_init);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_get_type_size);
    CHECK_FAIL_AND_STOP();
    return 0;
}

/**
 * @brief 基本数据类型读写测试组
 */
int test_group_basic_types(void) {
    RUN_TEST(test_embedded_flash_uint8_read_write);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_int8_read_write);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_uint16_read_write);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_int16_read_write);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_uint32_read_write);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_int32_read_write);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_uint64_read_write);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_int64_read_write);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_float_read_write);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_bool_read_write);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_string_read_write);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_hex_read_write);
    CHECK_FAIL_AND_STOP();
    return 0;
}

/**
 * @brief 边界值测试组
 */
int test_group_boundary(void) {
    RUN_TEST(test_embedded_flash_boundary_max_values);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_boundary_min_values);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_max_length_data);
    CHECK_FAIL_AND_STOP();
    return 0;
}

/**
 * @brief 错误处理测试组
 */
int test_group_error_handling(void) {
    RUN_TEST(test_embedded_flash_error_invalid_key);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_error_null_pointer);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_error_oversize_data);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_error_zero_length);
    CHECK_FAIL_AND_STOP();
    return 0;
    
}

/**
 * @brief 高级功能测试组
 */
int test_group_advanced(void) {
    RUN_TEST(test_embedded_flash_batch_read_write);
    CHECK_FAIL_AND_STOP();
    RUN_TEST(test_embedded_flash_data_overwrite);
    CHECK_FAIL_AND_STOP();
    return 0;
}

/**
 * @brief 压力测试组
 */
int test_group_stress(void) {
    RUN_TEST(test_embedded_flash_stress_test);
    CHECK_FAIL_AND_STOP();
    return 0;
}


/**
 * @brief 运行所有测试用例
 * Unity测试框架的主运行函数
 * 注意：如果某个测试失败，会立即停止执行后续测试
 * @return 返回测试失败的数量，0表示所有测试通过
 */
int RunAllTests(void) {
    UnityBegin(__FILE__);
    // RUN_TEST(test_error_display_verification_1);  // 添加错误显示验证测试
    // CHECK_FAIL_AND_STOP();
    // RUN_TEST(test_error_display_verification_2);  // 添加错误显示验证测试
    // CHECK_FAIL_AND_STOP();
    // 运行测试组
    // 注意：使用UnityPrint而不是printf，避免干扰Unity的错误输出
    UnityPrint("\n========== Running Init Tests ==========\n");
    test_group_init();

    UnityPrint("\n========== Running Basic Type Tests ==========\n");
    test_group_basic_types();

    UnityPrint("\n========== Running Boundary Tests ==========\n");
    test_group_boundary();

    UnityPrint("\n========== Running Error Handling Tests ==========\n");
    test_group_error_handling();
    
    UnityPrint("\n========== Running Advanced Tests ==========\n");
    test_group_advanced();
    
    UnityPrint("\n========== Running Stress Tests ==========\n");
    test_group_stress();
    
    int failures = UnityEnd();
		printf("*********failures:%d **************\r\n",failures);
    return failures;
}

/**
 * @brief 主测试函数
 * 运行所有Unity测试用例
 * 注意：此函数会调用suiteSetUp和suiteTearDown
 * @return 0表示成功，非0表示失败（返回失败数量）
 */
int embedded_flash_run_manual_tests(void) {
    UnityPrint("\n");
    UnityPrint("========================================\n");
    UnityPrint("  EmbeddedFlash Unity Test Suite\n");
    UnityPrint("========================================\n");
    
    // 调用测试套件初始化（擦除Flash、初始化模块等）
    suiteSetUp();
    
    // 运行所有测试用例
    int failures = RunAllTests();
    
    // 调用测试套件清理
    int exit_code = suiteTearDown(failures);
    
    return exit_code;
}

/**
 * @brief 快速验证函数
 * 运行少量关键测试用例进行快速验证
 * 注意：如果某个测试失败，会立即停止执行后续测试
 * @return 0表示成功，非0表示失败（返回失败数量）
 */
int embedded_flash_quick_manual_test(void) {
    UnityPrint("\n========== Quick Manual Test ==========\n");
    
    UnityBegin(__FILE__);
    
    // 只运行关键测试用例
    RUN_TEST(test_embedded_flash_init);
    CHECK_FAIL_AND_STOP();  // 检查失败并停止
    
    RUN_TEST(test_embedded_flash_uint8_read_write);
    CHECK_FAIL_AND_STOP();  // 检查失败并停止
    
    RUN_TEST(test_embedded_flash_uint32_read_write);
    CHECK_FAIL_AND_STOP();  // 检查失败并停止
    
    RUN_TEST(test_embedded_flash_string_read_write);
    CHECK_FAIL_AND_STOP();  // 检查失败并停止
    
    RUN_TEST(test_embedded_flash_hex_read_write);
    CHECK_FAIL_AND_STOP();  // 检查失败并停止
    
    int failures = UnityEnd();
		
    return failures;
}



/* ==================== 使用说明 ==================== */
/*
 * Unity测试框架使用指南：
 * 
 * 1. 添加新的测试用例：
 *    - 创建测试函数，函数名以test_开头，返回void
 *    - 在函数中使用TEST_ASSERT_*系列宏进行断言
 *    - 示例：
 *      void test_my_new_feature(void) {
 *          EF_ErrCode err = embedded_flash_set_uint8(0x01, 100);
 *          TEST_ASSERT_EQUAL_INT(EF_OK, err);
 *      }
 * 
 * 2. 将测试用例添加到测试组：
 *    - 在相应的test_group_*函数中使用RUN_TEST宏
 *    - 或者创建新的测试组函数
 *    - 在RunAllTests()函数中调用测试组
 * 
 * 3. Unity断言宏说明：
 *    - TEST_ASSERT_EQUAL_INT(expected, actual) - 比较整数
 *    - TEST_ASSERT_EQUAL_UINT8(expected, actual) - 比较uint8_t
 *    - TEST_ASSERT_EQUAL_UINT16(expected, actual) - 比较uint16_t
 *    - TEST_ASSERT_EQUAL_UINT32(expected, actual) - 比较uint32_t
 *    - TEST_ASSERT_EQUAL_FLOAT(expected, actual) - 比较浮点数
 *    - TEST_ASSERT_TRUE(condition) - 断言为真
 *    - TEST_ASSERT_FALSE(condition) - 断言为假
 *    - TEST_ASSERT_NULL(pointer) - 断言指针为NULL
 *    - TEST_ASSERT_NOT_NULL(pointer) - 断言指针不为NULL
 *    - 更多断言宏请参考Unity文档
 * 
 * 4. setUp和tearDown函数：
 *    - setUp()：每个测试用例运行前调用，用于初始化
 *    - tearDown()：每个测试用例运行后调用，用于清理
 * 
 * 5. suiteSetUp和suiteTearDown函数：
 *    - suiteSetUp()：整个测试套件运行前调用一次
 *    - suiteTearDown()：整个测试套件运行后调用一次
 * 
 * 6. 运行测试：
 *    - 调用embedded_flash_run_manual_tests()运行所有测试
 *    - 调用embedded_flash_quick_manual_test()运行快速测试
 * 
 * 7. 注意事项：
 *    - 测试用例应该是独立的，不依赖于其他测试用例的执行顺序
 *    - 每个测试用例应该测试一个特定的功能
 *    - 使用有意义的测试函数名和断言消息
 *    - 如果测试需要特定的Flash状态，在setUp()中初始化
 */
	
	
 
 