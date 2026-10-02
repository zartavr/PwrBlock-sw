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

#include "uart_ctl.h"

#include "bsp/drivers/ext_conn/uart_bus.h"
#include "bsp/drivers/ext_conn/uart_bus_def.h"
#include "scpi/bus_format.h"

#include <ctype.h>
#include <string.h>

/// Number of characters of a frame, e.g. "8N1"
#define FRAME_TEXT_LEN 3

// Buffers of the transfers. The commands are served from the USB device task,
// whose stack is too small to carry them, and only that task touches them
static uint8_t tx_buffer[UART_BUS_XFER_MAX_LEN];
static uint8_t rx_buffer[UART_BUS_XFER_MAX_LEN];

/**
 * @brief Report a result of the bus through the error queue.
 *
 * @param context
 * @param status Result of the call of the backend.
 * @return scpi_result_t SCPI_RES_ERR if the status is an error.
 */
static scpi_result_t uart_status_result(scpi_t* context, UartBusStatus status)
{
    int16_t error = 0;

    switch (status) {
        case UART_BUS_OK: {
            return SCPI_RES_OK;
        }
        case UART_BUS_ERR_TIMEOUT: {
            error = SCPI_ERROR_TIME_OUT;
            break;
        }
        case UART_BUS_ERR_DISABLED:
        case UART_BUS_ERR_BUSY: {
            error = SCPI_ERROR_SETTINGS_CONFLICT;
            break;
        }
        case UART_BUS_ERR_PARAM: {
            error = SCPI_ERROR_DATA_OUT_OF_RANGE;
            break;
        }
        case UART_BUS_ERR_BUS:
        default: {
            error = SCPI_ERROR_HARDWARE_ERROR;
            break;
        }
    }

    SCPI_ErrorPush(context, error);
    return SCPI_RES_ERR;
}

/**
 * @brief Report the receive errors latched since the previous report.
 *
 * The data received along with the errors is still returned, the errors only
 * tell that some of it may be wrong or missing.
 *
 * @param context
 */
static void uart_rx_errors_report(scpi_t* context)
{
    const uint32_t errors = uart_bus_rx_errors_take();

    if ((errors & UART_BUS_RX_ERR_OVERRUN) != 0) {
        SCPI_ErrorPush(context, SCPI_ERROR_INPUT_BUFFER_OVERRUN);
    }

    if ((errors & UART_BUS_RX_ERR_FRAMING) != 0) {
        SCPI_ErrorPush(context, SCPI_ERROR_FRAMING_ERROR_IN_CMD_MSG);
    }

    if ((errors & UART_BUS_RX_ERR_PARITY) != 0) {
        SCPI_ErrorPush(context, SCPI_ERROR_PARITY_ERROR_IN_CMD_MSG);
    }

    if ((errors & UART_BUS_RX_ERR_NOISE) != 0) {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMUNICATION_ERROR);
    }
}

/**
 * @brief Check that the bus is enabled before a transfer.
 *
 * @param context
 * @return scpi_bool_t FALSE if the bus is disabled.
 */
static scpi_bool_t uart_enabled_check(scpi_t* context)
{
    if (!uart_bus_state_get()) {
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
 * @param mandatory TRUE if the parameter has to be present.
 * @return scpi_bool_t FALSE if the parameter is missing or out of range.
 */
static scpi_bool_t uart_count_param(
    scpi_t* context, uint32_t* count, scpi_bool_t mandatory
)
{
    uint32_t value = 0;

    if (!SCPI_ParamUInt32(context, &value, mandatory)) {
        return FALSE;
    }

    if (value == 0) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return FALSE;
    }

    if (value > UART_BUS_XFER_MAX_LEN) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return FALSE;
    }

    *count = value;
    return TRUE;
}

/**
 * @brief Answer a read query with the received bytes.
 *
 * @param context
 * @param count Number of received bytes in rx_buffer.
 */
static void uart_rx_result(scpi_t* context, uint32_t count)
{
    uart_rx_errors_report(context);

    // An empty reply would leave a query without an answer
    if (count == 0) {
        SCPI_ResultMnemonic(context, "NONE");
        return;
    }

    bus_data_result(context, rx_buffer, count);
}

/**
 * @brief Decode a frame from its text form, e.g. "8N1".
 *
 * @param text Text of the frame, FRAME_TEXT_LEN characters, any case.
 * @param frame Decoded frame.
 * @return scpi_bool_t FALSE if the text is not a valid frame.
 */
static scpi_bool_t uart_frame_parse(const char* text, UartBusFrame* frame)
{
    const char data   = text[0];
    const char parity = (char)toupper((unsigned char)text[1]);
    const char stop   = text[2];

    if ((data != '7' && data != '8') || (stop != '1' && stop != '2')) {
        return FALSE;
    }

    switch (parity) {
        case 'N': {
            frame->parity = UART_BUS_PARITY_NONE;
            break;
        }
        case 'E': {
            frame->parity = UART_BUS_PARITY_EVEN;
            break;
        }
        case 'O': {
            frame->parity = UART_BUS_PARITY_ODD;
            break;
        }
        default: {
            return FALSE;
        }
    }

    frame->data_bits = (uint8_t)(data - '0');
    frame->stop_bits = (uint8_t)(stop - '0');
    return TRUE;
}

scpi_result_t SCPI_UartReset(scpi_t* context)
{
    (void)context;

    uart_bus_reset();
    return SCPI_RES_OK;
}

scpi_result_t SCPI_UartState(scpi_t* context)
{
    bool state = false;

    // Read first parameter if present
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    return uart_status_result(context, uart_bus_state_set(state));
}

scpi_result_t SCPI_UartStateQ(scpi_t* context)
{
    SCPI_ResultBool(context, uart_bus_state_get());
    return SCPI_RES_OK;
}

scpi_result_t SCPI_UartBaud(scpi_t* context)
{
    uint32_t baud = 0;

    // Read first parameter if present
    if (!SCPI_ParamUInt32(context, &baud, TRUE)) {
        return SCPI_RES_ERR;
    }

    return uart_status_result(context, uart_bus_baud_set(baud));
}

scpi_result_t SCPI_UartBaudQ(scpi_t* context)
{
    uint32_t baud = 0;

    // Read first parameter if present: map to scpi_special_numbers_def
    scpi_number_t par;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &par, FALSE)) {
        // If no parameter, than reply the current baud rate
        baud = uart_bus_baud_get();
    }
    else {
        // Select by special number descriptor
        switch (par.content.tag) {
            case SCPI_NUM_MIN: {
                baud = UART_BUS_BAUD_MIN;
                break;
            }
            case SCPI_NUM_MAX: {
                baud = UART_BUS_BAUD_MAX;
                break;
            }
            default: {
                SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
                return SCPI_RES_ERR;
            }
        }
    }

    SCPI_ResultUInt32(context, baud);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_UartFrame(scpi_t* context)
{
    // Room for one more character than a frame, so a longer text is told
    // apart from a truncated one
    char   text[FRAME_TEXT_LEN + 2] = {0};
    size_t len                      = 0;

    // Read first parameter if present
    if (!SCPI_ParamCopyText(context, text, sizeof(text), &len, TRUE)) {
        return SCPI_RES_ERR;
    }

    UartBusFrame frame;
    if (len != FRAME_TEXT_LEN || !uart_frame_parse(text, &frame)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    return uart_status_result(context, uart_bus_frame_set(frame));
}

scpi_result_t SCPI_UartFrameQ(scpi_t* context)
{
    static const char PARITY_NAMES[] = {'N', 'E', 'O'};

    const UartBusFrame frame = uart_bus_frame_get();

    const char text[] = {
        (char)('0' + frame.data_bits),
        PARITY_NAMES[frame.parity],
        (char)('0' + frame.stop_bits),
        '\0',
    };

    // Quoted, so the reply can be sent back as the parameter of FRAMe
    SCPI_ResultText(context, text);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_UartWrite(scpi_t* context)
{
    if (!uart_enabled_check(context)) {
        return SCPI_RES_ERR;
    }

    uint32_t len = 0;
    if (!bus_data_param(context, tx_buffer, UART_BUS_XFER_MAX_LEN, &len)) {
        return SCPI_RES_ERR;
    }

    return uart_status_result(context, uart_bus_write(tx_buffer, len));
}

scpi_result_t SCPI_UartReadQ(scpi_t* context)
{
    if (!uart_enabled_check(context)) {
        return SCPI_RES_ERR;
    }

    // Read first parameter if present
    uint32_t count = 0;
    if (!uart_count_param(context, &count, FALSE)) {
        // A missing count is not an error, it asks for the buffered bytes
        if (SCPI_ParamErrorOccurred(context)) {
            return SCPI_RES_ERR;
        }

        uart_rx_result(
            context, uart_bus_read(rx_buffer, UART_BUS_XFER_MAX_LEN)
        );
        return SCPI_RES_OK;
    }

    uart_rx_result(context, uart_bus_read_wait(rx_buffer, count));
    return SCPI_RES_OK;
}

scpi_result_t SCPI_UartTransferQ(scpi_t* context)
{
    if (!uart_enabled_check(context)) {
        return SCPI_RES_ERR;
    }

    // Read first parameter if present
    uint32_t count = 0;
    if (!uart_count_param(context, &count, TRUE)) {
        return SCPI_RES_ERR;
    }

    uint32_t len = 0;
    if (!bus_data_param(context, tx_buffer, UART_BUS_XFER_MAX_LEN, &len)) {
        return SCPI_RES_ERR;
    }

    // The reply has to follow the request, not a byte left from before it
    uart_bus_rx_clear();

    const UartBusStatus status = uart_bus_write(tx_buffer, len);
    if (status != UART_BUS_OK) {
        return uart_status_result(context, status);
    }

    uart_rx_result(context, uart_bus_read_wait(rx_buffer, count));
    return SCPI_RES_OK;
}

scpi_result_t SCPI_UartBufferCountQ(scpi_t* context)
{
    SCPI_ResultUInt32(context, uart_bus_rx_count());
    return SCPI_RES_OK;
}

scpi_result_t SCPI_UartBufferClear(scpi_t* context)
{
    (void)context;

    uart_bus_rx_clear();
    return SCPI_RES_OK;
}
