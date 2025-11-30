/*
 * common_data.h
 *
 *  Created on: 28 thg 11, 2025
 *      Author: buiqu
 */

#ifndef APP_DATA_H_
#define APP_DATA_H_
#include <stdint.h>
#include <stdbool.h>

/* --- PROTOCOL FRAMING --- */
#define PROTOCOL_START_BYTE  0xAA
#define PROTOCOL_END_BYTE    0x55

/* --- SYSTEM STATES --- */
typedef enum {
    STATE_INIT = 0,
    STATE_CONNECTING,
    STATE_STREAMING,
    STATE_BUFFERING,
    STATE_RETRANSMIT,
    STATE_ERROR
} SystemState_t;

/* --- DATA PACKET (Packed for UART efficiency) --- */
/* We use #pragma pack to ensure the struct is exactly 31 bytes */
#pragma pack(push, 1)
typedef struct {
    /* Time Data (7 bytes) */
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;

    /* ICP10101 Data (12 bytes) */
    float    pressure;    // Pa
    float    temperature; // C
    float    altitude;    // m

    /* ZMOD4410 Data (16 bytes) */
    float    iaq;
    float    tvoc;
    float    eco2;
    float    etoh;
} SystemData_t;
#pragma pack(pop)

#endif /* APP_DATA_H_ */
