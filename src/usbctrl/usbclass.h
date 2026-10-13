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

// The USB class selected with USB_MODE (usbtmc_app.c or usbcdc_app.c)

// Takes the results of the SCPI thread (scpi_tp_dispatch) and acts on them.
// Called by tud_app_task after every wake, in the TinyUSB thread.
void usbclass_dispatch(void);
