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

/// Maximum number of data bytes of a single transfer. The reply of a read is
/// built in the output buffer of the SCPI parser, and a byte costs up to five
/// characters of it in the hexadecimal response format.
#define I2C_BUS_XFER_MAX_LEN 32

/// Result of an operation of the bus
typedef enum
{
    I2C_BUS_OK = 0,
    I2C_BUS_ERR_NACK,      // No acknowledge from the slave
    I2C_BUS_ERR_TIMEOUT,   // Transaction timeout, or a stretched clock
    I2C_BUS_ERR_BUS,       // Arbitration lost, bus error or overrun
    I2C_BUS_ERR_DISABLED,  // The bus is disabled
    I2C_BUS_ERR_PARAM,     // Value is not supported by the hardware
} I2cBusStatus;

/// Width of the slave address
typedef enum
{
    I2C_BUS_WIDTH_7BIT = 0,
    I2C_BUS_WIDTH_10BIT,
} I2cBusAddrWidth;

/**
 * @brief Init the I2C bus of the external connector.
 *
 * Applies the power-on defaults, which leave the bus disabled. Must be called
 * before the SCPI parser starts to serve BUS:I2C commands.
 *
 * The transfers of this module block the calling thread until they complete
 * or a fixed transaction timeout of 100 ms expires, and its configuration
 * calls do a read-modify-write of the peripheral, so all of them are expected
 * to be called from a single thread, the one running the SCPI parser.
 */
void i2c_bus_init(void);

/**
 * @brief Restore the power-on defaults of the bus, used by *RST.
 *
 * The bus becomes disabled with an address width of 7 bits. The clock of the
 * bus is fixed at 100 kHz.
 */
void i2c_bus_reset(void);

/**
 * @brief Set state of the bus.
 *
 * @param enabled True - the bus is enabled, false - the bus is disabled.
 * @return I2cBusStatus I2C_BUS_ERR_BUS if the hardware rejected the setup.
 */
I2cBusStatus i2c_bus_state_set(bool enabled);

/**
 * @brief Get state of the bus.
 *
 * @return true The bus is enabled.
 * @return false The bus is disabled.
 */
bool i2c_bus_state_get(void);

/**
 * @brief Set width of the slave address.
 *
 * @param width Width to apply.
 * @return I2cBusStatus I2C_BUS_ERR_BUS if the hardware rejected the setup.
 */
I2cBusStatus i2c_bus_addr_width_set(I2cBusAddrWidth width);

/**
 * @brief Get width of the slave address.
 *
 * @return I2cBusAddrWidth Current width.
 */
I2cBusAddrWidth i2c_bus_addr_width_get(void);

/**
 * @brief Write data to a slave: START, address + W, data, STOP.
 *
 * @param addr Unshifted address of the slave, within the current width.
 * @param data Bytes to be written.
 * @param len Number of bytes, 1 to I2C_BUS_XFER_MAX_LEN.
 * @return I2cBusStatus Result of the transaction, I2C_BUS_ERR_PARAM if the
 * address or the length is out of range.
 */
I2cBusStatus i2c_bus_write(uint32_t addr, const uint8_t* data, uint32_t len);

/**
 * @brief Read data from a slave: START, address + R, data, STOP.
 *
 * @param addr Unshifted address of the slave, within the current width.
 * @param dst Destination of the read bytes.
 * @param count Number of bytes, 1 to I2C_BUS_XFER_MAX_LEN.
 * @return I2cBusStatus Result of the transaction, I2C_BUS_ERR_PARAM if the
 * address or the count is out of range.
 */
I2cBusStatus i2c_bus_read(uint32_t addr, uint8_t* dst, uint32_t count);
