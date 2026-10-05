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

/*
 * Fixed parameters and limits of the UART bus: timeout, baud rate range,
 * transfer length, size of the receive buffer and the receive error flags.
 *
 * Internal to the UART component: include it from its .c files only
 * (uart_bus.c and uart_ctl.c), the public interface of the backend is
 * uart_bus.h.
 */

/// Timeout in milliseconds of a transmission, and the time a read waits for
/// the requested number of bytes. The commands are served from the USB device
/// task, so it also bounds how long a read can delay the answer to any other
/// command. A transfer of UART_BUS_XFER_MAX_LEN bytes at the lowest baud rate
/// in the longest frame, 8E2, takes about 40 ms, so it fits.
#define UART_BUS_TIMEOUT_MS 100

/// Range of the baud rate. USART3 is clocked from PCLK1 at 64 MHz and
/// oversamples by 16, so 4 Mbaud is the highest rate the peripheral reaches.
#define UART_BUS_BAUD_MIN 9600
#define UART_BUS_BAUD_MAX 4000000

/// Baud rate applied on power-on and by *RST
#define UART_BUS_BAUD_DEFAULT 115200

/// Maximum number of data bytes of a single transfer. The reply of a read is
/// built in the output buffer of the SCPI parser, and a byte costs up to five
/// characters of it in the hexadecimal response format.
#define UART_BUS_XFER_MAX_LEN 32

/// Size of the receive buffer in bytes, a power of two so that the indexes of
/// the ring wrap with a mask
#define UART_BUS_RX_BUF_LEN 256

/// Size of the transmit buffer in bytes, a power of two like the receive one.
/// It holds several transfers of UART_BUS_XFER_MAX_LEN bytes, so that a write
/// does not have to wait for the previous one to leave the bus
#define UART_BUS_TX_BUF_LEN 256
