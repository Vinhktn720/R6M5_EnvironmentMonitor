#include <sensor_libs/icp10101_driver.h>
#include <sensor_libs/ra6m5_i2c.h>

// he so doi ap suat sang do cao don gian
#define PA_TO_METER_FACTOR 0.085f

static float start_pressure_pa = 0.0f; // luu ap suat goc de tinh do cao
static uint8_t is_base_set = 0;		   // danh dau da set muc co so
static float offset_altitude = 15.24f; // do cao offset tuy thuoc thuc te

// --- CAC HAM XU LY CAM BIEN ---
// khoi tao cac tham so
static void init_base(icp10101_t *s, short *otp) {
    for(int i = 0; i < 4; i++) s->sensor_constants[i] = (float)otp[i];
    s->p_Pa_calib[0] = 45000.0; s->p_Pa_calib[1] = 80000.0; s->p_Pa_calib[2] = 105000.0;
    s->LUT_lower = 3.5 * (1<<20); s->LUT_upper = 11.5 * (1<<20);
    s->quadr_factor = 1 / 16777216.0; s->offst_factor = 2048.0;
}

// tinh cac he so chuyen doi p_raw -> Pa
static void calculate_conversion_constants(icp10101_t *s, float *p_Pa, float *p_LUT, float *out) {
    (void)s; float A,B,C;
    C = (p_LUT[0]*p_LUT[1]*(p_Pa[0]-p_Pa[1]) + p_LUT[1]*p_LUT[2]*(p_Pa[1]-p_Pa[2]) +
         p_LUT[2]*p_LUT[0]*(p_Pa[2]-p_Pa[0])) / (p_LUT[2]*(p_Pa[0]-p_Pa[1]) +
         p_LUT[0]*(p_Pa[1]-p_Pa[2]) + p_LUT[1]*(p_Pa[2]-p_Pa[0]));
    A = (p_Pa[0]*p_LUT[0] - p_Pa[1]*p_LUT[1] - (p_Pa[1]-p_Pa[0])*C) / (p_LUT[0] - p_LUT[1]);
    B = (p_Pa[0] - A) * (p_LUT[0] + C);
    out[0] = A; out[1] = B; out[2] = C;
}

static int icp10101_read_otp(short *otp) {
	uint8_t buf[3];

		// Gui lenh Unlock OTP (0xC5 95 00 66 9C)
	    if (i2c_common_write_reg(ICP10101_I2C_ADDR_WRITE, 0xC59500669CULL, 5) != 0) return -1;
	    i2c_common_delay_ms(1);
	    // Doc 4 gia tri OTP
	    for (int i = 0; i < 4; i++) {
	        // Gui lenh Set Pointer (0xC7 F7)
	        if (i2c_common_write_reg(ICP10101_I2C_ADDR_WRITE, 0xC7F7ULL, 2) != 0) return -1;
	        i2c_common_start();
	        // Doc 3 byte (MSB, LSB, CRC)
	        if (i2c_common_read_burst(ICP10101_I2C_ADDR_READ, buf, 3) != 0) return -1;
	        // Gop byte
	        otp[i] = (short)((buf[0] << 8) | buf[1]);
	    }
	    return 0;
}

int icp10101_sensor_init(icp10101_t *s) {
    short otp[4];
    if (icp10101_read_otp(otp) != 0) return -1;
    init_base(s, otp);
    return 0;
}

void icp10101_start_measurement(void) {
	i2c_common_write_reg(ICP10101_I2C_ADDR_WRITE, 0x48A3ULL, 2);
	i2c_common_delay_ms(6); // Cho chuyen doi
}

void icp10101_read_raw_data(int32_t *p_raw, int16_t *t_raw) {
	uint8_t buf[9];
	// Doc mot lan 9 byte
		i2c_common_start();
	    if (i2c_common_read_burst(ICP10101_I2C_ADDR_READ, buf, 9) != 0) {
	        return; // Loi doc
	    }

	    // Pressure (buf[0], buf[1], buf[3])
	    int32_t p_val = (int32_t)((buf[0] << 16) | (buf[1] << 8) | buf[3]);
	    *p_raw = p_val;

	    // Temperature (buf[6], buf[7])
	    *t_raw = (int16_t)((buf[6] << 8) | buf[7]);
}


void icp10101_process_data(icp10101_t *s, int32_t p_raw, int16_t t_raw, float *pressure, float *temperature, float *altitude) {
    float t = (float)(t_raw - 32768);
    float s1,s2,s3, in[3], out[3];
    s1 = s->LUT_lower + (s->sensor_constants[0]*t*t)*s->quadr_factor;
    s2 = s->offst_factor*s->sensor_constants[3] + (s->sensor_constants[1]*t*t)*s->quadr_factor;
    s3 = s->LUT_upper + (s->sensor_constants[2]*t*t)*s->quadr_factor;
    in[0]=s1; in[1]=s2; in[2]=s3;
    calculate_conversion_constants(s, s->p_Pa_calib, in, out);
    float A=out[0], B=out[1], C=out[2];
    *pressure    = A + B / (C + (float)p_raw);
    *temperature = -45.f + (175.f/65536.f) * (float)t_raw;
    *temperature = *temperature - 5.8f; //bu nhiet

    // chi set muc co so khi sensor on dinh
    if (is_base_set == 0 && *pressure > 90000.0f) {
            start_pressure_pa = *pressure;
            is_base_set = 1;
    }

    if (is_base_set) {
    		// Cong thuc Datasheet: Chenh lech ap suat * 0.085
            float pressure_diff = start_pressure_pa - *pressure;
            *altitude = pressure_diff * PA_TO_METER_FACTOR + offset_altitude;
    } else {
            *altitude = 0.0f;
    }
}
