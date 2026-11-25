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

#include "lm51772.h"
#include "lm51772_conf.h"
#include "main.h"

#include <stm32g0xx_hal.h>

bool lm51772_read(void* bus_handler, uint8_t reg_addr, uint8_t* rx_buff)
{
    return HAL_I2C_Mem_Read_IT(
               (I2C_HandleTypeDef*)bus_handler,
               LM51772_ADDR,
               reg_addr,
               I2C_MEMADD_SIZE_8BIT,
               rx_buff,
               1
           ) == HAL_OK;
}

bool lm51772_write(void* bus_handler, uint8_t reg_addr, uint8_t* tx_buff)
{
    return HAL_I2C_Mem_Write_IT(
               (I2C_HandleTypeDef*)bus_handler,
               LM51772_ADDR,
               reg_addr,
               I2C_MEMADD_SIZE_8BIT,
               tx_buff,
               1
           ) == HAL_OK;
}

void lm51772_on(void)
{
    HAL_GPIO_WritePin(LM_EN_GPIO_Port, LM_EN_Pin, GPIO_PIN_SET);
}

void lm51772_off(void)
{
    HAL_GPIO_WritePin(LM_EN_GPIO_Port, LM_EN_Pin, GPIO_PIN_RESET);
}

void output_en_mcu_set(bool enabled)
{
    if (enabled) {
        HAL_GPIO_WritePin(OUT_EN_MCU_GPIO_Port, OUT_EN_MCU_Pin, GPIO_PIN_SET);
    }
    else {
        HAL_GPIO_WritePin(OUT_EN_MCU_GPIO_Port, OUT_EN_MCU_Pin, GPIO_PIN_RESET);
    }
}

void converter_sensor_set_resolution(bool high)
{
    if (high) {
        HAL_GPIO_WritePin(SNS_SW_GPIO_Port, SNS_SW_Pin, GPIO_PIN_SET);
    }
    else {
        HAL_GPIO_WritePin(SNS_SW_GPIO_Port, SNS_SW_Pin, GPIO_PIN_RESET);
    }
}
