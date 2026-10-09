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

#include "usbtmc_exchange.h"

#include "scpi/scpi_io.h"
#include "usbctrl/usb_class.h"
#include <cmsis_os2.h>

_Static_assert(
    EXCH_MSG_SIZE == SCPI_INPUT_BUFFER_LENGTH,
    "the transport message buffer must match the scpi_io input buffer"
);

// Events from the TinyUSB thread to the SCPI thread. scpi_io is called only
// by the SCPI thread.
#define EVT_RX           (1u << 0)  // a whole message is lent in rx_msg
#define EVT_TX_DONE      (1u << 1)  // the response was sent or dropped
#define EVT_INTERRUPTED  (1u << 2)  // a command came before the response read
#define EVT_UNTERMINATED (1u << 3)  // the host asked for a missing response
#define EVT_CLEAR        (1u << 4)  // INITIATE_CLEAR
#define EVT_ALL                                                                \
    (EVT_RX | EVT_TX_DONE | EVT_INTERRUPTED | EVT_UNTERMINATED | EVT_CLEAR)

static osEventFlagsId_t tmc_evt;

// Message: set by the TinyUSB thread before EVT_RX, read by the SCPI thread.
// Bulk-OUT stays NAKed until usbtmc_rx_resume, so they never overlap.
static const uint8_t* rx_msg;
static size_t         rx_msg_len;
static bool           rx_msg_end;
static bool           rx_msg_overrun;

// Response: owned by scpi_io, fixed by the SCPI thread after parsing, sent by
// the TinyUSB thread. out_len is 0 if there is no response to send.
static const uint8_t*  tx_buf;
static volatile size_t out_len;

static volatile uint8_t stb_snapshot;  // STB for READ_STATUS_BYTE
static volatile bool    clear_done;

void usb_class_init(void)
{
    tmc_evt = osEventFlagsNew(NULL);
    scpi_io_init();
}

static void command_parse(void)
{
    if (rx_msg_overrun) {
        scpi_io_input_overrun();
    }
    else {
        scpi_io_input((const char*)rx_msg, rx_msg_len);
        if (rx_msg_end) {
            // EOM is the IEEE 488.2 END: it terminates a message without a
            // newline
            scpi_io_input_end();
        }
    }

    if (osEventFlagsGet(tmc_evt) & EVT_CLEAR) {
        // Device clear came while parsing: the next iteration drops the
        // response, and the clear sequence re-arms bulk-OUT
        return;
    }

    out_len = scpi_io_response(&tx_buf);
    usbtmc_rx_resume();
}

void usb_class_scpi_iter(void)
{
    uint32_t evt =
        osEventFlagsWait(tmc_evt, EVT_ALL, osFlagsWaitAny, osWaitForever);
    if (evt & osFlagsError) {
        return;
    }

    if (evt & EVT_CLEAR) {
        // IEEE 488.2 device clear: input and output are dropped, status
        // registers and the error queue are kept
        scpi_io_device_clear();
        evt &= ~EVT_RX;
    }
    if (evt & EVT_TX_DONE) {
        scpi_io_response_drop();
    }
    if (evt & EVT_INTERRUPTED) {
        scpi_io_query_interrupted();
    }
    if (evt & EVT_UNTERMINATED) {
        scpi_io_query_unterminated();
    }
    if (evt & EVT_RX) {
        command_parse();
    }

    stb_snapshot = scpi_io_stb();
    if (evt & EVT_CLEAR) {
        clear_done = true;
    }
}

void exch_msg_start(void)
{
    if (out_len > 0) {
        // A new message before the response is read discards the response
        // and reports Query INTERRUPTED (IEEE 488.2)
        out_len = 0;
        osEventFlagsSet(tmc_evt, EVT_INTERRUPTED);
    }
}

void exch_msg_received(const uint8_t* msg, size_t len, bool end, bool overrun)
{
    rx_msg         = msg;
    rx_msg_len     = len;
    rx_msg_end     = end;
    rx_msg_overrun = overrun;
    osEventFlagsSet(tmc_evt, EVT_RX);
}

size_t exch_response(const uint8_t** data)
{
    if (out_len == 0) {
        // Nothing to send: Query UNTERMINATED (IEEE 488.2)
        osEventFlagsSet(tmc_evt, EVT_UNTERMINATED);
        return 0;
    }
    *data = tx_buf;
    return out_len;
}

void exch_response_done(void)
{
    out_len = 0;
    osEventFlagsSet(tmc_evt, EVT_TX_DONE);
}

void exch_clear(void)
{
    out_len    = 0;
    clear_done = false;
    osEventFlagsSet(tmc_evt, EVT_CLEAR);
}

bool exch_clear_done(void)
{
    return clear_done;
}

uint8_t exch_stb(void)
{
    return stb_snapshot;
}
