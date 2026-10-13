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

// IEEE 488.2 message exchange over libscpi for the SCPI task. The task sees
// messages, the response and the status byte, never libscpi itself. Every
// function except scpi_io_init and scpi_io_stb_compose is called from the
// SCPI thread only.
//
// IEEE 488.2 status byte: the status registers are libscpi's, changed in the
// SCPI thread only. MAV reflects the output queue in real time, so only the
// transport holding the response knows it, and MSS depends on MAV. The
// transport gets the rest with scpi_io_status and composes the byte itself.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCPI_INPUT_BUFFER_LENGTH  256
#define SCPI_OUTPUT_BUFFER_LENGTH 256

// Initializes libscpi, call before the scheduler starts
void scpi_io_init(void);

// Feeds input, commands are parsed on each newline. A message is limited to
// SCPI_INPUT_BUFFER_LENGTH - 1 bytes, a longer one is dropped with -363.
void scpi_io_input(const char* data, size_t len);
// IEEE 488.2 END (USBTMC EOM): parses the rest of the message that has no
// trailing newline
void scpi_io_input_end(void);
// A message longer than the class buffer was dropped: -363
void scpi_io_input_overrun(void);

// Response collected since the last scpi_io_input. The data stays valid until
// scpi_io_response_drop or the next input.
size_t scpi_io_response(const uint8_t** data);
// The response is sent or discarded
void scpi_io_response_drop(void);

// IEEE 488.2 device clear: drops the input and the response, keeps the status
// registers and the error queue
void scpi_io_device_clear(void);
// A new message came before the response was read: drop it, -410
void scpi_io_query_interrupted(void);
// The host asked for a response that does not exist: -420
void scpi_io_query_unterminated(void);

// Status for the transport: STB summary bits without MAV and MSS in the low
// byte, SRE in the high one. 16 bits so a single store publishes both.
uint16_t scpi_io_status(void);
// IEEE 488.2 status byte from scpi_io_status and MAV, MSS in bit 6. Pure,
// callable from any thread.
uint8_t scpi_io_stb_compose(uint16_t status, bool mav);
