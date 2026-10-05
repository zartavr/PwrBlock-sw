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
    bool              enabled;
    uint32_t          baud;
    UartBusFrame      frame;
    volatile uint32_t errors;
} UartBus;

/// Frame applied on power-on and by *RST, 8N1
static const UartBusFrame FRAME_DEFAULT = {
    .data_bits = 8, .parity = UART_BUS_PARITY_NONE, .stop_bits = 1
};

extern UART_HandleTypeDef huart3;

static UartBus bus;

// Ring buffer of the received bytes, filled by the interrupt and drained by
// the thread of the parser. The indexes run free and are masked on access,
// each side writes only its own one, and an aligned 32 bit access is atomic,
// so the ring needs no lock
static uint8_t           rx_buf[UART_BUS_RX_BUF_LEN];
static volatile uint32_t rx_head;
static volatile uint32_t rx_tail;

// Ring buffer of the bytes to transmit, the other way round: filled by the
// thread of the parser and drained by the interrupt
static uint8_t           tx_buf[UART_BUS_TX_BUF_LEN];
static volatile uint32_t tx_head;
static volatile uint32_t tx_tail;

// Private prototypes
static uint32_t uart_bus_word_length(void);
static uint32_t uart_bus_parity(void);

static UartBusStatus uart_bus_apply(void);
static UartBusStatus uart_bus_refresh(void);

static uint32_t uart_bus_tx_count(void);
static void     uart_bus_tx_clear(void);
static void     uart_bus_tx_drain(void);

void uart_bus_init(void)
{
    uart_bus_reset();
}

void uart_bus_reset(void)
{
    bus.enabled = false;
    bus.baud    = UART_BUS_BAUD_DEFAULT;
    bus.frame   = FRAME_DEFAULT;

    uart_bus_rx_clear();
    uart_bus_tx_clear();

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

    if (enabled == bus.enabled) {
        return UART_BUS_OK;
    }

    if (!enabled) {
        // Wait for TX finish
        uart_bus_tx_drain();

        bus.enabled = false;
        HAL_UART_DeInit(&huart3);
        uart_bus_rx_clear();
        return UART_BUS_OK;
    }

    UartBusStatus status = uart_bus_apply();
    if (status != UART_BUS_OK) {
        bus.enabled = false;
        HAL_UART_DeInit(&huart3);
        return status;
    }

    bus.enabled = true;
    return status;
}

bool uart_bus_state_get(void)
{
    return bus.enabled;
}

UartBusStatus uart_bus_baud_set(uint32_t baud)
{
    if (baud < UART_BUS_BAUD_MIN || baud > UART_BUS_BAUD_MAX) {
        return UART_BUS_ERR_PARAM;
    }

    bus.baud = baud;

    return uart_bus_refresh();
}

uint32_t uart_bus_baud_get(void)
{
    return bus.baud;
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

    bus.frame = frame;

    return uart_bus_refresh();
}

UartBusFrame uart_bus_frame_get(void)
{
    return bus.frame;
}

UartBusStatus uart_bus_write(const uint8_t* data, uint32_t len)
{
    if (!bus.enabled) {
        return UART_BUS_ERR_DISABLED;
    }

    if (len == 0 || len > UART_BUS_XFER_MAX_LEN) {
        return UART_BUS_ERR_PARAM;
    }

    if ((UART_BUS_TX_BUF_LEN - uart_bus_tx_count()) < len) {
        return UART_BUS_ERR_TX_FULL;
    }

    uint32_t head = tx_head;
    for (uint32_t index = 0; index < len; index++) {
        tx_buf[head & (UART_BUS_TX_BUF_LEN - 1)] = data[index];
        head++;
    }
    tx_head = head;

    __HAL_UART_ENABLE_IT(&huart3, UART_IT_TXFNF);

    return UART_BUS_OK;
}

uint32_t uart_bus_read(uint8_t* dst, uint32_t max)
{
    uint32_t count = 0;
    uint32_t tail  = rx_tail;

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
    uint32_t start = osKernelGetTickCount();

    while (uart_bus_rx_count() < count &&
           (osKernelGetTickCount() - start) < UART_BUS_TIMEOUT_MS) {
        osDelay(1);
    }

    return uart_bus_read(dst, count);
}

uint32_t uart_bus_rx_count(void)
{
    return rx_head - rx_tail;
}

void uart_bus_rx_clear(void)
{
    __disable_irq();
    rx_tail    = rx_head;
    bus.errors = 0;
    __enable_irq();
}

UartBusStatus uart_bus_rx_errors_get(void)
{
    uint32_t errors = bus.errors;
    if (errors & USART_ISR_ORE) {
        return UART_BUS_ERR_RX_OVERRUN;
    }
    if (errors & USART_ISR_FE) {
        return UART_BUS_ERR_RX_FRAMING;
    }
    if (errors & USART_ISR_PE) {
        return UART_BUS_ERR_RX_PARITY;
    }
    if (errors & USART_ISR_NE) {
        return UART_BUS_ERR_RX_NOISE;
    }
    return UART_BUS_OK;
}

void uart_bus_irq_handler(void)
{
    USART_TypeDef* const uart = USART3;

    uint32_t errors = uart->ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_PE |
                                   USART_ISR_NE);

    if (errors != 0) {
        uart->ICR =
            USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_PECF | USART_ICR_NECF;
    }

    // Feed the transmitter while it has room and the buffer has bytes. The
    // interrupt is disabled on the last one, it would otherwise fire forever
    if ((uart->CR1 & USART_CR1_TXEIE_TXFNFIE) != 0) {
        while ((uart->ISR & USART_ISR_TXE_TXFNF) != 0) {
            uint32_t tail = tx_tail;

            if (tail == tx_head) {
                __HAL_UART_DISABLE_IT(&huart3, UART_IT_TXFNF);
                break;
            }

            uart->TDR = tx_buf[tail & (UART_BUS_TX_BUF_LEN - 1)];
            tx_tail   = tail + 1;
        }
    }

    uint8_t mask = (bus.frame.data_bits == 7) ? 0x7F : 0xFF;
    while ((uart->ISR & USART_ISR_RXNE_RXFNE) != 0) {
        uint8_t byte = (uint8_t)(uart->RDR & mask);

        if (!bus.enabled) {
            continue;
        }

        uint32_t head = rx_head;
        if ((head - rx_tail) >= UART_BUS_RX_BUF_LEN) {
            errors |= USART_ISR_ORE;
            continue;
        }

        rx_buf[head & (UART_BUS_RX_BUF_LEN - 1)] = byte;
        rx_head                                  = head + 1;
    }

    if (bus.enabled) {
        bus.errors |= errors;
    }
}

static uint32_t uart_bus_word_length(void)
{
    bool     parity = (bus.frame.parity != UART_BUS_PARITY_NONE);
    uint32_t bits   = bus.frame.data_bits + (parity ? 1 : 0);

    if (bits == 7) {
        return UART_WORDLENGTH_7B;
    }

    if (bits == 9) {
        return UART_WORDLENGTH_9B;
    }

    return UART_WORDLENGTH_8B;
}

static uint32_t uart_bus_parity(void)
{
    switch (bus.frame.parity) {
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

static UartBusStatus uart_bus_apply(void)
{
    HAL_UART_DeInit(&huart3);

    huart3.Instance        = USART3;
    huart3.Init.BaudRate   = bus.baud;
    huart3.Init.WordLength = uart_bus_word_length();
    huart3.Init.StopBits =
        (bus.frame.stop_bits == 2) ? UART_STOPBITS_2 : UART_STOPBITS_1;
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

static UartBusStatus uart_bus_refresh(void)
{
    if (!bus.enabled) {
        return UART_BUS_OK;
    }

    // Make sure that tx_buffer is empty
    uart_bus_tx_drain();
    UartBusStatus status = uart_bus_apply();
    if (status != UART_BUS_OK) {
        bus.enabled = false;
        HAL_UART_DeInit(&huart3);
    }

    return status;
}

static uint32_t uart_bus_tx_count(void)
{
    return tx_head - tx_tail;
}

static void uart_bus_tx_clear(void)
{
    __HAL_UART_DISABLE_IT(&huart3, UART_IT_TXFNF);
    tx_tail = tx_head;
}

static void uart_bus_tx_drain(void)
{
    // The tick of the kernel is 1 ms
    uint32_t start = osKernelGetTickCount();

    while (uart_bus_tx_count() != 0 &&
           (osKernelGetTickCount() - start) < UART_BUS_TIMEOUT_MS) {
        osDelay(1);
    }

    uart_bus_tx_clear();
}
