/*
 * rtc.h
 *
 *  Created on: 28 thg 11, 2025
 *      Author: buiqu
 */

#ifndef RTC_H_
#define RTC_H_

#include "app_data.h"
#include "hal_data.h"

void rtc_init_hardware(uint16_t y, uint8_t mon, uint8_t d, uint8_t h, uint8_t m, uint8_t s);
void rtc_get_system_data_time(SystemData_t *data_ptr);


#endif /* RTC_H_ */
