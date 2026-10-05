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
 * @brief Init the UART bus of the external connector.
 *
 * The bus is USART3 on dedicated pins, TX on PB2 and RX on PB0, not shared
 * with the GPIO subsystem, with no hardware flow control. Received bytes are
 * collected in the background into a ring buffer of UART_BUS_RX_BUF_LEN
 * (uart_bus_def.h) bytes while the bus is enabled.
 *
 * USART3 is also the port of the USB-PD tracer. When the firmware is built
 * with the tracer (_TRACE), the module leaves the peripheral alone and the bus
 * cannot be enabled.
 *
 * Applies the power-on defaults, which leave the bus disabled. Must be called
 * before the SCPI parser starts to serve BUS:UART commands.
 *
 * Transmissions and waiting reads block the calling thread for up to
 * UART_BUS_TIMEOUT_MS, and the configuration calls re-initialize the
 * peripheral, so all of the calls, except uart_bus_irq_handler(), are expected
 * to be made from a single thread, the one running the SCPI parser.
 */
void uart_bus_init(void);

/**
 * @brief Restore the power-on defaults of the bus, used by *RST.
 *
 * The bus becomes disabled at 115200 baud with a 8N1 frame, the receive
 * buffer and the latched errors are cleared.
 */
void uart_bus_reset(void);

/**
 * @brief Set state of the bus.
 *
 * Disabling the bus clears the receive buffer and the latched errors.
 *
 * @param enabled True - the bus is enabled, false - the bus is disabled.
 * @return UartBusStatus UART_BUS_ERR_BUSY if USART3 is owned by the tracer,
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
 * May be set while the bus is disabled, the rate then applies at the next
 * enable. On an enabled bus the peripheral is re-initialized at once, the
 * receive buffer is kept.
 *
 * @param baud Baud rate, UART_BUS_BAUD_MIN to UART_BUS_BAUD_MAX
 * (uart_bus_def.h).
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
 * @brief Set format of the frame.
 *
 * Applies the same way as uart_bus_baud_set().
 *
 * @param frame Format to apply.
 * @return UartBusStatus UART_BUS_ERR_PARAM if the number of data or stop bits
 * is invalid, UART_BUS_ERR_BUS if the hardware rejected the setup.
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
 * The bytes are copied into the transmit buffer and put on the bus by the
 * interrupt, so the call does not wait for the transmission. A following
 * uart_bus_baud_set(), uart_bus_frame_set() or uart_bus_state_set(false) gives
 * them up to UART_BUS_TIMEOUT_MS to leave the bus and drops what is left.
 *
 * @param data Bytes to be sent.
 * @param len Number of bytes, 1 to UART_BUS_XFER_MAX_LEN (uart_bus_def.h).
 * @return UartBusStatus UART_BUS_OK if the bytes were queued,
 * UART_BUS_ERR_DISABLED if the bus is disabled, UART_BUS_ERR_PARAM if the
 * length is out of range, UART_BUS_ERR_TX_FULL if the transmit buffer has no
 * room for them, because earlier writes are still on the bus.
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
 * Waits until count bytes are buffered or UART_BUS_TIMEOUT_MS expires, then
 * takes up to count bytes.
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
 * @brief Get the receive error latched since the receive buffer was cleared.
 *
 * @return UartBusStatus UART_BUS_ERR_RX_OVERRUN, _RX_FRAMING, _RX_PARITY or
 * _RX_NOISE, the most severe one latched, UART_BUS_OK if none.
 */
UartBusStatus uart_bus_rx_errors_take(void);
