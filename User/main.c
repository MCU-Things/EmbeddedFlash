
#include <stdio.h>
#include "system.h"
#include "SysTick.h"
#include "led.h"
#include "usart.h"
#include "SEGGER_RTT.h"

#include "EmbeddedFlash_port.h"
#include "embedded_flash_manual_tests.h"
/*******************************************************************************
* �� �� ��         : main
* ��������		   : ������
* ��    ��         : ��
* ��    ��         : ��
*******************************************************************************/
int main()
{
	u8 i=0; 
	u16 data=1234;
	float fdata=12.34;
	char str[]="Hello World!";	
	SysTick_Init(72);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  //�ж����ȼ����� ��2��
	LED_Init();
	USART1_Init(115200);
	
	printf("main\r\n");
	
//	// Flash接口测试示例
//	printf("\n========== Flash Port Test Example ==========\n");
//	uint32_t test_addr = 0x0807E000; // 使用Flash末尾区域进行测试
//	printf("Testing Flash port at address: 0x%08X\n", test_addr);
//	
//	// 初始化Flash
//	flash_port_init();
//	
//	// 执行Flash Port测试
//	if (flash_port_test(test_addr) == EF_OK) {
//		printf("✅ Flash port test passed!\n");
//	} else {
//		printf("❌ Flash port test failed!\n");
//	}
//	
//	printf("========== Flash Test Complete ==========\n\n");
	
	// ==================== 测试模式选择 ====================
	// 可以通过定义宏来选择运行哪种测试
	// 1. EMBEDDED_FLASH_DEMO_ENABLE_TESTS - 运行demo_tests（原有测试）
	// 2. EMBEDDED_FLASH_MANUAL_TESTS_ENABLE - 运行Unity手动测试（新增）
	//    使用方法：在项目配置中定义 EMBEDDED_FLASH_MANUAL_TESTS_ENABLE (1) 来启用Unity测试
	
	// 不再支持EMBEDDED_FLASH_DEMO_ENABLE_TESTS，已替换为手动测试框架
	#if (EMBEDDED_FLASH_MANUAL_TESTS_ENABLE == 1)
		// 运行Unity手动测试用例
		test_sysTick_init();
		printf("\r\n\r\n");
		printf("========================================\r\n");
		printf("  Starting Unity Manual Tests\r\n");
		printf("========================================\r\n");
		
		// 运行所有Unity测试用例
		int test_result = embedded_flash_run_manual_tests();
		
		printf("\r\n========================================\r\n");
		if (test_result == 0) {
			printf("  All Unity Tests PASSED!\r\n");
		} else {
			printf("  Unity Tests FAILED: %d test(s) failed\r\n", test_result);
		}
		printf("========================================\r\n\r\n");
		#if EFLASH_ENABLE_ERASE_COUNTER
		// 打印擦除统计信息
		embedded_flash_print_erase_stats();
		#endif
		// 测试完成后进入循环
		printf("Unity tests completed. Entering main loop...\r\n");
	#else
		// 正常运行模式（非测试模式）
		embedded_flash_demo_init();
	#endif
	
	// 只有在非测试模式下才运行正常应用代码
	#if (EMBEDDED_FLASH_MANUAL_TESTS_ENABLE != 1)
	//??????
	powerOffofFlashSaveData_ToSysData();
	
	serial_init();
	qmi8658_init(); //??????,???????????ms????
	printf("main run.\n");
	APP_IWDG_Init();
	HALL_Init(); //???powerOffofFlashSaveData_ToSysData??!!
	Buzzer_Play_Twinkle(1); //Buzzer_Play_OK(1);
	//testIO_Init();
	#endif
		
	while(1)
	{
		i++;
		if(i%20==0)
		{
			led1=!led1;
			
	
		}
		delay_ms(10);
	}
}
