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

// Transport that carries SCPI messages to the SCPI thread and its responses
// back, implemented by usbctrl/scpi_transport.c over the USB class selected
// with USB_MODE. The class owns the transport state in the TinyUSB thread,
// the SCPI thread only waits for events, reads the lent message and hands
// results back. Every function except scpi_transport_init is called from the
// SCPI thread only.

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
