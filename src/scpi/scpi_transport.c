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

#include "scpi_transport.h"

#include <cmsis_os2.h>

static osEventFlagsId_t tp_evt;

// Lent message: set by the TinyUSB thread before SCPI_TP_MSG, read by the
// SCPI thread. The class receives nothing more until the message is returned,
// so they never overlap.
static const uint8_t* msg_data;
static size_t         msg_len;
static bool           msg_end;
static bool           msg_overrun;

// Results of the SCPI thread: set by it, taken and cleared by
// scpi_transport_poll in the TinyUSB thread. Each flag is set again only
// after the poll has handled it: a new publish needs a new lent message, a
// new clear needs the class to finish the previous one in
// scpi_transport_cleared_cb.
static const uint8_t* volatile pub_resp;
static volatile size_t         pub_len;
static volatile bool           published;
static volatile bool           cleared;

// What the class waits for from the SCPI thread, TinyUSB thread only: the
// poll runs every 1 ms while one of them is set
static bool wait_publish;
static bool wait_clear;

static volatile uint8_t stb_snapshot;

//--------------------------------------------------------------------+
// SCPI thread
//--------------------------------------------------------------------+

void scpi_transport_init(void)
{
    tp_evt = osEventFlagsNew(NULL);
}

uint32_t scpi_transport_wait(void)
{
    uint32_t evt =
        osEventFlagsWait(tp_evt, SCPI_TP_ALL, osFlagsWaitAny, osWaitForever);
    if (evt & osFlagsError) {
        return 0;
    }
    return evt;
}

size_t scpi_transport_msg(const uint8_t** data, bool* end, bool* overrun)
{
    *data    = msg_data;
    *end     = msg_end;
    *overrun = msg_overrun;
    return msg_len;
}

void scpi_transport_publish(const uint8_t* resp, size_t len)
{
    pub_resp  = resp;
    pub_len   = len;
    published = true;
}

void scpi_transport_clear_done(void)
{
    cleared = true;
}

void scpi_transport_stb(uint8_t stb)
{
    stb_snapshot = stb;
}

//--------------------------------------------------------------------+
// USB class, TinyUSB thread
//--------------------------------------------------------------------+

void scpi_transport_lend(const uint8_t* msg, size_t len, bool end, bool overrun)
{
    msg_data     = msg;
    msg_len      = len;
    msg_end      = end;
    msg_overrun  = overrun;
    wait_publish = true;
    osEventFlagsSet(tp_evt, SCPI_TP_MSG);
}

void scpi_transport_post(uint32_t evt)
{
    if (evt & SCPI_TP_CLEAR) {
        wait_clear = true;
    }
    osEventFlagsSet(tp_evt, evt);
}

uint8_t scpi_transport_host_stb(void)
{
    return stb_snapshot;
}

uint32_t scpi_transport_poll(void)
{
    // A publish pending together with a clear came first: the SCPI thread
    // publishes the message lent before the clear, then handles the clear.
    // In the other order the class would take a response from before the
    // clear.
    if (published) {
        published    = false;
        wait_publish = false;
        scpi_transport_published_cb(pub_resp, pub_len);
    }
    if (cleared) {
        cleared      = false;
        wait_clear   = false;
        // A message lent before the clear is dropped, not published
        wait_publish = false;
        scpi_transport_cleared_cb();
    }

    return (wait_publish || wait_clear) ? 1U : UINT32_MAX;
}
