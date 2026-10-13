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

// Transport that carries SCPI messages between the USB class selected with
// USB_MODE (TinyUSB thread) and the SCPI thread.
//
// In:  the class lends a message and posts events, the SCPI thread waits on
//      them (event flags).
// Out: the SCPI thread only sets flags, scpi_transport_poll in the tud_task
//      loop picks them up and calls the class callbacks in the TinyUSB thread.
//
// So the class owns all transport state in the TinyUSB thread and the SCPI
// thread never calls TinyUSB.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Largest message a transport lends: the scpi_io input buffer
#define SCPI_TP_MSG_SIZE 256

// Events the USB class posts to the SCPI thread with scpi_transport_evt_set.
// A message is posted with scpi_transport_req_set.
typedef enum
{
    SCPI_TP_EVT_RESP_DONE,     // the response was sent or discarded
    SCPI_TP_EVT_INTERRUPTED,   // a message came before the response read
    SCPI_TP_EVT_UNTERMINATED,  // the host asked for a missing response
    SCPI_TP_EVT_CLEAR,         // device clear or USB re-open
} ScpiTpEvt;

// Lends msg to the SCPI thread until scpi_transport_resp_cb or, if a clear
// comes meanwhile, scpi_transport_cleared_cb.
void scpi_transport_req_set(
    const uint8_t* msg, size_t len, bool end, bool overrun
);
// Posts the event to the SCPI thread
void scpi_transport_evt_set(ScpiTpEvt evt);
// Status byte last reported by the SCPI thread, for the host
uint8_t scpi_transport_stb_get(void);

// Called by the tud_task loop after every tud_task_ext: hands what the SCPI
// thread published to the class callbacks below. Returns the time in ms to
// wait for the next USB event before polling again: 1 while the class waits
// for the SCPI thread, UINT32_MAX otherwise.
uint32_t scpi_transport_poll(void);

// Implemented by the USB class, called from scpi_transport_poll:
// The lent message is consumed. resp stays valid until the class posts
// SCPI_TP_EVT_RESP_DONE or SCPI_TP_EVT_CLEAR; len is 0 if there is no
// response.
void scpi_transport_resp_cb(const uint8_t* resp, size_t len);
// The clear is handled by the SCPI thread, the lent message is returned
void scpi_transport_cleared_cb(void);
