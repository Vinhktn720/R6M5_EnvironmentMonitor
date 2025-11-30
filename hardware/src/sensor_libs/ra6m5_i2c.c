#include <sensor_libs/ra6m5_i2c.h>
#include "hal_data.h"

/* FreeRTOS Includes */
#include "FreeRTOS.h"
#include "task.h"

/* * REMOVED: volatile uint32_t g_tick_count...
 * REMOVED: void SysTick_Handler(void)...
 * FreeRTOS owns SysTick now.
 */

/* * Wrapper for system time (ms)
 * NOTE: Ensure configTICK_RATE_HZ is 1000 in FreeRTOSConfig.h for 1ms resolution
 */
uint32_t millis(void) {
    /* Check if scheduler is running to prevent crashes during early init */
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return 0;
    }

    /* Convert Ticks to Milliseconds */
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/* * Delay ms using RTOS (Yielding)
 * This allows other tasks to run while waiting, which is much better than polling.
 */
void i2c_common_delay_ms(volatile uint32_t ms) {
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        /* RTOS Way: Yield CPU to other tasks */
        vTaskDelay(pdMS_TO_TICKS(ms));
    } else {
        /* Fallback Way: Blocking delay if called before scheduler starts */
        R_BSP_SoftwareDelay(ms, BSP_DELAY_UNITS_MILLISECONDS);
    }
}

// delay ngan dung trong recovery
void i2c_recovery_delay_us(void) {
    for (volatile int i = 0; i < 2000; i++);
}

// Bao ve bus I2C khi bi treo
void i2c_bus_recovery(void) {
	// mo khoa ghi thanh ghi pin (cho phep sua PFS)
    R_PMISC->PWPR = 0;
    R_PMISC->PWPR = 0x40; // cho phep ghi PFSWE

    // dua SCL/SDA ve GPIO
    SCL_PIN_PFS &= ~(1U << 16);
    SDA_PIN_PFS &= ~(1U << 16);

    // dat PODR=0 (muc thap khi output)
    SCL_PIN_PFS &= ~1U; // PODR = 0
    SDA_PIN_PFS &= ~1U; // PODR = 0

    // dat chan o che do input (PDR=0)
    SCL_PIN_PFS &= ~(1U << 2);
    SDA_PIN_PFS &= ~(1U << 2);

    i2c_recovery_delay_us();

    // tao 9 xung clock de giai phong SDA
    for (int i = 0; i < 9; i++) {
    	 // keo SCL low
        SCL_PIN_PFS |= (1U << 2);
        i2c_common_delay_ms(1);

        // tha SCL high
        SCL_PIN_PFS &= ~(1U << 2);
        i2c_common_delay_ms(1);
    }

    // tao STOP condition bang bit-bang
    SCL_PIN_PFS |= (1U << 2);  // low
    i2c_recovery_delay_us();
    SDA_PIN_PFS |= (1U << 2); // low
    i2c_recovery_delay_us();
    SCL_PIN_PFS &= ~(1U << 2); // high
    i2c_recovery_delay_us();
    SDA_PIN_PFS &= ~(1U << 2); // high => STOP
    i2c_recovery_delay_us();
    // khoa lai ghi thanh ghi pin
    R_PMISC->PWPR = 0;
    R_PMISC->PWPR = 0x80; // khoa PFS
}

// --- TRIEN KHAI HAM MUC THAP ---
int32_t i2c_wait_flag(volatile uint8_t *reg, uint8_t flag, uint8_t expected_val) {
    volatile uint32_t timeout = 100000;
    while (timeout--) {
        if (((*reg) & flag) == expected_val) return 0;
    }
    return -1;
}

// khoi tao I2C o che do MASTER
void i2c_common_init(void) {
	// mo clock IIC0
	R_SYSTEM_PRCR = 0xA502;
	R_MSTP->MSTPCRB &= (uint32_t) ~(MSTPCRB_MSTPB9);
	R_SYSTEM_PRCR = 0xA500;

	// cau hinh chan SCL/SDA
	R_PMISC->PWPR = 0;
	R_PMISC->PWPR = 0x40;
	R_P400PFS = (0x07 << 24) | (1 << 16) | (1 << 6); // P400 = SCL
	R_P401PFS = (0x07 << 24) | (1 << 16) | (1 << 6); // P401 = SDA
	R_PMISC->PWPR = 0x80;

	// dua IIC vao reset
	R_IIC0->ICCR1 &= ~(1 << 7); //SCL, SDA o trang thai inactive
	R_IIC0->ICCR1 |= (1 << 6); //Reset khoi IIC

	// tat che do slave
	R_IIC0->ICSER = 0x00; // Khong dung Slave status

	// cai dat toc do I2C (BRL/BRH)
	R_IIC0->ICMR1 &= ~(0b000 << 4); // CKS = 000 (PCLK/1)
	R_IIC0->ICBRL = 74;             // Low width
	R_IIC0->ICBRH = 74;             // High width

	// tat wait, tat bo loc nhieu
	R_IIC0->ICMR2 = 0x00;
	R_IIC0->ICMR3 = 0x00; // Tat Wait insertion, tat bo loc nhieu so muc cao

	// bat cac chuc nang (Timeout, SCLE...)
	R_IIC0->ICFER = (1 << 4) | (1 << 6); // Bat SCLE (SCL Synchronous)
	R_IIC0->ICIER = 0x00; // Cai dat ngat (Tat het vi dung polling)
	R_IIC0->ICCR1 &= ~(1 << 6); // Thoat khoi trang thai internal reset
	R_IIC0->ICCR1 |= (1 << 7); //Internal reset, chan o trang thai active

	 // cau hinh chan reset (P306) la output
	 // mo khoa ghi thanh ghi
	 R_PMISC->PWPR = 0;
	 R_PMISC->PWPR = 0x40;
	 // cau hinh P307: output, high
	 R_PFS->PORT[3].PIN[6].PmnPFS = (1 << 2); // PDR = 1 (Output)
	 R_PFS->PORT[3].PIN[6].PmnPFS_b.PODR = 1; // Mac dinh muc cao
	 // khoa lai
	 R_PMISC->PWPR = 0x80;
	 // keo chan P307 xuong thap de reset
	 R_PFS->PORT[3].PIN[6].PmnPFS_b.PODR = 0;
	 i2c_common_delay_ms(10);
	 // keo len cao de hoat dong
     R_PFS->PORT[3].PIN[6].PmnPFS_b.PODR = 1;
	 i2c_common_delay_ms(50);
}

// gui START condition
void i2c_common_start(void) {
    i2c_wait_flag(&(R_IIC0->ICCR2), IIC_ICCR2_BBSY, 0);
    R_IIC0->ICCR2 |= IIC_ICCR2_ST;
}

// gui REPEATED START
void i2c_common_repeated_start(void) {
    // cho bus ban (BBSY = 1) - restart chi xay ra khi dang giu bus
    // va cho TEND cua byte truoc do (hoac TDRE)
    i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_TEND, IIC_ICSR2_TEND);
    // yeu cau Restart (RS bit)
    R_IIC0->ICCR2 |= IIC_ICCR2_RS;
    // cho TDRE bat len (san sang ghi Address tiep theo)
    i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_TDRE, IIC_ICSR2_TDRE);
}

// gui STOP
void i2c_common_stop(void) {
	    i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_TEND, IIC_ICSR2_TEND);
	    R_IIC0->ICCR2 |= IIC_ICCR2_SP;
	    i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_STOP, IIC_ICSR2_STOP);
	    R_IIC0->ICSR2 &= ~(IIC_ICSR2_NACKF | IIC_ICSR2_STOP);
}

// ghi 1 byte thap cap (raw)
int8_t i2c_common_write_byte_raw(uint8_t data) {
	if (i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_TDRE, IIC_ICSR2_TDRE) != 0) {
	        return -1; // Timeout
	    }
    R_IIC0->ICDRT = data;
    return 0;
}

// doc 1 byte thap cap (raw)
uint8_t i2c_common_read_byte_raw(uint8_t send_nack) {
    if (send_nack) R_IIC0->ICMR3 |= (1 << 0);
    else R_IIC0->ICMR3 &= ~(1 << 0);
    (void)R_IIC0->ICDRR;
    i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_RDRF, IIC_ICSR2_RDRF);
    return R_IIC0->ICDRR;
}

// doc byte trong che do noi bo
uint8_t i2c_read_byte_internal(void) {
    i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_RDRF, IIC_ICSR2_RDRF);
    return R_IIC0->ICDRR;
}

// kiem tra NACKF
int8_t i2c_common_check_NACKF(void){
	 if (R_IIC0->ICSR2 & IIC_ICSR2_NACKF) {
		        //NACKF = 1 (Error)
		        R_IIC0->ICCR2 |= IIC_ICCR2_SP;
		        i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_STOP, IIC_ICSR2_STOP);
		        R_IIC0->ICSR2 &= ~(IIC_ICSR2_NACKF | IIC_ICSR2_STOP);
		        return -1;
		 }
	 return 0;
}

// --- TRIEN KHAI HAM MUC CAO ---
// Ham Master Transmit
int8_t i2c_common_write_reg(uint8_t ADDR_WRITE, uint64_t data, uint8_t length) {
	    i2c_common_start();

	    if (i2c_common_check_NACKF() != 0) return -1;

	    // doi TDRE san sang nhan byte
	    i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_TDRE, IIC_ICSR2_TDRE);

	    for (int i = -1; i < length; i++) {
	            uint8_t byte_to_send;
	            if (i == -1) byte_to_send = ADDR_WRITE;
	            else byte_to_send = (uint8_t)((data >> ((length - 1 - i) * 8)) & 0xFF);
	            i2c_common_write_byte_raw(byte_to_send);
	            if (i2c_common_check_NACKF() != 0) return -1;
	    }

	    i2c_common_stop();
	    return 0;
}

// Ham Master Receive
int8_t i2c_common_read_burst(uint8_t ADD_READ, uint8_t* buffer, uint16_t length) {
	if (length == 0) return 0;

	// gui dia chi doc
	i2c_common_write_byte_raw(ADD_READ);
	if (i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_RDRF, IIC_ICSR2_RDRF) != 0) return -1;
	if (i2c_common_check_NACKF() != 0) {
				(void)R_IIC0->ICDRR;              // Dummy read
		        return -1;
		    }

	// cac truong hop dac biet 1 byte / 2 byte
	if (length == 1) {
		R_IIC0->ICMR3 |= IIC_ICMR3_WAIT;
		R_IIC0->ICMR3 |= IIC_ICMR3_ACKBT; // Set NACK ngay neu chi doc 1 byte
		(void)R_IIC0->ICDRR;              // Dummy read
	}
		else if (length == 2) {
		    	R_IIC0->ICMR3 |= IIC_ICMR3_WAIT;
		        (void)R_IIC0->ICDRR;              // Dummy read
		        i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_RDRF, IIC_ICSR2_RDRF);
		        R_IIC0->ICMR3 |= IIC_ICMR3_ACKBT;
		        buffer[0] = R_IIC0->ICDRR;
		}
		else {
		        // truong hop > 2 byte
		        (void)R_IIC0->ICDRR;              // Dummy read (ACKBT mac dinh la 0 - ACK)
		        // Vong lap doc cac byte giua (tu 0 den length-3)
		        for (uint16_t i = 0; i < (length - 3); i++) {
		            buffer[i] = i2c_read_byte_internal(); // Doc va Auto ACK
		        }
		        // chuan bi cho byte ap chot (giong truong hop length=2)
		        R_IIC0->ICMR3 |= IIC_ICMR3_WAIT;
		        buffer[length - 3] = i2c_read_byte_internal();
		        // chuan bi NACK cho byte cuoi cung
		       	i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_RDRF, IIC_ICSR2_RDRF);
		        R_IIC0->ICMR3 |= IIC_ICMR3_ACKBT;
		        buffer[length - 2] = i2c_read_byte_internal();
		}
	// doc byte cuoi cung va gui stop
	i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_RDRF, IIC_ICSR2_RDRF);
	R_IIC0->ICSR2 &= ~IIC_ICSR2_STOP;
    R_IIC0->ICCR2 |= IIC_ICCR2_SP;      // Yeu cau STOP
	buffer[length - 1] = R_IIC0->ICDRR; // Doc byte cuoi cung
	R_IIC0->ICMR3 &= ~IIC_ICMR3_WAIT;
    // cho Stop hoan tat
	i2c_wait_flag(&(R_IIC0->ICSR2), IIC_ICSR2_STOP, IIC_ICSR2_STOP);
	R_IIC0->ICSR2 &= ~(IIC_ICSR2_NACKF | IIC_ICSR2_STOP);
	R_IIC0->ICCR2 &= ~IIC_ICCR2_SP;
    return 0;
}
