/*
 * sensors.c
 *
 *  Created on: 28 thg 11, 2025
 *      Author: buiqu
 */
#include "sensors.h"

static icp10101_t my_icp_sensor;
static icp10101_results_t icp_val;
static zmod_results_t zmod_res;

void sensors_init(void)
{
    /* 1. I2C Bus Recovery & Init */
    i2c_bus_recovery();
    i2c_common_init();

    /* 2. Init ICP10101 (Pressure/Temp) */
    if (icp10101_sensor_init(&my_icp_sensor) != 0)
    {
        /* Handle Error - maybe blink Red LED forever? */
        while(1);
    }

    /* 3. Init ZMOD4410 (Air Quality) */
    /* This starts the warm-up process */
    if (zmod4410_init_system() != 0)
    {
        while(1);
    }
}

/* Read all sensors and populate the struct */
void sensors_read_all(SystemData_t *data)
{
    /* --- 1. ICP10101 READ SEQUENCE --- */
    int32_t p_raw;
    int16_t t_raw;
    float pressure, temperature, altitude;

    /* Start Measurement */
    icp10101_start_measurement();


    vTaskDelay(pdMS_TO_TICKS(7));

    icp10101_read_raw_data(&p_raw, &t_raw);
    icp10101_process_data(&my_icp_sensor, p_raw, t_raw, &pressure, &temperature, &altitude);

    /* Update Struct */
    data->pressure    = pressure;
    data->temperature = temperature;
    data->altitude    = altitude;

    /* Update global ICP struct for ZMOD compensation */
    icp_val.pressure    = pressure;
    icp_val.temperature = temperature;
    icp_val.altitude    = altitude;


    /* --- 2. ZMOD4410 READ SEQUENCE --- */
    /* Drive the ZMOD state machine */
    zmod4410_loop_task(&icp_val);

    /* Check if results are ready */
    zmod_res = zmod4410_get_current_results();

    if (zmod_res.new_data_available)
    {
        /* Valid Data Available (Warmup Complete) */
        data->iaq  = zmod_res.iaq;
        data->tvoc = zmod_res.tvoc;
        data->eco2 = zmod_res.eco2;
        data->etoh = zmod_res.etoh;
    }
    else
    {
        /* Still warming up (first 3 mins) or stabilizing */
        /* Send specific flag values like -1.0 so Python knows it's warming up */
        data->iaq  = -1.0f;
        data->tvoc = -1.0f;
        data->eco2 = -1.0f;
        data->etoh = -1.0f;
    }
}
