/*
 * sensor_task.c
 *
 *  Created on: 28 thg 11, 2025
 *      Author: buiqu
 */
#include "sensor_task.h"
#include "app_data.h"
#include "sensors.h"
#include "rtc.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

extern QueueHandle_t g_data_queue;

void sensor_task_worker(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    SystemData_t current_data;
    TickType_t xLastWakeTime;

    /* ZMOD requires a specific timing. Your loop had ~10ms i2c delay + 1000ms delay.
       We will aim for a 1-second interval (1000ms). */
    const TickType_t xFrequency = pdMS_TO_TICKS(1000);

    /* 1. Hardware Init (Real I2C & Sensors) */
    sensors_init();

    xLastWakeTime = xTaskGetTickCount();

    while(1)
    {
        /* Wait for next cycle (precise 1s interval) */
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        /* 2. Get Time */
        rtc_get_system_data_time(&current_data);

        /* 3. Read All Real Sensors */
        /* This handles ICP reading and ZMOD state machine steps */
        sensors_read_all(&current_data);

        /* 4. Send to Queue */
        /* If ZMOD is warming up, values will be -1.0, which is fine to log */
        xQueueSend(g_data_queue, &current_data, 0);
    }
}
