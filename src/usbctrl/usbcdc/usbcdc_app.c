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
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

// CDC transport: lends received chunks to the SCPI thread through
// scpi_tp and writes responses to the CDC TX FIFO as it frees up. All
// transport state is changed in the TinyUSB thread only, the responses come
// in resp_cb, called from scpi_tp_dispatch.

#include "scpi/scpi_app_tp.h"
#include "tusb.h"
#include "usbctrl/usbclass.h"

// Chunk read from the CDC RX FIFO, lent to the SCPI thread until it publishes
// the response. The rest stays in the FIFO, TinyUSB NAKs the host when it is
// full.
typedef struct
{
    uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE];
    bool    lent;
    // The port was opened or closed while the chunk was lent: its response
    // belongs to the old session and is dropped
    bool stale;
} CdcRx;

// Response published by the SCPI thread, written to the TX FIFO from sent
typedef struct
{
    const uint8_t* data;
    size_t         len;   // 0 if there is nothing to write
    size_t         sent;  // bytes already written to the TX FIFO
} CdcResp;

// All of it is changed in the TinyUSB thread only
static CdcRx   rx;
static CdcResp resp;

_Static_assert(
    sizeof(rx.buf) < SCPI_TP_MSG_SIZE,
    "a CDC chunk must fit the scpi_io input buffer"
);

// Lend the next received chunk unless one is lent or a response is written
static void rx_lend_next(void)
{
    if (rx.lent || (resp.len > 0)) {
        return;
    }

    uint32_t len = tud_cdc_read(rx.buf, sizeof(rx.buf));
    if (len > 0) {
        rx.lent = true;
        scpi_tp_req_set(rx.buf, len, false, false);
    }
}

// The response is written or dropped
static void resp_finish(void)
{
    resp.len  = 0;
    resp.sent = 0;
    scpi_tp_evt_set(SCPI_TP_EVT_RESP_DONE);
    rx_lend_next();
}

// Writes the response as the TX FIFO frees up, drops it if the terminal
// disconnects
static void tx_continue(void)
{
    while ((resp.sent < resp.len) && tud_cdc_connected()) {
        uint32_t n = tud_cdc_write(&resp.data[resp.sent], resp.len - resp.sent);
        tud_cdc_write_flush();
        if (n == 0) {
            // FIFO full: continues in tud_cdc_tx_complete_cb
            return;
        }
        resp.sent += n;
    }
    resp_finish();
}

//--------------------------------------------------------------------+
// scpi_app_tp.h callbacks, registered in usb_class_init and called from
// scpi_tp_dispatch
//--------------------------------------------------------------------+

static void resp_cb(const uint8_t* data, size_t len)
{
    bool stale = rx.stale;

    rx.lent  = false;
    rx.stale = false;
    if (len == 0) {
        rx_lend_next();
        return;
    }
    resp.data = data;
    resp.len  = len;
    resp.sent = 0;
    if (stale) {
        // Parsed in the old session: dropped, not written
        resp_finish();
        return;
    }
    tx_continue();
}

void usbclass_init(void)
{
    scpi_tp_cb_set(resp_cb, NULL);
}

//--------------------------------------------------------------------+
// TinyUSB callbacks
//--------------------------------------------------------------------+

void tud_cdc_rx_cb(uint8_t itf)
{
    (void)itf;
    rx_lend_next();
}

void tud_cdc_tx_complete_cb(uint8_t itf)
{
    (void)itf;
    if (resp.len > 0) {
        tx_continue();
    }
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void)itf;
    (void)dtr;
    (void)rts;
    // The terminal opened or closed the port: the response of a chunk still
    // being parsed, or still being written, belongs to the old session. The
    // latter also recovers from an unplug in the middle of a response, which
    // never completes its transfer.
    if (rx.lent) {
        rx.stale = true;
    }
    if (resp.len > 0) {
        resp_finish();
    }
}
