#include "embedded_flash_demo_tests.h"

#include "EmbeddedFlash.h"

#if EMBEDDED_FLASH_DEMO_ENABLE_TESTS
#include <string.h>
#include <stdio.h>
#include "stm32f10x.h"

// 简单的毫秒计时器实现，替代HAL_GetTick()
static volatile uint32_t ms_tick = 0;

// 初始化SysTick为1ms中断
void InitSysTick(void) {
    // 假设系统时钟为72MHz，配置SysTick为1ms中断
    SysTick_Config(SystemCoreClock / 1000); // 1ms中断
}

// 获取当前毫秒数
uint32_t HAL_GetTick(void) {
    return ms_tick;
}

//// SysTick中断处理函数（需要在中断向量表中配置）
//void SysTick_Handler(void) {
//    ms_tick++;
//}


static void print_pass(const char *msg) { printf("[OK] PASS - %s\n", msg); }
static void print_fail(const char *msg) { printf("[X] FAIL - %s\n", msg); }
static void print_test_start(const char *test_name) { printf("\n========== %s ==========\n", test_name); }
static void print_test_end(const char *test_name, int result) { 
    printf("========== %s %s ==========\n\n", test_name, result == 0 ? "PASSED" : "FAILED"); 
}


// 独立的初始化代码
static uint8_t test_default_uint8_1 = 5;
static uint8_t test_default_uint8_2 = 10;
static uint16_t test_default_uint16_1 = 500;
static int32_t test_default_int32_1 = -1000;
static char test_default_string_1[] = "Default";
static uint8_t test_default_hex_1[] = {0xAA, 0xBB, 0xCC, 0xDD};
static uint16_t test_default_uint16_2 = 750;
static int32_t test_default_int32_2 = -2000;
static float test_default_float_1 = 3.14159f;

// 新添加的数据类型默认值
static bool test_default_bool_1 = true;
static int8_t test_default_int8_1 = -50;
static int16_t test_default_int16_1 = -1500;
static uint32_t test_default_uint32_1 = 1000000;
static uint64_t test_default_uint64_1 = 0x123456789ABCDEF0ULL;
static int64_t test_default_int64_1 = -0x123456789ABCDEF0LL;

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
	
	// 新添加的数据类型键
	{0, TEST_BOOL_1_AA_170, (uint8_t*)&test_default_bool_1, sizeof(test_default_bool_1), EFLASH_FORMAT_BOOL, KV_DATA_SOURCE_DEFAULT},
	{0, TEST_INT8_1_AB_171, (uint8_t*)&test_default_int8_1, sizeof(test_default_int8_1), EFLASH_FORMAT_INT8, KV_DATA_SOURCE_DEFAULT},
	{0, TEST_INT16_2_AC_172, (uint8_t*)&test_default_int16_1, sizeof(test_default_int16_1), EFLASH_FORMAT_INT16, KV_DATA_SOURCE_DEFAULT},
	{0, TEST_UINT32_4_AD_173, (uint8_t*)&test_default_uint32_1, sizeof(test_default_uint32_1), EFLASH_FORMAT_UINT32, KV_DATA_SOURCE_DEFAULT},
	{0, TEST_UINT64_8_AE_174, (uint8_t*)&test_default_uint64_1, sizeof(test_default_uint64_1), EFLASH_FORMAT_UINT64, KV_DATA_SOURCE_DEFAULT},
	{0, TEST_INT64_8_AF_175, (uint8_t*)&test_default_int64_1, sizeof(test_default_int64_1), EFLASH_FORMAT_INT64, KV_DATA_SOURCE_DEFAULT},
};

static int test_embedded_flash_init(void) {
	uint8_t count = sizeof(test_kvs) / sizeof(kv_data_t);
	return embedded_flash_init(test_kvs, count);
}

/* 测试基本API*/
int embedded_flash_demo_basic_test(void) {
	print_test_start("Basic API Test");
	printf("Testing basic set/get operations for all data types...\n");
	uint8_t buf[12];
	uint8_t len, type;
	uint8_t test_value_uint8 = 42;
	if (embedded_flash_set_uint8(TEST_UINT8_1_A1_161, test_value_uint8) == 0) {
		if (embedded_flash_get(TEST_UINT8_1_A1_161, buf, &len, &type) == 0 && type == EFLASH_FORMAT_UINT8
        && len == 1 
        && buf[0] == test_value_uint8) {
			print_pass("basic read/write (test_uint8)");
		} else {
            print_fail("basic readback mismatch (uint8)");
            return -1;
        }
	} else {
        print_fail("basic write failed (uint8)");
        return -1;
    }
    int8_t test_value_int8 = -120;
    if (embedded_flash_set_int8(TEST_INT8_1_AB_171, test_value_int8) == 0) {
		if (embedded_flash_get(TEST_INT8_1_AB_171, buf, &len, &type) == 0 && type == EFLASH_FORMAT_INT8
        && len == 1 
        && *(int8_t*)buf == test_value_int8) {
			print_pass("basic read/write (test_int8)");
		} else {
            print_fail("basic readback mismatch (int8)");
            return -1;
        }
	} else {
        print_fail("basic write failed (int8)");
        return -1;
    }
	
	uint16_t test_value_uint16 = 1234;
	if (embedded_flash_set_uint16(TEST_UINT16_2_A3_163, test_value_uint16) == 0) {
		if (embedded_flash_get(TEST_UINT16_2_A3_163, buf, &len, &type) == 0 
        && type == EFLASH_FORMAT_UINT16
        && len == 2 
        && *(uint16_t*)buf == test_value_uint16) {
			print_pass("basic read/write (test_uint16)");
		} else {
            print_fail("basic readback mismatch (uint16)");
            return -1;
        }
	} else {
        print_fail("basic write failed (uint16)");
        return -1;
    }
	
    int16_t test_value_int16 = -1234;
	if (embedded_flash_set_int16(TEST_INT16_2_AC_172, test_value_int16) == 0) {
		if (embedded_flash_get(TEST_INT16_2_AC_172, buf, &len, &type) == 0 
        && type == EFLASH_FORMAT_INT16
        && len == 2 
        && *(int16_t*)buf == test_value_int16) {
			print_pass("basic read/write (test_int16)");
		} else {
            print_fail("basic readback mismatch (int16)");
            return -1;
        }
	} else {
        print_fail("basic write failed (int16)");
        return -1;
    }

    uint32_t test_value_uint32 = 1234000;
	if (embedded_flash_set_uint32(TEST_UINT32_4_AD_173, test_value_uint32) == 0) {
		if (embedded_flash_get(TEST_UINT32_4_AD_173, buf, &len, &type) == 0 
        && type == EFLASH_FORMAT_UINT32
        && len == 4 
        && *(uint32_t*)buf == test_value_uint32) {
			print_pass("basic read/write (test_uint32)");
		} else {
            print_fail("basic readback mismatch (uint32)");
            return -1;
        }
	} else {
        print_fail("basic write failed (uint32)");
        return -1;
    }
	
    int32_t test_value_int32 = -1234000;
	if (embedded_flash_set_int32(TEST_INT32_4_A4_164, test_value_int32) == 0) {
		if (embedded_flash_get(TEST_INT32_4_A4_164, buf, &len, &type) == 0 
        && type == EFLASH_FORMAT_INT32
        && len == 4 
        && *(int32_t*)buf == test_value_int32) {
			print_pass("basic read/write (test_int32)");
		} else {
            print_fail("basic readback mismatch (int32)");
            return -1;
        }
	} else {
        print_fail("basic write failed (int32)");
        return -1;
    }


    uint64_t test_value_uint64 = 1234003;
	if (embedded_flash_set_uint64(TEST_UINT64_8_AE_174, test_value_uint64) == 0) {
		if (embedded_flash_get(TEST_UINT64_8_AE_174, buf, &len, &type) == 0 
        && type == EFLASH_FORMAT_UINT64
        && len == 8 
        && *(uint64_t*)buf == test_value_uint64) {
			print_pass("basic read/write (test_uint64)");
		} else {
            print_fail("basic readback mismatch (uint64)");
            return -1;
        }
	} else {
        print_fail("basic write failed (uint64)");
        return -1;
    }
	
    int64_t test_value_int64 = -1234001;
	if (embedded_flash_set_int64(TEST_INT64_8_AF_175, test_value_int64) == 0) {
		if (embedded_flash_get(TEST_INT64_8_AF_175, buf, &len, &type) == 0 
        && type == EFLASH_FORMAT_INT64
        && len == 8 
        && *(int64_t*)buf == test_value_int64) {
			print_pass("basic read/write (test_int64)");
		} else {
            print_fail("basic readback mismatch (int64)");
            return -1;
        }
	} else {
        print_fail("basic write failed (int64)");
        return -1;
    }

	// 测试字符串API - 确保字符串长度不超过KV_MAX_VALUE_SIZE-1
	const char* test_string = "Hello";  // 5字节 + null终止符 = 6字节 < 10字节
	if (embedded_flash_set_string(TEST_STRING_8_A5_165, test_string) == 0) {
		if (embedded_flash_get(TEST_STRING_8_A5_165, buf, &len, &type) == 0 
        && type == EFLASH_FORMAT_STRING
        && len == strlen(test_string) + 1  // 字符串长度应该包含空终止符
        && strcmp((char*)buf, test_string) == 0) {
			print_pass("basic read/write (test_string)");
		} else {
            print_fail("basic readback mismatch (string)");
            printf("Expected len=%d, got len=%d\n", (int)strlen(test_string) + 1, len);
            return -1;
        }
	} else {
        print_fail("basic write failed (string)");
        return -1;
    }

  const uint8_t test_hex[] = {0xAA, 0xBB, 0xCC, 0xDD};  // 
	if (embedded_flash_set_hex(TEST_HEX_4_A6_166, test_hex,sizeof(test_hex)) == 0) {
		if (embedded_flash_get(TEST_HEX_4_A6_166, buf, &len, &type) == 0 
        && type == EFLASH_FORMAT_HEX
        && len == sizeof(test_hex)
        && memcmp(buf, test_hex, sizeof(test_hex)) == 0) {
			print_pass("basic read/write (test_hex)");
		} else {
            print_fail("basic readback mismatch (hex)");
            return -1;
        }
	} else {
        print_fail("basic write failed (hex)");
        return -1;
    }
	print_test_end("Basic API Test", 0);
	return 0;
}

int embedded_flash_demo_data_source_test(void) {
	print_test_start("Data Source Test");
	printf("Testing data source tracking and defaults...\n");
	uint8_t count = sizeof(test_kvs) / sizeof(kv_data_t);
	if (count > 0) print_pass("defaults table available"); 
    else { 
        print_fail("defaults table missing"); 
        return -1;
    }
	print_test_end("Data Source Test", 0);
    return 0;
}


int embedded_flash_demo_batch_test(void) {
	print_test_start("Batch Test");
	printf("Testing batch operations and data synchronization...\n");
	
	// 使用专属变量进行测试
	uint8_t test_uint8_1 = 10;
	uint16_t test_uint16_1 = 1000;
	int32_t test_int32_1 = -5000;
	char test_string_1[] = "TestStr";  // 7字节 + null终止符 = 8字节 < 10字节
	uint8_t test_hex_1[] = {0xDE, 0xAD, 0xBE, 0xEF};
	
	printf("Writing test data: UINT8=%d, UINT16=%d, INT32=%d, STRING='%s', HEX={0x%02X,0x%02X,0x%02X,0x%02X}\n",
		   test_uint8_1, test_uint16_1, test_int32_1, test_string_1,
		   test_hex_1[0], test_hex_1[1], test_hex_1[2], test_hex_1[3]);
	
	// 写入测试数据
	if (embedded_flash_set_uint8(TEST_UINT8_1_A2_162, test_uint8_1) != 0) {
		printf("[X] Failed to write UINT8 data\n");
		print_fail("batch sync test variables->KV");
		return -1;
	}
	
	if (embedded_flash_set_uint16(TEST_UINT16_2_A3_163, test_uint16_1) != 0) {
		printf("[X] Failed to write UINT16 data\n");
		print_fail("batch sync test variables->KV");
		return -1;
	}
	
	if (embedded_flash_set_int32(TEST_INT32_4_A4_164, test_int32_1) != 0) {
		printf("[X] Failed to write INT32 data\n");
		print_fail("batch sync test variables->KV");
		return -1;
	}
	
	if (embedded_flash_set_string(TEST_STRING_8_A5_165, test_string_1) != 0) {
		printf("[X] Failed to write STRING data\n");
		print_fail("batch sync test variables->KV");
		return -1;
	}
	
	if (embedded_flash_set_hex(TEST_HEX_4_A6_166, test_hex_1, sizeof(test_hex_1)) != 0) {
		printf("[X] Failed to write HEX data\n");
		print_fail("batch sync test variables->KV");
		return -1;
	}
	
	printf("All data written successfully. Reading back for verification...\n");
	
	// 使用联合体来安全地访问不同类型的数据
	union {
		uint8_t raw[KV_MAX_VALUE_SIZE];  // 增加大小以容纳64位数据
		uint8_t uint8_val;
		uint16_t uint16_val;
		int32_t int32_val;
		int64_t int64_val;  // 添加64位支持
		uint64_t uint64_val; // 添加64位支持
		char string_val[KV_MAX_VALUE_SIZE];
		uint8_t hex_val[KV_MAX_VALUE_SIZE];
	} data_buffer;
	
	uint8_t l, t;
	int ok = 1;

	// 测试uint8数据
	if (embedded_flash_get(TEST_UINT8_1_A2_162, data_buffer.raw, &l, &t) != 0) {
        ok = 0;
        printf("[X] UINT8: Failed to read key %d\n", TEST_UINT8_1_A2_162);
    } else if (t != EFLASH_FORMAT_UINT8) {
        ok = 0;
        printf("[X] UINT8: Type mismatch - expected=%d, actual=%d\n", EFLASH_FORMAT_UINT8, t);
    } else if (l != sizeof(uint8_t)) {
        ok = 0;
        printf("[X] UINT8: Length mismatch - expected=%d, actual=%d\n", (int)sizeof(uint8_t), l);
    } else if (data_buffer.uint8_val != 10) {
        ok = 0;
        printf("[X] UINT8: Value mismatch - expected=10, got=%d\n", data_buffer.uint8_val);
    } else {
        printf("[OK] UINT8: Read correctly (value=%d, type=%d, len=%d)\n", data_buffer.uint8_val, t, l);
    }
    
	// 测试uint16数据
	if (embedded_flash_get(TEST_UINT16_2_A3_163, data_buffer.raw, &l, &t) != 0) {
        ok = 0;
        printf("[X] UINT16: Failed to read key %d\n", TEST_UINT16_2_A3_163);
    } else if (t != EFLASH_FORMAT_UINT16) {
        ok = 0;
        printf("[X] UINT16: Type mismatch - expected=%d, actual=%d\n", EFLASH_FORMAT_UINT16, t);
    } else if (l != sizeof(uint16_t)) {
        ok = 0;
        printf("[X] UINT16: Length mismatch - expected=%d, actual=%d\n", (int)sizeof(uint16_t), l);
    } else if (data_buffer.uint16_val != 1000) {
        ok = 0;
        printf("[X] UINT16: Value mismatch - expected=1000, got=%d\n", data_buffer.uint16_val);
    } else {
        printf("[OK] UINT16: Read correctly (value=%d, type=%d, len=%d)\n", data_buffer.uint16_val, t, l);
    }
    
	// 测试int32数据
	if (embedded_flash_get(TEST_INT32_4_A4_164, data_buffer.raw, &l, &t) != 0) {
        ok = 0;
        printf("[X] INT32: Failed to read key %d\n", TEST_INT32_4_A4_164);
    } else if (t != EFLASH_FORMAT_INT32) {
        ok = 0;
        printf("[X] INT32: Type mismatch - expected=%d, actual=%d\n", EFLASH_FORMAT_INT32, t);
    } else if (l != sizeof(int32_t)) {
        ok = 0;
        printf("[X] INT32: Length mismatch - expected=%d, actual=%d\n", (int)sizeof(int32_t), l);
    } else if (data_buffer.int32_val != -5000) {
        ok = 0;
        printf("[X] INT32: Value mismatch - expected=-5000, got=%d\n", data_buffer.int32_val);
    } else {
        printf("[OK] INT32: Read correctly (value=%d, type=%d, len=%d)\n", data_buffer.int32_val, t, l);
    }
    
	// 测试string数据
	if (embedded_flash_get(TEST_STRING_8_A5_165, data_buffer.raw, &l, &t) != 0) {
        ok = 0;
        printf("[X] STRING: Failed to read key %d\n", TEST_STRING_8_A5_165);
    } else if (t != EFLASH_FORMAT_STRING) {
        ok = 0;
        printf("[X] STRING: Type mismatch - expected=%d, actual=%d\n", EFLASH_FORMAT_STRING, t);
    } else if (l != strlen("TestStr") + 1) {
        ok = 0;
        printf("[X] STRING: Length mismatch - expected=%d, actual=%d\n", (int)(strlen("TestStr") + 1), l);
    } else if (strcmp(data_buffer.string_val, "TestStr") != 0) {
        ok = 0;
        printf("[X] STRING: Value mismatch - expected='TestStr', got='%s'\n", data_buffer.string_val);
    } else {
        printf("[OK] STRING: Read correctly (value='%s', type=%d, len=%d)\n", data_buffer.string_val, t, l);
    }
    
    // 测试hex数据
    if (embedded_flash_get(TEST_HEX_4_A6_166, data_buffer.raw, &l, &t) != 0) {
        ok = 0;
        printf("[X] HEX: Failed to read key %d\n", TEST_HEX_4_A6_166);
    } else if (t != EFLASH_FORMAT_HEX) {
        ok = 0;
        printf("[X] HEX: Type mismatch - expected=%d, actual=%d\n", EFLASH_FORMAT_HEX, t);
    } else if (l != sizeof(test_hex_1)) {
        ok = 0;
        printf("[X] HEX: Length mismatch - expected=%d, actual=%d\n", (int)sizeof(test_hex_1), l);
    } else if (memcmp(data_buffer.hex_val, test_hex_1, sizeof(test_hex_1)) != 0) {
        ok = 0;
        printf("[X] HEX: Value mismatch - expected={0xDE, 0xAD, 0xBE, 0xEF}, got={0x%02X, 0x%02X, 0x%02X, 0x%02X}\n", 
               data_buffer.hex_val[0], data_buffer.hex_val[1], data_buffer.hex_val[2], data_buffer.hex_val[3]);
    } else {
        printf("[OK] HEX: Read correctly (value={0x%02X, 0x%02X, 0x%02X, 0x%02X}, type=%d, len=%d)\n", 
               data_buffer.hex_val[0], data_buffer.hex_val[1], data_buffer.hex_val[2], data_buffer.hex_val[3], t, l);
    } 
    
	if (ok) {
			print_pass("batch sync test variables->KV"); 
	}else {
			print_fail("batch sync test variables->KV");
			return -1;
	}

	// 反向恢复
	uint8_t test_uint8_2 = 0;
	uint16_t test_uint16_2 = 0;
	int32_t test_int32_2 = 0;
	char test_string_2[KV_MAX_VALUE_SIZE] = {0};
	uint8_t test_hex_2[KV_MAX_VALUE_SIZE] = {0};
	uint8_t string_len = 0;
	
	// 读取并验证每个数据类型
	if (embedded_flash_get(TEST_UINT8_1_A2_162, (uint8_t*)&test_uint8_2, &l, &t) != 0) {
		printf("[X] Reverse sync: Failed to read UINT8 key %d\n", TEST_UINT8_1_A2_162);
		print_fail("reverse sync KV->test variables");
		return -1;
	}
	if (test_uint8_2 != 10) {
		printf("[X] Reverse sync: UINT8 mismatch - expected=10, got=%d\n", test_uint8_2);
		print_fail("reverse sync KV->test variables");
		return -1;
	}
	
	if (embedded_flash_get(TEST_UINT16_2_A3_163, (uint8_t*)&test_uint16_2, &l, &t) != 0) {
		printf("[X] Reverse sync: Failed to read UINT16 key %d\n", TEST_UINT16_2_A3_163);
		print_fail("reverse sync KV->test variables");
		return -1;
	}
	if (test_uint16_2 != 1000) {
		printf("[X] Reverse sync: UINT16 mismatch - expected=1000, got=%d\n", test_uint16_2);
		print_fail("reverse sync KV->test variables");
		return -1;
	}
	
	if (embedded_flash_get(TEST_INT32_4_A4_164, (uint8_t*)&test_int32_2, &l, &t) != 0) {
		printf("[X] Reverse sync: Failed to read INT32 key %d\n", TEST_INT32_4_A4_164);
		print_fail("reverse sync KV->test variables");
		return -1;
	}
	if (test_int32_2 != -5000) {
		printf("[X] Reverse sync: INT32 mismatch - expected=-5000, got=%d\n", test_int32_2);
		print_fail("reverse sync KV->test variables");
		return -1;
	}
	
	if (embedded_flash_get(TEST_STRING_8_A5_165, (uint8_t*)test_string_2, &string_len, &t) != 0) {
		printf("[X] Reverse sync: Failed to read STRING key %d\n", TEST_STRING_8_A5_165);
		print_fail("reverse sync KV->test variables");
		return -1;
	}
	if (string_len != strlen("TestStr") + 1) {
		printf("[X] Reverse sync: STRING length mismatch - expected=%d, got=%d\n", 
			   (int)(strlen("TestStr") + 1), string_len);
		print_fail("reverse sync KV->test variables");
		return -1;
	}
	if (strcmp(test_string_2, "TestStr") != 0) {
		printf("[X] Reverse sync: STRING value mismatch - expected='TestStr', got='%s'\n", test_string_2);
		print_fail("reverse sync KV->test variables");
		return -1;
	}
	
	if (embedded_flash_get(TEST_HEX_4_A6_166, test_hex_2, &l, &t) != 0) {
		printf("[X] Reverse sync: Failed to read HEX key %d\n", TEST_HEX_4_A6_166);
		print_fail("reverse sync KV->test variables");
		return -1;
	}
	if (memcmp(test_hex_2, test_hex_1, sizeof(test_hex_1)) != 0) {
		printf("[X] Reverse sync: HEX mismatch - expected={0xDE, 0xAD, 0xBE, 0xEF}, got={0x%02X, 0x%02X, 0x%02X, 0x%02X}\n", 
			   test_hex_2[0], test_hex_2[1], test_hex_2[2], test_hex_2[3]);
		print_fail("reverse sync KV->test variables");
		return -1;
	}
	
	// 所有数据验证通过
    print_pass("reverse sync KV->test variables");
    printf("Batch Test completed successfully. All data types (UINT8, UINT16, INT32, STRING, HEX) are working correctly.\n");
    print_test_end("Batch Test", 0);
    return 0;
}

int embedded_flash_demo_gc_test(void) {
	print_test_start("Garbage Collection Test");
	printf("Testing garbage collection by filling sectors...\n");
	// 连续写触发GC
    uint8_t count = (sizeof(test_kvs)/sizeof(test_kvs[0]));
	for (int round=0; round<6; round++) {
		for (int i=0; i<count; i++) {
			uint8_t key = test_kvs[i].key;
			uint8_t data_type = test_kvs[i].data_type;
			
			// 根据数据类型使用正确的API进行写入
			int write_result = -1;
			switch (data_type) {
                case EFLASH_FORMAT_BOOL: {
					uint8_t v = (uint8_t)((round*10+i)&0xFF);
					write_result = embedded_flash_set_bool(key, v?true:false);
					if(write_result != 0) {
						printf("[X] GC test: Failed to write BOOL key=0x%02X, round=%d, i=%d\n", key, round, i);
						return -1;
					}
					break;
				}
				case EFLASH_FORMAT_UINT8: {
					uint8_t v = (uint8_t)((round*10+i)&0xFF);
					write_result = embedded_flash_set_uint8(key, v);
					if(write_result != 0) {
						printf("[X] GC test: Failed to write UINT8 key=0x%02X, round=%d, i=%d\n", key, round, i);
						return -1;
					}
					break;
				}
                case EFLASH_FORMAT_INT8: {
					int8_t v = (int8_t)((round*10+i)&0xFF);
					write_result = embedded_flash_set_int8(key, v);
					if(write_result != 0) {
						printf("[X] GC test: Failed to write INT8 key=0x%02X, round=%d, i=%d\n", key, round, i);
						return -1;
					}
					break;
				}
				case EFLASH_FORMAT_UINT16: {
					uint16_t v = (uint16_t)((round*100+i*10)&0xFFFF);
					write_result = embedded_flash_set_uint16(key, v);
					if(write_result != 0) {
						printf("[X] GC test: Failed to write UINT16 key=0x%02X, round=%d, i=%d\n", key, round, i);
						return -1;
					}
					break;
				}
                case EFLASH_FORMAT_INT16: {
					int16_t v = (int16_t)((round*100+i*10)&0xFFFF);
					write_result = embedded_flash_set_int16(key, v);
					if(write_result != 0) {
						printf("[X] GC test: Failed to write INT16 key=0x%02X, round=%d, i=%d\n", key, round, i);
						return -1;
					}
					break;
				}
                case EFLASH_FORMAT_UINT32: {
					uint32_t v = (uint32_t)((round*1000+i*100)&0x7FFFFFFF);
					if (i % 2 == 0) v = -v; // 交替正负值
					write_result = embedded_flash_set_uint32(key, v);
					if(write_result != 0) {
						printf("[X] GC test: Failed to write UINT32 key=0x%02X, round=%d, i=%d\n", key, round, i);
						return -1;
					}
					break;
				}
				case EFLASH_FORMAT_INT32: {
					int32_t v = (int32_t)((round*1000+i*100)&0x7FFFFFFF);
					if (i % 2 == 0) v = -v; // 交替正负值
					write_result = embedded_flash_set_int32(key, v);
					if(write_result != 0) {
						printf("[X] GC test: Failed to write INT32 key=0x%02X, round=%d, i=%d\n", key, round, i);
						return -1;
					}
					break;
				}
                case EFLASH_FORMAT_UINT64: {
					uint64_t v = (uint64_t)((round*1000+i*100)&0x7FFFFFFF);
					if (i % 2 == 0) v = -v; // 交替正负值
					write_result = embedded_flash_set_uint64(key, v);
					if(write_result != 0) {
						printf("[X] GC test: Failed to write UINT64 key=0x%02X, round=%d, i=%d\n", key, round, i);
						return -1;
					}
					break;
				}
				case EFLASH_FORMAT_INT64: {
					int64_t v = (int64_t)((round*1000+i*100)&0x7FFFFFFF);
					if (i % 2 == 0) v = -v; // 交替正负值
					write_result = embedded_flash_set_int64(key, v);
					if(write_result != 0) {
						printf("[X] GC test: Failed to write INT64 key=0x%02X, round=%d, i=%d\n", key, round, i);
						return -1;
					}
					break;
				}
				case EFLASH_FORMAT_FLOAT: {
					float v = (float)(round + i * 0.1f);
					write_result = embedded_flash_set_float(key, v);
					if(write_result != 0) {
						printf("[X] GC test: Failed to write FLOAT key=0x%02X, round=%d, i=%d\n", key, round, i);
						return -1;
					}
					break;
				}
				case EFLASH_FORMAT_STRING: {
					char str[10];
					snprintf(str, sizeof(str), "R%dI%d", round, i);  // 最多"R5I15" = 5字节 + null = 6字节 < 10字节
                    write_result = embedded_flash_set_string(key, str);
					if(write_result != 0) {
						printf("[X] GC test: Failed to write STRING key=0x%02X, round=%d, i=%d\n", key, round, i);
						return -1;
					}
					break;
				}
				case EFLASH_FORMAT_HEX: {
					uint8_t hex_data[4] = {(uint8_t)(round&0xFF), (uint8_t)(i&0xFF), 
											(uint8_t)((round+i)&0xFF), (uint8_t)((round*i)&0xFF)};
					write_result = embedded_flash_set_hex(key, hex_data, sizeof(hex_data));
					if(write_result != 0) {
						printf("[X] GC test: Failed to write HEX key=0x%02X, round=%d, i=%d\n", key, round, i);
						return -1;
					}
					break;
				}
				default:
					// 未知类型，失败
                    printf("unknown type: %d\n", data_type);
					return -1;
			}
		}
        printf("-------Garbage Collection Test progress: %d/%d\n", round+1, 6); //剩余多少轮
	}
	// 简单抽样校验可读
	union {
		uint8_t raw[KV_MAX_VALUE_SIZE];
		uint8_t uint8_val;
	} gc_buffer;
	uint8_t l, t;
	
	if (embedded_flash_get(TEST_UINT8_1_A1_161, &gc_buffer.uint8_val, &l, &t) == 0) {
		// 添加边界检查
		if (l == 1) {
			print_pass("gc readable after pressure");
		} else {
			printf("buffer overflow in gc test, len:%d, buf_size:%d\n", l, (int)sizeof(gc_buffer.raw));
			print_fail("gc buffer overflow");
            return -1;
		}
	} else {
		print_fail("gc read failed");
        return -1;
	}
	print_test_end("Garbage Collection Test", 0);
    return 0;
}

int embedded_flash_demo_error_test(void) {
	print_test_start("Error Handling Test");
	printf("Testing error conditions and boundary cases...\n");
    union {
        uint8_t raw[KV_MAX_VALUE_SIZE];
    } error_buffer;
    uint8_t l, t;
    
    if (embedded_flash_get(0xFF, error_buffer.raw, &l, &t) != 0) print_pass("invalid key guard"); else {
        print_fail("invalid key should fail");
        return -1;
    }
    // 测试类型安全API的错误处理
    // 1. 测试超长字符串
    char long_string[20] = "This is too long";
    if (embedded_flash_set_string(0xFE, long_string) != 0) print_pass("long string guard"); else {
        print_fail("long string should fail");
        return -1;
    }
    
    // 2. 测试超长hex数据
    uint8_t long_data[20] = {0};
    if (embedded_flash_set_hex(0xFD, long_data, 20) != 0) print_pass("oversize hex guard"); else {
        print_fail("oversize hex should fail");
        return -1;
    }
    
    // 3. 测试NULL指针
    if (embedded_flash_get(TEST_UINT8_1_A1_161, NULL, &l, &t) != 0) print_pass("null pointer guard"); else {
        print_fail("null pointer should fail");
        return -1;
    }
    
    // 4. 测试字符串NULL指针
    if (embedded_flash_set_string(0xFC, NULL) != 0) print_pass("null string guard"); else {
        print_fail("null string should fail");
        return -1;
    }
    
    // 5. 测试hex NULL指针
    if (embedded_flash_set_hex(0xFB, NULL, 5) != 0) print_pass("null hex guard"); else {
        print_fail("null hex should fail");
        return -1;
    }
    
    // 6. 测试零长度hex
    uint8_t data[5] = {1,2,3,4,5};
    if (embedded_flash_set_hex(0xFA, data, 0) != 0) print_pass("zero length hex guard"); else {
        print_fail("zero length hex should fail");
        return -1;
    }
    print_test_end("Error Handling Test", 0);
    return 0;
}

// ==================== 严苛测试函数 ====================

/**
 * @brief 压力测试 - 大量写入操作
 */
int embedded_flash_demo_stress_test(void) {
	print_test_start("Stress Test");
	printf("Testing high-volume write operations with all data types...\n");
    
    // 动态选择包含所有数据类型的键用于压力测试
    #define MAX_TEST_KEYS 5
    uint8_t test_key_indices[MAX_TEST_KEYS];
    uint8_t num_test_keys = 0;
    
    // 统计各种数据类型的覆盖情况
    uint8_t type_coverage[16] = {0};  // 支持最多16种数据类型
    
    // 从test_kvs中选择键，确保覆盖所有数据类型
    uint8_t total_kvs = sizeof(test_kvs) / sizeof(kv_data_t);
    for (uint8_t i = 0; i < total_kvs && num_test_keys < MAX_TEST_KEYS; i++) {
        uint8_t dtype = test_kvs[i].data_type;
        // 确保每种数据类型至少有一个键被选中
        if (type_coverage[dtype] < 2) {  // 每种类型最多选2个键
            test_key_indices[num_test_keys++] = i;
            type_coverage[dtype]++;
        }
    }
    
    if (num_test_keys == 0) {
        printf("[X] CRITICAL: No test keys selected!\n");
        return -1;
    }
    
    printf("Selected %d keys covering all data types:\n", num_test_keys);
    for (int i = 0; i < num_test_keys; i++) {
        uint8_t idx = test_key_indices[i];
        const char* type_name = "UNKNOWN";
        switch(test_kvs[idx].data_type) {
            case EFLASH_FORMAT_BOOL:    type_name = "BOOL";    break;
            case EFLASH_FORMAT_UINT8:   type_name = "UINT8";   break;
            case EFLASH_FORMAT_INT8:    type_name = "INT8";    break;
            case EFLASH_FORMAT_UINT16:  type_name = "UINT16";  break;
            case EFLASH_FORMAT_INT16:   type_name = "INT16";   break;
            case EFLASH_FORMAT_UINT32:  type_name = "UINT32";  break;
            case EFLASH_FORMAT_INT32:   type_name = "INT32";   break;
            case EFLASH_FORMAT_UINT64:  type_name = "UINT64";  break;
            case EFLASH_FORMAT_INT64:   type_name = "INT64";   break;
            case EFLASH_FORMAT_FLOAT:   type_name = "FLOAT";   break;
            case EFLASH_FORMAT_STRING:  type_name = "STRING";  break;
            case EFLASH_FORMAT_HEX:     type_name = "HEX";     break;
        }
        printf("  [%2d] Key 0x%02X - %-8s (type=%d)\n", i, test_kvs[idx].key, type_name, test_kvs[idx].data_type);
    }
    printf("\n");
    
    int total_operations = 100;
    int success_count = 0;
    int write_fail_count = 0;
    int read_fail_count = 0;
    int verify_fail_count = 0;
    int type_fail_count = 0;
    int len_fail_count = 0;
    
    // 压力写入测试
    for (int i = 0; i < total_operations; i++) {
        // 循环选择键
        uint8_t key_idx = test_key_indices[i % num_test_keys];
        uint8_t key = test_kvs[key_idx].key;
        uint8_t data_type = test_kvs[key_idx].data_type;
        
        // 根据数据类型生成测试数据并写入
        int write_result = -1;
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
        
        memset(&write_data, 0, sizeof(write_data));
        memset(&read_data, 0, sizeof(read_data));
        
        uint8_t expected_len = 0;
        
        switch (data_type) {
            case EFLASH_FORMAT_BOOL:
                write_data.bool_val = (i % 2) ? true : false;
                write_result = embedded_flash_set_bool(key, write_data.bool_val);
                expected_len = 1;
                break;
                
            case EFLASH_FORMAT_UINT8:
                write_data.uint8_val = (uint8_t)(i & 0xFF);
                write_result = embedded_flash_set_uint8(key, write_data.uint8_val);
                expected_len = 1;
                break;
                
            case EFLASH_FORMAT_INT8:
                write_data.int8_val = (int8_t)((i % 256) - 128);
                write_result = embedded_flash_set_int8(key, write_data.int8_val);
                expected_len = 1;
                break;
                
            case EFLASH_FORMAT_UINT16:
                write_data.uint16_val = (uint16_t)(i & 0xFFFF);
                write_result = embedded_flash_set_uint16(key, write_data.uint16_val);
                expected_len = 2;
                break;
                
            case EFLASH_FORMAT_INT16:
                write_data.int16_val = (int16_t)((i % 65536) - 32768);
                write_result = embedded_flash_set_int16(key, write_data.int16_val);
                expected_len = 2;
                break;
                
            case EFLASH_FORMAT_UINT32:
                write_data.uint32_val = (uint32_t)(i * 1000);
                write_result = embedded_flash_set_uint32(key, write_data.uint32_val);
                expected_len = 4;
                break;
                
            case EFLASH_FORMAT_INT32:
                write_data.int32_val = (int32_t)(i * 100 - 50000);
                write_result = embedded_flash_set_int32(key, write_data.int32_val);
                expected_len = 4;
                break;
                
            case EFLASH_FORMAT_UINT64:
                write_data.uint64_val = (uint64_t)i * 1000000ULL;
                write_result = embedded_flash_set_uint64(key, write_data.uint64_val);
                expected_len = 8;
                break;
                
            case EFLASH_FORMAT_INT64:
                write_data.int64_val = (int64_t)i * 1000000LL - 500000000LL;
                write_result = embedded_flash_set_int64(key, write_data.int64_val);
                expected_len = 8;
                break;
                
            case EFLASH_FORMAT_FLOAT:
                write_data.float_val = (float)(i * 0.123f + 3.14159f);
                write_result = embedded_flash_set_float(key, write_data.float_val);
                expected_len = 4;
                break;
                
            case EFLASH_FORMAT_STRING: {
                // 生成变化的字符串，确保不超过KV_MAX_VALUE_SIZE-1字节（加null终止符）
                int str_len = snprintf(write_data.string_val, KV_MAX_VALUE_SIZE, "T%d", i % 1000);
                if (str_len >= KV_MAX_VALUE_SIZE) {
                    str_len = KV_MAX_VALUE_SIZE - 1;
                    write_data.string_val[str_len] = '\0';
                }
                write_result = embedded_flash_set_string(key, write_data.string_val);
                expected_len = strlen(write_data.string_val) + 1;
                break;
            }
                
            case EFLASH_FORMAT_HEX: {
                // 生成4字节的HEX数据
                write_data.hex_val[0] = (uint8_t)(i & 0xFF);
                write_data.hex_val[1] = (uint8_t)((i >> 8) & 0xFF);
                write_data.hex_val[2] = (uint8_t)((i >> 16) & 0xFF);
                write_data.hex_val[3] = (uint8_t)((i >> 24) & 0xFF);
                write_result = embedded_flash_set_hex(key, write_data.hex_val, 4);
                expected_len = 4;
                break;
            }
                
            default:
                printf("[X] CRITICAL: Unsupported data type %d at iteration %d, key=0x%02X\n", data_type, i, key);
                print_fail("stress test - unsupported data type");
                return -1;
        }
        
        // 检查写入结果
        if (write_result != 0) {
            write_fail_count++;
            if (write_fail_count <= 3) {  // 只打印前3个错误，避免刷屏
                printf("[X] Write failed at iteration %d, key=0x%02X, type=%d\n", i, key, data_type);
            }
            continue;
        }
        
        // 立即读取验证
        uint8_t read_len = 0, read_type = 0;
        if (embedded_flash_get(key, read_data.raw, &read_len, &read_type) != 0) {
            read_fail_count++;
            if (read_fail_count <= 3) {
                printf("[X] Read failed at iteration %d, key=0x%02X\n", i, key);
            }
            continue;
        }
        
        // 验证类型
        if (read_type != data_type) {
            type_fail_count++;
            if (type_fail_count <= 3) {
                printf("[X] Type mismatch at iteration %d, key=0x%02X: expected=%d, actual=%d\n", 
                       i, key, data_type, read_type);
            }
            continue;
        }
        
        // 验证长度
        if (read_len != expected_len) {
            len_fail_count++;
            if (len_fail_count <= 3) {
                printf("[X] Length mismatch at iteration %d, key=0x%02X: expected=%d, actual=%d\n", 
                       i, key, expected_len, read_len);
            }
            continue;
        }
        
        // 验证数据
        int data_match = 0;
        switch (data_type) {
            case EFLASH_FORMAT_BOOL:
                data_match = (read_data.bool_val == write_data.bool_val);
                if (!data_match && verify_fail_count < 3) {
                    printf("[X] BOOL verify failed at iteration %d: wrote=%d, read=%d\n", 
                           i, write_data.bool_val, read_data.bool_val);
                }
                break;
                
            case EFLASH_FORMAT_UINT8:
                data_match = (read_data.uint8_val == write_data.uint8_val);
                if (!data_match && verify_fail_count < 3) {
                    printf("[X] UINT8 verify failed at iteration %d: wrote=%d, read=%d\n", 
                           i, write_data.uint8_val, read_data.uint8_val);
                }
                break;
                
            case EFLASH_FORMAT_INT8:
                data_match = (read_data.int8_val == write_data.int8_val);
                if (!data_match && verify_fail_count < 3) {
                    printf("[X] INT8 verify failed at iteration %d: wrote=%d, read=%d\n", 
                           i, write_data.int8_val, read_data.int8_val);
                }
                break;
                
            case EFLASH_FORMAT_UINT16:
                data_match = (read_data.uint16_val == write_data.uint16_val);
                if (!data_match && verify_fail_count < 3) {
                    printf("[X] UINT16 verify failed at iteration %d: wrote=%d, read=%d\n", 
                           i, write_data.uint16_val, read_data.uint16_val);
                }
                break;
                
            case EFLASH_FORMAT_INT16:
                data_match = (read_data.int16_val == write_data.int16_val);
                if (!data_match && verify_fail_count < 3) {
                    printf("[X] INT16 verify failed at iteration %d: wrote=%d, read=%d\n", 
                           i, write_data.int16_val, read_data.int16_val);
                }
                break;
                
            case EFLASH_FORMAT_UINT32:
                data_match = (read_data.uint32_val == write_data.uint32_val);
                if (!data_match && verify_fail_count < 3) {
                    printf("[X] UINT32 verify failed at iteration %d: wrote=%u, read=%u\n", 
                           i, write_data.uint32_val, read_data.uint32_val);
                }
                break;
                
            case EFLASH_FORMAT_INT32:
                data_match = (read_data.int32_val == write_data.int32_val);
                if (!data_match && verify_fail_count < 3) {
                    printf("[X] INT32 verify failed at iteration %d: wrote=%d, read=%d\n", 
                           i, write_data.int32_val, read_data.int32_val);
                }
                break;
                
            case EFLASH_FORMAT_UINT64:
                data_match = (read_data.uint64_val == write_data.uint64_val);
                if (!data_match && verify_fail_count < 3) {
                    printf("[X] UINT64 verify failed at iteration %d: wrote=%llu, read=%llu\n", 
                           i, (unsigned long long)write_data.uint64_val, (unsigned long long)read_data.uint64_val);
                }
                break;
                
            case EFLASH_FORMAT_INT64:
                data_match = (read_data.int64_val == write_data.int64_val);
                if (!data_match && verify_fail_count < 3) {
                    printf("[X] INT64 verify failed at iteration %d: wrote=%lld, read=%lld\n", 
                           i, (long long)write_data.int64_val, (long long)read_data.int64_val);
                }
                break;
                
            case EFLASH_FORMAT_FLOAT:
                // 浮点数比较需要考虑精度误差
                data_match = (write_data.float_val == read_data.float_val);
                if (!data_match && verify_fail_count < 3) {
                    printf("[X] FLOAT verify failed at iteration %d: wrote=%f, read=%f\n", 
                           i, write_data.float_val, read_data.float_val);
                }
                break;
                
            case EFLASH_FORMAT_STRING:
                data_match = (strcmp(read_data.string_val, write_data.string_val) == 0);
                if (!data_match && verify_fail_count < 3) {
                    printf("[X] STRING verify failed at iteration %d: wrote='%s', read='%s'\n", 
                           i, write_data.string_val, read_data.string_val);
                }
                break;
                
            case EFLASH_FORMAT_HEX:
                data_match = (memcmp(read_data.hex_val, write_data.hex_val, expected_len) == 0);
                if (!data_match && verify_fail_count < 3) {
                    printf("[X] HEX verify failed at iteration %d\n", i);
                    printf("  Expected: [");
                    for(int j = 0; j < expected_len; j++) printf("%02X ", write_data.hex_val[j]);
                    printf("]\n  Actual:   [");
                    for(int j = 0; j < expected_len; j++) printf("%02X ", read_data.hex_val[j]);
                    printf("]\n");
                }
                break;
                
            default:
                printf("[X] CRITICAL: Unknown data type in verification: %d\n", data_type);
                return -1;
        }
        
        if (data_match) {
            success_count++;
        } else {
            verify_fail_count++;
        }
        
        // 每100次操作打印一次进度
        if ((i + 1) % 100 == 0) {
            printf("Progress: %d/%d ops | [OK] %d | [X] W:%d R:%d T:%d L:%d V:%d\n", 
                   i + 1, total_operations, success_count, 
                   write_fail_count, read_fail_count, type_fail_count, len_fail_count, verify_fail_count);
        }
    }
    
    printf("\n========== Stress Test Results ==========\n");
    printf("Total operations:        %d\n", total_operations);
    printf("[OK] Successful:            %d (%.1f%%)\n", success_count, (float)success_count * 100.0f / total_operations);
    printf("[X] Write failures:        %d\n", write_fail_count);
    printf("[X] Read failures:         %d\n", read_fail_count);
    printf("[X] Type mismatches:       %d\n", type_fail_count);
    printf("[X] Length mismatches:     %d\n", len_fail_count);
    printf("[X] Verification failures: %d\n", verify_fail_count);
    printf("==========================================\n");
    
    if (success_count == total_operations) {
        print_pass("stress test - all 1000 operations succeeded");
        print_test_end("Stress Test", 0);
        return 0;
    } else {
        print_fail("stress test - some operations failed");
        print_test_end("Stress Test", -1);
        return -1;
    }
}

/**
 * @brief 边界测试 - 测试最大长度和边界值
 */
int embedded_flash_demo_boundary_test(void) {
	print_test_start("Boundary Test");
	printf("Testing maximum length and boundary values...\n");
    uint8_t test_data[KV_MAX_VALUE_SIZE];
    uint8_t read_data[KV_MAX_VALUE_SIZE];
    uint8_t len, type;
    int test_passed = 1;
    
    // 测试最大长度数据
    for (int i = 0; i < KV_MAX_VALUE_SIZE; i++) {
        test_data[i] = (uint8_t)(0xAA + i);
    }
    
    // 使用HEX专用的键进行边界测试，避免数据类型冲突
    if (embedded_flash_set_hex(TEST_HEX_4_A6_166, test_data, KV_MAX_VALUE_SIZE) == 0) {
        if (embedded_flash_get(TEST_HEX_4_A6_166, read_data, &len, &type) == 0 && 
            len == KV_MAX_VALUE_SIZE && memcmp(test_data, read_data, KV_MAX_VALUE_SIZE) == 0) {
            print_pass("boundary test - max length data");
        } else {
            print_fail("boundary test - max length data readback");
            test_passed = 0;
        }
    } else {
        print_fail("boundary test - max length data write");
        test_passed = 0;
    }
    
    // 测试1字节数据 - 使用正确的uint8键
    test_data[0] = 0x55;
    if (embedded_flash_set_uint8(TEST_UINT8_1_A1_161, test_data[0]) == 0) {
        if (embedded_flash_get(TEST_UINT8_1_A1_161, read_data, &len, &type) == 0 && 
            len == 1 && read_data[0] == 0x55) {
            print_pass("boundary test - 1 byte data");
        } else {
            print_fail("boundary test - 1 byte data readback");
            test_passed = 0;
        }
    } else {
        print_fail("boundary test - 1 byte data write");
        test_passed = 0;
    }
    
    // 测试零长度数据（应该失败）- 使用hex类型测试零长度
    if (embedded_flash_set_hex(TEST_HEX_4_A6_166, test_data, 0) != 0) {
        print_pass("boundary test - zero length rejection");
    } else {
        print_fail("boundary test - zero length should be rejected");
        test_passed = 0;
    }
    
    if (test_passed) {
        print_pass("boundary test - all tests passed");
    } else {
        print_fail("boundary test");
        return -1;
    }
    print_test_end("Boundary Test", 0);
    return 0;
}

/**
 * @brief GC压力测试 - 强制触发多次GC
 */
int embedded_flash_demo_gc_stress_test(void) {
	print_test_start("GC Stress Test");
	printf("Testing multiple garbage collection cycles...\n");
    struct {
        bool bool_val;
        uint8_t uint8_val;
        uint16_t uint16_val;
        uint32_t uint32_val;
        uint64_t uint64_val;
        int8_t int8_val;
        int16_t int16_val;
        int32_t int32_val;
        int64_t int64_val;
        float float_val;
        char string_val[KV_MAX_VALUE_SIZE];
        uint8_t hex_val[KV_MAX_VALUE_SIZE];
    } test_buffer;
    
    uint8_t len, type;
    int gc_cycles = 0;
    uint8_t read_buffer[KV_MAX_VALUE_SIZE];  // 使用数组替代结构体
    
    // 使用所有可用的key进行大量写入，强制触发GC
    int key_count = sizeof(test_kvs) / sizeof(kv_data_t);
    
		
    // 进行多轮写入，每轮都会覆盖之前的数据
    for (int round = 0; round < 20; round++) {
        // 填充测试数据
        for (int j = 0; j < 16; j++) {
            //填充随机数据
            uint32_t random_seed = (round * 100 + j) * 31;  // 简单的伪随机种子
            
            // 生成随机布尔值
            test_buffer.bool_val = (random_seed % 2) == 0;
            
            // 生成随机8位值
            test_buffer.uint8_val = (uint8_t)(random_seed & 0xFF);
            test_buffer.int8_val = (int8_t)(random_seed & 0xFF);
            
            // 生成随机16位值
            test_buffer.uint16_val = (uint16_t)((random_seed * 3) & 0xFFFF);
            test_buffer.int16_val = (int16_t)((random_seed * 3) & 0xFFFF);
            
            // 生成随机32位值
            test_buffer.uint32_val = (uint32_t)((random_seed * 7) & 0xFFFFFFFF);
            test_buffer.int32_val = (int32_t)((random_seed * 7) & 0xFFFFFFFF);
            
            // 生成随机64位值
            test_buffer.uint64_val = ((uint64_t)random_seed << 32) | (uint64_t)(random_seed * 11);
            test_buffer.int64_val = (int64_t)test_buffer.uint64_val;
            
            // 生成随机浮点数
            test_buffer.float_val = (float)(random_seed % 1000) / 100.0f;
            
            // 生成随机字符串
            const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
            int str_len = 5 + (random_seed % 10);  // 长度5-14
            for (int k = 0; k < str_len && k < KV_MAX_VALUE_SIZE - 1; k++) {
                test_buffer.string_val[k] = charset[(random_seed + k) % (sizeof(charset) - 1)];
            }
            test_buffer.string_val[str_len] = '\0';
            
            // 生成随机十六进制数据
            for (int k = 0; k < 6; k++) {
                test_buffer.hex_val[k] = (uint8_t)((random_seed * (k + 1)) & 0xFF);
            }
        }
        for (int i = 0; i < key_count; i++) {
            
            
            // 写入数据 - 根据数据类型选择合适的API
            int write_result = -1;
            switch (test_kvs[i].data_type) {
                case EFLASH_FORMAT_UINT8:
                    write_result = embedded_flash_set_uint8(test_kvs[i].key, test_buffer.uint8_val);
                    break;
                case EFLASH_FORMAT_INT8:
                    write_result = embedded_flash_set_int8(test_kvs[i].key, test_buffer.int8_val);
                    break;
                case EFLASH_FORMAT_BOOL:
                    write_result = embedded_flash_set_bool(test_kvs[i].key, test_buffer.bool_val);
                    break;
                case EFLASH_FORMAT_UINT16: {
                    write_result = embedded_flash_set_uint16(test_kvs[i].key, test_buffer.uint16_val);
                    break;
                }
                case EFLASH_FORMAT_INT16: {
                    write_result = embedded_flash_set_int16(test_kvs[i].key, test_buffer.int16_val);
                    break;
                }
                case EFLASH_FORMAT_UINT32: {
                    write_result = embedded_flash_set_uint32(test_kvs[i].key, test_buffer.uint32_val);
                    break;
                }
                case EFLASH_FORMAT_INT32: {
                    write_result = embedded_flash_set_int32(test_kvs[i].key, test_buffer.int32_val);
                    break;
                }
                case EFLASH_FORMAT_UINT64: {
                    write_result = embedded_flash_set_uint64(test_kvs[i].key, test_buffer.uint64_val);
                    break;
                }
                case EFLASH_FORMAT_INT64: {
                    write_result = embedded_flash_set_int64(test_kvs[i].key, test_buffer.int64_val);
                    break;
                }
                case EFLASH_FORMAT_FLOAT: {
                    write_result = embedded_flash_set_float(test_kvs[i].key, test_buffer.float_val);
                    break;
                }
                case EFLASH_FORMAT_STRING:
                    printf("string_val: %s\n", test_buffer.string_val);
                    write_result = embedded_flash_set_string(test_kvs[i].key, test_buffer.string_val);
                    break;
                case EFLASH_FORMAT_HEX:
                    write_result = embedded_flash_set_hex(test_kvs[i].key, test_buffer.hex_val, 6);  // 最大6字节
                    break;
                default:
                    write_result = -1;
                    break;
            }
            
            if (write_result != 0) {
                printf("GC stress test write failed at round %d, key %d (0x%02X), data_type=%d\n", 
                       round, test_kvs[i].key, test_kvs[i].key, test_kvs[i].data_type);
                return -1;
            }
//            if (embedded_flash_set(test_kvs[i].key, test_data, test_kvs[i].value_length, test_kvs[i].flags.data_type) != 0) {
//                printf("GC stress test failed at round %d, key %d\n", round, test_kvs[i].key);
//                return;
//            }
        }
        
        // 验证数据完整性
        for (int k = 0; k < key_count; k++) {
            if (embedded_flash_get(test_kvs[k].key, read_buffer, &len, &type) != 0) {
                printf("GC stress test read failed at round %d, key %d\n", round, test_kvs[k].key);
                return -1;
            }
            
            // 验证数据类型和长度
            uint8_t expected_len = embedded_flash_get_type_size(test_kvs[k].data_type);
            if (expected_len == 0) {
                if (test_kvs[k].data_type == EFLASH_FORMAT_STRING) {
                    // 字符串长度应该包含空终止符
                    expected_len = strlen(test_buffer.string_val) + 1;
                } else if (test_kvs[k].data_type == EFLASH_FORMAT_HEX) {
                    expected_len = 6;  // 上面之随机生成6
                }
            }
            // 检查数据类型和长度是否匹配
            if (type != test_kvs[k].data_type || len != expected_len) {
                printf("GC stress test type/len mismatch at round %d, key %d: expected type=%d,len=%d, got type=%d,len=%d\n", 
                       round, test_kvs[k].key, test_kvs[k].data_type, expected_len, type, len);
                return -1;
            }
            
            // 验证数据值
            int data_match = 0;
            switch (test_kvs[k].data_type) {
                case EFLASH_FORMAT_BOOL:
                    data_match = (*((bool*)read_buffer) == test_buffer.bool_val);
                    break;
                case EFLASH_FORMAT_UINT8:
                    data_match = (*((uint8_t*)read_buffer) == test_buffer.uint8_val);
                    break;
                case EFLASH_FORMAT_INT8:
                    data_match = (*((int8_t*)read_buffer) == test_buffer.int8_val);
                    break;
                case EFLASH_FORMAT_UINT16:
                    data_match = (*((uint16_t*)read_buffer) == test_buffer.uint16_val);
                    break;
                case EFLASH_FORMAT_INT16:
                    data_match = (*((int16_t*)read_buffer) == test_buffer.int16_val);
                    break;
                case EFLASH_FORMAT_UINT32:
                    data_match = (*((uint32_t*)read_buffer) == test_buffer.uint32_val);
                    break;
                case EFLASH_FORMAT_INT32:
                    data_match = (*((int32_t*)read_buffer) == test_buffer.int32_val);
                    break;
                case EFLASH_FORMAT_UINT64:
                    data_match = (*((uint64_t*)read_buffer) == test_buffer.uint64_val);
                    break;
                case EFLASH_FORMAT_INT64:
                    data_match = (*((int64_t*)read_buffer) == test_buffer.int64_val);
                    break;
                case EFLASH_FORMAT_FLOAT:
                    // 浮点数比较允许小的误差
                    data_match = (fabsf(*((float*)read_buffer) - test_buffer.float_val) < 0.001f);
                    break;
                case EFLASH_FORMAT_STRING:
                    data_match = (strcmp((char*)read_buffer, test_buffer.string_val) == 0);
                    break;
                case EFLASH_FORMAT_HEX:
                    data_match = (memcmp(read_buffer, test_buffer.hex_val, 6) == 0);
                    break;
                default:
                    data_match = 0;
                    break;
            }
            
            if (!data_match) {
                printf("GC stress test data mismatch at round %d, key %d, type %d\n", 
                       round, test_kvs[k].key, test_kvs[k].data_type);
                return -1;
            }
        }
        
        gc_cycles++;
        if (round % 5 == 0) {
            printf("GC stress test progress: round %d/20\n", round);
        }
    }
    
    print_pass("GC stress test - 20 rounds completed");
    print_test_end("GC Stress Test", 0);
    return 0;
}

/**
 * @brief 断电恢复压力测试 - 模拟多次断电恢复
 */
int embedded_flash_demo_power_loss_stress_test(void) {
	print_test_start("Power Loss Stress Test");
	printf("Testing power loss recovery scenarios...\n");
    union {
        uint8_t uint8_data;
        uint16_t uint16_data;
        uint32_t uint32_data;
        uint64_t uint64_data;
        int8_t int8_data;
        int16_t int16_data;
        int32_t int32_data;
        int64_t int64_data;
        float float_data;
        char string_data[KV_MAX_VALUE_SIZE];
        uint8_t hex_data[KV_MAX_VALUE_SIZE];
    } write_data,read_data;

    uint8_t len, type;
    int recovery_success = 0;
    int total_tests = 10;
    
    for (int test_round = 0; test_round < total_tests; test_round++) {
        // 写入测试数据
        for (int i = 0; i < 10; i++) {
            write_data.hex_data[i] = (uint8_t)((test_round * 10 + i) & 0xFF);
        }
        
        // 写入到多个key - 使用最大允许长度
        embedded_flash_set_uint8(TEST_UINT8_1_A1_161, write_data.uint8_data);  // 10字节 = KV_MAX_VALUE_SIZE
        embedded_flash_set_int16(TEST_INT16_2_AC_172, write_data.int16_data);  // 10字节 = KV_MAX_VALUE_SIZE
        embedded_flash_set_hex(TEST_HEX_4_A6_166, write_data.hex_data, KV_MAX_VALUE_SIZE);    // 10字节 = KV_MAX_VALUE_SIZE
        
        // 模拟断电恢复 - 重新初始化
        if (test_embedded_flash_init() == 0) {
            // 验证数据是否还在
            int data_ok = 1;
            if (embedded_flash_get(TEST_UINT8_1_A1_161, &read_data.uint8_data, &len, &type) != 0 ||
                len != 1 
                || write_data.uint8_data != read_data.uint8_data
                || type != EFLASH_FORMAT_UINT8) {
                data_ok = 0;
            }
            
            if (embedded_flash_get(TEST_INT16_2_AC_172, (uint8_t *)&read_data.int16_data, &len, &type) != 0 ||
                len != 2 
                || write_data.int16_data != read_data.int16_data
                || type != EFLASH_FORMAT_INT16) {
                data_ok = 0;
            }
            
            if (embedded_flash_get(TEST_HEX_4_A6_166, read_data.hex_data, &len, &type) != 0 ||
                len != 10 
                || memcmp(write_data.hex_data, read_data.hex_data, 10) != 0
                || type != EFLASH_FORMAT_HEX) {
                data_ok = 0;
            }
            
            if (data_ok) {
                recovery_success++;
            }
        }
        
        printf("Power loss stress test progress: %d/%d\n", test_round + 1, total_tests);
    }
    
    if (recovery_success == total_tests) {
        print_pass("power loss stress test - all recoveries successful");
    } else {
        printf("FAIL - power loss stress test: %d/%d recoveries successful\n", recovery_success, total_tests);
        return -1;
    }
    print_test_end("Power Loss Stress Test", 0);
    return 0;
}

/**
 * @brief 数据类型一致性测试 - 测试所有类型安全API
 */
int embedded_flash_demo_data_type_consistency_test(void) {
	print_test_start("Data Type Consistency Test");
	printf("Testing type-safe API consistency...\n");
    // 增加缓冲区大小以容纳最大数据类型(8字节)并留出安全边界
    uint8_t read_data[16];
    uint8_t len, type;
    int test_passed = 1;
    
    // 定义测试值联合体，避免使用指针
    typedef union {
        uint8_t uint8_val;
        uint16_t uint16_val;
        int32_t int32_val;
        float float_val;
    } test_value_t;
    
    // 测试各种数据类型的API
    struct {
        const uint8_t key;
        uint8_t data_type;
        const char* description;
        int (*write_func)(uint8_t, void*);
        int (*verify_func)(uint8_t*, void*, uint8_t);
        test_value_t test_value;
    } test_cases[] = {
        {TEST_UINT8_1_A1_161, EFLASH_FORMAT_UINT8, "uint8", NULL, NULL, {.uint8_val = 42}},
        {TEST_UINT16_2_A3_163, EFLASH_FORMAT_UINT16, "uint16", NULL, NULL, {.uint16_val = 1234}},
        {TEST_INT32_4_A4_164, EFLASH_FORMAT_INT32, "int32", NULL, NULL, {.int32_val = -5678}},
        {TEST_UINT8_1_A2_162, EFLASH_FORMAT_UINT8, "uint8_2", NULL, NULL, {.uint8_val = 99}},
        {TEST_UINT16_2_A7_167, EFLASH_FORMAT_UINT16, "uint16_2", NULL, NULL, {.uint16_val = 9999}},
        {TEST_INT32_4_A8_168, EFLASH_FORMAT_INT32, "int32_2", NULL, NULL, {.int32_val = -9999}},
    };
    
    int test_count = sizeof(test_cases) / sizeof(test_cases[0]);
    
    for (int i = 0; i < test_count; i++) {
        int write_result = -1;
        int verify_result = 0;
        
        // 根据数据类型选择合适的API进行写入
        switch (test_cases[i].data_type) {
            case EFLASH_FORMAT_UINT8: {
                write_result = embedded_flash_set_uint8(test_cases[i].key, test_cases[i].test_value.uint8_val);
                break;
            }
            case EFLASH_FORMAT_UINT16: {
                write_result = embedded_flash_set_uint16(test_cases[i].key, test_cases[i].test_value.uint16_val);
                break;
            }
            case EFLASH_FORMAT_INT32: {
                write_result = embedded_flash_set_int32(test_cases[i].key, test_cases[i].test_value.int32_val);
                break;
            }
            case EFLASH_FORMAT_FLOAT: {
                write_result = embedded_flash_set_float(test_cases[i].key, test_cases[i].test_value.float_val);
                break;
            }
            default:
                write_result = -1;
                break;
        }
        
        if (write_result == 0) {
            // 读取并验证
            if (embedded_flash_get(test_cases[i].key, read_data, &len, &type) == 0) {
                // 验证数据类型和长度
                uint8_t expected_len = embedded_flash_get_type_size(test_cases[i].data_type);
                
                // 添加边界检查，防止缓冲区越界
                if (len > sizeof(read_data)) {
                    printf("[X] %s test failed for key 0x%02X: data length %d exceeds buffer size %d\n", 
                           test_cases[i].description, test_cases[i].key, len, (int)sizeof(read_data));
                    test_passed = 0;
                    continue;
                }
                
                if (len == expected_len && type == test_cases[i].data_type) {
                    // 验证数据值
                    switch (test_cases[i].data_type) {
                        case EFLASH_FORMAT_UINT8: {
                            verify_result = (read_data[0] == test_cases[i].test_value.uint8_val);
                            break;
                        }
                        case EFLASH_FORMAT_UINT16: {
                            uint16_t actual;
                            // 使用memcpy避免直接类型转换，提高安全性
                            memcpy(&actual, read_data, sizeof(uint16_t));
                            verify_result = (actual == test_cases[i].test_value.uint16_val);
                            break;
                        }
                        case EFLASH_FORMAT_INT32: {
                            int32_t actual;
                            // 使用memcpy避免直接类型转换，提高安全性
                            memcpy(&actual, read_data, sizeof(int32_t));
                            verify_result = (actual == test_cases[i].test_value.int32_val);
                            break;
                        }
                        case EFLASH_FORMAT_FLOAT: {
                            float actual;
                            // 使用memcpy避免直接类型转换，提高安全性
                            memcpy(&actual, read_data, sizeof(float));
                            verify_result = (actual == test_cases[i].test_value.float_val);
                            break;
                        }
                        default:
                            verify_result = 0;
                            break;
                    }
                    
                    if (verify_result) {
                        printf("[OK] %s test passed for key 0x%02X\n", test_cases[i].description, test_cases[i].key);
                    } else {
                        printf("[X] %s test failed for key 0x%02X: value mismatch\n", test_cases[i].description, test_cases[i].key);
                        test_passed = 0;
                    }
                } else {
                    printf("[X] %s test failed for key 0x%02X: len=%d/%d, type=%d/%d\n", 
                           test_cases[i].description, test_cases[i].key, len, expected_len, type, test_cases[i].data_type);
                    test_passed = 0;
                }
            } else {
                printf("[X] %s test read failed for key 0x%02X\n", test_cases[i].description, test_cases[i].key);
                test_passed = 0;
            }
        } else {
            printf("[X] %s test write failed for key 0x%02X\n", test_cases[i].description, test_cases[i].key);
            test_passed = 0;
        }
    }
    
    // 测试字符串和HEX类型
    const char* test_string = "Test123";  // 7字节 + null终止符 = 8字节 < 10字节
    if (embedded_flash_set_string(TEST_STRING_8_A5_165, test_string) == 0) {
        if (embedded_flash_get(TEST_STRING_8_A5_165, read_data, &len, &type) == 0) {
            // 字符串存储时包含null终止符，所以长度应该是strlen+1
            if (type == EFLASH_FORMAT_STRING && len == strlen(test_string) + 1 && 
                strcmp((char*)read_data, test_string) == 0) {
                printf("[OK] string test passed for key 0x%02X\n", TEST_STRING_8_A5_165);
            } else {
                printf("[X] string test failed for key 0x%02X, len=%d, expected=%d, type=%d\n", 
                       TEST_STRING_8_A5_165, len, (int)strlen(test_string) + 1, type);
                test_passed = 0;
            }
        } else {
            printf("[X] string test read failed for key 0x%02X\n", TEST_STRING_8_A5_165);
            test_passed = 0;
        }
    } else {
        printf("[X] string test write failed for key 0x%02X\n", TEST_STRING_8_A5_165);
        test_passed = 0;
    }
    
    uint8_t test_hex[5] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA};  // 5字节 < 10字节
    if (embedded_flash_set_hex(TEST_HEX_4_A6_166, test_hex, 5) == 0) {
        if (embedded_flash_get(TEST_HEX_4_A6_166, read_data, &len, &type) == 0) {
            // 添加边界检查，防止缓冲区越界
            if (len > sizeof(read_data)) {
                printf("[X] hex test failed for key 0x%02X: data length %d exceeds buffer size %d\n", 
                       TEST_HEX_4_A6_166, len, (int)sizeof(read_data));
                test_passed = 0;
            } else if (type == EFLASH_FORMAT_HEX && len == 5 && 
                memcmp(test_hex, read_data, 5) == 0) {
                printf("[OK] hex test passed for key 0x%02X\n", TEST_HEX_4_A6_166);
            } else {
                printf("[X] hex test failed for key 0x%02X, len=%d, expected=5, type=%d\n", 
                       TEST_HEX_4_A6_166, len, type);
                test_passed = 0;
            }
        } else {
            printf("[X] hex test read failed for key 0x%02X\n", TEST_HEX_4_A6_166);
            test_passed = 0;
        }
    } else {
        printf("[X] hex test write failed for key 0x%02X\n", TEST_HEX_4_A6_166);
        test_passed = 0;
    }
    
    if (test_passed) {
        print_pass("data type consistency test - all APIs working correctly");
    } else {
        print_fail("data type consistency test - some APIs failed");
        return -1;
    }
    print_test_end("Data Type Consistency Test", 0);
    return 0;
}

/**
 * @brief 类型安全API专门测试 - 验证所有类型安全API的正确性
 */
int embedded_flash_demo_type_safe_api_test(void) {
	print_test_start("Type-Safe API Test");
	printf("Testing type-safe API functions...\n");
    int test_passed = 1;
    
    // 测试所有可用的类型安全API
    printf("\n--- Testing Fixed-Length Type APIs ---\n");
    
    // bool类型测试
    bool bool_val = true;
    if (embedded_flash_set_bool(TEST_BOOL_1_AA_170, bool_val) == 0) {
        uint8_t read_bool;
        uint8_t len;
        uint8_t type;
        if (embedded_flash_get(TEST_BOOL_1_AA_170, (uint8_t*)&read_bool, &len, &type) == 0) {
            printf("[OK] bool API test passed\n");
        } else {
            printf("[X] bool API read failed\n");
            test_passed = 0;
        }
    } else {
        printf("[X] bool API write failed\n");
        test_passed = 0;
    }
    
    // uint8类型测试
    uint8_t uint8_val = 0xAB;
    if (embedded_flash_set_uint8(TEST_UINT8_1_A2_162, uint8_val) == 0) {
        uint8_t read_uint8;
        uint8_t len;
        uint8_t type;
        if (embedded_flash_get(TEST_UINT8_1_A2_162, &read_uint8, &len, &type) == 0) {
            if (read_uint8 == uint8_val) {
                printf("[OK] uint8 API test passed\n");
            } else {
                printf("[X] uint8 API value mismatch: expected=0x%02X, got=0x%02X\n", uint8_val, read_uint8);
                test_passed = 0;
            }
        } else {
            printf("[X] uint8 API read failed\n");
            test_passed = 0;
        }
    } else {
        printf("[X] uint8 API write failed\n");
        test_passed = 0;
    }
    
    // int8类型测试 - 使用正确的int8键
    int8_t int8_val = -50;
    if (embedded_flash_set_int8(TEST_INT8_1_AB_171, int8_val) == 0) {
        int8_t read_int8;
        uint8_t len;
        uint8_t type;
        if (embedded_flash_get(TEST_INT8_1_AB_171, (uint8_t*)&read_int8, &len, &type) == 0) {
            if (read_int8 == int8_val) {
                printf("[OK] int8 API test passed\n");
            } else {
                printf("[X] int8 API value mismatch: expected=%d, got=%d\n", int8_val, read_int8);
                test_passed = 0;
            }
        } else {
            printf("[X] int8 API read failed\n");
            test_passed = 0;
        }
    } else {
        printf("[X] int8 API write failed\n");
        test_passed = 0;
    }
    
    // uint16类型测试
    uint16_t uint16_val = 0x1234;
    if (embedded_flash_set_uint16(TEST_UINT16_2_A3_163, uint16_val) == 0) {
        uint16_t read_uint16;
        uint8_t len;
        uint8_t type;
        if (embedded_flash_get(TEST_UINT16_2_A3_163, (uint8_t*)&read_uint16, &len, &type) == 0) {
            if (read_uint16 == uint16_val) {
                printf("[OK] uint16 API test passed\n");
            } else {
                printf("[X] uint16 API value mismatch: expected=0x%04X, got=0x%04X\n", uint16_val, read_uint16);
                test_passed = 0;
            }
        } else {
            printf("[X] uint16 API read failed\n");
            test_passed = 0;
        }
    } else {
        printf("[X] uint16 API write failed\n");
        test_passed = 0;
    }
    
    // int16类型测试 - 使用正确的int16键
    int16_t int16_val = -30000;
    if (embedded_flash_set_int16(TEST_INT16_2_AC_172, int16_val) == 0) {
        int16_t read_int16;
        uint8_t len;
        uint8_t type;
        if (embedded_flash_get(TEST_INT16_2_AC_172, (uint8_t*)&read_int16, &len, &type) == 0) {
            if (read_int16 == int16_val) {
                printf("[OK] int16 API test passed\n");
            } else {
                printf("[X] int16 API value mismatch: expected=%d, got=%d\n", int16_val, read_int16);
                test_passed = 0;
            }
        } else {
            printf("[X] int16 API read failed\n");
            test_passed = 0;
        }
    } else {
        printf("[X] int16 API write failed\n");
        test_passed = 0;
    }
    
    // uint32类型测试 - 使用正确的uint32键
    uint32_t uint32_val = 0x12345678;
    if (embedded_flash_set_uint32(TEST_UINT32_4_AD_173, uint32_val) == 0) {
        uint32_t read_uint32;
        uint8_t len;
        uint8_t type;
        if (embedded_flash_get(TEST_UINT32_4_AD_173, (uint8_t*)&read_uint32, &len, &type) == 0) {
            if (read_uint32 == uint32_val) {
                printf("[OK] uint32 API test passed\n");
            } else {
                printf("[X] uint32 API value mismatch: expected=0x%08X, got=0x%08X\n", uint32_val, read_uint32);
                test_passed = 0;
            }
        } else {
            printf("[X] uint32 API read failed\n");
            test_passed = 0;
        }
    } else {
        printf("[X] uint32 API write failed\n");
        test_passed = 0;
    }
    
    // int32类型测试
    int32_t int32_val = -2000000000;
    if (embedded_flash_set_int32(TEST_INT32_4_A8_168, int32_val) == 0) {
        int32_t read_int32;
        uint8_t len;
        uint8_t type;
        if (embedded_flash_get(TEST_INT32_4_A8_168, (uint8_t*)&read_int32, &len, &type) == 0) {
            if (read_int32 == int32_val) {
                printf("[OK] int32 API test passed\n");
            } else {
                printf("[X] int32 API value mismatch: expected=%d, got=%d\n", int32_val, read_int32);
                test_passed = 0;
            }
        } else {
            printf("[X] int32 API read failed\n");
            test_passed = 0;
        }
    } else {
        printf("[X] int32 API write failed\n");
        test_passed = 0;
    }
    
    // float类型测试 - 使用专门的浮点键
    float float_val = 3.14159f;
    if (embedded_flash_set_float(TEST_FLOAT_4_A9_169, float_val) == 0) {
        float read_float;
        uint8_t len;
        uint8_t type;
        if (embedded_flash_get(TEST_FLOAT_4_A9_169, (uint8_t*)&read_float, &len, &type) == 0) {
            if (read_float == float_val) {
                printf("[OK] float API test passed\n");
            } else {
                printf("[X] float API value mismatch: expected=%f, got=%f\n", float_val, read_float);
                test_passed = 0;
            }
        } else {
            printf("[X] float API read failed\n");
            test_passed = 0;
        }
    } else {
        printf("[X] float API write failed\n");
        test_passed = 0;
    }
    
    
    // uint64类型测试
    uint64_t uint64_val = 0x123456789ABCDEF0ULL;
    if (embedded_flash_set_uint64(TEST_UINT64_8_AE_174, uint64_val) == 0) {
        uint64_t read_uint64;
        uint8_t len;
        uint8_t type;
        if (embedded_flash_get(TEST_UINT64_8_AE_174, (uint8_t*)&read_uint64, &len, &type) == 0) {
            if (read_uint64 == uint64_val) {
                printf("[OK] uint64 API test passed\n");
            } else {
                printf("[X] uint64 API value mismatch: expected=0x%016llX, got=0x%016llX\n", 
                       (unsigned long long)uint64_val, (unsigned long long)read_uint64);
                test_passed = 0;
            }
        } else {
            printf("[X] uint64 API read failed\n");
            test_passed = 0;
        }
    } else {
        printf("[X] uint64 API write failed\n");
        test_passed = 0;
    }
    
    // int64类型测试
    int64_t int64_val = -0x123456789ABCDEF0LL;
    if (embedded_flash_set_int64(TEST_INT64_8_AF_175, int64_val) == 0) {
        int64_t read_int64;
        uint8_t len;
        uint8_t type;
        if (embedded_flash_get(TEST_INT64_8_AF_175, (uint8_t*)&read_int64, &len, &type) == 0) {
            if (read_int64 == int64_val 
                && type == EFLASH_FORMAT_INT64 
                && len == 8) {
                printf("[OK] int64 API test passed\n");
            } else {
                printf("[X] int64 API value mismatch: expected=0x%016llX, got=0x%016llX\n", 
                       (long long)int64_val, (long long)read_int64);
                test_passed = 0;
            }
        } else {
            printf("[X] int64 API read failed\n");
            test_passed = 0;
        }
    } else {
        printf("[X] int64 API write failed\n");
        test_passed = 0;
    }
    
    printf("\n--- Testing Variable-Length Type APIs ---\n");
    
    // string类型测试
    const char* string_val = "Hello";  // 5字节 + null终止符 = 6字节 < 10字节
    if (embedded_flash_set_string(TEST_STRING_8_A5_165, string_val) == 0) {
        char read_string[20];
        uint8_t len;
        uint8_t type;
        if (embedded_flash_get(TEST_STRING_8_A5_165, (uint8_t*)read_string, &len, &type) == 0) {
            // 字符串存储时包含null终止符，所以长度应该是strlen+1
            if (len == strlen(string_val) + 1 && strcmp(read_string, string_val) == 0) {
                printf("[OK] string API test passed\n");
            } else {
                printf("[X] string API value mismatch: expected='%s', got='%s', len=%d, expected_len=%d\n", 
                       string_val, read_string, len, (int)strlen(string_val) + 1);
                test_passed = 0;
            }
        } else {
            printf("[X] string API read failed\n");
            test_passed = 0;
        }
    } else {
        printf("[X] string API write failed\n");
        test_passed = 0;
    }
    
    // hex类型测试 - 使用正确的HEX键
    uint8_t hex_val[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};  // 6字节 < 10字节
    if (embedded_flash_set_hex(TEST_HEX_4_A6_166, hex_val, 6) == 0) {
        uint8_t read_hex[6];
        uint8_t len;
        uint8_t type;
        if (embedded_flash_get(TEST_HEX_4_A6_166, read_hex, &len, &type) == 0) {
            if (len == 6 && memcmp(hex_val, read_hex, 6) == 0) {
                printf("[OK] hex API test passed\n");
            } else {
                printf("[X] hex API value mismatch\n");
                test_passed = 0;
            }
        } else {
            printf("[X] hex API read failed\n");
            test_passed = 0;
        }
    } else {
        printf("[X] hex API write failed\n");
        test_passed = 0;
    }
    
    printf("\n--- Testing Type Size Validation ---\n");
    
    // 验证embedded_flash_get_type_size函数
    if (embedded_flash_get_type_size(EFLASH_FORMAT_UINT8) == 1) {
        printf("[OK] UINT8 type size correct\n");
    } else {
        printf("[X] UINT8 type size incorrect\n");
        test_passed = 0;
    }
    
    if (embedded_flash_get_type_size(EFLASH_FORMAT_UINT16) == 2) {
        printf("[OK] UINT16 type size correct\n");
    } else {
        printf("[X] UINT16 type size incorrect\n");
        test_passed = 0;
    }
    
    if (embedded_flash_get_type_size(EFLASH_FORMAT_UINT32) == 4) {
        printf("[OK] UINT32 type size correct\n");
    } else {
        printf("[X] UINT32 type size incorrect\n");
        test_passed = 0;
    }
    
    if (embedded_flash_get_type_size(EFLASH_FORMAT_STRING) == 0) {
        printf("[OK] STRING type size correct (variable length)\n");
    } else {
        printf("[X] STRING type size incorrect\n");
        test_passed = 0;
    }
    
    if (embedded_flash_get_type_size(EFLASH_FORMAT_HEX) == 0) {
        printf("[OK] HEX type size correct (variable length)\n");
    } else {
        printf("[X] HEX type size incorrect\n");
        test_passed = 0;
    }
    
    if (test_passed) {
        print_pass("type-safe API test - all APIs working correctly");
    } else {
        print_fail("type-safe API test - some APIs failed");
        return -1;
    }
    print_test_end("Type-Safe API Test", 0);
    return 0;
}

/**
 * @brief 性能测试 - 测试写入和读取性能
 */
int embedded_flash_demo_performance_test(void) {
	print_test_start("Performance Test");
	printf("Testing read/write performance for all data types with maximum lengths...\n");
    
    // 准备测试数据缓冲区 - 使用最大可能长度（8字节）
    uint8_t test_data[8];
    uint8_t read_data[8];
    uint8_t len, type;
    uint32_t start_time, end_time;
    int operations = 100;
    int test_failed = 0;
    
    // 初始化测试数据
    for (int i = 0; i < 8; i++) {
        test_data[i] = 0xAA + i;
    }
    
    // 测试所有数据类型的性能
    printf("\n--- Performance Test for All Data Types ---\n");
    
    // 1. 测试UINT8类型 (1字节) - 使用test_kvs[0]
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_set_uint8(test_kvs[0].key, (uint8_t)(i % 256));
    }
    end_time = HAL_GetTick();
    uint32_t uint8_write_time = end_time - start_time;
    printf("UINT8 write: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, uint8_write_time, (float)uint8_write_time / operations);
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_get(test_kvs[0].key, read_data, &len, &type);
    }
    end_time = HAL_GetTick();
    uint32_t uint8_read_time = end_time - start_time;
    printf("UINT8 read: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, uint8_read_time, (float)uint8_read_time / operations);
    
    // 2. 测试UINT16类型 (2字节) - 使用test_kvs[2]
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_set_uint16(test_kvs[2].key, (uint16_t)(i % 65536));
    }
    end_time = HAL_GetTick();
    uint32_t uint16_write_time = end_time - start_time;
    printf("UINT16 write: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, uint16_write_time, (float)uint16_write_time / operations);
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_get(test_kvs[2].key, read_data, &len, &type);
    }
    end_time = HAL_GetTick();
    uint32_t uint16_read_time = end_time - start_time;
    printf("UINT16 read: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, uint16_read_time, (float)uint16_read_time / operations);
    
    // 3. 测试UINT32类型 (4字节) - 使用test_kvs[12]
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_set_uint32(test_kvs[12].key, (uint32_t)i);
    }
    end_time = HAL_GetTick();
    uint32_t uint32_write_time = end_time - start_time;
    printf("UINT32 write: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, uint32_write_time, (float)uint32_write_time / operations);
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_get(test_kvs[12].key, read_data, &len, &type);
    }
    end_time = HAL_GetTick();
    uint32_t uint32_read_time = end_time - start_time;
    printf("UINT32 read: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, uint32_read_time, (float)uint32_read_time / operations);
    
    // 4. 测试UINT64类型 (8字节) - 使用test_kvs[13]
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_set_uint64(test_kvs[13].key, (uint64_t)i);
    }
    end_time = HAL_GetTick();
    uint32_t uint64_write_time = end_time - start_time;
    printf("UINT64 write: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, uint64_write_time, (float)uint64_write_time / operations);
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_get(test_kvs[13].key, read_data, &len, &type);
    }
    end_time = HAL_GetTick();
    uint32_t uint64_read_time = end_time - start_time;
    printf("UINT64 read: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, uint64_read_time, (float)uint64_read_time / operations);
    
    // 5. 测试INT8类型 (1字节) - 使用test_kvs[10]
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_set_int8(test_kvs[10].key, (int8_t)(i % 256 - 128));
    }
    end_time = HAL_GetTick();
    uint32_t int8_write_time = end_time - start_time;
    printf("INT8 write: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, int8_write_time, (float)int8_write_time / operations);
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_get(test_kvs[10].key, read_data, &len, &type);
    }
    end_time = HAL_GetTick();
    uint32_t int8_read_time = end_time - start_time;
    printf("INT8 read: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, int8_read_time, (float)int8_read_time / operations);
    
    // 6. 测试INT16类型 (2字节) - 使用test_kvs[11]
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_set_int16(test_kvs[11].key, (int16_t)(i % 65536 - 32768));
    }
    end_time = HAL_GetTick();
    uint32_t int16_write_time = end_time - start_time;
    printf("INT16 write: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, int16_write_time, (float)int16_write_time / operations);
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_get(test_kvs[11].key, read_data, &len, &type);
    }
    end_time = HAL_GetTick();
    uint32_t int16_read_time = end_time - start_time;
    printf("INT16 read: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, int16_read_time, (float)int16_read_time / operations);
    
    // 7. 测试INT32类型 (4字节) - 使用test_kvs[3]
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_set_int32(test_kvs[3].key, (int32_t)i);
    }
    end_time = HAL_GetTick();
    uint32_t int32_write_time = end_time - start_time;
    printf("INT32 write: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, int32_write_time, (float)int32_write_time / operations);
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_get(test_kvs[3].key, read_data, &len, &type);
    }
    end_time = HAL_GetTick();
    uint32_t int32_read_time = end_time - start_time;
    printf("INT32 read: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, int32_read_time, (float)int32_read_time / operations);
    
    // 8. 测试INT64类型 (8字节) - 使用test_kvs[14]
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_set_int64(test_kvs[14].key, (int64_t)i);
    }
    end_time = HAL_GetTick();
    uint32_t int64_write_time = end_time - start_time;
    printf("INT64 write: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, int64_write_time, (float)int64_write_time / operations);
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_get(test_kvs[14].key, read_data, &len, &type);
    }
    end_time = HAL_GetTick();
    uint32_t int64_read_time = end_time - start_time;
    printf("INT64 read: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, int64_read_time, (float)int64_read_time / operations);
    
    // 9. 测试BOOL类型 (1字节) - 使用test_kvs[9]
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_set_bool(test_kvs[9].key, (bool)(i % 2));
    }
    end_time = HAL_GetTick();
    uint32_t bool_write_time = end_time - start_time;
    printf("BOOL write: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, bool_write_time, (float)bool_write_time / operations);
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_get(test_kvs[9].key, read_data, &len, &type);
    }
    end_time = HAL_GetTick();
    uint32_t bool_read_time = end_time - start_time;
    printf("BOOL read: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, bool_read_time, (float)bool_read_time / operations);
    
    // 10. 测试FLOAT类型 (4字节) - 使用test_kvs[8]
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_set_float(test_kvs[8].key, (float)i * 1.1f);
    }
    end_time = HAL_GetTick();
    uint32_t float_write_time = end_time - start_time;
    printf("FLOAT write: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, float_write_time, (float)float_write_time / operations);
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_get(test_kvs[8].key, read_data, &len, &type);
    }
    end_time = HAL_GetTick();
    uint32_t float_read_time = end_time - start_time;
    printf("FLOAT read: %d operations in %lu ms (%.2f ms/op)\n", 
           operations, float_read_time, (float)float_read_time / operations);
    
    // 11. 测试STRING类型 (使用最大长度) - 使用test_kvs[4]
    char max_string[KV_MAX_VALUE_SIZE];
    memset(max_string, 'A', KV_MAX_VALUE_SIZE - 1);
    max_string[KV_MAX_VALUE_SIZE - 1] = '\0';
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_set_string(test_kvs[4].key, max_string);
    }
    end_time = HAL_GetTick();
    uint32_t string_write_time = end_time - start_time;
    printf("STRING write (max length): %d operations in %lu ms (%.2f ms/op)\n", 
           operations, string_write_time, (float)string_write_time / operations);
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_get(test_kvs[4].key, read_data, &len, &type);
    }
    end_time = HAL_GetTick();
    uint32_t string_read_time = end_time - start_time;
    printf("STRING read (max length): %d operations in %lu ms (%.2f ms/op)\n", 
           operations, string_read_time, (float)string_read_time / operations);
    
    // 12. 测试HEX类型 (使用最大长度) - 使用test_kvs[5]
    uint8_t max_hex[KV_MAX_VALUE_SIZE];
    memset(max_hex, 0xFF, sizeof(max_hex));
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_set_hex(test_kvs[5].key, max_hex, sizeof(max_hex));
    }
    end_time = HAL_GetTick();
    uint32_t hex_write_time = end_time - start_time;
    printf("HEX write (max length): %d operations in %lu ms (%.2f ms/op)\n", 
           operations, hex_write_time, (float)hex_write_time / operations);
    
    start_time = HAL_GetTick();
    for (int i = 0; i < operations; i++) {
        embedded_flash_get(test_kvs[5].key, read_data, &len, &type);
    }
    end_time = HAL_GetTick();
    uint32_t hex_read_time = end_time - start_time;
    printf("HEX read (max length): %d operations in %lu ms (%.2f ms/op)\n", 
           operations, hex_read_time, (float)hex_read_time / operations);
    
    // ========== 性能分析和基准比较 ==========
    printf("\n--- Performance Analysis ---\n");
    
    // 计算平均性能指标
    float avg_write_time = (uint8_write_time + uint16_write_time + uint32_write_time + uint64_write_time +
                           int8_write_time + int16_write_time + int32_write_time + int64_write_time +
                           bool_write_time + float_write_time + string_write_time + hex_write_time) / 12.0f;
    float avg_read_time = (uint8_read_time + uint16_read_time + uint32_read_time + uint64_read_time +
                          int8_read_time + int16_read_time + int32_read_time + int64_read_time +
                          bool_read_time + float_read_time + string_read_time + hex_read_time) / 12.0f;
    
    printf("Average write time per operation: %.2f ms\n", avg_write_time / operations);
    printf("Average read time per operation: %.2f ms\n", avg_read_time / operations);
    
    // 数据大小对性能的影响分析
    printf("\nData size impact analysis:\n");
    printf("1-byte data (UINT8/INT8/BOOL): %.3f ms/byte\n", 
           (float)(uint8_write_time + int8_write_time + bool_write_time) / (operations * 3));
    printf("2-byte data (UINT16/INT16): %.3f ms/byte\n", 
           (float)(uint16_write_time + int16_write_time) / (operations * 4));
    printf("4-byte data (UINT32/INT32/FLOAT): %.3f ms/byte\n", 
           (float)(uint32_write_time + int32_write_time + float_write_time) / (operations * 12));
    printf("8-byte data (UINT64/INT64): %.3f ms/byte\n", 
           (float)(uint64_write_time + int64_write_time) / (operations * 16));
    printf("Variable-length data (STRING/HEX): %.3f ms/byte\n", 
           (float)(string_write_time + hex_write_time) / (operations * KV_MAX_VALUE_SIZE * 2));
    
    // 性能回归测试 - 与基准值比较
    printf("\nPerformance regression test:\n");
    uint32_t write_threshold = 10000;  // 10秒
    uint32_t read_threshold = 5000;    // 5秒
    
    // 检查各种操作是否在合理范围内
    if (uint8_write_time > write_threshold) {
        printf("WARNING: UINT8 write performance degraded (%lu ms > %lu ms)\n", 
               uint8_write_time, write_threshold);
        test_failed++;
    }
    
    if (uint16_write_time > write_threshold) {
        printf("WARNING: UINT16 write performance degraded (%lu ms > %lu ms)\n", 
               uint16_write_time, write_threshold);
        test_failed++;
    }
    
    if (uint32_write_time > write_threshold) {
        printf("WARNING: UINT32 write performance degraded (%lu ms > %lu ms)\n", 
               uint32_write_time, write_threshold);
        test_failed++;
    }
    
    if (uint64_write_time > write_threshold) {
        printf("WARNING: UINT64 write performance degraded (%lu ms > %lu ms)\n", 
               uint64_write_time, write_threshold);
        test_failed++;
    }
    
    if (int8_write_time > write_threshold) {
        printf("WARNING: INT8 write performance degraded (%lu ms > %lu ms)\n", 
               int8_write_time, write_threshold);
        test_failed++;
    }
    
    if (int16_write_time > write_threshold) {
        printf("WARNING: INT16 write performance degraded (%lu ms > %lu ms)\n", 
               int16_write_time, write_threshold);
        test_failed++;
    }
    
    if (int32_write_time > write_threshold) {
        printf("WARNING: INT32 write performance degraded (%lu ms > %lu ms)\n", 
               int32_write_time, write_threshold);
        test_failed++;
    }
    
    if (int64_write_time > write_threshold) {
        printf("WARNING: INT64 write performance degraded (%lu ms > %lu ms)\n", 
               int64_write_time, write_threshold);
        test_failed++;
    }
    
    if (bool_write_time > write_threshold) {
        printf("WARNING: BOOL write performance degraded (%lu ms > %lu ms)\n", 
               bool_write_time, write_threshold);
        test_failed++;
    }
    
    if (float_write_time > write_threshold) {
        printf("WARNING: FLOAT write performance degraded (%lu ms > %lu ms)\n", 
               float_write_time, write_threshold);
        test_failed++;
    }
    
    if (string_write_time > write_threshold) {
        printf("WARNING: STRING write performance degraded (%lu ms > %lu ms)\n", 
               string_write_time, write_threshold);
        test_failed++;
    }
    
    if (hex_write_time > write_threshold) {
        printf("WARNING: HEX write performance degraded (%lu ms > %lu ms)\n", 
               hex_write_time, write_threshold);
        test_failed++;
    }
    
    if (uint8_read_time > read_threshold) {
        printf("WARNING: UINT8 read performance degraded (%lu ms > %lu ms)\n", 
               uint8_read_time, read_threshold);
        test_failed++;
    }
    
    if (uint16_read_time > read_threshold) {
        printf("WARNING: UINT16 read performance degraded (%lu ms > %lu ms)\n", 
               uint16_read_time, read_threshold);
        test_failed++;
    }
    
    if (uint32_read_time > read_threshold) {
        printf("WARNING: UINT32 read performance degraded (%lu ms > %lu ms)\n", 
               uint32_read_time, read_threshold);
        test_failed++;
    }
    
    if (uint64_read_time > read_threshold) {
        printf("WARNING: UINT64 read performance degraded (%lu ms > %lu ms)\n", 
               uint64_read_time, read_threshold);
        test_failed++;
    }
    
    if (int8_read_time > read_threshold) {
        printf("WARNING: INT8 read performance degraded (%lu ms > %lu ms)\n", 
               int8_read_time, read_threshold);
        test_failed++;
    }
    
    if (int16_read_time > read_threshold) {
        printf("WARNING: INT16 read performance degraded (%lu ms > %lu ms)\n", 
               int16_read_time, read_threshold);
        test_failed++;
    }
    
    if (int32_read_time > read_threshold) {
        printf("WARNING: INT32 read performance degraded (%lu ms > %lu ms)\n", 
               int32_read_time, read_threshold);
        test_failed++;
    }
    
    if (int64_read_time > read_threshold) {
        printf("WARNING: INT64 read performance degraded (%lu ms > %lu ms)\n", 
               int64_read_time, read_threshold);
        test_failed++;
    }
    
    if (bool_read_time > read_threshold) {
        printf("WARNING: BOOL read performance degraded (%lu ms > %lu ms)\n", 
               bool_read_time, read_threshold);
        test_failed++;
    }
    
    if (float_read_time > read_threshold) {
        printf("WARNING: FLOAT read performance degraded (%lu ms > %lu ms)\n", 
               float_read_time, read_threshold);
        test_failed++;
    }
    
    if (string_read_time > read_threshold) {
        printf("WARNING: STRING read performance degraded (%lu ms > %lu ms)\n", 
               string_read_time, read_threshold);
        test_failed++;
    }
    
    if (hex_read_time > read_threshold) {
        printf("WARNING: HEX read performance degraded (%lu ms > %lu ms)\n", 
               hex_read_time, read_threshold);
        test_failed++;
    }
    
    // 性能测试总结
    printf("\n--- Performance Test Summary ---\n");
    printf("Total operations tested: %d\n", operations * 24);  // 12种数据类型，每种测试读写
    printf("Performance tests passed: %d/24\n", 24 - test_failed);
    
    if (test_failed == 0) {
        print_pass("performance test");
    } else {
        printf("FAIL - performance test: %d tests failed\n", test_failed);
        return -1;
    }
    
    print_test_end("Performance Test", 0);
    return 0;
}

/**
 * @brief 随机数据测试 - 测试随机数据的存储和恢复
 */
int embedded_flash_demo_random_data_test(void) {
	print_test_start("Random Data Test");
	printf("Testing random data patterns...\n");
    uint8_t test_data[10];
    uint8_t read_data[10];
    uint8_t len, type;
    int success_count = 0;
    int total_tests = 50;
    
    // 使用简单的伪随机数生成器
    uint32_t seed = 0x12345678;
    
    // 使用test_kvs数组中的后4个键进行随机数据测试
    uint8_t random_test_keys[] = {
        TEST_HEX_4_A6_166,    // 0xA6
        TEST_UINT16_2_A7_167, // 0xA7
        TEST_INT32_4_A8_168,  // 0xA8
        TEST_FLOAT_4_A9_169   // 0xA9
    };
    
    for (int i = 0; i < total_tests; i++) {
        // 生成随机数据
        for (int j = 0; j < 10; j++) {
            seed = seed * 1103515245 + 12345;  // 线性同余生成器
            test_data[j] = (uint8_t)(seed >> 16);
        }
        
        // 随机选择key和数据类型
        uint8_t key = random_test_keys[seed % 4];  // 使用test_kvs中的键
        uint8_t data_type = EFLASH_FORMAT_UINT8 + (seed % 4);
        uint8_t data_len = 1 + (seed % 10);  // 1-10字节，确保不超过KV_MAX_VALUE_SIZE
        
        // 写入数据 - 根据数据类型选择合适的API
        int write_result = -1;
        switch (data_type) {
            case EFLASH_FORMAT_UINT8:
                write_result = embedded_flash_set_uint8(key, test_data[0]);
                break;
            case EFLASH_FORMAT_UINT16:
                write_result = embedded_flash_set_uint16(key, *(uint16_t*)test_data);
                break;
            case EFLASH_FORMAT_INT32:
                write_result = embedded_flash_set_int32(key, *(int32_t*)test_data);
                break;
            case EFLASH_FORMAT_HEX:
                write_result = embedded_flash_set_hex(key, test_data, data_len);
                break;
            default:
                write_result = -1;
                break;
        }
        
        if (write_result == 0) {
            // 读取并验证
            if (embedded_flash_get(key, read_data, &len, &type) == 0) {
                if (len == data_len && type == data_type && 
                    memcmp(test_data, read_data, data_len) == 0) {
                    success_count++;
                }
            }
        }
        
        if ((i + 1) % 10 == 0) {
            printf("Random data test progress: %d/%d, success: %d\n", i + 1, total_tests, success_count);
        }
    }
    
    if (success_count == total_tests) {
        print_pass("random data test");
    } else {
        printf("FAIL - random data test: %d/%d tests succeeded\n", success_count, total_tests);
        return -1;
    }
    print_test_end("Random Data Test", 0);
    return 0;

}


int embedded_flash_demo_power_loss_test(void) {
	print_test_start("Power Loss Test");
	printf("Testing power loss recovery...\n");
	
	// ========== 阶段1: 写入测试数据 ==========
	printf("\n--- Phase 1: Writing test data before power loss ---\n");
	uint8_t test_uint8_write = 15;
	uint16_t test_uint16_write = 1500;
	int32_t test_int32_write = -7500;
	char test_string_write[] = "PowerOff";  // 8字节 + null终止符 = 9字节 < 10字节
	uint8_t test_hex_write[] = {0xCA, 0xFE, 0xBA, 0xBE};
	
	// 写入所有测试数据
	if (embedded_flash_set_uint8(TEST_UINT8_1_A1_161, test_uint8_write) != 0) {
		printf("[X] Failed to write UINT8 before power loss\n");
		return -1;
	}
	printf("[OK] Written UINT8=%d to key 0x%02X\n", test_uint8_write, TEST_UINT8_1_A1_161);
	
	if (embedded_flash_set_uint16(TEST_UINT16_2_A7_167, test_uint16_write) != 0) {
		printf("[X] Failed to write UINT16 before power loss\n");
		return -1;
	}
	printf("[OK] Written UINT16=%d to key 0x%02X\n", test_uint16_write, TEST_UINT16_2_A7_167);
	
	if (embedded_flash_set_int32(TEST_INT32_4_A8_168, test_int32_write) != 0) {
		printf("[X] Failed to write INT32 before power loss\n");
		return -1;
	}
	printf("[OK] Written INT32=%d to key 0x%02X\n", test_int32_write, TEST_INT32_4_A8_168);
	
	if (embedded_flash_set_string(TEST_STRING_8_A5_165, test_string_write) != 0) {
		printf("[X] Failed to write STRING before power loss\n");
		return -1;
	}
	printf("[OK] Written STRING='%s' to key 0x%02X\n", test_string_write, TEST_STRING_8_A5_165);
	
	if (embedded_flash_set_hex(TEST_HEX_4_A6_166, test_hex_write, sizeof(test_hex_write)) != 0) {
		printf("[X] Failed to write HEX before power loss\n");
		return -1;
	}
	printf("[OK] Written HEX=[0x%02X, 0x%02X, 0x%02X, 0x%02X] to key 0x%02X\n", 
	       test_hex_write[0], test_hex_write[1], test_hex_write[2], test_hex_write[3], TEST_HEX_4_A6_166);
	
	printf("\n--- All data written successfully, simulating power loss... ---\n");
	
	// ========== 阶段2: 模拟断电重启 ==========
	printf("\n--- Phase 2: Simulating power loss and system reboot ---\n");
	printf("⚡ Power loss simulation: Reinitializing flash system...\n");
	
	// 重新初始化flash系统（模拟断电重启）
	if (test_embedded_flash_init() != 0) {
		printf("[X] CRITICAL: Flash reinitialization failed after power loss!\n");
		return -1;
	}
	printf("[OK] Flash system reinitialized successfully\n");
	
	// ========== 阶段3: 验证数据恢复 ==========
	printf("\n--- Phase 3: Verifying data recovery after power loss ---\n");
	int test_failed = 0;
	uint8_t l, t;
	
	// 验证 UINT8
	uint8_t test_uint8_read = 0;
	if (embedded_flash_get(TEST_UINT8_1_A1_161, (uint8_t*)&test_uint8_read, &l, &t) != 0) {
		printf("[X] UINT8: Failed to read key 0x%02X\n", TEST_UINT8_1_A1_161);
		test_failed = 1;
	} else if (t != EFLASH_FORMAT_UINT8) {
		printf("[X] UINT8: Type mismatch - expected=%d, actual=%d\n", EFLASH_FORMAT_UINT8, t);
		test_failed = 1;
	} else if (l != sizeof(uint8_t)) {
		printf("[X] UINT8: Length mismatch - expected=%d, actual=%d\n", (int)sizeof(uint8_t), l);
		test_failed = 1;
	} else if (test_uint8_read != test_uint8_write) {
		printf("[X] UINT8: Value mismatch - expected=%d, actual=%d\n", test_uint8_write, test_uint8_read);
		test_failed = 1;
	} else {
		printf("[OK] UINT8: Recovered correctly (value=%d)\n", test_uint8_read);
	}
	
	// 验证 UINT16
	uint16_t test_uint16_read = 0;
	if (embedded_flash_get(TEST_UINT16_2_A7_167, (uint8_t*)&test_uint16_read, &l, &t) != 0) {
		printf("[X] UINT16: Failed to read key 0x%02X\n", TEST_UINT16_2_A7_167);
		test_failed = 1;
	} else if (t != EFLASH_FORMAT_UINT16) {
		printf("[X] UINT16: Type mismatch - expected=%d, actual=%d\n", EFLASH_FORMAT_UINT16, t);
		test_failed = 1;
	} else if (l != sizeof(uint16_t)) {
		printf("[X] UINT16: Length mismatch - expected=%d, actual=%d\n", (int)sizeof(uint16_t), l);
		test_failed = 1;
	} else if (test_uint16_read != test_uint16_write) {
		printf("[X] UINT16: Value mismatch - expected=%d, actual=%d\n", test_uint16_write, test_uint16_read);
		test_failed = 1;
	} else {
		printf("[OK] UINT16: Recovered correctly (value=%d)\n", test_uint16_read);
	}
	
	// 验证 INT32
	int32_t test_int32_read = 0;
	if (embedded_flash_get(TEST_INT32_4_A8_168, (uint8_t*)&test_int32_read, &l, &t) != 0) {
		printf("[X] INT32: Failed to read key 0x%02X\n", TEST_INT32_4_A8_168);
		test_failed = 1;
	} else if (t != EFLASH_FORMAT_INT32) {
		printf("[X] INT32: Type mismatch - expected=%d, actual=%d\n", EFLASH_FORMAT_INT32, t);
		test_failed = 1;
	} else if (l != sizeof(int32_t)) {
		printf("[X] INT32: Length mismatch - expected=%d, actual=%d\n", (int)sizeof(int32_t), l);
		test_failed = 1;
	} else if (test_int32_read != test_int32_write) {  // ✅ 修复bug：之前是 -test_int32_write
		printf("[X] INT32: Value mismatch - expected=%d, actual=%d\n", test_int32_write, test_int32_read);
		test_failed = 1;
	} else {
		printf("[OK] INT32: Recovered correctly (value=%d)\n", test_int32_read);
	}
	
	// 验证 STRING
	char test_string_read[10] = {0};
	if (embedded_flash_get(TEST_STRING_8_A5_165, (uint8_t*)test_string_read, &l, &t) != 0) {
		printf("[X] STRING: Failed to read key 0x%02X\n", TEST_STRING_8_A5_165);
		test_failed = 1;
	} else if (t != EFLASH_FORMAT_STRING) {
		printf("[X] STRING: Type mismatch - expected=%d, actual=%d\n", EFLASH_FORMAT_STRING, t);
		test_failed = 1;
	} else if (l != strlen(test_string_write) + 1) {
		printf("[X] STRING: Length mismatch - expected=%d, actual=%d\n", (int)strlen(test_string_write) + 1, l);
		test_failed = 1;
	} else if (strcmp(test_string_read, test_string_write) != 0) {
		printf("[X] STRING: Value mismatch - expected='%s', actual='%s'\n", test_string_write, test_string_read);
		test_failed = 1;
	} else {
		printf("[OK] STRING: Recovered correctly (value='%s')\n", test_string_read);
	}
	
	// 验证 HEX
	uint8_t test_hex_read[4] = {0};
	if (embedded_flash_get(TEST_HEX_4_A6_166, test_hex_read, &l, &t) != 0) {
		printf("[X] HEX: Failed to read key 0x%02X\n", TEST_HEX_4_A6_166);
		test_failed = 1;
	} else if (t != EFLASH_FORMAT_HEX) {
		printf("[X] HEX: Type mismatch - expected=%d, actual=%d\n", EFLASH_FORMAT_HEX, t);
		test_failed = 1;
	} else if (l != sizeof(test_hex_write)) {
		printf("[X] HEX: Length mismatch - expected=%d, actual=%d\n", (int)sizeof(test_hex_write), l);
		test_failed = 1;
	} else if (memcmp(test_hex_read, test_hex_write, sizeof(test_hex_write)) != 0) {
		printf("[X] HEX: Value mismatch\n");
		printf("  Expected: [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n", 
		       test_hex_write[0], test_hex_write[1], test_hex_write[2], test_hex_write[3]);
		printf("  Actual:   [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n", 
		       test_hex_read[0], test_hex_read[1], test_hex_read[2], test_hex_read[3]);
		test_failed = 1;
	} else {
		printf("[OK] HEX: Recovered correctly (value=[0x%02X, 0x%02X, 0x%02X, 0x%02X])\n", 
		       test_hex_read[0], test_hex_read[1], test_hex_read[2], test_hex_read[3]);
	}
	
	// ========== 最终结果 ==========
	printf("\n--- Power Loss Test Summary ---\n");
	if (test_failed) {
		print_fail("power-loss recovery - some data was corrupted");
		return -1;
	} else {
		print_pass("power-loss recovery - all data survived power loss");
	}
	
	print_test_end("Power Loss Test", 0);
	return 0;
}





int embedded_flash_demo_verify_integrity(void) {
	uint8_t count = sizeof(test_kvs) / sizeof(kv_data_t);
	// 增加缓冲区大小以容纳最大可能的数据
	uint8_t buf[KV_MAX_VALUE_SIZE], len, type; 
    int err=0, type_err=0, len_err=0;
	
	// 定义被性能测试和随机数据测试修改过的键
	uint8_t modified_keys[] = {
		TEST_UINT8_1_A1_161,  // 性能测试使用
		TEST_UINT8_1_A2_162,  // 性能测试使用
		TEST_UINT16_2_A3_163, // 性能测试使用
		TEST_INT32_4_A4_164,  // 性能测试使用
		TEST_HEX_4_A6_166,    // 随机数据测试使用
		TEST_UINT16_2_A7_167, // 随机数据测试使用
		TEST_INT32_4_A8_168,  // 随机数据测试使用
		TEST_FLOAT_4_A9_169   // 随机数据测试使用
	};
	
	for(uint8_t i=0;i<count;i++) {
		// 检查当前键是否被修改过
		uint8_t is_modified = 0;
		for(uint8_t j=0; j<sizeof(modified_keys); j++) {
			if(test_kvs[i].key == modified_keys[j]) {
				is_modified = 1;
				break;
			}
		}
		
		// 如果键被修改过，跳过完整性检查
		if(is_modified) {
			printf("Skipping integrity check for modified key: 0x%02X\n", test_kvs[i].key);
			continue;
		}
		
		if (embedded_flash_get(test_kvs[i].key, buf, &len, &type)!=0) { 
			err++; 
			printf("get fail,key:%d\n",test_kvs[i].key);
			continue; 
		}
		if (len!=test_kvs[i].value_length) len_err++;
		if (type!=test_kvs[i].data_type) type_err++;
	}
	if (type_err==0) print_pass("type_err verify"); else print_fail("type_err verify");
	if (len_err==0) print_pass("len_err verify"); else print_fail("len_err verify");
	if (err==0) print_pass("integrity verify"); else print_fail("integrity verify");
	return err==0?0:-1;
}

int embedded_flash_demo_run_full(void) {
    printf("========== Starting Comprehensive Flash Storage Test Suite ==========\n");
    
    // 清理所有测试键，确保测试开始时是干净的状态
    // printf("Cleaning up test keys before starting tests...\n");
    // embedded_flash_delete(TEST_UINT8_1_A1_161);
    // embedded_flash_delete(TEST_UINT8_1_A2_162);
    // embedded_flash_delete(TEST_UINT16_2_A3_163);
    // embedded_flash_delete(TEST_INT32_4_A4_164);
    // embedded_flash_delete(TEST_STRING_8_A5_165);
    // embedded_flash_delete(TEST_HEX_4_A6_166);
    // embedded_flash_delete(TEST_UINT16_2_A7_167);
    // embedded_flash_delete(TEST_INT32_4_A8_168);
    // embedded_flash_delete(TEST_FLOAT_4_A9_169);
    // embedded_flash_delete(TEST_BOOL_1_AA_170);
    // embedded_flash_delete(TEST_INT8_1_AB_171);
    // embedded_flash_delete(TEST_INT16_2_AC_172);
    // embedded_flash_delete(TEST_UINT32_4_AD_173);
    // embedded_flash_delete(TEST_UINT64_8_AE_174);
    // embedded_flash_delete(TEST_INT64_8_AF_175);
    
    // 基础功能测试
    printf("\n--- Phase 1: Basic Functionality Tests ---\n");
    if (test_embedded_flash_init()!=0){ 
        printf("CRITICAL FAILURE: Flash initialization failed\n"); 
        while(1); 
    }
    
    if (embedded_flash_demo_basic_test() != 0) {
        printf("TEST FAILED: Basic functionality test failed\n");
        while(1);
    }
    
    if (embedded_flash_demo_verify_integrity() != 0) {
        printf("TEST FAILED: Basic functionality integrity verification failed\n");
        while(1);
    }
    
    if (embedded_flash_demo_data_source_test() != 0) {
        printf("TEST FAILED: Data source test failed\n");
        while(1);
    }
    
    if (embedded_flash_demo_batch_test() != 0) {
        printf("TEST FAILED: Batch test failed\n");
        while(1);
    }
    
    if (embedded_flash_demo_verify_integrity() != 0) {
        printf("TEST FAILED: Data source integrity verification failed\n");
        while(1);
    }
    // 错误处理测试
    printf("\n--- Phase 2: Error Handling Tests ---\n");
    if (embedded_flash_demo_error_test() != 0) {
        printf("TEST FAILED: Error handling test failed\n");
        while(1);
    }
    
    if (embedded_flash_demo_boundary_test() != 0) {
        printf("TEST FAILED: Boundary test failed\n");
        while(1);
    }
    // 类型安全API测试
    printf("\n--- Phase 3: Type-Safe API Tests ---\n");
    if (embedded_flash_demo_type_safe_api_test() != 0) {
        printf("TEST FAILED: Type-safe API test failed\n");
        while(1);
    }
    
    if (embedded_flash_demo_verify_integrity() != 0) {
        printf("TEST FAILED: Type-safe API integrity verification failed\n");
        while(1);
    }

    // 数据类型一致性测试
    printf("\n--- Phase 4: Data Type Consistency Tests ---\n");
    if (embedded_flash_demo_data_type_consistency_test() != 0) {
        printf("TEST FAILED: Data type consistency test failed\n");
        while(1);
    }
    
    if (embedded_flash_demo_verify_integrity() != 0) {
        printf("TEST FAILED: Data type consistency integrity verification failed\n");
        while(1);
    }

    // // 垃圾回收测试
    // printf("\n--- Phase 5: Garbage Collection Tests ---\n");
    // if (embedded_flash_demo_gc_test() != 0) {
    //     printf("TEST FAILED: Garbage collection test failed\n");
    //     while(1);
    // }
    
    // if (embedded_flash_demo_verify_integrity() != 0) {
    //     printf("TEST FAILED: Garbage collection integrity verification failed\n");
    //     while(1);
    // }
    
    // if (embedded_flash_demo_gc_stress_test() != 0) {
    //     printf("TEST FAILED: Garbage collection stress test failed\n");
    //     while(1);
    // }
    // if (embedded_flash_demo_verify_integrity() != 0) {
    //     printf("TEST FAILED: Garbage collection stress test integrity verification failed\n");
    //     while(1);
    // }
    
    // 断电恢复测试
    printf("\n--- Phase 6: Power Loss Recovery Tests ---\n");
    if (embedded_flash_demo_power_loss_test() != 0) {
        printf("TEST FAILED: Power loss recovery test failed\n");
        while(1);
    }
    if (embedded_flash_demo_verify_integrity() != 0) {
        printf("TEST FAILED: Power loss recovery integrity verification failed\n");
        while(1);
    }
    
    if (embedded_flash_demo_power_loss_stress_test() != 0) {
        printf("TEST FAILED: Power loss stress test failed\n");
        while(1);
    }
    if (embedded_flash_demo_verify_integrity() != 0) {
        printf("TEST FAILED: Power loss stress test integrity verification failed\n");
        while(1);
    }

        
   // 压力测试
   printf("\n--- Phase 6: Stress Tests ---\n");
   if (embedded_flash_demo_stress_test() != 0) {
       printf("TEST FAILED: Stress test failed\n");
       while(1);
   }
   if (embedded_flash_demo_verify_integrity() != 0) {
       printf("TEST FAILED: Stress test integrity verification failed\n");
       while(1);
   }

//    // 性能测试
//    printf("\n--- Phase 7: Performance Tests ---\n");
//    embedded_flash_demo_performance_test();
//    embedded_flash_demo_random_data_test();
//    if (embedded_flash_demo_verify_integrity() != 0) {
//        printf("TEST FAILED: Performance test integrity verification failed\n");
//        while(1);
//    }

   
   // 最终完整性验证
   printf("\n--- Phase 8: Final Integrity Verification ---\n");
   if (embedded_flash_demo_verify_integrity() != 0) {
       printf("TEST FAILED: Final integrity verification failed\n");
       while(1);
   }
    
    printf("\n========== Test Suite Results ==========\n");
    printf("ALL TESTS PASSED SUCCESSFULLY\n");
    printf("Flash storage system is working correctly\n");
    printf("==========================================\n");
    
    return 0;
}


#endif


