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

/// Number of GPIO pins IO1 - IO7 exposed on the external connector. Pin <n>
/// of the DIGital subsystem corresponds to the IO<n> label on the connector.
#define DIGITAL_PIN_COUNT 7

/// Bit mask of all pins of the subsystem, bit 0 corresponds to pin 1
#define DIGITAL_PIN_MASK ((1UL << DIGITAL_PIN_COUNT) - 1UL)

/// Direction of a pin
typedef enum
{
    DIGITAL_DIRECTION_INPUT = 0,
    DIGITAL_DIRECTION_OUTPUT,
} DigitalDirection;

/// Output driver type of a pin
typedef enum
{
    DIGITAL_MODE_PUSHPULL = 0,
    DIGITAL_MODE_ODRAIN,
} DigitalMode;

/// Internal pull resistor of a pin
typedef enum
{
    DIGITAL_PULL_NONE = 0,
    DIGITAL_PULL_UP,
    DIGITAL_PULL_DOWN,
} DigitalPull;

/// Logical polarity of a pin
typedef enum
{
    DIGITAL_POLARITY_NORMAL = 0,
    DIGITAL_POLARITY_INVERTED,
} DigitalPolarity;

/**
 * @brief Init the GPIO subsystem of the external connector.
 *
 * Enables the port clocks of IO1 - IO7 and applies the power-on defaults.
 * Must be called before the SCPI parser starts to serve DIGital commands.
 *
 * The configuration calls of this module do a read-modify-write of the port
 * registers, so all of them are expected to be called from a single thread,
 * the one running the SCPI parser.
 */
void digital_gpio_init(void);

/**
 * @brief Restore the power-on defaults of all pins, used by *RST.
 *
 * Every pin becomes an input without a pull resistor, with a push-pull
 * driver, normal polarity and a stored output level of 0.
 */
void digital_gpio_reset(void);

/**
 * @brief Set up direction of a pin.
 *
 * Switching a pin to output drives the level stored by
 * digital_gpio_level_set().
 *
 * @param pin Pin number, 1 to DIGITAL_PIN_COUNT.
 * @param direction Direction to apply.
 */
void digital_gpio_direction_set(uint32_t pin, DigitalDirection direction);

/**
 * @brief Get direction of a pin.
 *
 * @param pin Pin number, 1 to DIGITAL_PIN_COUNT.
 * @return DigitalDirection Current direction of the pin.
 */
DigitalDirection digital_gpio_direction_get(uint32_t pin);

/**
 * @brief Set up output driver type of a pin.
 *
 * @param pin Pin number, 1 to DIGITAL_PIN_COUNT.
 * @param mode Output driver type to apply.
 */
void digital_gpio_mode_set(uint32_t pin, DigitalMode mode);

/**
 * @brief Get output driver type of a pin.
 *
 * @param pin Pin number, 1 to DIGITAL_PIN_COUNT.
 * @return DigitalMode Current output driver type of the pin.
 */
DigitalMode digital_gpio_mode_get(uint32_t pin);

/**
 * @brief Set up internal pull resistor of a pin.
 *
 * @param pin Pin number, 1 to DIGITAL_PIN_COUNT.
 * @param pull Pull resistor to apply.
 */
void digital_gpio_pull_set(uint32_t pin, DigitalPull pull);

/**
 * @brief Get internal pull resistor of a pin.
 *
 * @param pin Pin number, 1 to DIGITAL_PIN_COUNT.
 * @return DigitalPull Current pull resistor of the pin.
 */
DigitalPull digital_gpio_pull_get(uint32_t pin);

/**
 * @brief Set up logical polarity of a pin.
 *
 * The logical level of the pin is kept, so an output pin is re-driven with
 * the physical level matching the new polarity.
 *
 * @param pin Pin number, 1 to DIGITAL_PIN_COUNT.
 * @param polarity Polarity to apply.
 */
void digital_gpio_polarity_set(uint32_t pin, DigitalPolarity polarity);

/**
 * @brief Get logical polarity of a pin.
 *
 * @param pin Pin number, 1 to DIGITAL_PIN_COUNT.
 * @return DigitalPolarity Current polarity of the pin.
 */
DigitalPolarity digital_gpio_polarity_get(uint32_t pin);

/**
 * @brief Set up logical output level of a pin.
 *
 * For an input pin the level is only stored and is applied when the pin is
 * switched to output.
 *
 * @param pin Pin number, 1 to DIGITAL_PIN_COUNT.
 * @param level Logical level to apply.
 */
void digital_gpio_level_set(uint32_t pin, bool level);

/**
 * @brief Get the actual logical level on a pin, for both input and output
 * pins.
 *
 * @param pin Pin number, 1 to DIGITAL_PIN_COUNT.
 * @return bool Logical level read from the pin.
 */
bool digital_gpio_level_get(uint32_t pin);

/**
 * @brief Set up logical levels of all output pins at once.
 *
 * Bit 0 of the mask corresponds to pin 1. Bits of input pins are ignored,
 * their stored level is left untouched.
 *
 * @param mask Logical levels to apply.
 */
void digital_gpio_output_data_set(uint32_t mask);

/**
 * @brief Get the logical levels set on all output pins.
 *
 * Bit 0 of the mask corresponds to pin 1. Bits of input pins read as 0.
 *
 * @return uint32_t Logical levels of the output pins.
 */
uint32_t digital_gpio_output_data_get(void);

/**
 * @brief Get the actual logical levels of all pins.
 *
 * Bit 0 of the mask corresponds to pin 1.
 *
 * @return uint32_t Logical levels read from the pins.
 */
uint32_t digital_gpio_input_data_get(void);
