#include <sensor_libs/icp10101_driver.h>
#include <sensor_libs/ra6m5_i2c.h>
#include <sensor_libs/zmod4410_driver.h>
#include "hal_data.h"
#include <string.h>

// Include thu vien ZMOD4410
#include <sensor_libs/iaq_2nd_gen.h>
#include <sensor_libs/zmod4410_config_iaq2.h>
#include <sensor_libs/zmod4xxx.h>

// Cac trang thai cua may trang thai
typedef enum {
    ZMOD_STATE_IDLE,		// cho sensor nghi
    ZMOD_STATE_MEASURING,	// dang do
    ZMOD_STATE_READING,		// dang doc ket qua
    ZMOD_STATE_WAITING		// doi giua cac lan do
} zmod_state_t;

// Bien tinh
static zmod4xxx_dev_t dev;
static iaq_2nd_gen_handle_t algo_handle;
static iaq_2nd_gen_results_t lib_results;
static zmod_results_t public_results = {0};
static uint8_t zmod_prod_data[ZMOD4410_PROD_DATA_LEN];
static uint8_t adc_result[ZMOD4410_ADC_DATA_LEN] = {0};
static zmod_state_t current_state = ZMOD_STATE_IDLE;
static uint32_t wait_counter = 0;
static uint32_t sample = 0;
static uint32_t sequence_start_tick = 0; // Luu thoi diem bat dau do

/* --- CAC HAM WRAPPER I2C --- */
static void zmod_delay_ms(uint32_t ms) {
    i2c_common_delay_ms(ms);
}

static int8_t zmod_i2c_write(uint8_t addr, uint8_t reg_addr, uint8_t *data_buf, uint8_t len) {
	i2c_common_start();
	if (i2c_common_write_byte_raw((uint8_t)(addr << 1)) != 0) {
	    i2c_common_stop();
	    return -1;
	}
	if (i2c_common_check_NACKF() != 0) return -1;

	if (i2c_common_write_byte_raw(reg_addr) != 0) {
	    i2c_common_stop();
	    return -1;
	}
	if (i2c_common_check_NACKF() != 0) return -1;

	for (int i = 0; i < len; i++) {
	     for(volatile int k=0; k<1000; k++); // Delay an toan
	     if (i2c_common_write_byte_raw(data_buf[i]) != 0) {
	             i2c_common_stop();
	             return -1;
	     }
	     if (i2c_common_check_NACKF() != 0) return -1;
	}
	i2c_common_stop();
	return 0;
}

static int8_t zmod_i2c_read(uint8_t addr, uint8_t reg_addr, uint8_t *data_buf, uint8_t len) {
	if (len == 0) return -1;
	// ---  Ghi dia chi va Repeated Start ---
	i2c_common_start();
	if (i2c_common_write_byte_raw((uint8_t)(addr << 1)) != 0) { i2c_common_stop(); return -1; }
	if (i2c_common_check_NACKF() != 0) return -1;

	if (i2c_common_write_byte_raw(reg_addr) != 0) { i2c_common_stop(); return -1; }
	if (i2c_common_check_NACKF() != 0) return -1;

	i2c_common_repeated_start();
	i2c_recovery_delay_us();

	i2c_common_read_burst((uint8_t)((addr << 1) | 1), data_buf, len);
	// CHECK BBSY: Cho Bus thuc su ranh
	uint32_t timeout = 10000;
	while ((R_IIC0->ICCR2 & (1 << 7)) != 0) { //
			if (--timeout == 0) {
				i2c_bus_recovery();
				i2c_common_init();
				i2c_common_delay_ms(100);
			}
	}
	return 0;
}

int8_t zmod4410_init_system(void) {
    int8_t ret;

    // Cau hinh struct driver
    dev.i2c_addr = ZMOD4410_ADDR;
    dev.pid = ZMOD4410_PID; // Su dung PID tu file config

    // Gan cau hinh INIT (Index 0) cho viec khoi tao
    dev.init_conf = &zmod_iaq2_sensor_cfg[INIT];

    // Gan cau hinh MEASUREMENT (Index 1) cho viec do dac
    dev.meas_conf = &zmod_iaq2_sensor_cfg[MEASUREMENT];
    // ----------------------------------
    dev.prod_data = zmod_prod_data;
    dev.write = zmod_i2c_write;
    dev.read = zmod_i2c_read;
    dev.delay_ms = zmod_delay_ms;

    // Doc info & prepare sensor
    ret = zmod4xxx_read_sensor_info(&dev);
    if (ret != 0) return -1;

    ret = zmod4xxx_prepare_sensor(&dev);
        if (ret != 0) return -1;

    // Init thuat toan
    ret = init_iaq_2nd_gen(&algo_handle);
    if (ret != 0) return -1;

    current_state = ZMOD_STATE_IDLE;
    return 0;
}

void zmod4410_loop_task(icp10101_results_t *s) {
    int8_t ret;
    uint8_t status;
    static uint32_t polling_timer = 0;

    switch (current_state) {
    		case ZMOD_STATE_IDLE:
    			// bat dau measurement
   	            sequence_start_tick = millis();
   	            ret = zmod4xxx_start_measurement(&dev);
   	            if (ret != 0){
   	                // error
   	                current_state = ZMOD_STATE_IDLE;
   	                break;
   	            }
   	            // Optionally delay a bit after start
   	            polling_timer = millis();
   	            current_state = ZMOD_STATE_MEASURING;
   	            break;
    		case ZMOD_STATE_MEASURING:
    			// check sequencer moi 100ms
    			if ((millis() - polling_timer) < 100) return; // Chua den luc kiem tra, thoat ham de CPU lam viec khac
    			polling_timer = millis(); // Reset timer cho lan check ke tiep
    			ret = zmod4xxx_read_status(&dev, &status);
    		    if (ret != 0) {
    		        current_state = ZMOD_STATE_IDLE;
    		        return;
    		    }
    		         // Neu sequencer van dang chay, thoat ham, lan sau quay lai check tiep
    		    if (status & STATUS_SEQUENCER_RUNNING_MASK) return;
    		         // Optional: check error event (POR / access conflict)
    		    ret = zmod4xxx_check_error_event(&dev);
    		    if (ret != ZMOD4XXX_OK) {
    		       // Co loi (POR, v.v.), reset ve IDLE de do lai tu dau
    		       if (ret == ERROR_POR_EVENT) zmod4xxx_prepare_sensor(&dev); // Can prepare lai neu POR
    		       current_state = ZMOD_STATE_IDLE;
    		       return;
    		    }
    		    current_state = ZMOD_STATE_READING;
    		    break;
    		case ZMOD_STATE_READING:
    			ret = zmod4xxx_read_adc_result(&dev, adc_result);
    			if (ret != 0) {
           	        current_state = ZMOD_STATE_IDLE;
           	        break;
           	    }
    			else {
    				// Kiem tra neu toan 0 → coi nhu invalid / sensor chua ready
    				uint8_t sum = 0;
	                for (int i = 0; i < ZMOD4410_ADC_DATA_LEN; i++) sum += adc_result[i];
	                if (sum == 0) {
	                    // invalid read -> thu do lai
	                    current_state = ZMOD_STATE_IDLE;
	                    break;
	                }

	                // Chuan bi input cho thuat toan Gen 2
	                iaq_2nd_gen_inputs_t algo_input;
	                memset(&algo_input, 0, sizeof(algo_input));
	                algo_input.adc_result = adc_result;
	                if (s != NULL) {
                       algo_input.humidity_pct = 50.0f; // Hoac s->humidity neu co
                       algo_input.temperature_degc = s->temperature;
                    }
	                else {
                       algo_input.humidity_pct = 50.0f;
                       algo_input.temperature_degc = 25.0f; // Gia tri mac dinh an toan
                    }

	                // Goi ham tinh toan (5 tham so)
	                ret = calc_iaq_2nd_gen(&algo_handle, &dev, NULL, &algo_input, &lib_results);

	                if(ret == IAQ_2ND_GEN_OK) {
	                	public_results.iaq  = lib_results.iaq;
	                	public_results.tvoc = lib_results.tvoc;
	                	public_results.eco2 = lib_results.eco2;
	                	public_results.etoh = lib_results.etoh;
	                	public_results.new_data_available = 1;
	                }
	                else if(ret == IAQ_2ND_GEN_STABILIZATION) {
                    // Sensor chua on dinh, co the log hoac bo qua
	                }
    			}

            wait_counter = 0;
            current_state = ZMOD_STATE_WAITING;
            break;
    		case ZMOD_STATE_WAITING:
        	     // Sau moi measurement (IAQ/TVOC/eCO2/ETOH) delay it nhat 3000 ms
    			if ((millis() - sequence_start_tick) >= 3000) {
    				current_state = ZMOD_STATE_IDLE;
    				sample++;
    			}
    			break;
    }
}

zmod_results_t zmod4410_get_current_results(void) {
    zmod_results_t temp = public_results;
    public_results.new_data_available = 0;
    return temp;
}
