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

/// Result of an operation of the bus
typedef enum
{
    UART_BUS_OK = 0,
    UART_BUS_ERR_TIMEOUT,     // Transmission did not complete in time
    UART_BUS_ERR_BUS,         // The hardware rejected the setup or the transfer
    UART_BUS_ERR_DISABLED,    // The bus is disabled
    UART_BUS_ERR_PARAM,       // Baud rate, frame or length out of range
    UART_BUS_ERR_BUSY,        // USART3 is owned by the USB-PD tracer
    UART_BUS_ERR_TX_FULL,     // No room in the transmit buffer
    UART_BUS_ERR_RX_OVERRUN,  // Received bytes were lost
    UART_BUS_ERR_RX_FRAMING,  // Invalid stop bit
    UART_BUS_ERR_RX_PARITY,   // Parity check failed
    UART_BUS_ERR_RX_NOISE,    // Noise detected on a bit
} UartBusStatus;

/// Parity of the frame
typedef enum
{
    UART_BUS_PARITY_NONE = 0,
    UART_BUS_PARITY_EVEN = 1,
    UART_BUS_PARITY_ODD  = 2,
} UartBusParity;

/// Format of the frame, e.g. 8N1
typedef struct
{
    uint8_t       data_bits;  // 7-9
    UartBusParity parity;
    uint8_t       stop_bits;  // 1 or 2
} UartBusFrame;

/**
 * @brief Init the UART bus of the external connector, disabled by default.
 *
 * The bus is USART3 on dedicated pins, TX on PB2 and RX on PB0, with no
 * hardware flow control. In a build with the USB-PD tracer (_TRACE) the
 * peripheral stays with the tracer and the bus cannot be enabled.
 *
 * Every call but uart_bus_irq_handler() is expected to come from the thread of
 * the SCPI parser, some of them block it for up to UART_BUS_TIMEOUT_MS.
 */
void uart_bus_init(void);

/**
 * @brief Restore the power-on defaults, used by *RST: disabled, 115200, 8N1.
 */
void uart_bus_reset(void);

/**
 * @brief Set state of the bus, disabling it clears the buffers.
 *
 * @param enabled True - the bus is enabled, false - the bus is disabled.
 * @return UartBusStatus UART_BUS_ERR_BUSY if the tracer owns USART3,
 * UART_BUS_ERR_BUS if the hardware rejected the setup.
 */
UartBusStatus uart_bus_state_set(bool enabled);

/**
 * @brief Get state of the bus.
 *
 * @return true The bus is enabled.
 * @return false The bus is disabled.
 */
bool uart_bus_state_get(void);

/**
 * @brief Set baud rate of the bus.
 *
 * On a disabled bus it applies at the next enable, on an enabled one at once.
 *
 * @param baud UART_BUS_BAUD_MIN to UART_BUS_BAUD_MAX (uart_bus_def.h).
 * @return UartBusStatus UART_BUS_ERR_PARAM if the rate is out of range,
 * UART_BUS_ERR_BUS if the hardware rejected the setup.
 */
UartBusStatus uart_bus_baud_set(uint32_t baud);

/**
 * @brief Get baud rate of the bus.
 *
 * @return uint32_t Current baud rate.
 */
uint32_t uart_bus_baud_get(void);

/**
 * @brief Set format of the frame, applies as uart_bus_baud_set() does.
 *
 * @param frame Format to apply.
 * @return UartBusStatus UART_BUS_ERR_PARAM if the frame is invalid,
 * UART_BUS_ERR_BUS if the hardware rejected the setup.
 */
UartBusStatus uart_bus_frame_set(UartBusFrame frame);

/**
 * @brief Get format of the frame.
 *
 * @return UartBusFrame Current format.
 */
UartBusFrame uart_bus_frame_get(void);

/**
 * @brief Queue data to transmit, returns before the bytes are sent.
 *
 * The interrupt puts them on the bus. A following setup call or disable gives
 * them UART_BUS_TIMEOUT_MS to leave it and drops what is left.
 *
 * @param data Bytes to be sent.
 * @param len Number of bytes, 1 to UART_BUS_XFER_MAX_LEN (uart_bus_def.h).
 * @return UartBusStatus UART_BUS_ERR_DISABLED if the bus is disabled,
 * UART_BUS_ERR_PARAM if the length is out of range, UART_BUS_ERR_TX_FULL if
 * earlier writes still hold the transmit buffer.
 */
UartBusStatus uart_bus_write(const uint8_t* data, uint32_t len);

/**
 * @brief Take bytes from the receive buffer, without waiting.
 *
 * @param dst Destination of the bytes.
 * @param max Capacity of dst in bytes.
 * @return uint32_t Number of bytes taken, 0 if the buffer is empty.
 */
uint32_t uart_bus_read(uint8_t* dst, uint32_t max);

/**
 * @brief Take bytes from the receive buffer, waiting for them to arrive.
 *
 * Waits for count bytes up to UART_BUS_TIMEOUT_MS.
 *
 * @param dst Destination of the bytes.
 * @param count Number of bytes to wait for, the capacity of dst.
 * @return uint32_t Number of bytes taken, less than count on a timeout.
 */
uint32_t uart_bus_read_wait(uint8_t* dst, uint32_t count);

/**
 * @brief Get number of bytes in the receive buffer.
 *
 * @return uint32_t Number of bytes.
 */
uint32_t uart_bus_rx_count(void);

/**
 * @brief Clear the receive buffer and the latched receive errors.
 */
void uart_bus_rx_clear(void);

/**
 * @brief Take the receive error latched since the previous call, clearing it.
 *
 * @return UartBusStatus The most severe of UART_BUS_ERR_RX_*, UART_BUS_OK if
 * none.
 */
UartBusStatus uart_bus_rx_errors_take(void);
