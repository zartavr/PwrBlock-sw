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

/// Maximum number of slave addresses reported by a scan of the bus
#define I2C_BUS_SCAN_MAX_FOUND 16

/// Maximum length of the write phase of a combined transfer. The blocking HAL
/// carries the phase in the memory address of a read, which is 8 or 16 bits.
#define I2C_BUS_PREFIX_MAX_LEN 2

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
 * or the transaction timeout expires, and its configuration calls do a
 * read-modify-write of the peripheral, so all of them are expected to be
 * called from a single thread, the one running the SCPI parser.
 */
void i2c_bus_init(void);

/**
 * @brief Restore the power-on defaults of the bus, used by *RST.
 *
 * The bus becomes disabled with a clock of 100 kHz, a slave address of 0 of
 * 7 bits, without internal pull-up resistors.
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
 * @brief Set frequency of the clock.
 *
 * Only the discrete values of the internal table are supported, they are the
 * ones with a known good timing setup of the peripheral.
 *
 * @param hz Value in hertz.
 * @return I2cBusStatus I2C_BUS_ERR_PARAM if the value is not supported.
 */
I2cBusStatus i2c_bus_freq_set(uint32_t hz);

/**
 * @brief Get frequency of the clock.
 *
 * @return uint32_t Value in hertz.
 */
uint32_t i2c_bus_freq_get(void);

/**
 * @brief Get min supported frequency of the clock.
 *
 * @return uint32_t Value in hertz.
 */
uint32_t i2c_bus_freq_min_get(void);

/**
 * @brief Get max supported frequency of the clock.
 *
 * @return uint32_t Value in hertz.
 */
uint32_t i2c_bus_freq_max_get(void);

/**
 * @brief Set address of the slave, used by the transfers.
 *
 * @param addr Unshifted address of the slave.
 */
void i2c_bus_addr_set(uint32_t addr);

/**
 * @brief Get address of the slave.
 *
 * @return uint32_t Unshifted address of the slave.
 */
uint32_t i2c_bus_addr_get(void);

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
 * @brief Get max address of the slave allowed by the current width.
 *
 * @return uint32_t Unshifted address of the slave.
 */
uint32_t i2c_bus_addr_max_get(void);

/**
 * @brief Set state of the internal pull-up resistors of SDA and SCL.
 *
 * @param enabled True - the resistors are connected, false - disconnected.
 * @return I2cBusStatus I2C_BUS_OK, the setup cannot fail.
 */
I2cBusStatus i2c_bus_pullup_set(bool enabled);

/**
 * @brief Get state of the internal pull-up resistors of SDA and SCL.
 *
 * @return true The resistors are connected.
 * @return false The resistors are disconnected.
 */
bool i2c_bus_pullup_get(void);

/**
 * @brief Set timeout of a transaction, including a clock stretched by a slave.
 *
 * @param ms Value in milliseconds, clamped to the supported range.
 */
void i2c_bus_timeout_set(uint32_t ms);

/**
 * @brief Get timeout of a transaction.
 *
 * @return uint32_t Value in milliseconds.
 */
uint32_t i2c_bus_timeout_get(void);

/**
 * @brief Get min supported timeout of a transaction.
 *
 * @return uint32_t Value in milliseconds.
 */
uint32_t i2c_bus_timeout_min_get(void);

/**
 * @brief Get max supported timeout of a transaction.
 *
 * @return uint32_t Value in milliseconds.
 */
uint32_t i2c_bus_timeout_max_get(void);

/**
 * @brief Write data to the slave: START, address + W, data, STOP.
 *
 * @param data Bytes to be written.
 * @param len Number of bytes, 1 to I2C_BUS_XFER_MAX_LEN.
 * @return I2cBusStatus Result of the transaction.
 */
I2cBusStatus i2c_bus_write(const uint8_t* data, uint32_t len);

/**
 * @brief Read data from the slave: START, address + R, data, STOP.
 *
 * @param dst Destination of the read bytes.
 * @param count Number of bytes, 1 to I2C_BUS_XFER_MAX_LEN.
 * @return I2cBusStatus Result of the transaction.
 */
I2cBusStatus i2c_bus_read(uint8_t* dst, uint32_t count);

/**
 * @brief Write data to the slave, then read in the same transaction using a
 * repeated START.
 *
 * @param prefix Bytes to be written before the repeated START, the first one
 * is sent first.
 * @param prefix_len Number of bytes, 1 to I2C_BUS_PREFIX_MAX_LEN.
 * @param dst Destination of the read bytes.
 * @param count Number of bytes, 1 to I2C_BUS_XFER_MAX_LEN.
 * @return I2cBusStatus Result of the transaction.
 */
I2cBusStatus i2c_bus_transfer(
    const uint8_t* prefix, uint32_t prefix_len, uint8_t* dst, uint32_t count
);

/**
 * @brief Scan the bus for slaves that acknowledge their address.
 *
 * Supported for a 7 bit address only.
 *
 * @param found Destination of the addresses that answered.
 * @param max Capacity of found.
 * @param count Number of addresses that answered.
 * @return I2cBusStatus I2C_BUS_ERR_PARAM for a 10 bit address.
 */
I2cBusStatus i2c_bus_scan(uint8_t* found, uint32_t max, uint32_t* count);

/**
 * @brief Recover the bus: generate up to nine clock pulses followed by a STOP
 * to release SDA held low by a slave.
 *
 * Allowed while the bus is disabled, it is a recovery tool.
 *
 * @return I2cBusStatus Result of the recovery.
 */
I2cBusStatus i2c_bus_recover(void);
