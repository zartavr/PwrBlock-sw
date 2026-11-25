/*
 * Copyright 2026 Everypin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "regulator.h"

#include <math.h>

#include "middleware/i2c_mgr/i2c_mgr.h"

#include "bsp/drivers/dac/dac.h"
#include "bsp/drivers/lm51772/lm51772.h"
#include "bsp/drivers/lm51772/lm51772_conf.h"

#include "storage/calib_const.h"

#define REGULATOR_RXTX_TIMEOUT_MS (50)

extern I2CBusHandle i2c_bus1;

typedef struct
{
    uint32_t    v_target_mv;
    uint32_t    i_target_ma;
    uint32_t    v_calib_mv;
    uint32_t    i_calib_ma;
    bool        is_hiv_mode;
    bool        is_drv1_en;
    bool        is_ext_curr_lim_en;
    osMutexId_t mutex;

    CalibConst_t calib_const;
    bool         calib_const_is_write_prot;

    RegulatorDrvOutMode mode;
    RegulatorCallbacks  callbacks;

    bool is_init;
} LM51772;

static LM51772 lm_drv;

// Private prototypes
static osStatus_t read(uint8_t reg, uint8_t* val);
static osStatus_t write(uint8_t reg, uint8_t* val);
static osStatus_t modify(uint8_t reg, uint8_t mask, uint8_t val);
static uint32_t   constrain(uint32_t val, uint32_t val_min, uint32_t val_max);
static uint32_t   constrain_f(float val, uint32_t val_min, uint32_t val_max);

osStatus_t regulator_drv_init(RegulatorCallbacks* callbacks)
{
    // Init mutex for structures
    lm_drv.mutex = osMutexNew(NULL);
    // Register callback functios
    lm_drv.callbacks.on_mode_change = callbacks->on_mode_change;
    return regulator_drv_reset();
}

osStatus_t regulator_drv_reset(void)
{
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) != osOK) {
        return osError;
    }

    lm_drv.i_target_ma        = LM51772_DEFAULT_I_OUT;
    lm_drv.v_target_mv        = LM51772_DEFAULT_V_OUT;
    lm_drv.is_hiv_mode        = true;
    lm_drv.is_drv1_en         = false;
    lm_drv.is_ext_curr_lim_en = false;
    lm_drv.mode               = REGULATOR_DRV_OUT_MODE_OFF;

    // Init storage of calibration consts
    // (calib_const_init falls back to neutral coeffs on failure)
    lm_drv.calib_const_is_write_prot = true;
    osStatus_t calib_status          = calib_const_init(&lm_drv.calib_const);

    lm_drv.is_init = false;

    osMutexRelease(lm_drv.mutex);
    return calib_status;
}

osStatus_t regulator_drv_setup(uint32_t voltage_mv, uint32_t current_ma)
{
    osStatus_t status = osError;

    do {
        lm51772_off();

        if (regulator_drv_clear_faults() != osOK) {
            status = osError;
            break;
        }

        // Disable output
        if (regulator_drv_output_set(false) != osOK) {
            status = osError;
            break;
        }

        // Set ILIM mode (internal via I2C or external via DAC)
        if (regulator_drv_ext_cur_lim_set(true) != osOK) {
            status = osError;
            break;
        }

        // Disable PCM window
        if (regulator_drv_pcm_set(LM51772_PCM_WIN_LO_0) != osOK) {
            status = osError;
            break;
        }

        // Set target output voltage
        if (regulator_drv_vout_set(voltage_mv) != osOK) {
            status = osError;
            break;
        }

        // Set target current limit
        if (regulator_drv_ilim_set(current_ma) != osOK) {
            status = osError;
            break;
        }

        // Set SRAMP to minimal
        if (regulator_drv_sramp_set(LM51772_MFR_D2_DVS_SLEW_RAMP_05mv_us) !=
            osOK) {
            status = osError;
            break;
        }

        // Enable CDC
        if (regulator_drv_cdc_en_set(true) != osOK) {
            status = osError;
            break;
        }

        // Set default CDC_GAIN
        if (regulator_drv_cdc_gain_set(LM51772_MFR_D8_CDC_GAIN_250mv) != osOK) {
            status = osError;
            break;
        }

        // Enable convertion (Power stage)
        if (regulator_drv_conv_en_set(true) != osOK) {
            status = osError;
            break;
        }

        status = osOK;
    } while (0);

    // Internal flag of init ready
    if (status == osOK) {
        lm51772_on();
        lm_drv.is_init = true;
    }

    return status;
}

osStatus_t regulator_drv_pcm_set(uint8_t val)
{
    uint8_t    buf    = val;
    uint8_t    mask   = LM51772_MFR_SPECIFIC_D9_PCM_WINDOW_LOW_Msk;
    osStatus_t status = osError;

    if (modify(LM51772_REG_MFR_SPECIFIC_D9, mask, buf) == osOK) {
        status = osOK;
    }

    return status;
}

osStatus_t regulator_drv_sramp_set(uint8_t val)
{
    uint8_t    buf    = val;
    uint8_t    mask   = LM51772_MFR_SPECIFIC_D2_DVS_SLEW_RAMP_Msk;
    osStatus_t status = osError;

    if (modify(LM51772_REG_MFR_SPECIFIC_D2, mask, buf) == osOK) {
        status = osOK;
    }

    return status;
}

osStatus_t regulator_drv_cdc_en_set(bool is_enabled)
{
    uint8_t    buf;
    uint8_t    mask   = LM51772_MFR_SPECIFIC_D8_EN_CDC_Msk;
    osStatus_t status = osError;


    if (is_enabled == true) {
        buf = LM51772_MFR_SPECIFIC_D8_EN_CDC;
    }
    else {
        buf = (uint8_t)~LM51772_MFR_SPECIFIC_D8_EN_CDC;
    }

    if (modify(LM51772_REG_MFR_SPECIFIC_D8, mask, buf) == osOK) {
        status = osOK;
    }

    return status;
}

osStatus_t regulator_drv_cdc_gain_set(uint8_t val)
{
    uint8_t    buf    = val;
    uint8_t    mask   = LM51772_MFR_SPECIFIC_D8_CDC_GAIN_Msk;
    osStatus_t status = osError;

    if (modify(LM51772_REG_MFR_SPECIFIC_D8, mask, buf) == osOK) {
        status = osOK;
    }

    return status;
}

osStatus_t regulator_drv_clear_faults(void)
{
    uint8_t buf;
    return read(LM51772_REG_CLEAR_FAULTS, &buf);
}

osStatus_t regulator_drv_conv_en_set(bool is_enabled)
{
    uint8_t    buf;
    uint8_t    mask   = LM51772_MFR_SPECIFIC_D0_CONV_EN_Msk;
    osStatus_t status = osError;

    if (is_enabled == true) {
        buf = LM51772_MFR_SPECIFIC_D0_CONV_EN;
    }
    else {
        buf = (uint8_t)~LM51772_MFR_SPECIFIC_D0_CONV_EN;
    }

    if (modify(LM51772_REG_MFR_SPECIFIC_D0, mask, buf) == osOK) {
        status = osOK;
    }

    return status;
}

osStatus_t regulator_drv_output_set(bool is_enabled)
{
    uint8_t buf = LM51772_MFR_D8_SEL_DRV1_SEQ_PULL_LOW_CR_RUN_CONV_OFF;
    uint8_t mask =
        (LM51772_MFR_SPECIFIC_D8_SEL_DRV1_SEQ_Msk |
         LM51772_MFR_SPECIFIC_D8_SEL_DRV1_SUP_Msk);
    osStatus_t status = osError;

    if (is_enabled == true) {
        buf |= LM51772_MFR_D8_SEL_DRV1_SUP_VCC2;
    }
    else {
        buf |= LM51772_MFR_D8_SEL_DRV1_SUP_OPEN_DRAIN;
    }


    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) != osOK) {
        return osError;
    }

    if (modify(LM51772_REG_MFR_SPECIFIC_D8, mask, buf) == osOK) {
        status             = osOK;
        lm_drv.is_drv1_en  = is_enabled;
        lm_drv.is_hiv_mode = true;
    }

    output_en_mcu_set(is_enabled);

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return status;
}

bool regulator_drv_output_get(void)
{
    bool val = false;
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        val = lm_drv.is_drv1_en;
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return val;
}

osStatus_t regulator_drv_vout_set(uint32_t voltage_mv)
{
    osStatus_t status = osError;

    uint32_t v_min =
        lm_drv.is_hiv_mode ?
            LM51772_HI_V_MIN_VOLTAGE_MV :  // Mode: 3.3-48V, step 20 mV
            LM51772_LO_V_MIN_VOLTAGE_MV;   // Mode: 1.0-24V, step 10 mV
    uint32_t v_max =
        lm_drv.is_hiv_mode ?
            LM51772_HI_V_MAX_VOLTAGE_MV :  // Mode: 3.3-48V, step 20 mV
            LM51772_LO_V_MAX_VOLTAGE_MV;   // Mode: 1.0-24V, step 10 mV
    uint32_t step =
        lm_drv.is_hiv_mode ?
            LM51772_HI_V_VOLTAGE_STEP_MV :  // Mode: 3.3-48V, step 20 mV
            LM51772_LO_V_VOLTAGE_STEP_MV;   // Mode: 1.0-24V, step 10 mV

    // Clamp incoming target voltage
    voltage_mv = constrain(voltage_mv, v_min, v_max);

    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) != osOK) {
        return osError;
    }

    // Calibration coeffs apply
    float v_calib_mv = (float)voltage_mv * lm_drv.calib_const.val.v_slope +
                       lm_drv.calib_const.val.v_offset;

    // Re-clamp after calibration
    lm_drv.v_calib_mv = constrain_f(v_calib_mv, v_min, v_max);

    uint8_t vout[2] = {
        ((lm_drv.v_calib_mv / step) >> 8) & 0x0F,
        ((lm_drv.v_calib_mv / step)) & 0xFF,
    };

    do {
        if (write(LM51772_REG_VOUT_TARGET1_MSB, &vout[0]) != osOK) {
            status = osError;
            break;
        }

        if (write(LM51772_REG_VOUT_TARGET1_LSB, &vout[1]) != osOK) {
            status = osError;
            break;
        }

        lm_drv.v_target_mv = voltage_mv;
        status             = osOK;

    } while (0);

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return status;
}

uint32_t regulator_drv_vout_get(void)
{
    uint32_t val = 0;
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        val = lm_drv.v_target_mv;
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return val;
}

osStatus_t regulator_drv_ilim_set(uint32_t current_ma)
{
    uint8_t    buf;
    osStatus_t status = osError;

    current_ma =
        constrain(current_ma, LM51772_CURRENT_MIN_MA, LM51772_CURRENT_MAX_MA);

    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) != osOK) {
        return osError;
    }

    // Calibration coeffs apply
    float i_calib_f = (float)current_ma * lm_drv.calib_const.val.i_slope +
                      lm_drv.calib_const.val.i_offset;

    // Re-clamp after calibration
    lm_drv.i_calib_ma =
        constrain_f(i_calib_f, LM51772_CURRENT_MIN_MA, LM51772_CURRENT_MAX_MA);

    if (lm_drv.is_ext_curr_lim_en == false) {
        buf = (uint8_t)(lm_drv.i_calib_ma / LM51772_CURRENT_LIM_STEP_MA);

        if (write(LM51772_REG_ILIM_THRESHOLD, &buf) == osOK) {
            status = osOK;
        }
    }
    else {
        float dac_vout =
            LM51772_EXT_ILIM_RATIO_COEFF * (float)lm_drv.i_calib_ma / 1000.0F;
        dac_write(dac_vout);
        status = osOK;
    }

    if (status == osOK) {
        lm_drv.i_target_ma = current_ma;
    }
    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return status;
}

uint32_t regulator_drv_ilim_get(void)
{
    uint32_t val = 0;
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        val = lm_drv.i_target_ma;
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return val;
}

uint32_t regulator_drv_v_max_get(void)
{
    uint32_t val = 0;
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        if (lm_drv.is_hiv_mode) {
            val = LM51772_HI_V_MAX_VOLTAGE_MV;
        }
        else {
            val = LM51772_LO_V_MAX_VOLTAGE_MV;
        }
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return val;
}

uint32_t regulator_drv_v_min_get(void)
{
    uint32_t val = 0;
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        if (lm_drv.is_hiv_mode) {
            val = LM51772_HI_V_MIN_VOLTAGE_MV;
        }
        else {
            val = LM51772_LO_V_MIN_VOLTAGE_MV;
        }
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return val;
}

uint32_t regulator_drv_i_max_get(void)
{
    return LM51772_CURRENT_MAX_MA;
}

uint32_t regulator_drv_i_min_get(void)
{
    return LM51772_CURRENT_MIN_MA;
}

osStatus_t regulator_drv_mode_poll(void)
{
    osStatus_t read_status = osError;
    uint8_t    val;

    if (lm_drv.is_init == false) {
        return osErrorResource;
    }

    read_status = read(LM51772_REG_STATUS_BYTE, &val);
    if (read_status == osOK) {
        RegulatorDrvOutMode new_mode;

        // Check is VOUT unit in off state
        if (val & LM51772_STATUS_BYTE_OFF_Msk) {
            // Device NOT providing VOUT
            new_mode = lm_drv.is_drv1_en ? REGULATOR_DRV_OUT_MODE_CC :
                                           REGULATOR_DRV_OUT_MODE_OFF;
        }
        else {
            // Device providing VOUT
            new_mode = lm_drv.is_drv1_en ? REGULATOR_DRV_OUT_MODE_CV :
                                           REGULATOR_DRV_OUT_MODE_OFF;
        }

        if (new_mode != lm_drv.mode) {
            // Acquire driver mutex
            if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
                lm_drv.mode = new_mode;
            }
            // Release driver mutex
            osMutexRelease(lm_drv.mutex);
            // Callback
            lm_drv.callbacks.on_mode_change();
        }
    }

    return read_status;
}

osStatus_t regulator_drv_ext_cur_lim_set(bool is_enabled)
{
    uint8_t    buf;
    uint8_t    mask   = LM51772_MFR_SPECIFIC_D9_SEL_ISET_PIN_Msk;
    osStatus_t status = osError;

    if (is_enabled == true) {
        // Current control limiter by ext 16 pin -> ISET.
        buf = LM51772_MFR_SPECIFIC_D9_SEL_ISET_PIN;
    }
    else {
        // Current control limit by I2C (internal DAC).
        buf = (uint8_t)~LM51772_MFR_SPECIFIC_D9_SEL_ISET_PIN;
    }

    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) != osOK) {
        return osError;
    }

    if (modify(LM51772_REG_MFR_SPECIFIC_D9, mask, buf) == osOK) {
        status                    = osOK;
        lm_drv.is_ext_curr_lim_en = is_enabled;
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return status;
}

RegulatorDrvOutMode regulator_drv_mode_get(void)
{
    RegulatorDrvOutMode val = REGULATOR_DRV_OUT_MODE_OFF;
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        val = lm_drv.mode;
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return val;
}

bool regulator_drv_calib_is_write_prot_get(void)
{
    bool val = true;
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        val = lm_drv.calib_const_is_write_prot;
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return val;
}

osStatus_t regulator_drv_calib_is_write_prot_set(bool state)
{
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        lm_drv.calib_const_is_write_prot = state;
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return osOK;
}

osStatus_t regulator_drv_calib_v_slope_set(float val)
{
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        if (lm_drv.calib_const_is_write_prot == false) {
            lm_drv.calib_const.val.v_slope = val;
        }
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return osOK;
}

float regulator_drv_calib_v_slope_get(void)
{
    float val = 0;
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        val = lm_drv.calib_const.val.v_slope;
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return val;
}

osStatus_t regulator_drv_calib_v_offset_set(float val)
{
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        if (lm_drv.calib_const_is_write_prot == false) {
            lm_drv.calib_const.val.v_offset = val;
        }
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return osOK;
}

float regulator_drv_calib_v_offset_get(void)
{
    float val = 0;
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        val = lm_drv.calib_const.val.v_offset;
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return val;
}

osStatus_t regulator_drv_calib_i_slope_set(float val)
{
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        if (lm_drv.calib_const_is_write_prot == false) {
            lm_drv.calib_const.val.i_slope = val;
        }
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return osOK;
}

float regulator_drv_calib_i_slope_get(void)
{
    float val = 0;
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        val = lm_drv.calib_const.val.i_slope;
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return val;
}

osStatus_t regulator_drv_calib_i_offset_set(float val)
{
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        if (lm_drv.calib_const_is_write_prot == false) {
            lm_drv.calib_const.val.i_offset = val;
        }
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return osOK;
}

float regulator_drv_calib_i_offset_get(void)
{
    float val = 0;
    // Acquire driver mutex
    if (osMutexAcquire(lm_drv.mutex, osWaitForever) == osOK) {
        val = lm_drv.calib_const.val.i_offset;
    }

    // Release driver mutex
    osMutexRelease(lm_drv.mutex);
    return val;
}

osStatus_t regulator_drv_calib_store(void)
{
    if (lm_drv.calib_const_is_write_prot == false) {
        return calib_const_store(&lm_drv.calib_const);
    }

    return osError;
}

static osStatus_t read(uint8_t reg, uint8_t* val)
{
    osStatus_t status = osOK;

    // I2C bus mutex acquire
    if (osMutexAcquire(i2c_bus1.mutex, osWaitForever) != osOK) {
        return osError;
    }

    // Process reading
    if (lm51772_read(i2c_bus1.hi2c, reg, val) == false) {
        status = osError;
    }
    else if (osSemaphoreAcquire(
                 i2c_bus1.ready_sem, REGULATOR_RXTX_TIMEOUT_MS
             ) != osOK) {
        // Wait for semaphore - reading done
        status = osError;
    }

    // I2C bus mutex release
    osMutexRelease(i2c_bus1.mutex);
    return status;
}

static osStatus_t write(uint8_t reg, uint8_t* val)
{
    osStatus_t status = osOK;

    // I2C bus mutex acquire
    if (osMutexAcquire(i2c_bus1.mutex, osWaitForever) != osOK) {
        return osError;
    }

    // Process writing
    if (lm51772_write(i2c_bus1.hi2c, reg, val) == false) {
        status = osError;
    }
    else if (osSemaphoreAcquire(
                 i2c_bus1.ready_sem, REGULATOR_RXTX_TIMEOUT_MS
             ) != osOK) {
        // Wait for semaphore - writing done
        status = osError;
    }

    // I2C bus mutex release
    osMutexRelease(i2c_bus1.mutex);
    return status;
}

static osStatus_t modify(uint8_t reg, uint8_t mask, uint8_t val)
{
    uint8_t current;
    if (read(reg, &current) != osOK) {
        return osError;
    }

    current &= ~mask;
    current |= (val & mask);

    return write(reg, &current);
}

static uint32_t constrain(uint32_t val, uint32_t val_min, uint32_t val_max)
{
    if (val > val_max) {
        return val_max;
    }

    if (val < val_min) {
        return val_min;
    }

    return val;
}

// Clamp a post-calibration float into [val_min, val_max] before it is cast to
// uint32_t. Calibration slope/offset come from user-supplied SCPI values and
// are not range-checked there, so NaN/Inf/negative/overflowing results must
// be caught here, ahead of the raw register write.
static uint32_t constrain_f(float val, uint32_t val_min, uint32_t val_max)
{
    if (!isfinite(val) || val <= (float)val_min) {
        return val_min;
    }

    if (val >= (float)val_max) {
        return val_max;
    }

    return (uint32_t)val;
}
