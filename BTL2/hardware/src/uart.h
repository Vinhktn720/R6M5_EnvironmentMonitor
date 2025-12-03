/*
 * uart.h
 *
 *  Created on: 28 thg 11, 2025
 *      Author: buiqu
 */

#ifndef UART_H_
#define UART_H_
#include "app_data.h"
#include "hal_data.h"
#include "FreeRTOS.h"
#include "task.h"

void uart5_init(void);
void uart5_send_packet(SystemData_t *pkt);
bool uart5_wait_ack(uint32_t timeout_ms);
bool uart5_check_connection(void);

#endif /* UART_H_ */
