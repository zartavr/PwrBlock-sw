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

#include <scpi/scpi.h>

/**
 * @brief Restore the power-on defaults of the I2C bus.
 *
 * Wraps i2c_bus_reset() so that callers outside the periph_ctl backend,
 * e.g. *RST, do not depend on it directly.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cReset(scpi_t* context);

/**
 * BUS:I2C:STATe {OFF | ON | 0 | 1}
 * @brief Enable and disable the I2C bus. Default is OFF.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cState(scpi_t* context);

/**
 * BUS:I2C:STATe?
 * @brief This query returns state of the I2C bus.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cStateQ(scpi_t* context);

/**
 * BUS:I2C:ADDRess:WIDTh {7 | 10}
 * @brief Set up width of the slave address in bits. Default is 7.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cAddressWidth(scpi_t* context);

/**
 * BUS:I2C:ADDRess:WIDTh?
 * @brief This query returns width of the slave address in bits.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cAddressWidthQ(scpi_t* context);

/**
 * BUS:I2C:PULLup {OFF | ON | 0 | 1}
 * @brief Enable and disable the internal pull-up resistors of SDA and SCL.
 * Default is OFF.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cPullup(scpi_t* context);

/**
 * BUS:I2C:PULLup?
 * @brief This query returns state of the internal pull-up resistors.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cPullupQ(scpi_t* context);

/**
 * BUS:I2C:TIMEout {<Timeout>}
 * @brief Set up timeout of a transaction in milliseconds, including a clock
 * stretched by the slave. Default is 100.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cTimeout(scpi_t* context);

/**
 * BUS:I2C:TIMEout? [MIN | MAX]
 * @brief This query returns the timeout of a transaction, or the allowed
 * minimum or maximum value.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cTimeoutQ(scpi_t* context);

/**
 * BUS:I2C:WRITe {<Address>},{<Data>}
 * @brief Write data to the slave: START, address + W, data, STOP.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cWrite(scpi_t* context);

/**
 * BUS:I2C:READ? {<Address>},{<Count>}
 * @brief Read Count bytes from the slave: START, address + R, data, STOP.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cReadQ(scpi_t* context);
