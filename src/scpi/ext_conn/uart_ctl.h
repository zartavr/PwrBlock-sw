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
 * @brief Restore the power-on defaults of the UART bus.
 *
 * Wraps uart_bus_reset() so that callers outside the periph_ctl backend,
 * e.g. *RST, do not depend on it directly.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_UartReset(scpi_t* context);

/**
 * BUS:UART:STATe {OFF | ON | 0 | 1}
 * @brief Enable and disable the UART bus. Default is OFF. Disabling the bus
 * clears the receive buffer.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_UartState(scpi_t* context);

/**
 * BUS:UART:STATe?
 * @brief This query returns state of the UART bus.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_UartStateQ(scpi_t* context);

/**
 * BUS:UART:BAUD {<Baud>}
 * @brief Set up the baud rate. Default is 115200.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_UartBaud(scpi_t* context);

/**
 * BUS:UART:BAUD? [MIN | MAX]
 * @brief This query returns the baud rate, or the allowed minimum/maximum.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_UartBaudQ(scpi_t* context);

/**
 * BUS:UART:FRAMe {"<Frame>"}
 * @brief Set up the format of the frame as a string of data bits, parity and
 * stop bits, e.g. "8N1". Default is "8N1".
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_UartFrame(scpi_t* context);

/**
 * BUS:UART:FRAMe?
 * @brief This query returns the format of the frame, e.g. "8N1".
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_UartFrameQ(scpi_t* context);

/**
 * BUS:UART:WRITe {<Data>}
 * @brief Transmit data, Data is up to 32 bytes in any form accepted by
 * bus_data_param(). Completes when all bytes are sent.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_UartWrite(scpi_t* context);

/**
 * BUS:UART:READ? [<Count>]
 * @brief Return bytes from the receive buffer. With Count, 1 to 32, waits for
 * Count bytes up to the timeout. Without Count, returns up to 32 buffered
 * bytes at once. The reply follows FORMat[:DATA], "NONE" if no bytes arrived.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_UartReadQ(scpi_t* context);

/**
 * BUS:UART:TRANsfer? {<Count>},{<Data>}
 * @brief Clear the receive buffer, transmit Data and wait for Count bytes up
 * to the timeout. The reply is the same as of READ?.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_UartTransferQ(scpi_t* context);

/**
 * BUS:UART:BUFFer:COUNt?
 * @brief This query returns the number of bytes in the receive buffer.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_UartBufferCountQ(scpi_t* context);

/**
 * BUS:UART:BUFFer:CLEar
 * @brief Clear the receive buffer.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_UartBufferClear(scpi_t* context);
