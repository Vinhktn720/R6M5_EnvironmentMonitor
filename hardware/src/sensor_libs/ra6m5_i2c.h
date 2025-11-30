#ifndef RA6M5_I2C_H
#define RA6M5_I2C_H

#include <stdint.h>

// --- DINH NGHIA DIA CHI CAM BIEN ---
#define ICP10101_I2C_ADDR_WRITE 0xC6
#define ICP10101_I2C_ADDR_READ  0xC7

#define ZMOD4410_ADDR 0x32
#define ZMOD4410_ADDR_WRITE 0x64
#define ZMOD4410_ADDR_READ  0x65

// --- THANH GHI HE THONG MCU RA6M5 ---
#define R_SYSTEM_PRCR   (*(volatile uint16_t*)0x4001E3FE) // cho phep ghi he thong
#define R_MSTPCRB       (*(volatile uint32_t*)0x40084004) // dieu khien clock module
#define MSTPCRB_MSTPB9  (1 << 9)						  // bit enable IIC1
#define R_P400PFS       (*(volatile uint32_t*)0x40080900) // config chan P400
#define R_P401PFS       (*(volatile uint32_t*)0x40080904) // config chan P401

// --- BIT CO TRANG THAI I2C ---
#define IIC_ICSR2_TDRE  (1 << 7)
#define IIC_ICSR2_TEND 	(1 << 6)
#define IIC_ICSR2_RDRF  (1 << 5)
#define IIC_ICSR2_NACKF (1 << 4)
#define IIC_ICSR2_STOP  (1 << 3)
#define IIC_ICSR2_START (1 << 2)
#define IIC_ICCR2_BBSY  (1 << 7)
#define IIC_ICCR2_SP    (1 << 3)
#define IIC_ICCR2_RS    (1 << 2)
#define IIC_ICCR2_ST    (1 << 1)
#define IIC_ICMR3_WAIT  (1 << 6)
#define IIC_ICMR3_ACKBT (1 << 3)

// --- GPIO CHO I2C RECOVERY (P400=SCL, P401=SDA) ---
#define SCL_PIN_PFS  R_PFS->PORT[4].PIN[0].PmnPFS
#define SDA_PIN_PFS  R_PFS->PORT[4].PIN[1].PmnPFS
#define SCL_PIN_PODR R_PFS->PORT[4].PIN[0].PmnPFS_b.PODR
#define SDA_PIN_PODR R_PFS->PORT[4].PIN[1].PmnPFS_b.PODR
#define SDA_PIN_PIDR R_PFS->PORT[4].PIN[1].PmnPFS_b.PIDR

// ham khoi tao he thong I2C
void i2c_system_init(void);

// --- NHOM HAM I2C MUC THAP ---
int32_t i2c_wait_flag(volatile uint8_t *reg, uint8_t flag, uint8_t expected_val);
uint32_t millis(void);
void i2c_recovery_delay_us(void);
void i2c_bus_recovery(void);
void i2c_common_init(void);
void i2c_common_delay_ms(volatile uint32_t ms);
void i2c_common_start(void);
void i2c_common_stop(void);
void i2c_common_repeated_start(void);
int8_t i2c_common_check_NACKF(void);
uint8_t i2c_read_byte_internal(void);
int8_t i2c_common_write_byte_raw(uint8_t data);
uint8_t i2c_common_read_byte_raw(uint8_t send_nack);

// --- HAM I2C MUC CAO ---
int8_t i2c_common_write_reg(uint8_t ADDR_WRITE, uint64_t data, uint8_t len);
int8_t i2c_common_read_burst(uint8_t ADD_READ, uint8_t* buffer, uint16_t length);
#endif
