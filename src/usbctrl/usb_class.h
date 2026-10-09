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

// SCPI transport implemented by the USB class selected with USB_MODE
// (usbtmc/ or usbcdc/). The class moves messages between TinyUSB and scpi_io.

// Creates the RTOS objects and initializes scpi_io, call before the USB and
// SCPI threads start
void usb_class_init(void);
// Waits for USB events and runs the SCPI parser, call from the SCPI thread
void usb_class_scpi_iter(void);
