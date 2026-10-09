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

// ST USBPD Threads
#define DPM_STACK_SIZE_ADDON_FOR_CMSIS 4

#define OS_PE_PRIORITY   osPriorityAboveNormal
#define OS_PE_STACK_SIZE (500 * DPM_STACK_SIZE_ADDON_FOR_CMSIS)

#define OS_CAD_PRIORITY   osPriorityRealtime
#define OS_CAD_STACK_SIZE (300 * DPM_STACK_SIZE_ADDON_FOR_CMSIS)

// USBPD one-shot init task
#define OS_USBPD_PRIORITY   osPriorityHigh
#define OS_USBPD_STACK_SIZE 1024
#define OS_USBPD_NAME       "usbpd_init_task"

// TinyUSB Threads
#define OS_TUD_PRIORITY   osPriorityNormal
#define OS_TUD_STACK_SIZE 2048
#define OS_TUD_NAME       "tud_app_task"

#define OS_TMC_PRIORITY    osPriorityBelowNormal
#define OS_SCPI_STACK_SIZE 1024
#define OS_TMC_NAME        "scpi_parser_app_task"

// Application Threads
#define OS_IND_PRIORITY   osPriorityBelowNormal
#define OS_IND_STACK_SIZE 512
#define OS_IND_NAME       "led_app_task"

#define OS_REGULATOR_PRIORITY   osPriorityNormal
#define OS_REGULATOR_STACK_SIZE 512
#define OS_REGULATOR_NAME       "regulator_app_task"

#define OS_BUTTON_PRIORITY   osPriorityNormal
#define OS_BUTTON_STACK_SIZE 256
#define OS_BUTTON_NAME       "button_app_task"

#define OS_MONITOR_PRIORITY   osPriorityNormal
#define OS_MONITOR_STACK_SIZE 512
#define OS_MONITOR_NAME       "monitor_app_task"

#define OS_SYS_STATUS_PRIORITY   osPriorityNormal
#define OS_SYS_STATUS_STACK_SIZE 512
#define OS_SYS_STATUS_NAME       "sys_status_app_task"
