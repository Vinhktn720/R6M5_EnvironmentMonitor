/*
 * led_btn.c
 *
 *  Created on: 29 thg 11, 2025
 *      Author: buiqu
 */
#include "led_btn.h"

void ui_init(void)
{
    /* Unlock PFS Registers */
    R_PMISC->PWPR_b.B0WI = 0;
    R_PMISC->PWPR_b.PFSWE = 1;

    /* LED Configuration (Output) */
    /* P601 (Blue), P609 (Green), P610 (Red) */
    /* Set Pin Mode Control to GPIO (0) implicitly by default or explicitly */
    R_PFS->PORT[6].PIN[1].PmnPFS_b.PMR = 0;
    R_PFS->PORT[6].PIN[9].PmnPFS_b.PMR = 0;
    R_PFS->PORT[6].PIN[10].PmnPFS_b.PMR = 0;

    /* Set Direction to Output (PDR = 1) */
    R_PORT6->PDR_b.PDR1 = 1;
    R_PORT6->PDR_b.PDR9 = 1;
    R_PORT6->PDR_b.PDR10 = 1;

    /* Turn OFF all LEDs initially (Active Low: 1=OFF, 0=ON) */
    R_PORT6->PODR_b.PODR1 = 1;
    R_PORT6->PODR_b.PODR9 = 1;
    R_PORT6->PODR_b.PODR10 = 1;

    /* Button Configuration (Input) */
    /* P804 */
    R_PORT8->PDR_b.PDR4 = 0;            // Input
    R_PFS->PORT[8].PIN[4].PmnPFS_b.PCR = 1; // Enable Pull-up resistor

    /* Lock PFS Registers */
    R_PMISC->PWPR_b.PFSWE = 0;
    R_PMISC->PWPR_b.B0WI = 1;
}

/* Helper to set LED colors */
/* State: 0 = ALL OFF, 1 = BLUE, 2 = GREEN, 3 = RED */
void led_set_color(uint8_t color)
{
    /* Turn ALL OFF first */
    R_PORT6->PODR_b.PODR1 = 0;  // Blue Off
    R_PORT6->PODR_b.PODR9 = 0;  // Green Off
    R_PORT6->PODR_b.PODR10 = 0; // Red Off

    if (color == 1)      R_PORT6->PODR_b.PODR1 = 1;  // Blue ON
    else if (color == 2) R_PORT6->PODR_b.PODR9 = 1;  // Green ON
    else if (color == 3) R_PORT6->PODR_b.PODR10 = 1; // Red ON
}

void led_toggle_blue(void)
{
    /* XOR with 1 to toggle bit */
    R_PORT6->PODR_b.PODR1 ^= 1;
    /* Ensure others are off */
    R_PORT6->PODR_b.PODR9 = 0;
    R_PORT6->PODR_b.PODR10 = 0;
}

void led_toggle_green(void)
{
    R_PORT6->PODR_b.PODR9 ^= 1;
    R_PORT6->PODR_b.PODR1 = 0;
    R_PORT6->PODR_b.PODR10 = 0;
}

/* Returns 1 if button is pressed (Active Low), 0 otherwise */
uint8_t is_button_pressed(void)
{
    /* Read PIDR (Port Input Data Register) */
    /* If bit is 0, button is pressed (due to pull-up) */
    if (R_PORT8->PIDR_b.PIDR4 == 0) return 1;
    else return 0;
}

