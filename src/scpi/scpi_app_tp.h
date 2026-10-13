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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Largest message a transport lends: the scpi_io input buffer
#define SCPI_TP_MSG_SIZE 256

// Message exchange with the SCPI thread. The state changes in the TinyUSB
// thread only: by the scpi_tp_* calls below and by scpi_tp_dispatch before it
// calls the class callbacks.
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

// IDLE -> PARSING: lends msg to the SCPI thread until ScpiTpRespCb or, if a
// clear comes meanwhile, ScpiTpClearedCb. end: the message ended with
// IEEE 488.2 END; overrun: it did not fit and is dropped with -363.
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

// IEEE 488.2 status byte for the host: the status last published by the
// SCPI thread with MAV (SCPI_TP_STATE_RESPONSE) and MSS
uint8_t scpi_tp_stb_get(void);

// PARSING -> RESPONSE, or IDLE if len is 0: the lent message is consumed.
// resp stays valid until scpi_tp_resp_done, scpi_tp_interrupted or
// scpi_tp_clear.
typedef void (*ScpiTpRespCb)(const uint8_t* resp, size_t len);
// CLEARING -> IDLE: the clear is handled by the SCPI thread, a lent message
// is returned
typedef void (*ScpiTpClearedCb)(void);

// Registers the class callbacks, call once from the class init before
// scpi_tp_dispatch. Both are optional, a NULL one is skipped.
void scpi_tp_cb_set(ScpiTpRespCb resp_cb, ScpiTpClearedCb cleared_cb);

// Wakes the thread that runs scpi_tp_dispatch, called from the SCPI thread
// each time it leaves something for the dispatch
typedef void (*ScpiTpWakeFn)(void);
// Registers the wake, call once before the USB stack starts
void scpi_tp_wake_set(ScpiTpWakeFn wake);

// Called by the tud_task loop after every wake: hands what the SCPI thread
// published to the registered class callbacks, in the calling thread
void scpi_tp_dispatch(void);
