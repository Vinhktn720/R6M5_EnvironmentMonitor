#include "rtc.h"
#include "hal_data.h"

/* --- HELPER FUNCTIONS (Must be defined before use) --- */

static uint8_t dec_to_bcd(uint8_t val)
{
    return (uint8_t)( (val / 10 * 16) + (val % 10) );
}

static uint8_t bcd_to_dec(uint8_t val)
{
    return (uint8_t)( (val / 16 * 10) + (val & 0x0F) );
}

/* --- PUBLIC FUNCTIONS --- */

void rtc_init_hardware(uint16_t y, uint8_t mon, uint8_t d, uint8_t h, uint8_t m, uint8_t s)
{
    /* 1. UNLOCK Register Write Protection */
    R_SYSTEM->PRCR = (uint16_t)0xA50B;

    /* 2. START THE SUB-CLOCK (if stopped) */
    if (R_SYSTEM->SOSCCR_b.SOSTP == 0)
    {
        R_SYSTEM->SOSCCR_b.SOSTP = 1; // Stop
        while (R_SYSTEM->SOSCCR_b.SOSTP == 0); // Wait
    }

    R_SYSTEM->SOMCR_b.SODRV = 0; // Standard Drive

    R_SYSTEM->SOSCCR_b.SOSTP = 0; // Start
    while (R_SYSTEM->SOSCCR_b.SOSTP == 1); // Wait

    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_SECONDS);

    if (R_SYSTEM->LOCOCR_b.LCSTP == 1)
    {
        R_SYSTEM->LOCOCR_b.LCSTP = 0;
        R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
    }

    /* 3. SELECT CLOCK SOURCE */
    R_RTC->RCR4_b.RCKSEL = 1; // Use LOCO
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_SECONDS);

    /* 4. STOP RTC */
    R_RTC->RCR2_b.START = 0;
    while(R_RTC->RCR2_b.START);

    /* Frequency Register */
    R_RTC->RFRH = 0x0000;
    R_RTC->RFRL = 0x00FF;

    /* 5. RESET RTC */
    R_RTC->RCR2_b.CNTMD = 0;
    while(R_RTC->RCR2_b.CNTMD);

    R_RTC->RCR2_b.RESET = 1;
    while(R_RTC->RCR2_b.RESET);

    /* 6. SET TIME */
    /* Explicit casting to uint8_t to fix warnings */
    R_RTC->RYRCNT  = (uint16_t)dec_to_bcd((uint8_t)(y % 100));
    R_RTC->RMONCNT = (uint8_t)dec_to_bcd(mon);
    R_RTC->RDAYCNT = (uint8_t)dec_to_bcd(d);
    R_RTC->RWKCNT  = 0;
    R_RTC->RHRCNT  = (uint8_t)dec_to_bcd(h);
    R_RTC->RMINCNT = (uint8_t)dec_to_bcd(m);
    R_RTC->RSECCNT = (uint8_t)dec_to_bcd(s);

    /* 7. START RTC */
    R_RTC->RCR2_b.START = 1;
    while(!R_RTC->RCR2_b.START);

    /* 8. LOCK */
    R_SYSTEM->PRCR = (uint16_t)0xA500;
}

void rtc_get_system_data_time(SystemData_t *data_ptr)
{
    /* Read registers with casting */
    /* RYRCNT is 16-bit, but only lower 8 bits contain BCD year */
    uint16_t yr_reg = R_RTC->RYRCNT;
    data_ptr->year   = (uint16_t)(bcd_to_dec((uint8_t)(yr_reg & 0xFF)) + 2000);

    data_ptr->month  = bcd_to_dec((uint8_t)R_RTC->RMONCNT);
    data_ptr->day    = bcd_to_dec((uint8_t)R_RTC->RDAYCNT);
    data_ptr->hour   = bcd_to_dec((uint8_t)R_RTC->RHRCNT);
    data_ptr->minute = bcd_to_dec((uint8_t)R_RTC->RMINCNT);
    data_ptr->second = bcd_to_dec((uint8_t)R_RTC->RSECCNT);
}
