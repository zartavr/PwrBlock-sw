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

#include "i2c_bus.h"

#include "i2c_bus_def.h"

#include <stm32g0xx_hal.h>

/// Timeout of a transaction in milliseconds, including a clock stretched by a
/// slave. The commands are served from the USB device task, so it also bounds
/// how long an unresponsive slave can delay the answer to any other command.
#define I2C_BUS_TIMEOUT_MS 100

/**
 * TIMINGR value of the peripheral, the clock of the bus is fixed at 100 kHz.
 * I2C2 is clocked from PCLK1 at 64 MHz, so a prescaled tick is
 * t_PRESC = (PRESC + 1) / 64 MHz and the period of the clock is
 * (SCLL + 1 + SCLH + 1) * t_PRESC plus the rise and fall times.
 *
 * The value is the one CubeMX generated for MX_I2C2_Init(): PRESC = 1,
 * SCLDEL = 0xB, SDADEL = 1, SCLH = 125, SCLL = 181, that is 126 * 31.25 ns high
 * and 182 * 31.25 ns low.
 */
#define I2C_BUS_TIMING_100_KHZ 0x10B17DB5

/// Current configuration of the bus
typedef struct
{
    bool            enabled;
    I2cBusAddrWidth width;
} I2cBusCfg;

extern I2C_HandleTypeDef hi2c2;

static I2cBusCfg cfg;

/**
 * @brief Get max address of the slave allowed by the current width.
 *
 * @return uint32_t Unshifted address of the slave.
 */
static uint32_t i2c_bus_addr_max(void)
{
    if (cfg.width == I2C_BUS_WIDTH_10BIT) {
        return I2C_BUS_ADDR_MAX_10BIT;
    }

    return I2C_BUS_ADDR_MAX_7BIT;
}

/**
 * @brief Get the address of a slave in the form expected by the HAL.
 *
 * A 7 bit address sits in SADD[7:1] of the peripheral, so it is shifted, a
 * 10 bit one fills SADD[9:0] and is passed as it is.
 *
 * @param addr Unshifted address of the slave.
 * @return uint16_t Address of the slave.
 */
static uint16_t i2c_bus_dev_addr(uint32_t addr)
{
    if (cfg.width == I2C_BUS_WIDTH_10BIT) {
        return (uint16_t)addr;
    }

    return (uint16_t)(addr << 1);
}

/**
 * @brief Convert a result of the HAL into a status of the bus.
 *
 * @param status Result of the call of the HAL.
 * @return I2cBusStatus Status of the bus.
 */
static I2cBusStatus i2c_bus_status(HAL_StatusTypeDef status)
{
    if (status == HAL_OK) {
        return I2C_BUS_OK;
    }

    const uint32_t error = HAL_I2C_GetError(&hi2c2);

    if ((error & HAL_I2C_ERROR_AF) != 0) {
        return I2C_BUS_ERR_NACK;
    }

    if ((status == HAL_TIMEOUT) || ((error & HAL_I2C_ERROR_TIMEOUT) != 0)) {
        return I2C_BUS_ERR_TIMEOUT;
    }

    return I2C_BUS_ERR_BUS;
}

/**
 * @brief Apply the stored configuration to the peripheral.
 *
 * @return I2cBusStatus I2C_BUS_ERR_BUS if the hardware rejected the setup.
 */
static I2cBusStatus i2c_bus_apply(void)
{
    HAL_I2C_DeInit(&hi2c2);

    hi2c2.Instance              = I2C2;
    hi2c2.Init.Timing           = I2C_BUS_TIMING_100_KHZ;
    hi2c2.Init.OwnAddress1      = 0;
    hi2c2.Init.AddressingMode   = (cfg.width == I2C_BUS_WIDTH_10BIT) ?
                                      I2C_ADDRESSINGMODE_10BIT :
                                      I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2      = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        return I2C_BUS_ERR_BUS;
    }

    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) !=
        HAL_OK) {
        return I2C_BUS_ERR_BUS;
    }

    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK) {
        return I2C_BUS_ERR_BUS;
    }

    return I2C_BUS_OK;
}

/**
 * @brief Re-apply the stored configuration if the bus is enabled.
 *
 * @return I2cBusStatus Result of the setup.
 */
static I2cBusStatus i2c_bus_refresh(void)
{
    if (!cfg.enabled) {
        return I2C_BUS_OK;
    }

    return i2c_bus_apply();
}

void i2c_bus_init(void)
{
    i2c_bus_reset();
}

void i2c_bus_reset(void)
{
    cfg.enabled    = false;
    cfg.width      = I2C_BUS_WIDTH_7BIT;

    // The peripheral is brought up by the setup of the board, so a disabled
    // bus has to be released rather than just left alone
    HAL_I2C_DeInit(&hi2c2);
}

I2cBusStatus i2c_bus_state_set(bool enabled)
{
    if (enabled == cfg.enabled) {
        return I2C_BUS_OK;
    }

    if (!enabled) {
        cfg.enabled = false;
        HAL_I2C_DeInit(&hi2c2);
        return I2C_BUS_OK;
    }

    const I2cBusStatus status = i2c_bus_apply();
    if (status != I2C_BUS_OK) {
        HAL_I2C_DeInit(&hi2c2);
        return status;
    }

    cfg.enabled = true;
    return I2C_BUS_OK;
}

bool i2c_bus_state_get(void)
{
    return cfg.enabled;
}

I2cBusStatus i2c_bus_addr_width_set(I2cBusAddrWidth width)
{
    cfg.width = width;

    return i2c_bus_refresh();
}

I2cBusAddrWidth i2c_bus_addr_width_get(void)
{
    return cfg.width;
}

I2cBusStatus i2c_bus_write(uint32_t addr, const uint8_t* data, uint32_t len)
{
    if (!cfg.enabled) {
        return I2C_BUS_ERR_DISABLED;
    }

    if (addr > i2c_bus_addr_max()) {
        return I2C_BUS_ERR_PARAM;
    }

    if (len == 0 || len > I2C_BUS_XFER_MAX_LEN) {
        return I2C_BUS_ERR_PARAM;
    }

    const HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
        &hi2c2,
        i2c_bus_dev_addr(addr),
        (uint8_t*)data,
        (uint16_t)len,
        I2C_BUS_TIMEOUT_MS
    );

    return i2c_bus_status(status);
}

I2cBusStatus i2c_bus_read(uint32_t addr, uint8_t* dst, uint32_t count)
{
    if (!cfg.enabled) {
        return I2C_BUS_ERR_DISABLED;
    }

    if (addr > i2c_bus_addr_max()) {
        return I2C_BUS_ERR_PARAM;
    }

    if (count == 0 || count > I2C_BUS_XFER_MAX_LEN) {
        return I2C_BUS_ERR_PARAM;
    }

    const HAL_StatusTypeDef status = HAL_I2C_Master_Receive(
        &hi2c2, i2c_bus_dev_addr(addr), dst, (uint16_t)count, I2C_BUS_TIMEOUT_MS
    );

    return i2c_bus_status(status);
}
