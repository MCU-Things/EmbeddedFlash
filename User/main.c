
#include <stdio.h>
#include "system.h"
#include "SysTick.h"
#include "led.h"
#include "usart.h"
#include "SEGGER_RTT.h"

#include "EmbeddedFlash_port.h"
#include "embedded_flash_demo_tests.h"
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
	
	#if EMBEDDED_FLASH_DEMO_ENABLE_TESTS
	InitSysTick();
	if (flash_port_erase(KV_SECTOR_START_ADDR, KV_SECTOR_SIZE*KV_SECTOR_COUNT) != EF_OK) {

			printf ("erase fail \n");
	}
	uint32_t test_count = 0;
	while(test_count<10){
		test_count++;
		printf("\r\n\r\n********      start             ************\r\n");
		int ret = embedded_flash_demo_run_full();
		printf("********      end               ************\r\n\r\n");
		printf("test_count:%d",	test_count);
		while(ret == -1){
			//printf("fai!,test_count:%d\r\n",test_count);
			delay_ms(60*1000);
		}
		delay_ms(5000);
		embedded_flash_print_erase_stats();
		delay_ms(5000);
	}
	#else
	embedded_flash_demo_init();
	#endif
	
	
	#if !EMBEDDED_FLASH_DEMO_ENABLE_TESTS
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
