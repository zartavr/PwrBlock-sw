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

// I2C address
#define LM51772_ADDR (0x6AU << 1)

// Constrains
#define LM51772_HI_V_MIN_VOLTAGE_MV  (3300)
#define LM51772_HI_V_MAX_VOLTAGE_MV  (48000)
#define LM51772_HI_V_VOLTAGE_STEP_MV (20)

#define LM51772_LO_V_MIN_VOLTAGE_MV  (1000)
#define LM51772_LO_V_MAX_VOLTAGE_MV  (24000)
#define LM51772_LO_V_VOLTAGE_STEP_MV (10)

#define LM51772_CURRENT_LIM_STEP_MA (50)

#define LM51772_CURRENT_MIN_MA (500)
#define LM51772_CURRENT_MAX_MA (7000)


#define LM51772_DEFAULT_V_OUT (LM51772_HI_V_MIN_VOLTAGE_MV)
#define LM51772_DEFAULT_I_OUT (LM51772_CURRENT_MIN_MA)

// Current sense amplifier transconductance for I2C interface disabled
#define LM51772_GM_ISET (0.001F)

/**
 * Voltage Divider Circuit:
 *
 *          Vin (Input Voltage)
 *           |
 *           |
 *         [ R1 ]  <-- Upper Resistor
 *           |
 *           +------- Vout (Output Voltage)
 *           |
 *         [ R2 ]  <-- Lower Resistor
 *           |
 *           |
 *          GND (Ground)
 *
 * Formula: Vout = Vin * (R2 / (R1 + R2))
 */
#define LM51772_EXT_ILIM_DAC_DIVIDER_R1 (160000.0F)
#define LM51772_EXT_ILIM_DAC_DIVIDER_R2 (68000.0F)
#define LM51772_EXT_ILIM_DAC_DIVIDER_RATIO                                     \
    (LM51772_EXT_ILIM_DAC_DIVIDER_R2 /                                         \
     (LM51772_EXT_ILIM_DAC_DIVIDER_R1 + LM51772_EXT_ILIM_DAC_DIVIDER_R2))

// Resistor in current source circuit
#define LM51772_EXT_ILIM_CURRENT_SOURCE_R (22000.0F)

// Shunt resistor value
#define LM51772_EXT_ILIM_SENS_SHUNT_0A3_R (0.220F)
#define LM51772_EXT_ILIM_SENS_SHUNT_3A_R  (0.020F)

#define LM51772_EXT_ILIM_RATIO_COEFF                                           \
    ((LM51772_EXT_ILIM_CURRENT_SOURCE_R * LM51772_GM_ISET /                    \
      LM51772_EXT_ILIM_DAC_DIVIDER_RATIO))
