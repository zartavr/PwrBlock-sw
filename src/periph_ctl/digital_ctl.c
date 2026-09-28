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

#include "digital_ctl.h"

#include <stdio.h>
#include <string.h>

/// Owner of a pin, reported by [SOURce]:DIGital:PIN<n>:FUNCtion?. Buses are
/// wired to their own dedicated pins, so IO1 - IO7 are always GPIO.
typedef enum {
    DIGITAL_FUNCTION_GPIO = 0,
} DigitalFunction;

/// Direction of a pin
typedef enum {
    DIGITAL_DIRECTION_INPUT = 0,
    DIGITAL_DIRECTION_OUTPUT,
} DigitalDirection;

/// Output driver type of a pin
typedef enum {
    DIGITAL_MODE_PUSHPULL = 0,
    DIGITAL_MODE_ODRAIN,
} DigitalMode;

/// Internal pull resistor of a pin
typedef enum {
    DIGITAL_PULL_NONE = 0,
    DIGITAL_PULL_UP,
    DIGITAL_PULL_DOWN,
} DigitalPull;

/// Logical polarity of a pin
typedef enum {
    DIGITAL_POLARITY_NORMAL = 0,
    DIGITAL_POLARITY_INVERTED,
} DigitalPolarity;

// Parameter patterns, used to decode an enumerated value of a command
static const scpi_choice_def_t DIRECTION_CHOICES[] = {
    {"INPut",   DIGITAL_DIRECTION_INPUT },
    {"OUTPut",  DIGITAL_DIRECTION_OUTPUT},
    SCPI_CHOICE_LIST_END
};

static const scpi_choice_def_t MODE_CHOICES[] = {
    {"PUSHpull", DIGITAL_MODE_PUSHPULL},
    {"ODRain",   DIGITAL_MODE_ODRAIN  },
    SCPI_CHOICE_LIST_END
};

static const scpi_choice_def_t PULL_CHOICES[] = {
    {"NONE", DIGITAL_PULL_NONE},
    {"UP",   DIGITAL_PULL_UP  },
    {"DOWN", DIGITAL_PULL_DOWN},
    SCPI_CHOICE_LIST_END
};

static const scpi_choice_def_t POLARITY_CHOICES[] = {
    {"NORMal",   DIGITAL_POLARITY_NORMAL  },
    {"INVerted", DIGITAL_POLARITY_INVERTED},
    SCPI_CHOICE_LIST_END
};

// Short forms of the enumerated values, used to answer a query
static const char* const FUNCTION_NAMES[]  = {"GPIO"};
static const char* const DIRECTION_NAMES[] = {"INP", "OUTP"};
static const char* const MODE_NAMES[]      = {"PUSH", "ODR"};
static const char* const PULL_NAMES[]      = {"NONE", "UP", "DOWN"};
static const char* const POLARITY_NAMES[]  = {"NORM", "INV"};

/**
 * @brief Decode <n> suffix of the PIN<n> header into a pin number.
 *
 * @param context
 * @param pin Decoded pin number, 1 to DIGITAL_PIN_COUNT.
 * @return scpi_bool_t FALSE if the suffix is out of range.
 */
static scpi_bool_t digital_pin_decode(scpi_t* context, int32_t* pin)
{
    int32_t numbers[1] = {0};

    // Pin 1 is used when the suffix is omitted
    SCPI_CommandNumbers(context, numbers, 1, 1);

    if (numbers[0] < 1 || numbers[0] > DIGITAL_PIN_COUNT) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return FALSE;
    }

    *pin = numbers[0];
    return TRUE;
}

/**
 * @brief Read an enumerated parameter of a PIN<n> command.
 *
 * @param context
 * @param choices Accepted values of the parameter.
 * @param pin Decoded pin number, 1 to DIGITAL_PIN_COUNT.
 * @param value Decoded value of the parameter.
 * @return scpi_bool_t FALSE if the suffix or the parameter is invalid.
 */
static scpi_bool_t digital_pin_choice_decode(
    scpi_t*                  context,
    const scpi_choice_def_t* choices,
    int32_t*                 pin,
    int32_t*                 value
)
{
    if (!digital_pin_decode(context, pin)) {
        return FALSE;
    }

    // Read first parameter if present
    if (!SCPI_ParamChoice(context, choices, value, TRUE)) {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Answer a query with a short form of an enumerated value.
 *
 * @param context
 * @param names Short forms of the enumerated values.
 * @param value Value to reply.
 */
static void digital_name_result(
    scpi_t* context, const char* const* names, int32_t value
)
{
    const char* name = names[value];
    SCPI_ResultCharacters(context, name, strlen(name));
}

scpi_result_t SCPI_DigitalCountQ(scpi_t* context)
{
    printf("DIG:COUN?\r\n");

    SCPI_ResultUInt32(context, DIGITAL_PIN_COUNT);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalPinFunctionQ(scpi_t* context)
{
    int32_t pin = 0;
    if (!digital_pin_decode(context, &pin)) {
        return SCPI_RES_ERR;
    }

    printf("DIG:PIN%ld:FUNC?\r\n", (long)pin);

    digital_name_result(context, FUNCTION_NAMES, DIGITAL_FUNCTION_GPIO);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalPinDirection(scpi_t* context)
{
    int32_t pin   = 0;
    int32_t value = 0;
    if (!digital_pin_choice_decode(context, DIRECTION_CHOICES, &pin, &value)) {
        return SCPI_RES_ERR;
    }

    // TODO: apply the direction to the pin
    printf("DIG:PIN%ld:DIR %s\r\n", (long)pin, DIRECTION_NAMES[value]);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalPinDirectionQ(scpi_t* context)
{
    int32_t pin = 0;
    if (!digital_pin_decode(context, &pin)) {
        return SCPI_RES_ERR;
    }

    printf("DIG:PIN%ld:DIR?\r\n", (long)pin);

    // TODO: reply the actual direction of the pin
    digital_name_result(context, DIRECTION_NAMES, DIGITAL_DIRECTION_INPUT);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalPinMode(scpi_t* context)
{
    int32_t pin   = 0;
    int32_t value = 0;
    if (!digital_pin_choice_decode(context, MODE_CHOICES, &pin, &value)) {
        return SCPI_RES_ERR;
    }

    // TODO: apply the output driver type to the pin
    printf("DIG:PIN%ld:MODE %s\r\n", (long)pin, MODE_NAMES[value]);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalPinModeQ(scpi_t* context)
{
    int32_t pin = 0;
    if (!digital_pin_decode(context, &pin)) {
        return SCPI_RES_ERR;
    }

    printf("DIG:PIN%ld:MODE?\r\n", (long)pin);

    // TODO: reply the actual output driver type of the pin
    digital_name_result(context, MODE_NAMES, DIGITAL_MODE_PUSHPULL);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalPinPull(scpi_t* context)
{
    int32_t pin   = 0;
    int32_t value = 0;
    if (!digital_pin_choice_decode(context, PULL_CHOICES, &pin, &value)) {
        return SCPI_RES_ERR;
    }

    // TODO: apply the pull resistor to the pin
    printf("DIG:PIN%ld:PULL %s\r\n", (long)pin, PULL_NAMES[value]);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalPinPullQ(scpi_t* context)
{
    int32_t pin = 0;
    if (!digital_pin_decode(context, &pin)) {
        return SCPI_RES_ERR;
    }

    printf("DIG:PIN%ld:PULL?\r\n", (long)pin);

    // TODO: reply the actual pull resistor of the pin
    digital_name_result(context, PULL_NAMES, DIGITAL_PULL_NONE);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalPinPolarity(scpi_t* context)
{
    int32_t pin   = 0;
    int32_t value = 0;
    if (!digital_pin_choice_decode(context, POLARITY_CHOICES, &pin, &value)) {
        return SCPI_RES_ERR;
    }

    // TODO: apply the polarity to the pin
    printf("DIG:PIN%ld:POL %s\r\n", (long)pin, POLARITY_NAMES[value]);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalPinPolarityQ(scpi_t* context)
{
    int32_t pin = 0;
    if (!digital_pin_decode(context, &pin)) {
        return SCPI_RES_ERR;
    }

    printf("DIG:PIN%ld:POL?\r\n", (long)pin);

    // TODO: reply the actual polarity of the pin
    digital_name_result(context, POLARITY_NAMES, DIGITAL_POLARITY_NORMAL);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalPinLevel(scpi_t* context)
{
    int32_t pin = 0;
    if (!digital_pin_decode(context, &pin)) {
        return SCPI_RES_ERR;
    }

    bool state = false;

    // Read first parameter if present
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    // TODO: apply the logical level to the pin
    printf("DIG:PIN%ld:LEV %u\r\n", (long)pin, (unsigned)state);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalPinLevelQ(scpi_t* context)
{
    int32_t pin = 0;
    if (!digital_pin_decode(context, &pin)) {
        return SCPI_RES_ERR;
    }

    printf("DIG:PIN%ld:LEV?\r\n", (long)pin);

    // TODO: reply the actual logical level on the pin
    SCPI_ResultBool(context, false);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalOutputData(scpi_t* context)
{
    uint32_t mask = 0;

    // Read first parameter if present, decimal or non-decimal
    if (!SCPI_ParamUInt32(context, &mask, TRUE)) {
        return SCPI_RES_ERR;
    }

    // TODO: apply the mask to all output pins at once
    printf("DIG:OUTP:DATA %lu\r\n", (unsigned long)mask);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalOutputDataQ(scpi_t* context)
{
    printf("DIG:OUTP:DATA?\r\n");

    // TODO: reply the levels set on all output pins
    SCPI_ResultUInt32(context, 0);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DigitalInputDataQ(scpi_t* context)
{
    printf("DIG:INP:DATA?\r\n");

    // TODO: reply the actual levels of all pins
    SCPI_ResultUInt32(context, 0);
    return SCPI_RES_OK;
}
