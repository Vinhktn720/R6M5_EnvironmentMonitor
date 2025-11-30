#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Project Includes */
#include "app_data.h"   /* Formerly common_data.h - ensure file name matches */
#include "uart.h"
#include "rtc.h"
#include "storage.h"
#include "sensor_task.h"
#include "led_btn.h"
#include "new_thread0.h"

/* ======================================================
 * Static RTOS Runtimes (buffers must be global)
 * ====================================================== */

#define DATA_QUEUE_LENGTH 20

/* Queue Buffers */
static StaticQueue_t data_queue_struct;
static uint8_t data_queue_storage[DATA_QUEUE_LENGTH * sizeof(SystemData_t)];

/* Task Buffers */
static StaticTask_t sensor_task_tcb;
static StackType_t sensor_task_stack[512];

/* Handles */
QueueHandle_t g_data_queue;
TaskHandle_t g_sensor_task_handle;
SystemState_t current_state = STATE_INIT;

void new_thread0_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    SystemData_t data_packet;
    SystemData_t stored_packet;

    /* 1. Initialization */
    ui_init();
    led_set_color(1);

    uart5_init();
    rtc_init_hardware(2025, 11, 28, 13, 00, 00);
    storage_init();

    /* 2. Create Static Queue */
    /* Returns NULL on failure, but static creation rarely fails if buffers exist */
    g_data_queue = xQueueCreateStatic(
        DATA_QUEUE_LENGTH,
        sizeof(SystemData_t),
        data_queue_storage,
        &data_queue_struct
    );

    /* 3. Create Static Sensor Task */
    g_sensor_task_handle = xTaskCreateStatic(
        sensor_task_worker,
        "Sensors",
        512,             /* Stack depth in WORDS, not bytes */
        NULL,            /* Parameters */
        2,               /* Priority */
        sensor_task_stack,
        &sensor_task_tcb
    );

    current_state = STATE_CONNECTING;

    while (1)
    {
        if (is_button_pressed())
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (is_button_pressed())
            {
                current_state = STATE_CONNECTING;
                led_set_color(0);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }
        switch (current_state)
        {
            case STATE_CONNECTING:
                led_set_color(1);
                if (uart5_check_connection())
                {
                    current_state = STATE_STREAMING;
                }
                else
                {
                    if (uxQueueMessagesWaiting(g_data_queue) > 0)
                    {
                        if (xQueuePeek(g_data_queue, &data_packet, 0) == pdPASS)
                        {
                            uart5_send_packet(&data_packet);
                            if (uart5_wait_ack(100)) {
                                current_state = STATE_STREAMING;
                            }else{
                                current_state = STATE_BUFFERING;
                            }
                        }
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                break;

            case STATE_STREAMING:
                /* Wait forever for new data */
                if (xQueueReceive(g_data_queue, &data_packet, portMAX_DELAY) == pdPASS)
                {
                    uart5_send_packet(&data_packet);

                    if (!uart5_wait_ack(50))
                    {
                        storage_save(&data_packet);
                        current_state = STATE_BUFFERING;
                        led_set_color(3); // Red ON briefly (Error detected)
                    } else {
                        led_toggle_blue();
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                }
                break;

            case STATE_BUFFERING:
                led_set_color(2);
                /* Drain queue to storage without blocking (wait 0) */
                while (xQueueReceive(g_data_queue, &data_packet, 0) == pdPASS)
                {
                    if (!storage_save(&data_packet))
                    {
                        current_state = STATE_ERROR;
                    }
                }

                if(!storage_is_empty()){
                    storage_read_oldest(&stored_packet);

                    uart5_send_packet(&stored_packet);

                    if(uart5_wait_ack(100)){
                        storage_mark_oldest_as_sent();
                        current_state = STATE_RETRANSMIT;
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }else {
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                } else {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                break;

            case STATE_RETRANSMIT:
                led_toggle_green();
                if (!storage_is_empty())
                {
                    storage_read_oldest(&stored_packet);
                    uart5_send_packet(&stored_packet);

                    if (uart5_wait_ack(100))
                    {
                        storage_mark_oldest_as_sent();
                    }
                    else
                    {
                        current_state = STATE_BUFFERING;
                    }
                }
                else
                {
                    current_state = STATE_STREAMING;
                }
                break;

            case STATE_ERROR:
                led_set_color(3);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
        }
    }
}
