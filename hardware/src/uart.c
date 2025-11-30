#include "uart.h"

static bool last_packet_was_acked = false; // Initialize to false

/* * Initialize UART5
 * Follows the strict sequence:
 * 1. Stop Module -> 2. Setup Registers -> 3. Setup Pins -> 4. Start Module
 */
void uart5_init(void) {
    /* 1. Stop Module & Disable Interrupts */
    R_MSTP->MSTPCRB_b.MSTPB26 = 0;      // Enable SCI5 Module (Cancel Stop)
    R_SCI5->SCR = 0;                    // Disable TE, RE, TIE, RIE, TEIE (Reset State)

    /* 2. Configure Baud Rate & Format (9600 8N1) */
    R_SCI5->SMR_b.CKS = 0;              // PCLK/1 clock source
    R_SCI5->SMR_b.CM = 0;               // Asynchronous Mode
    R_SCI5->SMR_b.CHR = 0;              // 8-bit data
    R_SCI5->SMR_b.PE = 0;               // No Parity
    R_SCI5->SMR_b.STOP = 0;             // 1 Stop Bit
    R_SCI5->SCMR_b.SMIF = 0;            // No Smart Card interface

    /* Baud Rate Calculation for 9600 bps @ PCLK (assuming 48MHz or similar) */
    /* You had CKS=1 and N=80. Ensure this matches your PCLK frequency! */
    /* Assuming standard config from your previous code: */
    R_SCI5->SMR_b.CKS = 0b01;
    R_SCI5->BRR = 80;

    /* 3. Configure Pins (P501=RX, P502=TX) */
    R_PMISC->PWPR_b.B0WI = 0;
    R_PMISC->PWPR_b.PFSWE = 1;

    /* Configure TX Pin (P501) */
    R_PFS->PORT[5].PIN[1].PmnPFS_b.PMR = 1;
    R_PFS->PORT[5].PIN[1].PmnPFS_b.PSEL = 5; // SCI5 TX

    /* Configure RX Pin (P502) */
    R_PFS->PORT[5].PIN[2].PmnPFS_b.PMR = 1;
    R_PFS->PORT[5].PIN[2].PmnPFS_b.PSEL = 5; // SCI5 RX

    R_PMISC->PWPR_b.PFSWE = 0;
    R_PMISC->PWPR_b.B0WI = 1;

    /* 4. Enable Transmitter and Receiver */
    R_SCI5->SCR_b.TE = 1;               // Enable TX
    R_SCI5->SCR_b.RE = 1;               // Enable RX

    /* Dummy read to clear any initial garbage */
    volatile uint8_t dummy = R_SCI5->RDR;
    (void)dummy;
}

/* * Blocking Send Packet with TEND Check
 */
void uart5_send_packet(SystemData_t *pkt) {
    uint8_t *raw_bytes = (uint8_t *)pkt;
    size_t len = sizeof(SystemData_t);
    taskENTER_CRITICAL();
    /* 1. Send START */
    while (R_SCI5->SSR_b.TDRE == 0);
    R_SCI5->TDR = 0xAA;
    R_SCI5->SSR_b.TDRE = 0;

    /* 2. Send Payload */
    for(size_t i = 0; i < len; i++) {
        while (R_SCI5->SSR_b.TDRE == 0);
        R_SCI5->TDR = raw_bytes[i];
        R_SCI5->SSR_b.TDRE = 0;
    }

    /* 3. Send END */
    while (R_SCI5->SSR_b.TDRE == 0);
    R_SCI5->TDR = 0x55;
    R_SCI5->SSR_b.TDRE = 0;

    /* 4. Wait for Shift Register Empty */
    while (R_SCI5->SSR_b.TEND == 0);
    taskEXIT_CRITICAL();
}

/* * Wait for ACK with Error Clearing
 */
bool uart5_wait_ack(uint32_t timeout_ms) {
    for(uint32_t i = 0; i < timeout_ms; i++) {

        /* ERROR HANDLING: Clear Frame/Overrun errors if they exist */
        if (R_SCI5->SSR_b.ORER || R_SCI5->SSR_b.FER || R_SCI5->SSR_b.PER) {
            /* Clear error flags to re-enable reception */
            R_SCI5->SSR_b.ORER = 0;
            R_SCI5->SSR_b.FER = 0;
            R_SCI5->SSR_b.PER = 0;
            /* Dummy read RDR to clear buffer */
            volatile uint8_t dummy = R_SCI5->RDR;
            (void)dummy;
        }

        /* Check for Data */
        if(R_SCI5->SSR_b.RDRF) {
            uint8_t val = R_SCI5->RDR;
            R_SCI5->SSR_b.RDRF = 0; // Clear flag

            if (val == 'A') {
                last_packet_was_acked = true;
                return true;
            }
        }
        R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
    }

    last_packet_was_acked = false;
    return false;
}

bool uart5_check_connection(void) {
    return last_packet_was_acked;
}
