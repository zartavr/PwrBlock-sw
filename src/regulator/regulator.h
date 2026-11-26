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

#pragma once

#include <cmsis_os2.h>
#include <stdbool.h>

/**
 * @brief Regulator driver output mode typedef.
 *
 */
typedef enum
{
    REGULATOR_DRV_OUT_MODE_OFF,
    REGULATOR_DRV_OUT_MODE_CC,
    REGULATOR_DRV_OUT_MODE_CV,
} RegulatorDrvOutMode;

/**
 * @brief Regulator callback prototype typedef.
 *
 */
typedef void (*RegulatorCb)(void);

/**
 * @brief Callback function container.
 *
 */
typedef struct
{
    RegulatorCb on_mode_change;
} RegulatorCallbacks;

/**
 * @brief Init regulator driver internals.
 *
 * @param callbacks Pointer to container of callback functions.
 * @return osStatus_t osOk - init done, osError - init fail.
 */
osStatus_t regulator_drv_init(RegulatorCallbacks* callbacks);

/**
 * @brief Reset internal states of regulator.
 *
 * @return osStatus_t osOk - reset done, osError - reset fail.
 */
osStatus_t regulator_drv_reset(void);

/**
 * @brief Base setup of regulator on power on.
 *
 * @param voltage_mv Initial Vout target in millivolts, applied before the
 * power stage is enabled so the converter soft-starts straight to it instead
 * of DVS-ramping to it afterwards.
 * @param current_ma Initial Ilim target in milliamperes.
 * @return osStatus_t osOk - setup done, osError - setup fail.
 */
osStatus_t regulator_drv_setup(uint32_t voltage_mv, uint32_t current_ma);

/**
 * @brief Set state of main output.
 *
 * @param is_enabled True - output is enabled, false - output is disabled.
 * @return osStatus_t osOk - set state done, osError - set state fail.
 */
osStatus_t regulator_drv_output_set(bool is_enabled);

/**
 * @brief Get state of main output.
 *
 * @return true Output is enabled.
 * @return false Output is disabled.
 */
bool regulator_drv_output_get(void);

/**
 * @brief Set value of Vout.
 *
 * @param voltage_mv Value of voltage in millivolts.
 * @return osStatus_t osOk - set state done, osError - set state fail.
 */
osStatus_t regulator_drv_vout_set(uint32_t voltage_mv);

/**
 * @brief Get value of Vout.
 *
 * @return uint32_t Voltage in millivolts.
 */
uint32_t regulator_drv_vout_get(void);

/**
 * @brief Set value of Ilim.
 *
 * @param current_ma Value of current in millamperes.
 * @return osStatus_t osOk - set state done, osError - set state fail.
 */
osStatus_t regulator_drv_ilim_set(uint32_t current_ma);

/**
 * @brief Get value of Ilim.
 *
 * @return uint32_t Current in milliamperes.
 */
uint32_t regulator_drv_ilim_get(void);

/**
 * @brief Get max value of supported voltage to setup.
 *
 * @return uint32_t Voltage in millivolts.
 */
uint32_t regulator_drv_v_max_get(void);

/**
 * @brief Get min value of supported voltage to setup.
 *
 * @return uint32_t Voltage in millivolts.
 */
uint32_t regulator_drv_v_min_get(void);

/**
 * @brief Get max value of supported current to setup.
 *
 * @return uint32_t Current in milliamperes.
 */
uint32_t regulator_drv_i_max_get(void);

/**
 * @brief Get min value of supported current to setup.
 *
 * @return uint32_t Current in milliamperes.
 */
uint32_t regulator_drv_i_min_get(void);

/**
 * @brief Request for status from DC-DC driver and polling internal mode value
 * of output.
 *
 * @return osStatus_t osStatus_t osOk - polling done, osError - polling fail.
 */
osStatus_t regulator_drv_mode_poll(void);

/**
 * @brief Get value of driver output mode.
 *
 * @return RegulatorDrvOutMode Mode value.
 */
RegulatorDrvOutMode regulator_drv_mode_get(void);

/**
 * @brief Set external current limit mode.
 *
 * @param is_enabled True - external mode enabled (control via DAC), False -
 * external mode disabled (control via LM registers val).
 * @return osStatus_t osOk - set state done, osError - set state fail.
 */
osStatus_t regulator_drv_ext_cur_lim_set(bool is_enabled);

/**
 * @brief Set PCM window low threshold.
 *
 * @param val New value of PCM window.
 * @return osStatus_t osOk - setting done, osError - setting fail.
 */
osStatus_t regulator_drv_pcm_set(uint8_t val);

/**
 * @brief Sets the positive and negative Vo slew rate for DVS.
 *
 * @param val New value of SRAMP.
 * @return osStatus_t osOk - setting done, osError - setting fail.
 */
osStatus_t regulator_drv_sramp_set(uint8_t val);

/**
 * @brief Enables the cable drop compensation.
 *
 * @param is_enabled True - enable CDC, False - disable.
 * @return osStatus_t osOk - setting done, osError - setting fail.
 */
osStatus_t regulator_drv_cdc_en_set(bool is_enabled);

/**
 * @brief Selects the Gain for the CDC voltage (1V) with respect to Vout.
 *
 * @param val New value of CDC_GAIN.
 * @return osStatus_t osOk - setting done, osError - setting fail.
 */
osStatus_t regulator_drv_cdc_gain_set(uint8_t val);

/**
 * @brief Clear faults command.
 *
 * @return osStatus_t osOk - command done, osError - command fail.
 */
osStatus_t regulator_drv_clear_faults(void);

/**
 * @brief Get latched status byte, read on poll state
 *
 */
uint8_t regulator_drv_status_get(void);

/**
 * @brief Set power stage.
 *
 * @param is_enabled True - conversion (power stage) enabled, False - conversion
 * disabled.
 * @return osStatus_t osOk - setting done, osError - setting fail.
 */
osStatus_t regulator_drv_conv_en_set(bool is_enabled);

/**
 * @brief Getter for calibration mode state.
 *
 * @return true Calibration mode ON.
 * @return false Calibration mode OFF.
 */
bool regulator_drv_calib_is_write_prot_get(void);

/**
 * @brief Setter for calibration mode state.
 *
 * @param state true - calibration mode ON, false - calibration mode OFF.
 * @return osStatus_t osOk - setting done, osError - setting fail.
 */
osStatus_t regulator_drv_calib_is_write_prot_set(bool state);

/**
 * @brief Setter for voltage slope.
 *
 * @param val Value of calibration const.
 * @return osStatus_t osOk - setting done, osError - setting fail.
 */
osStatus_t regulator_drv_calib_v_slope_set(float val);

/**
 * @brief Getter for voltage slope.
 *
 * @return float Value of calibration const.
 */
float regulator_drv_calib_v_slope_get(void);

/**
 * @brief Setter for voltage offset.
 *
 * @param val Value of calibration const.
 * @return osStatus_t osOk - setting done, osError - setting fail.
 */
osStatus_t regulator_drv_calib_v_offset_set(float val);

/**
 * @brief Getter for voltage offset.
 *
 * @return float Value of calibration const.
 */
float regulator_drv_calib_v_offset_get(void);

/**
 * @brief Setter for current slope.
 *
 * @param val Value of calibration const.
 * @return osStatus_t osOk - setting done, osError - setting fail.
 */
osStatus_t regulator_drv_calib_i_slope_set(float val);

/**
 * @brief Getter for current slope.
 *
 * @return float Value of calibration const.
 */
float regulator_drv_calib_i_slope_get(void);

/**
 * @brief Setter for current offset.
 *
 * @param val Value of calibration const.
 * @return osStatus_t osOk - setting done, osError - setting fail.
 */
osStatus_t regulator_drv_calib_i_offset_set(float val);

/**
 * @brief Getter for current offset.
 *
 * @return float Value of calibration const.
 */
float regulator_drv_calib_i_offset_get(void);

/**
 * @brief Store all calibration consts in memory.
 *
 * @return osStatus_t osOk - setting done, osError - setting fail.
 */
osStatus_t regulator_drv_calib_store(void);
