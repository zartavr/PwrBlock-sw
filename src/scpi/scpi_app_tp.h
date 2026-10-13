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
// Out: the SCPI thread only sets flags and wakes the TinyUSB thread, the
//      class picks them up there with scpi_tp_dispatch.
//
// So the class owns all transport state in the TinyUSB thread and the SCPI
// thread never calls TinyUSB.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Largest message a transport lends: the scpi_io input buffer
#define SCPI_TP_MSG_SIZE 256

// Message exchange with the SCPI thread. The state changes in the TinyUSB
// thread only: by the scpi_tp_* calls of the class and, for the results of
// the SCPI thread, by scpi_tp_dispatch.
//
//   IDLE --lend--> PARSING --published--> RESPONSE --resp_done-->  IDLE
//                          (len 0: IDLE)           --interrupted-->
//   any --clear--> CLEARING --cleared--> IDLE
typedef enum
{
    // Nothing lent, the class receives the next message
    SCPI_TP_STATE_IDLE,
    // A message is lent to the SCPI thread
    SCPI_TP_STATE_PARSING,
    // The class holds a response for the host (MAV)
    SCPI_TP_STATE_RESPONSE,
    // Device clear, waits for the SCPI thread. A message lent before stays
    // lent, its response is dropped.
    SCPI_TP_STATE_CLEARING,
} ScpiTpState;

ScpiTpState scpi_tp_state(void);

// IDLE -> PARSING: lends msg to the SCPI thread until the state leaves
// PARSING, or CLEARING if a clear comes meanwhile. end: the message ended
// with IEEE 488.2 END; overrun: it did not fit and is dropped with -363.
// Returns false, nothing lent, outside IDLE.
bool scpi_tp_msg_lend(const uint8_t* msg, size_t len, bool end, bool overrun);
// RESPONSE -> IDLE: the response was sent or aborted
void scpi_tp_resp_done(void);
// RESPONSE -> IDLE: a new message came before the response was read, the
// response is dropped with -410 (IEEE 488.2). No-op in the other states, so
// the class may call it at every message start.
void scpi_tp_interrupted(void);
// IDLE: the host asked for a response that does not exist, -420
void scpi_tp_unterminated(void);
// Any -> CLEARING: IEEE 488.2 device clear or a new host session, the input
// and the response are dropped, the status is kept
void scpi_tp_clear(void);

// Response for the host, valid until the state leaves RESPONSE. 0 and NULL
// in the other states.
size_t scpi_tp_resp_get(const uint8_t** data);

// IEEE 488.2 status byte for the host: the status last published by the
// SCPI thread with MAV (SCPI_TP_STATE_RESPONSE) and MSS
uint8_t scpi_tp_stb_get(void);

// Wakes the thread that runs scpi_tp_dispatch, called from the SCPI thread
// after every event it handled (scpi_app_tud_wake)
typedef void (*ScpiTpWakeFn)(void);
// Registers the wake, call once before the USB stack starts
void scpi_tp_wake_set(ScpiTpWakeFn wake);

// Called by the class after every wake of the TinyUSB thread: takes what the
// SCPI thread left. PARSING -> RESPONSE, or IDLE without a response: the lent
// message is consumed. CLEARING -> IDLE: the clear is handled, a lent message
// is returned. Returns true if the state changed: the class receives again
// and, in RESPONSE, has a response to send.
bool scpi_tp_dispatch(void);
