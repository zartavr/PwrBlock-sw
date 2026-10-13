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
// In:  the class lends a message and posts events with the state transitions,
//      the SCPI thread waits on them (event flags).
// Out: the SCPI thread only sets flags and wakes the TinyUSB thread,
//      scpi_tp_dispatch in the tud_task loop picks them up and calls the
//      class callbacks there.
//
// So the class owns all transport state in the TinyUSB thread and the SCPI
// thread never calls TinyUSB.

#include "scpi_io.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Event flags of the transport, internal to scpi/: the USB class posts them
// with the scpi_tp_* transitions (scpi_app_tp.h)
// Events returned by scpi_app_evt_wait
enum
{
    SCPI_TP_MSG          = 1u << 0,  // a message is lent, see _req_get
    SCPI_TP_RESP_DONE    = 1u << 1,  // the response was sent or discarded
    SCPI_TP_INTERRUPTED  = 1u << 2,  // a message came before the response read
    SCPI_TP_UNTERMINATED = 1u << 3,  // the host asked for a missing response
    SCPI_TP_CLEAR        = 1u << 4,  // device clear or USB re-open
    SCPI_TP_ALL          = SCPI_TP_MSG | SCPI_TP_RESP_DONE | SCPI_TP_INTERRUPTED
                  | SCPI_TP_UNTERMINATED | SCPI_TP_CLEAR,
};

// Creates the RTOS objects, call before the scheduler starts
void scpi_app_init(void);
// Blocks until the transport has events, returns SCPI_TP_* (0 on error)
uint32_t scpi_app_evt_wait(void);

// The message lent with SCPI_TP_MSG, valid until scpi_app_resp_set.
// end: the message ended with IEEE 488.2 END; overrun: it did not fit the
// transport buffer and is dropped.
size_t scpi_app_req_get(const uint8_t** data, bool* end, bool* overrun);
// The lent message is consumed: hands over its response (len 0 if none) and
// lets the transport accept the next message. resp stays valid until
// SCPI_TP_RESP_DONE or SCPI_TP_CLEAR.
void scpi_app_resp_set(const uint8_t* resp, size_t len);

// SCPI_TP_CLEAR is handled: input and response are dropped
void scpi_app_clear_done(void);
// Status from scpi_io_status, the transport composes the status byte for the
// host from it (USB488 READ_STATUS_BYTE)
void scpi_app_status_set(uint16_t status);
