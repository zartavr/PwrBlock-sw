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

#include "scpi_io.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Largest message a transport lends: the scpi_io input buffer
#define SCPI_TP_MSG_SIZE SCPI_INPUT_BUFFER_LENGTH

// Events returned by scpi_transport_wait
#define SCPI_TP_MSG          (1u << 0)  // a message is lent, see scpi_transport_msg
#define SCPI_TP_RESP_DONE    (1u << 1)  // the response was sent or discarded
#define SCPI_TP_INTERRUPTED  (1u << 2)  // a message came before the response read
#define SCPI_TP_UNTERMINATED (1u << 3)  // the host asked for a missing response
#define SCPI_TP_CLEAR        (1u << 4)  // device clear or USB re-open
#define SCPI_TP_ALL                                                            \
    (SCPI_TP_MSG | SCPI_TP_RESP_DONE | SCPI_TP_INTERRUPTED                     \
     | SCPI_TP_UNTERMINATED | SCPI_TP_CLEAR)

//--------------------------------------------------------------------+
// SCPI thread
//--------------------------------------------------------------------+

// Creates the RTOS objects, call before the scheduler starts
void     scpi_transport_init(void);
// Blocks until the transport has events, returns SCPI_TP_* (0 on error)
uint32_t scpi_transport_wait(void);

// The message lent with SCPI_TP_MSG, valid until scpi_transport_publish.
// end: the message ended with IEEE 488.2 END; overrun: it did not fit the
// transport buffer and is dropped.
size_t scpi_transport_msg(const uint8_t** data, bool* end, bool* overrun);
// The lent message is consumed: hands over its response (len 0 if none) and
// lets the transport accept the next message. resp stays valid until
// SCPI_TP_RESP_DONE or SCPI_TP_CLEAR.
void   scpi_transport_publish(const uint8_t* resp, size_t len);

// SCPI_TP_CLEAR is handled: input and response are dropped
void scpi_transport_clear_done(void);
// Status byte the transport reports to the host (USB488 READ_STATUS_BYTE)
void scpi_transport_stb(uint8_t stb);

//--------------------------------------------------------------------+
// USB class, TinyUSB thread
//--------------------------------------------------------------------+

// Lends msg to the SCPI thread until scpi_transport_published_cb or, if a
// clear comes meanwhile, scpi_transport_cleared_cb. Posts SCPI_TP_MSG.
void    scpi_transport_lend(const uint8_t* msg, size_t len, bool end, bool overrun);
// Posts SCPI_TP_* events to the SCPI thread
void    scpi_transport_post(uint32_t evt);
// Status byte last reported by the SCPI thread, for the host
uint8_t scpi_transport_host_stb(void);

// Called by the tud_task loop after every tud_task_ext: hands what the SCPI
// thread published to the class callbacks below. Returns the time in ms to
// wait for the next USB event before polling again: 1 while the class waits
// for the SCPI thread, UINT32_MAX otherwise.
uint32_t scpi_transport_poll(void);

// Implemented by the USB class, called from scpi_transport_poll:
// The lent message is consumed. resp stays valid until the class posts
// SCPI_TP_RESP_DONE or SCPI_TP_CLEAR; len is 0 if there is no response.
void scpi_transport_published_cb(const uint8_t* resp, size_t len);
// SCPI_TP_CLEAR is handled by the SCPI thread, the lent message is returned
void scpi_transport_cleared_cb(void);
