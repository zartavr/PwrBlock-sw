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

// IEEE 488.2 message exchange between the USBTMC transport (TinyUSB thread)
// and scpi_io (SCPI thread). The transport frames messages and responses, the
// exchange hands them between the threads and decides on MAV, -410, -420 and
// device clear. The SCPI thread side is usb_class.h.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Largest message the transport collects, the scpi_io input buffer size
#define EXCH_MSG_SIZE 256u

// TinyUSB thread: a new message starts. A response not read yet is dropped
// with -410.
void exch_msg_start(void);
// TinyUSB thread: a whole message is received. msg is lent to the SCPI
// thread until it calls usbtmc_rx_resume. end: the message ended with EOM;
// overrun: it did not fit and is dropped with -363.
void exch_msg_received(const uint8_t* msg, size_t len, bool end, bool overrun);

// TinyUSB thread: the response to send, valid until exch_response_done. 0 if
// there is none: the host asked for a missing response, -420.
size_t exch_response(const uint8_t** data);
// TinyUSB thread: the response is sent or aborted
void   exch_response_done(void);

// TinyUSB thread: INITIATE_CLEAR, drops the message being parsed and the
// response
void exch_clear(void);
// TinyUSB thread: CHECK_CLEAR_STATUS, true once the SCPI thread has finished
// the command it was parsing and dropped its buffers
bool exch_clear_done(void);

// TinyUSB thread: status byte for READ_STATUS_BYTE
uint8_t exch_stb(void);

// Provided by the transport, called from the SCPI thread: the message lent by
// exch_msg_received is consumed, accept the next one
void usbtmc_rx_resume(void);
