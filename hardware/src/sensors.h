/*
 * sensors.h
 *
 *  Created on: 28 thg 11, 2025
 *      Author: buiqu
 */

#ifndef SENSORS_H_
#define SENSORS_H_

#include "hal_data.h"
#include "app_data.h"
#include <sensor_libs/icp10101_driver.h>
#include <sensor_libs/ra6m5_i2c.h>
#include <sensor_libs/zmod4410_driver.h>
#include "FreeRTOS.h"
#include "task.h"

/* Init hardware (I2C, GPIOs, Config registers) */
void sensors_init(void);

/* Reads both sensors and fills the SystemData_t struct */
void sensors_read_all(SystemData_t *data);


#endif /* SENSORS_H_ */
