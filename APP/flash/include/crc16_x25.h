/*** 
 * @Author: 刘永铿 陈科杰
 * @Date: 2024-01-03 09:27:09
 * @LastEditTime: 2024-01-09 13:58:19
 * @LastEditors: Please set LastEditors
 * @Description: 
 * @FilePath: \protocol\Common\crc16_x25.h
 * @Hostory: 
 * @...
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _CRC16_X25_H_
#define _CRC16_X25_H_

#include <stdint.h>
#include <stdio.h>
/**
 *
 *  CALCULATE THE CHECKSUM
 *
 */

#define X25_INIT_CRC 0xffff
#define X25_VALIDATE_CRC 0xf0b8

#ifndef HAVE_CRC_ACCUMULATE
/**
 * @brief Accumulate the X.25 CRC by adding one char at a time.
 *
 * The checksum function adds the hash of one char at a time to the
 * 16 bit checksum (uint16_t).
 *
 * @param data new char to hash
 * @param crcAccum the already accumulated checksum
 **/
static inline void crc16_x25_accumulate(uint8_t data, uint16_t *crcAccum)
{
        /*Accumulate one byte of data into the CRC*/
        uint8_t tmp;

        tmp = data ^ (uint8_t)(*crcAccum &0xff);
        tmp ^= (tmp<<4);
        *crcAccum = (*crcAccum>>8) ^ (tmp<<8) ^ (tmp <<3) ^ (tmp>>4);
        //printf("data:0x%02x  crcAccum:0x%04x\r\n",data,*crcAccum);
}
#endif


/**
 * @brief Initiliaze the buffer for the X.25 CRC
 *
 * @param crcAccum the 16 bit X.25 CRC
 */
static inline void crc16_x25_init(uint16_t* crcAccum)
{
        *crcAccum = X25_INIT_CRC;
}


/**
 * @brief Calculates the X.25 checksum on a byte buffer
 *
 * @param  pBuffer buffer containing the byte array to hash
 * @param  length  length of the byte array
 * @return the checksum over the buffer bytes
 **/
static inline uint16_t crc16_x25_calculate(const uint8_t* pBuffer, uint16_t length)
{
    uint16_t crcTmp;
    crc16_x25_init(&crcTmp);
    while (length--) {
            crc16_x25_accumulate(*pBuffer++, &crcTmp);
    }
    return crcTmp;
}


/**
 * @brief Accumulate the X.25 CRC by adding an array of bytes
 *
 * The checksum function adds the hash of one char at a time to the
 * 16 bit checksum (uint16_t).
 *
 * @param data new bytes to hash
 * @param crcAccum the already accumulated checksum
 **/
static inline void crc16_x25_accumulate_buffer(uint16_t *crcAccum, const char *pBuffer, uint16_t length)
{
	const uint8_t *p = (const uint8_t *)pBuffer;
	while (length--) {
                crc16_x25_accumulate(*p++, crcAccum);
        }
}

#endif /* _CRC16_X25_H_ */

#ifdef __cplusplus
}
#endif

