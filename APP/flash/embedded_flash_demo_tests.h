#ifndef __EMBEDDED_FLASH_DEMO_TESTS_H__
#define __EMBEDDED_FLASH_DEMO_TESTS_H__

#include "EmbeddedFlash.h"
#include <stdint.h>

// 毫秒计时器函数声明
void InitSysTick(void);
uint32_t HAL_GetTick(void);


#define EMBEDDED_FLASH_DEMO_ENABLE_TESTS (1)


// 定义测试用的键值对ID - 命名规则: TEST_数据类型_长度_16进制键值_10进制键值
#define TEST_UINT8_1_A1_161 0xA1
#define TEST_UINT8_1_A2_162 0xA2
#define TEST_UINT16_2_A3_163 0xA3
#define TEST_INT32_4_A4_164 0xA4
#define TEST_STRING_8_A5_165 0xA5
#define TEST_HEX_4_A6_166 0xA6
#define TEST_UINT16_2_A7_167 0xA7
#define TEST_INT32_4_A8_168 0xA8
#define TEST_FLOAT_4_A9_169 0xA9

// 添加缺少的数据类型键
#define TEST_BOOL_1_AA_170 0xAA
#define TEST_INT8_1_AB_171 0xAB
#define TEST_INT16_2_AC_172 0xAC
#define TEST_UINT32_4_AD_173 0xAD
#define TEST_UINT64_8_AE_174 0xAE
#define TEST_INT64_8_AF_175 0xAF

// 测试函数声明
int embedded_flash_demo_basic_test(void);
int embedded_flash_demo_data_source_test(void);
int embedded_flash_demo_batch_test(void);
int embedded_flash_demo_gc_test(void);
int embedded_flash_demo_error_test(void);
int embedded_flash_demo_power_loss_test(void);


int embedded_flash_demo_verify_integrity(void);

int embedded_flash_demo_run_full(void);

// 严苛测试函数声明
int embedded_flash_demo_stress_test(void);
int embedded_flash_demo_boundary_test(void);
int embedded_flash_demo_gc_stress_test(void);
int embedded_flash_demo_power_loss_stress_test(void);
int embedded_flash_demo_data_type_consistency_test(void);
int embedded_flash_demo_performance_test(void);
int embedded_flash_demo_random_data_test(void);
int embedded_flash_demo_type_safe_api_test(void);
//void embedded_flash_demo_run_type_safe_tests(void);

#endif // __EMBEDDED_FLASH_DEMO_TESTS_H__
