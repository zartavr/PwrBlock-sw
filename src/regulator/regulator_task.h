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

#include <stdbool.h>
#include <stdint.h>

void regulator_app_task(void* argument);

/**
 * @brief Init and base setup DC-DC driver.
 *
 */
void regulator_init(void);

/**
 * @brief Deinit DC-DC driver.
 *
 */
void regulator_deinit(void);

/**
 * @brief Set voltage limit for DC-DC driver.
 *
 * @param val New value in millivolts.
 */
void regulator_v_set(uint32_t val_mv);

/**
 * @brief Get target output voltage.
 *
 * @return uint32_t Value in millivolts.
 */
uint32_t regulator_v_get(void);

/**
 * @brief Get max target output voltage.
 *
 * @return uint32_t Value in millivolts.
 */
uint32_t regulator_v_max_get(void);

/**
 * @brief Get min target output voltage.
 *
 * @return uint32_t Value in millivolts.
 */
uint32_t regulator_v_min_get(void);

/**
 * @brief Read lathced status byte, read on poll state
 *
 */
uint8_t regulator_status_get(void);

/**
 * @brief Clear faults in status byte, return latched value
 *
 */
uint8_t regulator_status_clear(void);

/**
 * @brief Set current limit for DC-DC driver.
 *
 * @param val New value in milliamperes.
 */
void regulator_i_set(uint32_t val_ma);

/**
 * @brief Get target output current.
 *
 * @return uint32_t Value in milliamperes.
 */
uint32_t regulator_i_get(void);

/**
 * @brief Get max target output current.
 *
 * @return uint32_t Value in milliamperes.
 */
uint32_t regulator_i_max_get(void);

/**
 * @brief Get min target output current.
 *
 * @return uint32_t Value in milliamperes.
 */
uint32_t regulator_i_min_get(void);

/**
 * @brief Set DC-DC driver out.
 *
 * @param val True - out is enabled, false - out is disabled.
 */
void regulator_output_set(bool val);

/**
 * @brief Get DC-DC driver out state.
 *
 * @return true Out is enabled.
 * @return false Out is disabled.
 */
bool regulator_output_get(void);

/**
 * @brief Toggle main output of DC-DC driver.
 *
 */
void regulator_output_toggle(void);

/**
 * @brief Recall preset for regulator.
 *
 * @param preset_id Preset ID.
 */
void regulator_preset_recall(uint8_t preset_id);

/**
 * @brief Store preset of regulator to memory.
 *
 * @param preset_id Preset ID.
 */
void regulator_preset_store(uint8_t preset_id);

/**
 * @brief Getter write protection state for calibration coefficients.
 *
 * @return true Calibration mode ON.
 * @return false Calibration mode OFF.
 */
bool regulator_calib_wp_get(void);

/**
 * @brief Setter write protection mode for calibration coefficients.
 *
 * @param state true - calibration mode ON, false - calibration mode OFF.
 */
void regulator_calib_wp_set(bool state);

/**
 * @brief Setter for voltage slope.
 *
 * @param val Value of calibration const.
 */
void regulator_calib_v_slope_set(float val);

/**
 * @brief Getter for voltage slope.
 *
 * @return float Value of calibration const.
 */
float regulator_calib_v_slope_get(void);

/**
 * @brief Setter for voltage offset.
 *
 * @param val Value of calibration const.
 */
void regulator_calib_v_offset_set(float val);

/**
 * @brief Getter for voltage offset.
 *
 * @return float Value of calibration const.
 */
float regulator_calib_v_offset_get(void);

/**
 * @brief Setter for current slope.
 *
 * @param val Value of calibration const.
 */
void regulator_calib_i_slope_set(float val);

/**
 * @brief Getter for current slope.
 *
 * @return float Value of calibration const.
 */
float regulator_calib_i_slope_get(void);

/**
 * @brief Setter for current offset.
 *
 * @param val Value of calibration const.
 */
void regulator_calib_i_offset_set(float val);

/**
 * @brief Getter for current offset.
 *
 * @return float Value of calibration const.
 */
float regulator_calib_i_offset_get(void);

/**
 * @brief Store all calibration consts in memory.
 *
 */
void regulator_calib_store(void);
