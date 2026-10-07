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
 * Wraps i2c_bus_reset() so that callers outside the bsp/drivers/ext_conn
 * backend, e.g. *RST, do not depend on it directly.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cReset(scpi_t* context);

/**
 * BUS:I2C:STATe {OFF | ON | 0 | 1}
 * @brief Enable and disable the I2C bus. Default is OFF. The clock is fixed at
 * 100 kHz.
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
 * @brief Set up width of the slave address in bits. Default is 7. It sets the
 * range of the Address argument of WRITe and READ?.
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
 * BUS:I2C:WRITe {<Address>},{<Data>}
 * @brief Write data to the slave: START, address + W, data, STOP. Address is
 * the unshifted address of the slave, Data is up to 32 bytes in any form
 * accepted by bus_data_param().
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cWrite(scpi_t* context);

/**
 * BUS:I2C:READ? {<Address>},{<Count>}
 * @brief Read Count bytes from the slave: START, address + R, data, STOP.
 * Count is 1 to 32, the reply follows FORMat[:DATA].
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_I2cReadQ(scpi_t* context);
