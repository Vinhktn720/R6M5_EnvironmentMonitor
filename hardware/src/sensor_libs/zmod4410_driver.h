#ifndef ZMOD4410_DRIVER_H
#define ZMOD4410_DRIVER_H

#include <sensor_libs/icp10101_driver.h>
#include <stdint.h>

// Cau truc ket qua tra ve cho Main su dung
typedef struct {
    float iaq;      // Chi so IAQ (0.0 - 5.0)
    float tvoc;     // TVOC (mg/m^3)
    float eco2;     // eCO2 (ppm)
    float etoh;     // Ethanol (ppm)
    uint8_t new_data_available; // Co bao co du lieu moi (1: Co, 0: Khong)
} zmod_results_t;

// Ham khoi tao toan bo (Phan cung + Cam bien + Thuat toan)
int8_t zmod4410_init_system(void);

// Ham xu ly trong vong lap (Non-blocking)
void zmod4410_loop_task(icp10101_results_t *s);

// Ham lay ket qua (Main goi ham nay de lay so lieu)
zmod_results_t zmod4410_get_current_results(void);

#endif
