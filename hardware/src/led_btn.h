/*
 * led_btn.h
 *
 *  Created on: 29 thg 11, 2025
 *      Author: buiqu
 */

#ifndef LED_BTN_H_
#define LED_BTN_H_
#include "hal_data.h"

void ui_init(void);

/* Helper to set LED colors */
/* State: 0 = ALL OFF, 1 = BLUE, 2 = GREEN, 3 = RED */
void led_set_color(uint8_t color);

void led_toggle_blue(void);

void led_toggle_green(void);

/* Returns 1 if button is pressed (Active Low), 0 otherwise */
uint8_t is_button_pressed(void);


#endif /* LED_BTN_H_ */
