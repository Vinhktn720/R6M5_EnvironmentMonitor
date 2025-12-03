/*****************************************************************************
 * Copyright (c) 2024 Renesas Electronics Corporation
 * All Rights Reserved.
 * 
 * This code is proprietary to Renesas, and is license pursuant to the terms and
 * conditions that may be accessed at:
 * https://www.renesas.com/eu/en/document/msc/renesas-software-license-terms-gas-sensor-software
 *****************************************************************************/

/**
 * @addtogroup iaq2_api
 * @{
 * @file    iaq_2nd_gen.h
 * @author  Renesas Electronics Corporation
 * @version 4.2.0
 * @brief   This file contains the data structure definitions and
 *          the function definitions for the 2nd generation IAQ algorithm.
 * @details The library contains an algorithm to calculate an EtOH, TVOC and
 *          IAQ index from ZMOD4410 measurements.
 *          The implementation is made to allow more than one sensor.
 *
 */

#ifndef SENSOR_LIBS_IAQ_2ND_GEN_H_
#define SENSOR_LIBS_IAQ_2ND_GEN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <math.h>
#include <sensor_libs/zmod4xxx_types.h>

/**
* @brief Variables that describe the library version
*/
typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
} algorithm_version;

/** \addtogroup RetCodes Return codes of the algorithm functions.
 *  \ingroup iaq2_api
 *  @{
 */
#define IAQ_2ND_GEN_OK            (0) /**< everything okay */
#define IAQ_2ND_GEN_STABILIZATION (1) /**< sensor in stabilization */
#define IAQ_2ND_GEN_DAMAGE        (-102) /**< sensor damaged */
/** @}*/

/**
* @brief Variables that describe the sensor or the algorithm state.
*/

typedef struct {
    float log_rcda[9]; /**< log10 of CDA resistances. */
    float log_mox_225c_5_rcda;
    float rh_cda;
    float t_cda;
    float log_rmox_filtered[9];
    float log_mox_225c_5_filtered;
    float rh_filtered;
    float t_filtered;
    uint32_t sample_counter;
    float tvoc_smooth;
    float tvoc_deltafilter;
    float acchw;
    float accow;
    float eco2;
    float etoh;
    float iaq;
    float o3;
    float rg_mean;
    float rel_iaq_raw;
    float log10_rel_iaq_smooth;
    float var_log10_rel_iaq;
    float dev_log10_rel_iaq_target;
    uint8_t need_filter_init;
} iaq_2nd_gen_handle_t;

/**
* @brief Variables that receive the algorithm outputs.
*/
typedef struct {
    float rmox[13]; /**< MOx resistances ZMOD4410. */
    float zmod4510_rmox3; /**< MOx resistance of ZMOD4510. */
    float log_rcda; /**< log10 of CDA resistance. */
    float rhtr; /**< heater resistance. */
    float temperature; /**< Input or module temperature (degC). */
    float iaq; /**< IAQ index. */
    float tvoc; /**< TVOC concentration (mg/m^3). */
    float etoh; /**< EtOH concentration (ppm). */
    float eco2; /**< eCO2 concentration (ppm). */
    float rel_iaq; /**< relative IAQ index. */
} iaq_2nd_gen_results_t;

/**
* @brief Variables that are needed for algorithm
 * @param   [in] adc_result Value from read_adc_result function
 * @param   [in] adc_rmox3_4510 Value from read_adc_result function
 * @param   [in] humidity_pct relative ambient humidity (%)
 * @param   [in] temperature_degc ambient temperature (degC)
*/
typedef struct {
    uint8_t *adc_result;
    uint8_t *adc_rmox3_4510;
    float humidity_pct;
    float temperature_degc;
} iaq_2nd_gen_inputs_t;

/**
 * @brief   calculates algorithm results from present sample.
 * @param   [in] handle Pointer to algorithm state variable.
 * @param   [in] dev Pointer to the device.
 * @param   [in] dev4510 Pointer to the ZMOD4510 device.
 * @param   [in] algo_input Structure containing inputs required for algo calculation.
 * @param   [out] results Pointer for storing the algorithm results.
 * @return  error code.
 */
int8_t calc_iaq_2nd_gen(iaq_2nd_gen_handle_t *handle, const zmod4xxx_dev_t *dev,
                        const zmod4xxx_dev_t *dev4510,
                        const iaq_2nd_gen_inputs_t *algo_input,
                        iaq_2nd_gen_results_t *results);

/**
 * @brief   Initializes the algorithm.
 * @param   [out] handle Pointer to algorithm state variable.
 * @return  error code.
*/
int8_t init_iaq_2nd_gen(iaq_2nd_gen_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_LIBS_IAQ_2ND_GEN_H_ */

/**}*/
