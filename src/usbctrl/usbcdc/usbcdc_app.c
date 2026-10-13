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

// CDC transport: lends received chunks to the SCPI thread through scpi_tp,
// which holds the exchange state and the response, and writes responses to the
// CDC TX FIFO as it frees up. All of it is changed in the TinyUSB thread only,
// the results of the SCPI thread are taken in usbclass_dispatch.

#include "scpi/scpi_app_tp.h"
#include "tusb.h"
#include "usbctrl/usbclass.h"

// Chunk read from the CDC RX FIFO, lent to the SCPI thread while parsing,
// until it publishes the response. The rest stays in the FIFO, TinyUSB NAKs
// the host when it is full.
typedef struct
{
    uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE];
} CdcRx;

// Progress of the response (scpi_tp_resp_get) into the TX FIFO
typedef struct
{
    size_t sent;  // bytes already written to the TX FIFO
} CdcResp;

// All of it is changed in the TinyUSB thread only
static CdcRx   rx;
static CdcResp resp;

_Static_assert(
    sizeof(rx.buf) < SCPI_TP_MSG_SIZE,
    "a CDC chunk must fit the scpi_io input buffer"
);

// Lend the next received chunk unless one is lent, a response is written or a
// clear is in progress
static void rx_lend_next(void)
{
    if (scpi_tp_state() != SCPI_TP_STATE_IDLE) {
        return;
    }

    uint32_t len = tud_cdc_read(rx.buf, sizeof(rx.buf));
    if (len > 0) {
        scpi_tp_msg_lend(rx.buf, len, false, false);
    }
}

// The response is written or dropped
static void resp_finish(void)
{
    resp.sent = 0;
    scpi_tp_resp_done();
    rx_lend_next();
}

// Writes the response as the TX FIFO frees up, drops it if the terminal
// disconnects
static void tx_continue(void)
{
    const uint8_t* data;
    size_t         len = scpi_tp_resp_get(&data);

    while ((resp.sent < len) && tud_cdc_connected()) {
        uint32_t n = tud_cdc_write(&data[resp.sent], len - resp.sent);
        tud_cdc_write_flush();
        if (n == 0) {
            // FIFO full: continues in tud_cdc_tx_complete_cb
            return;
        }
        resp.sent += n;
    }
    resp_finish();
}

void usbclass_dispatch(void)
{
    if (!scpi_tp_dispatch()) {
        return;
    }
    // The chunk is consumed or the clear is handled: a response is written
    // from its start, without one the next chunk is lent from the RX FIFO
    resp.sent = 0;
    if (scpi_tp_state() == SCPI_TP_STATE_RESPONSE) {
        tx_continue();
    }
    else {
        rx_lend_next();
    }
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
    if (scpi_tp_state() == SCPI_TP_STATE_RESPONSE) {
        tx_continue();
    }
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void)itf;
    (void)dtr;
    (void)rts;
    // The terminal opened or closed the port: a new session starts with a
    // clear. The input of the old one, a response still being parsed or
    // written are dropped, and so are their bytes left in the FIFOs. The
    // latter also recovers from an unplug in the middle of a response, which
    // never completes its transfer.
    resp.sent = 0;
    tud_cdc_read_flush();
    tud_cdc_write_clear();
    scpi_tp_clear();
}
