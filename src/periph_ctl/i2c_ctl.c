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

#include "i2c_ctl.h"

#include "bus_format.h"
#include "i2c_bus.h"

/// Widths of the slave address accepted by BUS:I2C:ADDRess:WIDTh
#define I2C_CTL_WIDTH_7BIT  7
#define I2C_CTL_WIDTH_10BIT 10

// Buffers of the transfers. The commands are served from the USB device task,
// whose stack is too small to carry them, and only that task touches them
static uint8_t tx_buffer[I2C_BUS_XFER_MAX_LEN];
static uint8_t rx_buffer[I2C_BUS_XFER_MAX_LEN];

/**
 * @brief Report a result of the bus through the error queue.
 *
 * @param context
 * @param status Result of the call of the backend.
 * @return scpi_result_t SCPI_RES_ERR if the status is an error.
 */
static scpi_result_t i2c_status_result(scpi_t* context, I2cBusStatus status)
{
    int16_t error = 0;

    switch (status) {
        case I2C_BUS_OK: {
            return SCPI_RES_OK;
        }
        case I2C_BUS_ERR_NACK: {
            error = SCPI_ERROR_HARDWARE_MISSING;
            break;
        }
        case I2C_BUS_ERR_TIMEOUT: {
            error = SCPI_ERROR_TIME_OUT;
            break;
        }
        case I2C_BUS_ERR_DISABLED: {
            error = SCPI_ERROR_SETTINGS_CONFLICT;
            break;
        }
        case I2C_BUS_ERR_PARAM: {
            error = SCPI_ERROR_DATA_OUT_OF_RANGE;
            break;
        }
        case I2C_BUS_ERR_BUS:
        default: {
            error = SCPI_ERROR_HARDWARE_ERROR;
            break;
        }
    }

    SCPI_ErrorPush(context, error);
    return SCPI_RES_ERR;
}

/**
 * @brief Check that the bus is enabled before a transfer.
 *
 * @param context
 * @return scpi_bool_t FALSE if the bus is disabled.
 */
static scpi_bool_t i2c_enabled_check(scpi_t* context)
{
    if (!i2c_bus_state_get()) {
        SCPI_ErrorPush(context, SCPI_ERROR_SETTINGS_CONFLICT);
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Read a <Count> parameter of a read query.
 *
 * @param context
 * @param count Decoded number of bytes.
 * @return scpi_bool_t FALSE if the parameter is missing or out of range.
 */
static scpi_bool_t i2c_count_param(scpi_t* context, uint32_t* count)
{
    uint32_t value = 0;

    // Read first parameter if present
    if (!SCPI_ParamUInt32(context, &value, TRUE)) {
        return FALSE;
    }

    if (value == 0) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return FALSE;
    }

    if (value > I2C_BUS_XFER_MAX_LEN) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return FALSE;
    }

    *count = value;
    return TRUE;
}

/**
 * @brief Answer a query with a value, or with the min or max of it.
 *
 * @param context
 * @param value Current value.
 * @param min Allowed minimum value.
 * @param max Allowed maximum value.
 * @return scpi_result_t
 */
static scpi_result_t i2c_limit_result(
    scpi_t* context, uint32_t value, uint32_t min, uint32_t max
)
{
    uint32_t reply = value;

    // Read first parameter if present: map to scpi_special_numbers_def
    scpi_number_t par;
    if (SCPI_ParamNumber(context, scpi_special_numbers_def, &par, FALSE)) {
        // Select by special number descriptor
        switch (par.content.tag) {
            case SCPI_NUM_MIN: {
                reply = min;
                break;
            }
            case SCPI_NUM_MAX: {
                reply = max;
                break;
            }
            default: {
                SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
                return SCPI_RES_ERR;
            }
        }
    }

    SCPI_ResultUInt32(context, reply);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_I2cReset(scpi_t* context)
{
    (void)context;

    i2c_bus_reset();
    return SCPI_RES_OK;
}

scpi_result_t SCPI_I2cState(scpi_t* context)
{
    bool state = false;

    // Read first parameter if present
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    return i2c_status_result(context, i2c_bus_state_set(state));
}

scpi_result_t SCPI_I2cStateQ(scpi_t* context)
{
    SCPI_ResultBool(context, i2c_bus_state_get());
    return SCPI_RES_OK;
}

scpi_result_t SCPI_I2cFrequency(scpi_t* context)
{
    uint32_t value = 0;

    // Read first parameter if present
    if (!SCPI_ParamUInt32(context, &value, TRUE)) {
        return SCPI_RES_ERR;
    }

    return i2c_status_result(context, i2c_bus_freq_set(value));
}

scpi_result_t SCPI_I2cFrequencyQ(scpi_t* context)
{
    return i2c_limit_result(
        context,
        i2c_bus_freq_get(),
        i2c_bus_freq_min_get(),
        i2c_bus_freq_max_get()
    );
}

scpi_result_t SCPI_I2cAddress(scpi_t* context)
{
    uint32_t value = 0;

    // Read first parameter if present
    if (!SCPI_ParamUInt32(context, &value, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (value > i2c_bus_addr_max_get()) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    i2c_bus_addr_set(value);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_I2cAddressQ(scpi_t* context)
{
    SCPI_ResultUInt32(context, i2c_bus_addr_get());
    return SCPI_RES_OK;
}

scpi_result_t SCPI_I2cAddressWidth(scpi_t* context)
{
    uint32_t value = 0;

    // Read first parameter if present, the widths are numbers rather than
    // mnemonics, so they are not read as a choice
    if (!SCPI_ParamUInt32(context, &value, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (value != I2C_CTL_WIDTH_7BIT && value != I2C_CTL_WIDTH_10BIT) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    const I2cBusAddrWidth width = (value == I2C_CTL_WIDTH_10BIT) ?
                                      I2C_BUS_WIDTH_10BIT :
                                      I2C_BUS_WIDTH_7BIT;

    return i2c_status_result(context, i2c_bus_addr_width_set(width));
}

scpi_result_t SCPI_I2cAddressWidthQ(scpi_t* context)
{
    const uint32_t width = (i2c_bus_addr_width_get() == I2C_BUS_WIDTH_10BIT) ?
                               I2C_CTL_WIDTH_10BIT :
                               I2C_CTL_WIDTH_7BIT;

    SCPI_ResultUInt32(context, width);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_I2cPullup(scpi_t* context)
{
    bool state = false;

    // Read first parameter if present
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    return i2c_status_result(context, i2c_bus_pullup_set(state));
}

scpi_result_t SCPI_I2cPullupQ(scpi_t* context)
{
    SCPI_ResultBool(context, i2c_bus_pullup_get());
    return SCPI_RES_OK;
}

scpi_result_t SCPI_I2cTimeout(scpi_t* context)
{
    uint32_t value = 0;

    // Read first parameter if present
    if (!SCPI_ParamUInt32(context, &value, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (value < i2c_bus_timeout_min_get() ||
        value > i2c_bus_timeout_max_get()) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    i2c_bus_timeout_set(value);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_I2cTimeoutQ(scpi_t* context)
{
    return i2c_limit_result(
        context,
        i2c_bus_timeout_get(),
        i2c_bus_timeout_min_get(),
        i2c_bus_timeout_max_get()
    );
}

scpi_result_t SCPI_I2cWrite(scpi_t* context)
{
    if (!i2c_enabled_check(context)) {
        return SCPI_RES_ERR;
    }

    uint32_t len = 0;
    if (!bus_data_param(context, tx_buffer, I2C_BUS_XFER_MAX_LEN, &len)) {
        return SCPI_RES_ERR;
    }

    return i2c_status_result(context, i2c_bus_write(tx_buffer, len));
}

scpi_result_t SCPI_I2cReadQ(scpi_t* context)
{
    if (!i2c_enabled_check(context)) {
        return SCPI_RES_ERR;
    }

    uint32_t count = 0;
    if (!i2c_count_param(context, &count)) {
        return SCPI_RES_ERR;
    }

    const I2cBusStatus status = i2c_bus_read(rx_buffer, count);
    if (status != I2C_BUS_OK) {
        return i2c_status_result(context, status);
    }

    bus_data_result(context, rx_buffer, count);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_I2cTransferQ(scpi_t* context)
{
    if (!i2c_enabled_check(context)) {
        return SCPI_RES_ERR;
    }

    uint32_t count = 0;
    if (!i2c_count_param(context, &count)) {
        return SCPI_RES_ERR;
    }

    uint32_t prefix_len = 0;
    if (!bus_data_param(
            context, tx_buffer, I2C_BUS_XFER_MAX_LEN, &prefix_len
        )) {
        return SCPI_RES_ERR;
    }

    // The write phase is carried by the memory address of a read of the HAL,
    // so it is shorter than a full transfer. A prefix that does not fit is
    // a value the command cannot accept, not an overflow of the data buffer
    if (prefix_len > I2C_BUS_PREFIX_MAX_LEN) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    const I2cBusStatus status =
        i2c_bus_transfer(tx_buffer, prefix_len, rx_buffer, count);

    if (status != I2C_BUS_OK) {
        return i2c_status_result(context, status);
    }

    bus_data_result(context, rx_buffer, count);
    return SCPI_RES_OK;
}
