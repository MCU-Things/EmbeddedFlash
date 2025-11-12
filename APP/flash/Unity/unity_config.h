/**
 * @file unity_config.h
 * @brief Unity测试框架的私有配置文件
 * 
 * 此文件包含针对STM32F10x平台的Unity配置
 * 通过定义 UNITY_INCLUDE_CONFIG_H 宏，Unity会自动包含此文件
 * 
 * @note 此文件应该在包含 unity.h 之前定义 UNITY_INCLUDE_CONFIG_H 宏
 */

#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

/*-------------------------------------------------------
 * STM32F10x Platform Default Configuration
 * 为STM32F10x平台启用所有有助于调试和分析的配置
 *-------------------------------------------------------*/

// ========== 基本支持配置 ==========
// 显式设置整数宽度（STM32F103使用32位整数）
// 根据Unity文档，如果自动检测失败，应该显式设置这些值
#ifndef UNITY_INT_WIDTH
#define UNITY_INT_WIDTH (32)
#endif

#ifndef UNITY_LONG_WIDTH
#define UNITY_LONG_WIDTH (32)
#endif

#ifndef UNITY_POINTER_WIDTH
#define UNITY_POINTER_WIDTH (32)
#endif

// 启用Unity 64位整数支持（必须启用，因为测试用例中使用了uint64和int64）
#ifndef UNITY_SUPPORT_64
#define UNITY_SUPPORT_64
#endif

// ========== 调试和分析配置 ==========
// 启用详细错误信息显示（显示期望值和实际值的详细信息，有助于快速定位问题）
#ifndef UNITY_EXCLUDE_DETAILS
#ifndef UNITY_INCLUDE_DETAILS
#define UNITY_INCLUDE_DETAILS
#endif
#endif

// 启用测试执行时间显示（显示每个测试用例的执行时间，有助于性能分析）
#ifndef UNITY_EXCLUDE_EXEC_TIME
#ifndef UNITY_INCLUDE_EXEC_TIME
#define UNITY_INCLUDE_EXEC_TIME
#endif
#endif

// 启用完整的输出（确保所有输出信息都被打印）
//#ifndef UNITY_OUTPUT_COMPLETE
//#define UNITY_OUTPUT_COMPLETE
//#endif

// 在最终摘要中区分失败（打印FAILED而不是FAIL，便于自动化工具搜索失败）
#ifndef UNITY_DIFFERENTIATE_FINAL_FAIL
#define UNITY_DIFFERENTIATE_FINAL_FAIL
#endif

// ========== 浮点数支持配置 ==========
// 启用double类型支持（如果测试用例中使用了double类型）
#ifndef UNITY_EXCLUDE_DOUBLE
#ifndef UNITY_INCLUDE_DOUBLE
#define UNITY_INCLUDE_DOUBLE
#endif
#endif

// 启用浮点数打印（在错误信息中打印浮点数的实际值，有助于调试）
// 注意：如果内存紧张，可以注释掉这行来减小代码体积
// 默认启用，不需要额外定义

// ========== 输出配置 ==========
// 串口打印配置：使用SEGGER RTT和USART1进行输出
#ifndef UNITY_OUTPUT_CHAR
  /* STM32F10x平台：使用USART1和SEGGER RTT进行串口输出 */
  /* 注意：stm32f10x.h和usart.h应该在包含unity.h之前被包含 */
  /* 需要确保SEGGER_RTT_PutChar、USART_SendData、USART_GetFlagStatus函数可用 */
  #include "SEGGER_RTT.h"
  #include "stm32f10x.h"
  #include "usart.h"
  /* 
   * Unity框架使用逐字符输出方式，UNITY_OUTPUT_CHAR(a) 接收的是单个字符（int类型）
   * 根据Unity配置指南，参数类型是int（类似标准C的putchar函数）
   * 我们需要将其转换为char，然后通过SEGGER RTT和USART1输出
   * 注意：不能使用 printf("%s", a)，因为a是单个字符，不是字符串
   */
  #define UNITY_OUTPUT_CHAR(a) do { \
      int ch_int = (int)(a); \
      char ch = (char)ch_int; \
      SEGGER_RTT_PutChar(0, ch); \
      USART_SendData(USART1, (uint16_t)(uint8_t)ch); \
      while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET); \
  } while(0)
#endif


#ifndef UNITY_OUTPUT_FLUSH
  /* STM32F10x平台：等待USART1完成传输 */
  #define UNITY_OUTPUT_FLUSH() do { \
      while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET); \
  } while(0)
#endif

#endif /* UNITY_CONFIG_H */

