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

#include "uart_bus.h"

#include "uart_bus_def.h"
#include "uart_bus_it.h"

#include <cmsis_os2.h>
#include <stm32g0xx_hal.h>

/// Current configuration of the bus
typedef struct
{
    bool         enabled;
    uint32_t     baud;
    UartBusFrame frame;
} UartBusCfg;

/// Frame applied on power-on and by *RST, 8N1
static const UartBusFrame FRAME_DEFAULT = {
    .data_bits = 8,
    .parity    = UART_BUS_PARITY_NONE,
    .stop_bits = 1,
};

extern UART_HandleTypeDef huart3;

static UartBusCfg cfg;

// Ring buffer of the received bytes, filled by the interrupt and drained by
// the thread of the parser. The indexes run free and are masked on access,
// each side writes only its own one, and a 16 bit access is atomic, so the
// ring needs no lock
static uint8_t           rx_buf[UART_BUS_RX_BUF_LEN];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;

// Receive errors latched by the interrupt, UART_BUS_RX_ERR_* bits
static volatile uint32_t rx_errors;

/**
 * @brief Get the word length of the peripheral for the stored frame.
 *
 * The word length of the peripheral includes the parity bit.
 *
 * @return uint32_t Word length in the form expected by the HAL.
 */
static uint32_t uart_bus_word_length(void)
{
    const bool     parity = (cfg.frame.parity != UART_BUS_PARITY_NONE);
    const uint32_t bits   = cfg.frame.data_bits + (parity ? 1 : 0);

    if (bits == 7) {
        return UART_WORDLENGTH_7B;
    }

    if (bits == 9) {
        return UART_WORDLENGTH_9B;
    }

    return UART_WORDLENGTH_8B;
}

/**
 * @brief Get the parity of the peripheral for the stored frame.
 *
 * @return uint32_t Parity in the form expected by the HAL.
 */
static uint32_t uart_bus_parity(void)
{
    switch (cfg.frame.parity) {
        case UART_BUS_PARITY_EVEN: {
            return UART_PARITY_EVEN;
        }
        case UART_BUS_PARITY_ODD: {
            return UART_PARITY_ODD;
        }
        case UART_BUS_PARITY_NONE:
        default: {
            return UART_PARITY_NONE;
        }
    }
}

/**
 * @brief Apply the stored configuration to the peripheral.
 *
 * The interrupt of the receiver is enabled last, so the bus has to be marked
 * enabled before the call for the received bytes to be kept.
 *
 * @return UartBusStatus UART_BUS_ERR_BUS if the hardware rejected the setup.
 */
static UartBusStatus uart_bus_apply(void)
{
    HAL_UART_DeInit(&huart3);

    huart3.Instance        = USART3;
    huart3.Init.BaudRate   = cfg.baud;
    huart3.Init.WordLength = uart_bus_word_length();
    huart3.Init.StopBits =
        (cfg.frame.stop_bits == 2) ? UART_STOPBITS_2 : UART_STOPBITS_1;
    huart3.Init.Parity                 = uart_bus_parity();
    huart3.Init.Mode                   = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling           = UART_OVERSAMPLING_16;
    huart3.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    huart3.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart3) != HAL_OK) {
        return UART_BUS_ERR_BUS;
    }

    // The FIFO holds the received bytes while the interrupt is delayed, which
    // matters at the high baud rates
    if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) !=
        HAL_OK) {
        return UART_BUS_ERR_BUS;
    }

    if (HAL_UARTEx_EnableFifoMode(&huart3) != HAL_OK) {
        return UART_BUS_ERR_BUS;
    }

    // The receiver is served by uart_bus_irq_handler() rather than by a
    // reception of the HAL, which would stop on the first error
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_PE);
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_ERR);
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXFNE);

    return UART_BUS_OK;
}

/**
 * @brief Re-apply the stored configuration if the bus is enabled.
 *
 * @return UartBusStatus Result of the setup.
 */
static UartBusStatus uart_bus_refresh(void)
{
    if (!cfg.enabled) {
        return UART_BUS_OK;
    }

    const UartBusStatus status = uart_bus_apply();
    if (status != UART_BUS_OK) {
        cfg.enabled = false;
        HAL_UART_DeInit(&huart3);
    }

    return status;
}

void uart_bus_init(void)
{
    uart_bus_reset();
}

void uart_bus_reset(void)
{
    cfg.enabled = false;
    cfg.baud    = UART_BUS_BAUD_DEFAULT;
    cfg.frame   = FRAME_DEFAULT;

    uart_bus_rx_clear();

#ifndef _TRACE
    // The peripheral is brought up by the setup of the board, so a disabled
    // bus has to be released rather than just left alone
    HAL_UART_DeInit(&huart3);
#endif
}

UartBusStatus uart_bus_state_set(bool enabled)
{
#ifdef _TRACE
    // The tracer of the USB-PD stack owns USART3 in this build
    if (enabled) {
        return UART_BUS_ERR_BUSY;
    }
#endif

    if (enabled == cfg.enabled) {
        return UART_BUS_OK;
    }

    if (!enabled) {
        cfg.enabled = false;
        HAL_UART_DeInit(&huart3);
        uart_bus_rx_clear();
        return UART_BUS_OK;
    }

    // Marked before the setup, the interrupt keeps the received bytes only on
    // an enabled bus
    cfg.enabled = true;

    return uart_bus_refresh();
}

bool uart_bus_state_get(void)
{
    return cfg.enabled;
}

UartBusStatus uart_bus_baud_set(uint32_t baud)
{
    if (baud < UART_BUS_BAUD_MIN || baud > UART_BUS_BAUD_MAX) {
        return UART_BUS_ERR_PARAM;
    }

    cfg.baud = baud;

    return uart_bus_refresh();
}

uint32_t uart_bus_baud_get(void)
{
    return cfg.baud;
}

UartBusStatus uart_bus_frame_set(UartBusFrame frame)
{
    if (frame.data_bits != 7 && frame.data_bits != 8) {
        return UART_BUS_ERR_PARAM;
    }

    if (frame.stop_bits != 1 && frame.stop_bits != 2) {
        return UART_BUS_ERR_PARAM;
    }

    if (frame.parity != UART_BUS_PARITY_NONE &&
        frame.parity != UART_BUS_PARITY_EVEN &&
        frame.parity != UART_BUS_PARITY_ODD) {
        return UART_BUS_ERR_PARAM;
    }

    cfg.frame = frame;

    return uart_bus_refresh();
}

UartBusFrame uart_bus_frame_get(void)
{
    return cfg.frame;
}

UartBusStatus uart_bus_write(const uint8_t* data, uint32_t len)
{
    if (!cfg.enabled) {
        return UART_BUS_ERR_DISABLED;
    }

    if (len == 0 || len > UART_BUS_XFER_MAX_LEN) {
        return UART_BUS_ERR_PARAM;
    }

    const HAL_StatusTypeDef status = HAL_UART_Transmit(
        &huart3, (uint8_t*)data, (uint16_t)len, UART_BUS_TIMEOUT_MS
    );

    if (status == HAL_OK) {
        return UART_BUS_OK;
    }

    if (status == HAL_TIMEOUT) {
        return UART_BUS_ERR_TIMEOUT;
    }

    return UART_BUS_ERR_BUS;
}

uint32_t uart_bus_read(uint8_t* dst, uint32_t max)
{
    uint32_t count = 0;
    uint16_t tail  = rx_tail;

    while (count < max && tail != rx_head) {
        dst[count] = rx_buf[tail & (UART_BUS_RX_BUF_LEN - 1)];
        tail++;
        count++;
    }

    rx_tail = tail;
    return count;
}

uint32_t uart_bus_read_wait(uint8_t* dst, uint32_t count)
{
    // The tick of the kernel is 1 ms
    const uint32_t start = osKernelGetTickCount();

    while (uart_bus_rx_count() < count &&
           (osKernelGetTickCount() - start) < UART_BUS_TIMEOUT_MS) {
        osDelay(1);
    }

    return uart_bus_read(dst, count);
}

uint32_t uart_bus_rx_count(void)
{
    return (uint16_t)(rx_head - rx_tail);
}

void uart_bus_rx_clear(void)
{
    __disable_irq();
    rx_tail   = rx_head;
    rx_errors = 0;
    __enable_irq();
}

uint32_t uart_bus_rx_errors_take(void)
{
    __disable_irq();
    const uint32_t errors = rx_errors;
    rx_errors             = 0;
    __enable_irq();

    return errors;
}

void uart_bus_irq_handler(void)
{
    USART_TypeDef* const uart = USART3;

    const uint32_t isr    = uart->ISR;
    uint32_t       errors = 0;

    if ((isr & USART_ISR_ORE) != 0) {
        errors |= UART_BUS_RX_ERR_OVERRUN;
    }

    if ((isr & USART_ISR_FE) != 0) {
        errors |= UART_BUS_RX_ERR_FRAMING;
    }

    if ((isr & USART_ISR_PE) != 0) {
        errors |= UART_BUS_RX_ERR_PARITY;
    }

    if ((isr & USART_ISR_NE) != 0) {
        errors |= UART_BUS_RX_ERR_NOISE;
    }

    if (errors != 0) {
        uart->ICR =
            USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_PECF | USART_ICR_NECF;
    }

    // With parity the peripheral leaves the parity bit above the data
    const uint8_t mask = (cfg.frame.data_bits == 7) ? 0x7F : 0xFF;

    // The FIFO is drained even on a disabled bus, the pending interrupt would
    // otherwise fire again and again
    while ((uart->ISR & USART_ISR_RXNE_RXFNE) != 0) {
        const uint8_t byte = (uint8_t)(uart->RDR & mask);

        if (!cfg.enabled) {
            continue;
        }

        const uint16_t head = rx_head;
        if ((uint16_t)(head - rx_tail) >= UART_BUS_RX_BUF_LEN) {
            errors |= UART_BUS_RX_ERR_OVERRUN;
            continue;
        }

        rx_buf[head & (UART_BUS_RX_BUF_LEN - 1)] = byte;
        rx_head                                  = head + 1;
    }

    if (cfg.enabled) {
        rx_errors |= errors;
    }
}
