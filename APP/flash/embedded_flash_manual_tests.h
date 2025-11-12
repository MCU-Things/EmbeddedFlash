/**
 * @file embedded_flash_manual_tests.h
 * @brief 嵌入式Flash存储模块的手动测试用例头文件
 * 
 * 这个文件包含了对EmbeddedFlash模块的手动编写测试用例的声明
 * 使用Unity测试框架进行单元测试
 * 
 * @author 手动编写
 * @version 1.0
 * @date 2024
 */

#ifndef __EMBEDDED_FLASH_MANUAL_TESTS_H__
#define __EMBEDDED_FLASH_MANUAL_TESTS_H__

#include <stdint.h>
#include <stdbool.h>

// Unity测试框架配置
#define TEST_ENABLE_LOGGING (1)

// 测试使能宏定义（可以在项目中定义此宏来启用Unity测试）
// 默认不启用，需要在项目中定义 EMBEDDED_FLASH_MANUAL_TESTS_ENABLE 来启用
#ifndef EMBEDDED_FLASH_MANUAL_TESTS_ENABLE
#define EMBEDDED_FLASH_MANUAL_TESTS_ENABLE (1)
#endif
void test_sysTick_init(void);
uint32_t test_SysTick_GetTick(void);
// 测试函数声明
int embedded_flash_run_manual_tests(void);
int embedded_flash_quick_manual_test(void);

// // Unity测试运行器函数声明（在.c文件中定义）
// int RunAllTests(void);  // 返回测试失败数量

#endif // __EMBEDDED_FLASH_MANUAL_TESTS_H__

