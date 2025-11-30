#ifndef ICP10101_DRIVER_H
#define ICP10101_DRIVER_H

#include <stdint.h>

// Struct chua cac hang so hieu chuan va tinh toan
typedef struct {
    float sensor_constants[4];
    float p_Pa_calib[3];
    float LUT_lower;
    float LUT_upper;
    float quadr_factor;
    float offst_factor;
} icp10101_t;

typedef struct {
    float pressure;       // Pressure (Pa)
    float temperature;    // Temperature (oC)
    float altitude;       // Altitude (m)
} icp10101_results_t;

// Khoi tao phan cung MCU (IIC0, PFS, Clock).
void icp10101_init_hardware(void);
//Khoi tao cam bien (Doc OTP va tinh hang so), 0->thanh cong, -1->that bai
int icp10101_sensor_init(icp10101_t *s);
//Gui lenh yeu cau cam bien bat dau do
void icp10101_start_measurement(void);
//Doc data raw tu cam bien
void icp10101_read_raw_data(int32_t *p_raw, int16_t *t_raw);
//Reset
void icp10101_reset_altitude_base(void);
//Xu ly data raw -> ap suat, nhiet do, do cao
void icp10101_process_data(icp10101_t *s, int32_t p_raw, int16_t t_raw, float *pressure, float *temperature, float *altitude);
#endif
