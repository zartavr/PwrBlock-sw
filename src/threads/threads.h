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


// USBPD one-shot init task
void thread_usbpd_init(void);

// TinyUSB device Thread
void thread_tud_init(void);
// SCPI parser Thread
void thread_scpi_init(void);

// Application Threads
void thread_indication_init(void);
void thread_regulator_init(void);
void thread_button_init(void);
void thread_monitor_init(void);
void thread_sys_status_init(void);
