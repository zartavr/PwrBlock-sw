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

#include "lm51772_def.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Read interface for LM51772 driver.
 *
 * @param bus_handler Pointer to I2C bus handler.
 * @param reg_addr Address of register to read.
 * @param rx_buff Pointer to store resulting value.
 * @return true Reading successfully done.
 * @return false Reading failed.
 */
bool lm51772_read(void* bus_handler, uint8_t reg_addr, uint8_t* rx_buff);

/**
 * @brief Write interface for LM51772 driver.
 *
 * @param bus_handler Pointer to I2C bus handler.
 * @param reg_addr Address of register to write.
 * @param tx_buff Pointer to value to be send.
 * @return true Writing successfully done.
 * @return false Writin failed.
 */
bool lm51772_write(void* bus_handler, uint8_t reg_addr, uint8_t* tx_buff);

/**
 * @brief Enabling LM51772 driver
 *
 */
void lm51772_on(void);

/**
 * @brief Disabling LM51772 driver
 *
 */
void lm51772_off(void);

/**
 * @brief Set OUT_EN_MCU signal
 *
 * @param enabled - if true and no alert reported by INA_ALERT - VOUT is powered
 */
void lm51772_output_set(bool enabled);

/**
 * @brief Switch LM51772 ILIM shunts
 *
 * @param high - true: 200 mOm shunt is used (300 mA max)
 *             - false: 20 mOm shunt is used (3 A max)
 */
void lm51772_sns_resolution_set(bool high);
