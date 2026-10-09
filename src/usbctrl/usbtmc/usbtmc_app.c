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

// USBTMC transport: frames messages and responses over TinyUSB, the IEEE
// 488.2 exchange with the SCPI thread is in usbtmc_exchange.c. Everything
// here runs in the TinyUSB thread, except usbtmc_rx_resume.

#include "tusb.h"
#include "usbtmc_exchange.h"
// After tusb.h: relies on its types
#include "device/usbd_pvt.h" /* usbd_defer_func */
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
// Message: collected over the transfers up to its end, then lent to the
// exchange until usbtmc_rx_resume. Bulk-OUT stays NAKed meanwhile.
static uint8_t rx_buf[EXCH_MSG_SIZE];
static size_t  rx_len;
static bool    rx_overrun;  // the message did not fit, dropped up to its end
static bool    rx_eom;      // the transfer being received ends with EOM
static uint8_t rx_last;     // last byte of the transfer being received
static bool    rx_parsing;  // rx_buf is lent to the exchange

// Response being sent over bulk-IN, lent by the exchange until
// exch_response_done
static const uint8_t* tx_buf;
static size_t         tx_len;
static size_t         tx_ix;     // bytes of the response already sent
static size_t         tx_chunk;  // bytes in the bulk-IN transfer in flight

// Accept the next transfer unless rx_buf is lent to the exchange (a stray
// CLEAR_FEATURE or CHECK_ABORT must not let a new message overwrite it)
static void rx_arm(void)
{
    if (!rx_parsing) {
        tud_usbtmc_start_bus_read();
    }
}

// Drop the message being received
static void rx_reset(void)
{
    rx_len     = 0;
    rx_overrun = false;
}

static void tx_reset(void)
{
    tx_len   = 0;
    tx_ix    = 0;
    tx_chunk = 0;
}

static void rx_resume(void* param)
{
    (void)param;
    rx_reset();
    rx_parsing = false;
    rx_arm();
}

void usbtmc_rx_resume(void)
{
    // Called from the SCPI thread, the transport state is the TinyUSB thread's
    usbd_defer_func(rx_resume, NULL, false);
}

void tud_usbtmc_open_cb(uint8_t interface_id)
{
    (void)interface_id;
    rx_reset();
    rx_parsing = false;
    tx_reset();
    exch_response_done();
    rx_arm();
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
    rx_eom  = msgHeader->bmTransferAttributes.EOM;
    rx_last = 0;
    // A response is pending only before the first transfer of a message
    tx_reset();
    exch_msg_start();
    return true;
}

bool tud_usbtmc_msg_data_cb(void* data, size_t len, bool transfer_complete)
{
    if (!rx_overrun) {
        if (len > (sizeof(rx_buf) - rx_len)) {
            // Input buffer overrun: the rest of the message is dropped, the
            // exchange reports it
            rx_overrun = true;
        }
        else {
            memcpy(&rx_buf[rx_len], data, len);
            rx_len += len;
        }
    }

    if (len > 0) {
        rx_last = ((const uint8_t*)data)[len - 1];
    }

    // The message ends at EOM (IEEE 488.2 END) or at a newline closing the
    // transfer
    if (transfer_complete && (rx_eom || (rx_last == '\n'))) {
        // Bulk-OUT stays NAKed until the exchange resumes it
        rx_parsing = true;
        exch_msg_received(rx_buf, rx_len, rx_eom, rx_overrun);
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
    if (tx_len == 0) {
        tx_len = exch_response(&tx_buf);
        if (tx_len == 0) {
            // The request stays NAKed until the host times out and aborts it
            // (USBTMC)
            return true;
        }
    }

    tx_chunk = tu_min32(tx_len - tx_ix, request->TransferSize);
    return tud_usbtmc_transmit_dev_msg_data(
        &tx_buf[tx_ix], tx_chunk, (tx_ix + tx_chunk) == tx_len, false
    );
}

bool tud_usbtmc_msgBulkIn_complete_cb()
{
    tx_ix += tx_chunk;
    tx_chunk = 0;
    if (tx_ix >= tx_len) {
        tx_reset();
        exch_response_done();
    }
    rx_arm();

    return true;
}

bool tud_usbtmc_initiate_clear_cb(uint8_t* tmcResult)
{
    *tmcResult = USBTMC_STATUS_SUCCESS;
    rx_reset();
    tx_reset();
    exch_clear();
    return true;
}

bool tud_usbtmc_check_clear_cb(usbtmc_get_clear_status_rsp_t* rsp)
{
    // Pending until the SCPI thread finishes the command it is parsing
    if (exch_clear_done()) {
        // The parse ended without usbtmc_rx_resume, rx_buf is free
        rx_parsing         = false;
        rsp->USBTMC_status = USBTMC_STATUS_SUCCESS;
    }
    else {
        rsp->USBTMC_status = USBTMC_STATUS_PENDING;
    }
    rsp->bmClear.BulkInFifoBytes = 0u;
    return true;
}

bool tud_usbtmc_initiate_abort_bulk_in_cb(uint8_t* tmcResult)
{
    *tmcResult = USBTMC_STATUS_SUCCESS;
    tx_reset();
    exch_response_done();
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
    // (state RCV), never while rx_buf is lent to the exchange
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
uint8_t tud_usbtmc_get_stb_cb(uint8_t* tmcResult)
{
    *tmcResult = USBTMC_STATUS_SUCCESS;
    // TODO: send the USB488 SRQ notification (bNotify1 = 0x81, STB) on
    // interrupt-IN when RQS rises (SCPI_CTRL_SRQ), and return RQS instead of
    // MSS in bit 6. tud_usbtmc_transmit_notification_data() in TinyUSB 0.19.0
    // checks usbd_edpt_busy() without negation and never sends, so for now
    // the host has to poll READ_STATUS_BYTE.
    return exch_stb();
}

bool tud_usbtmc_indicator_pulse_cb(
    const tusb_control_request_t* msg, uint8_t* tmcResult
)
{
    (void)msg;
    *tmcResult = USBTMC_STATUS_SUCCESS;
    return true;
}
