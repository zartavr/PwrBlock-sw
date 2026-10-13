/*
 * Copyright (c) 2026 Everypin
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
 *
 * --- NOTICE OF MODIFICATION ---
 * This file has been modified by Everypin in 2026.
 * Original code is licensed under the MIT License (see below).
 * Modifications are licensed under the Apache License 2.0.
 *
 * --- ORIGINAL MIT LICENSE NOTE ---
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Nathan Conrad
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// USBTMC transport: frames messages and responses over TinyUSB and lends
// messages to the SCPI thread through scpi_tp, which holds the exchange state.
// All of it is changed in the TinyUSB thread only, the results of the SCPI
// thread come in resp_cb and cleared_cb, called from scpi_tp_dispatch.

#include "scpi/scpi_app_tp.h"
#include "tusb.h"
#include "usbctrl/usbclass.h"
#include <string.h>

#if (CFG_TUD_USBTMC_ENABLE_488)
static const usbtmc_response_capabilities_488_t
#else
static const usbtmc_response_capabilities_t
#endif
    tud_usbtmc_app_capabilities = {
        .USBTMC_status = USBTMC_STATUS_SUCCESS,
        .bcdUSBTMC     = USBTMC_VERSION,
        .bmIntfcCapabilities =
            {.listenOnly = 0, .talkOnly = 0, .supportsIndicatorPulse = 1},
        .bmDevCapabilities = {.canEndBulkInOnTermChar = 0},

#if (CFG_TUD_USBTMC_ENABLE_488)
        .bcdUSB488 = USBTMC_488_VERSION,
        .bmIntfcCapabilities488 =
            {.supportsTrigger = 0, .supportsREN_GTL_LLO = 0, .is488_2 = 1},
        .bmDevCapabilities488 =
            {
                                  .SCPI = 1,
                                  .SR1  = 0,
                                  .RL1  = 0,
                                  .DT1  = 0,
                                  }
#endif
};

// host centric naming: out - PC to device, in - device to PC

// Message: collected over the transfers up to EOM, then lent to the SCPI
// thread (SCPI_TP_STATE_PARSING) until it publishes the response or a clear
// completes. Bulk-OUT stays NAKed meanwhile.
typedef struct
{
    uint8_t buf[SCPI_TP_MSG_SIZE];
    size_t  len;
    bool    overrun;  // the message did not fit, dropped up to its end
    bool    eom;      // the transfer being received ends with EOM
} TmcRx;

// Response published by the SCPI thread, valid in SCPI_TP_STATE_RESPONSE, and
// its progress over bulk-IN
typedef struct
{
    const uint8_t* data;
    size_t         len;
    size_t         sent;   // bytes already sent
    size_t         chunk;  // bytes in the bulk-IN transfer in flight
} TmcResp;

// All of it is changed in the TinyUSB thread only
static TmcRx   rx;
static TmcResp resp;

// Accept the next transfer unless rx.buf is lent or a clear is in progress (a
// stray CLEAR_FEATURE or CHECK_ABORT must not let a new message overwrite it)
static void rx_arm(void)
{
    ScpiTpState state = scpi_tp_state();

    if ((state == SCPI_TP_STATE_IDLE) || (state == SCPI_TP_STATE_RESPONSE)) {
        tud_usbtmc_start_bus_read();
    }
}

// Drop the message being received
static void rx_reset(void)
{
    rx.len     = 0;
    rx.overrun = false;
}

static void tx_reset(void)
{
    resp.sent  = 0;
    resp.chunk = 0;
}

// The response is sent or aborted
static void resp_drop(void)
{
    tx_reset();
    scpi_tp_resp_done();
}

// IEEE 488.2 device clear: input and response are dropped here at once, the
// SCPI thread drops its own, cleared_cb follows. rx.buf stays lent, if it is,
// until then.
static void clear_start(void)
{
    rx_reset();
    tx_reset();
    scpi_tp_clear();
}

//--------------------------------------------------------------------+
// scpi_app_tp.h callbacks, registered in usb_class_init and called from
// scpi_tp_dispatch
//--------------------------------------------------------------------+

static void resp_cb(const uint8_t* data, size_t len)
{
    resp.data = data;
    resp.len  = len;
    tx_reset();
    rx_reset();
    rx_arm();
}

static void cleared_cb(void)
{
    rx_reset();
    // Arms bulk-OUT after a re-open. After INITIATE_CLEAR TinyUSB refuses
    // until the host clears the bulk-OUT halt.
    rx_arm();
}

void usbclass_init(void)
{
    scpi_tp_cb_set(resp_cb, cleared_cb);
}

//--------------------------------------------------------------------+
// TinyUSB callbacks
//--------------------------------------------------------------------+

void tud_usbtmc_open_cb(uint8_t interface_id)
{
    (void)interface_id;
    // The host starts over: the SCPI thread may still parse a message from
    // before, bulk-OUT is armed once it has dropped it
    clear_start();
}

#if (CFG_TUD_USBTMC_ENABLE_488)
const usbtmc_response_capabilities_488_t*
#else
const usbtmc_response_capabilities_t*
#endif
tud_usbtmc_get_capabilities_cb()
{
    return &tud_usbtmc_app_capabilities;
}

bool tud_usbtmc_msgBulkOut_start_cb(
    const usbtmc_msg_request_dev_dep_out* msgHeader
)
{
    rx.eom = msgHeader->bmTransferAttributes.EOM;
    tx_reset();
    // A new message before the response is read discards the response and
    // reports Query INTERRUPTED (IEEE 488.2). A message is lent only at EOM,
    // so a response is pending only before its first transfer.
    scpi_tp_interrupted();
    return true;
}

bool tud_usbtmc_msg_data_cb(void* data, size_t len, bool transfer_complete)
{
    if (!rx.overrun) {
        if (len > (sizeof(rx.buf) - rx.len)) {
            // Input buffer overrun: the rest of the message is dropped, the
            // SCPI thread reports it
            rx.overrun = true;
        }
        else {
            memcpy(&rx.buf[rx.len], data, len);
            rx.len += len;
        }
    }

    // The message ends at EOM (IEEE 488.2 END)
    if (transfer_complete && rx.eom) {
        // Bulk-OUT stays NAKed until the SCPI thread publishes the response.
        // A message that ends during a clear is dropped, cleared_cb arms.
        if (!scpi_tp_msg_lend(rx.buf, rx.len, true, rx.overrun)) {
            rx_reset();
        }
    }
    else {
        rx_arm();
    }
    return true;
}

bool tud_usbtmc_msgBulkIn_request_cb(
    const usbtmc_msg_request_dev_dep_in* request
)
{
    if (scpi_tp_state() != SCPI_TP_STATE_RESPONSE) {
        // Nothing to send: Query UNTERMINATED (IEEE 488.2). The request stays
        // NAKed until the host times out and aborts it (USBTMC).
        scpi_tp_unterminated();
        return true;
    }
    if (request->TransferSize == 0) {
        // The host allows no data: nothing can be sent, the request stays
        // NAKed until the host aborts it
        return true;
    }

    resp.chunk = tu_min32(resp.len - resp.sent, request->TransferSize);
    return tud_usbtmc_transmit_dev_msg_data(
        &resp.data[resp.sent],
        resp.chunk,
        (resp.sent + resp.chunk) == resp.len,
        false
    );
}

bool tud_usbtmc_msgBulkIn_complete_cb()
{
    resp.sent += resp.chunk;
    resp.chunk = 0;
    if (resp.sent >= resp.len) {
        resp_drop();
    }
    rx_arm();

    return true;
}

bool tud_usbtmc_initiate_clear_cb(uint8_t* tmcResult)
{
    *tmcResult = USBTMC_STATUS_SUCCESS;
    clear_start();
    return true;
}

bool tud_usbtmc_check_clear_cb(usbtmc_get_clear_status_rsp_t* rsp)
{
    // Pending until the SCPI thread finishes the command it is parsing
    rsp->USBTMC_status = (scpi_tp_state() == SCPI_TP_STATE_CLEARING)
                           ? USBTMC_STATUS_PENDING
                           : USBTMC_STATUS_SUCCESS;
    rsp->bmClear.BulkInFifoBytes = 0u;
    return true;
}

bool tud_usbtmc_initiate_abort_bulk_in_cb(uint8_t* tmcResult)
{
    *tmcResult = USBTMC_STATUS_SUCCESS;
    resp_drop();
    return true;
}

bool tud_usbtmc_check_abort_bulk_in_cb(usbtmc_check_abort_bulk_rsp_t* rsp)
{
    (void)rsp;
    rx_arm();
    return true;
}

bool tud_usbtmc_initiate_abort_bulk_out_cb(uint8_t* tmcResult)
{
    // TinyUSB accepts the abort only while a transfer is being received
    // (state RCV), never while rx.buf is lent
    *tmcResult = USBTMC_STATUS_SUCCESS;
    rx_reset();
    return true;
}

bool tud_usbtmc_check_abort_bulk_out_cb(usbtmc_check_abort_bulk_rsp_t* rsp)
{
    (void)rsp;
    rx_arm();
    return true;
}

void tud_usbtmc_bulkIn_clearFeature_cb(void) {}

void tud_usbtmc_bulkOut_clearFeature_cb(void)
{
    rx_arm();
}

// Return status byte, but put the transfer result status code in the rspResult
// argument.
// TODO: SR1: send the USB488 SRQ notification on interrupt-IN when MSS rises
// (on a new status or response) and return RQS instead of MSS in bit 6.
// tud_usbtmc_transmit_notification_data() in TinyUSB 0.19.0 checks
// usbd_edpt_busy() without negation and never sends, so for now the host has
// to poll READ_STATUS_BYTE.
uint8_t tud_usbtmc_get_stb_cb(uint8_t* tmcResult)
{
    *tmcResult = USBTMC_STATUS_SUCCESS;
    return scpi_tp_stb_get();
}

bool tud_usbtmc_indicator_pulse_cb(
    const tusb_control_request_t* msg, uint8_t* tmcResult
)
{
    (void)msg;
    *tmcResult = USBTMC_STATUS_SUCCESS;
    return true;
}
