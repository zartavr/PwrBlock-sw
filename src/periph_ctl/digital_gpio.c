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

#include "digital_gpio.h"

#include <stm32g0xx_hal.h>

/// Port and pin of a GPIO of the external connector
typedef struct
{
    GPIO_TypeDef* port;
    uint16_t      mask;
} DigitalPinHw;

/**
 * Current configuration of a GPIO of the external connector. The level is the
 * logical output level requested by digital_gpio_level_set(), it is kept for
 * an input pin as well and is driven when the pin becomes an output.
 */
typedef struct
{
    DigitalDirection direction;
    DigitalMode      mode;
    DigitalPull      pull;
    DigitalPolarity  polarity;
    bool             level;
} DigitalPinCfg;

/**
 * Pins of the external connector, indexed by pin number - 1. The IO<n> label
 * on the connector is wired to the GPIO<n> net of the board, IO7 is the
 * GPIO8 net.
 */
static const DigitalPinHw PIN_HW[DIGITAL_PIN_COUNT] = {
    {GPIOD, GPIO_PIN_0}, // IO1
    {GPIOD, GPIO_PIN_1}, // IO2
    {GPIOD, GPIO_PIN_2}, // IO3
    {GPIOD, GPIO_PIN_3}, // IO4
    {GPIOC, GPIO_PIN_6}, // IO5
    {GPIOC, GPIO_PIN_7}, // IO6
    {GPIOA, GPIO_PIN_3}, // IO7
};

static DigitalPinCfg pin_cfg[DIGITAL_PIN_COUNT];

/**
 * @brief Check a pin number and convert it into an index of the pin tables.
 *
 * @param pin Pin number, 1 to DIGITAL_PIN_COUNT.
 * @param index Index of the pin, pin number - 1.
 * @return bool false if the pin number is out of range.
 */
static bool digital_pin_index(uint32_t pin, uint32_t* index)
{
    if (pin < 1 || pin > DIGITAL_PIN_COUNT) {
        return false;
    }

    *index = pin - 1;
    return true;
}

/**
 * @brief Drive the physical level of a pin matching a logical level.
 *
 * Has no effect on the pin while it is an input, the output register keeps
 * the level until the pin is switched to output.
 *
 * @param index Index of the pin.
 * @param level Logical level to drive.
 */
static void digital_pin_drive(uint32_t index, bool level)
{
    const bool inverted = pin_cfg[index].polarity == DIGITAL_POLARITY_INVERTED;
    const bool physical = inverted ? !level : level;

    HAL_GPIO_WritePin(
        PIN_HW[index].port,
        PIN_HW[index].mask,
        physical ? GPIO_PIN_SET : GPIO_PIN_RESET
    );
}

/**
 * @brief Apply the stored configuration of a pin to the port registers.
 *
 * @param index Index of the pin.
 */
static void digital_pin_apply(uint32_t index)
{
    const DigitalPinCfg* cfg  = &pin_cfg[index];
    GPIO_InitTypeDef     init = {0};

    init.Pin   = PIN_HW[index].mask;
    init.Speed = GPIO_SPEED_FREQ_LOW;

    switch (cfg->pull) {
        case DIGITAL_PULL_UP: {
            init.Pull = GPIO_PULLUP;
            break;
        }
        case DIGITAL_PULL_DOWN: {
            init.Pull = GPIO_PULLDOWN;
            break;
        }
        case DIGITAL_PULL_NONE:
        default: {
            init.Pull = GPIO_NOPULL;
            break;
        }
    }

    if (cfg->direction == DIGITAL_DIRECTION_OUTPUT) {
        init.Mode = (cfg->mode == DIGITAL_MODE_ODRAIN) ? GPIO_MODE_OUTPUT_OD
                                                       : GPIO_MODE_OUTPUT_PP;
    } else {
        init.Mode = GPIO_MODE_INPUT;
    }

    // Load the output register before the mode is applied, so switching a pin
    // to output cannot emit a pulse of the level left by a previous setup
    digital_pin_drive(index, cfg->level);

    HAL_GPIO_Init(PIN_HW[index].port, &init);
}

void digital_gpio_init(void)
{
    // IO1 - IO4 live on GPIOD, which is not enabled by the generated setup
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    digital_gpio_reset();
}

void digital_gpio_reset(void)
{
    for (uint32_t index = 0; index < DIGITAL_PIN_COUNT; index++) {
        pin_cfg[index].direction = DIGITAL_DIRECTION_INPUT;
        pin_cfg[index].mode      = DIGITAL_MODE_PUSHPULL;
        pin_cfg[index].pull      = DIGITAL_PULL_NONE;
        pin_cfg[index].polarity  = DIGITAL_POLARITY_NORMAL;
        pin_cfg[index].level     = false;

        digital_pin_apply(index);
    }
}

void digital_gpio_direction_set(uint32_t pin, DigitalDirection direction)
{
    uint32_t index = 0;
    if (!digital_pin_index(pin, &index)) {
        return;
    }

    pin_cfg[index].direction = direction;
    digital_pin_apply(index);
}

DigitalDirection digital_gpio_direction_get(uint32_t pin)
{
    uint32_t index = 0;
    if (!digital_pin_index(pin, &index)) {
        return DIGITAL_DIRECTION_INPUT;
    }

    return pin_cfg[index].direction;
}

void digital_gpio_mode_set(uint32_t pin, DigitalMode mode)
{
    uint32_t index = 0;
    if (!digital_pin_index(pin, &index)) {
        return;
    }

    pin_cfg[index].mode = mode;
    digital_pin_apply(index);
}

DigitalMode digital_gpio_mode_get(uint32_t pin)
{
    uint32_t index = 0;
    if (!digital_pin_index(pin, &index)) {
        return DIGITAL_MODE_PUSHPULL;
    }

    return pin_cfg[index].mode;
}

void digital_gpio_pull_set(uint32_t pin, DigitalPull pull)
{
    uint32_t index = 0;
    if (!digital_pin_index(pin, &index)) {
        return;
    }

    pin_cfg[index].pull = pull;
    digital_pin_apply(index);
}

DigitalPull digital_gpio_pull_get(uint32_t pin)
{
    uint32_t index = 0;
    if (!digital_pin_index(pin, &index)) {
        return DIGITAL_PULL_NONE;
    }

    return pin_cfg[index].pull;
}

void digital_gpio_polarity_set(uint32_t pin, DigitalPolarity polarity)
{
    uint32_t index = 0;
    if (!digital_pin_index(pin, &index)) {
        return;
    }

    pin_cfg[index].polarity = polarity;

    // The logical level of the pin is kept, the physical one follows the new
    // polarity
    digital_pin_drive(index, pin_cfg[index].level);
}

DigitalPolarity digital_gpio_polarity_get(uint32_t pin)
{
    uint32_t index = 0;
    if (!digital_pin_index(pin, &index)) {
        return DIGITAL_POLARITY_NORMAL;
    }

    return pin_cfg[index].polarity;
}

void digital_gpio_level_set(uint32_t pin, bool level)
{
    uint32_t index = 0;
    if (!digital_pin_index(pin, &index)) {
        return;
    }

    pin_cfg[index].level = level;
    digital_pin_drive(index, level);
}

bool digital_gpio_level_get(uint32_t pin)
{
    uint32_t index = 0;
    if (!digital_pin_index(pin, &index)) {
        return false;
    }

    const bool physical =
        HAL_GPIO_ReadPin(PIN_HW[index].port, PIN_HW[index].mask) ==
        GPIO_PIN_SET;
    const bool inverted = pin_cfg[index].polarity == DIGITAL_POLARITY_INVERTED;

    return inverted ? !physical : physical;
}

void digital_gpio_output_data_set(uint32_t mask)
{
    for (uint32_t index = 0; index < DIGITAL_PIN_COUNT; index++) {
        if (pin_cfg[index].direction != DIGITAL_DIRECTION_OUTPUT) {
            continue;
        }

        const bool level = (mask & (1UL << index)) != 0;

        pin_cfg[index].level = level;
        digital_pin_drive(index, level);
    }
}

uint32_t digital_gpio_output_data_get(void)
{
    uint32_t mask = 0;

    for (uint32_t index = 0; index < DIGITAL_PIN_COUNT; index++) {
        if (pin_cfg[index].direction != DIGITAL_DIRECTION_OUTPUT) {
            continue;
        }

        if (pin_cfg[index].level) {
            mask |= 1UL << index;
        }
    }

    return mask;
}

uint32_t digital_gpio_input_data_get(void)
{
    uint32_t mask = 0;

    for (uint32_t index = 0; index < DIGITAL_PIN_COUNT; index++) {
        if (digital_gpio_level_get(index + 1)) {
            mask |= 1UL << index;
        }
    }

    return mask;
}
