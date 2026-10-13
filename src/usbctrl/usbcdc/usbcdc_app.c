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
// scpi_transport and writes responses to the CDC TX FIFO as it frees up. All
// transport state is changed in the TinyUSB thread only, the responses come
// in scpi_transport_published_cb, called from scpi_transport_poll.

#include "scpi/scpi_transport.h"
#include "tusb.h"

// Chunk read from the CDC RX FIFO, lent to the SCPI thread until it publishes
// the response. The rest stays in the FIFO, TinyUSB NAKs the host when it is
// full.
static uint8_t rx_buf[CFG_TUD_CDC_RX_BUFSIZE];
static bool    rx_lent;

_Static_assert(
    sizeof(rx_buf) < SCPI_TP_MSG_SIZE,
    "a CDC chunk must fit the scpi_io input buffer"
);

// Response published by the SCPI thread, written to the TX FIFO from tx_ix.
// resp_len is 0 if there is nothing to write.
static const uint8_t* resp;
static size_t         resp_len;
static size_t         tx_ix;

// Lend the next received chunk unless one is lent or a response is written
static void rx_lend_next(void)
{
    if (rx_lent || (resp_len > 0)) {
        return;
    }

    uint32_t len = tud_cdc_read(rx_buf, sizeof(rx_buf));
    if (len > 0) {
        rx_lent = true;
        scpi_transport_lend(rx_buf, len, false, false);
    }
}

// The response is written or dropped
static void resp_finish(void)
{
    resp_len = 0;
    tx_ix    = 0;
    scpi_transport_post(SCPI_TP_RESP_DONE);
    rx_lend_next();
}

// Writes the response as the TX FIFO frees up, drops it if the terminal
// disconnects
static void tx_continue(void)
{
    while ((tx_ix < resp_len) && tud_cdc_connected()) {
        uint32_t n = tud_cdc_write(&resp[tx_ix], resp_len - tx_ix);
        tud_cdc_write_flush();
        if (n == 0) {
            // FIFO full: continues in tud_cdc_tx_complete_cb
            return;
        }
        tx_ix += n;
    }
    resp_finish();
}

//--------------------------------------------------------------------+
// scpi_transport.h callbacks, called from scpi_transport_poll
//--------------------------------------------------------------------+

void scpi_transport_published_cb(const uint8_t* data, size_t len)
{
    rx_lent = false;
    if (len == 0) {
        rx_lend_next();
        return;
    }
    resp     = data;
    resp_len = len;
    tx_ix    = 0;
    tx_continue();
}

void scpi_transport_cleared_cb(void)
{
    // CDC never posts SCPI_TP_CLEAR
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
    if (resp_len > 0) {
        tx_continue();
    }
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void)itf;
    (void)dtr;
    (void)rts;
    // The terminal opened or closed the port: a response still being written
    // belongs to the old session. This also recovers from an unplug in the
    // middle of a response, which never completes its transfer.
    if (resp_len > 0) {
        resp_finish();
    }
}
