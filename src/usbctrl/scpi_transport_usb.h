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

// USB class side of the SCPI transport. scpi_transport.c implements
// scpi/scpi_transport.h for the SCPI thread and hands everything to the USB
// class selected with USB_MODE (usbtmc/ or usbcdc/) in the TinyUSB thread.
// Every function here is called in the TinyUSB thread only.

#include "scpi/scpi_transport.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Lends msg to the SCPI thread until scpi_transport_published_cb or, if a
// clear comes meanwhile, scpi_transport_cleared_cb. Posts SCPI_TP_MSG.
void    scpi_transport_lend(const uint8_t* msg, size_t len, bool end, bool overrun);
// Posts SCPI_TP_* events to the SCPI thread
void    scpi_transport_post(uint32_t evt);
// Status byte last reported by the SCPI thread, for the host
uint8_t scpi_transport_host_stb(void);

// Implemented by the USB class, deferred from the SCPI thread:
// The lent message is consumed. resp stays valid until the class posts
// SCPI_TP_RESP_DONE or SCPI_TP_CLEAR; len is 0 if there is no response.
void scpi_transport_published_cb(const uint8_t* resp, size_t len);
// SCPI_TP_CLEAR is handled by the SCPI thread, the lent message is returned
void scpi_transport_cleared_cb(void);
