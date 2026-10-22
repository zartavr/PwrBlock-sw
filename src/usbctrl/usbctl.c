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

#include "usbctl.h"

#include <cmsis_os2.h>
#include <device/dcd.h>
#include <stdbool.h>
#include <tusb.h>

#include "config/vbus_conf.h"
#include "main.h"
#include "middleware/adc_mgr/adc_mgr.h"

// PC VBUS detection thresholds with hysteresis in Volts
#define USBCTL_VBUS_ON_V  (4.0F)
#define USBCTL_VBUS_OFF_V (3.5F)

#define USBCTL_VBUS_PC_K ((float)(VBUS_PC_RA + VBUS_PC_RB) / (float)VBUS_PC_RB)

typedef struct
{
    UsbctlPort port;
    bool       pc_vbus;
    uint32_t   poll_tick;
} UsbctlHandle;

static UsbctlHandle usbctl;

// Private function prototypes
static void mux_set(UsbctlPort port);
static bool pc_vbus_present(bool prev);

void usbctl_init(void)
{
    usbctl.pc_vbus   = pc_vbus_present(false);
    usbctl.poll_tick = osKernelGetTickCount();

    mux_set(usbctl.pc_vbus ? USBCTL_PORT_PC : USBCTL_PORT_PLD);
}

void usbctl_task_iter(uint32_t period_ms)
{
    uint32_t now = osKernelGetTickCount();

    if ((now - usbctl.poll_tick) < period_ms) {
        return;
    }
    usbctl.poll_tick = now;

    bool pc_vbus = pc_vbus_present(usbctl.pc_vbus);

    if (pc_vbus == usbctl.pc_vbus) {
        return;
    }
    usbctl.pc_vbus = pc_vbus;

    UsbctlPort port = pc_vbus ? USBCTL_PORT_PC : USBCTL_PORT_PLD;

    if (port != usbctl.port) {
        mux_set(port);
    }
}

UsbctlPort usbctl_port_get(void)
{
    return usbctl.port;
}

static void mux_set(UsbctlPort port)
{
    // Mux code SW1,SW2: 01 - PLD, 11 - PC
    GPIO_PinState sw1 =
        (port == USBCTL_PORT_PC) ? GPIO_PIN_SET : GPIO_PIN_RESET;

    HAL_GPIO_WritePin(USB_SW1_GPIO_Port, USB_SW1_Pin, sw1);
    HAL_GPIO_WritePin(USB_SW2_GPIO_Port, USB_SW2_Pin, GPIO_PIN_SET);

    usbctl.port = port;
}

static bool pc_vbus_present(bool prev)
{
    float vbus = adc_manager_read(ADC_MNG_CH_VUSB_PC_ADC) * USBCTL_VBUS_PC_K;

    return vbus >= (prev ? USBCTL_VBUS_OFF_V : USBCTL_VBUS_ON_V);
}
