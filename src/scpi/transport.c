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

#include "transport_scpi.h"
#include "transport_usbctl.h"

#include <cmsis_os2.h>

// Written by the TinyUSB thread only
typedef struct
{
    // Lent message, read by the SCPI thread after SCPI_TP_MSG. The class
    // receives nothing more until the message is returned, so they never
    // overlap.
    const uint8_t* msg;
    size_t         msg_len;
    bool           msg_end;
    bool           msg_overrun;

    // What the class waits for from the SCPI thread: scpi_transport_poll
    // runs every 1 ms while one of them is set
    bool wait_publish;
    bool wait_clear;
} TpTudState;

// Written by the SCPI thread, read by scpi_transport_poll in the TinyUSB
// thread. The poll also resets the flags once it has handled them; each flag
// is set again only after that: a new publish needs a new lent message, a new
// clear needs the class to finish the previous one in
// ScpiTpClearedCb.
typedef struct
{
    const uint8_t* volatile resp;
    volatile size_t  resp_len;
    volatile bool    published;      // resp / resp_len are set
    volatile bool    clear_handled;  // SCPI_TP_CLEAR is handled
    volatile uint8_t stb;            // status byte for the host
} TpScpiState;

static osEventFlagsId_t tp_evt;
static TpTudState       tud;
static TpScpiState      scpi;
static ScpiTpRespCb     tp_resp_cb;
static ScpiTpClearedCb  tp_cleared_cb;

//--------------------------------------------------------------------+
// SCPI thread
//--------------------------------------------------------------------+

void scpi_transport_init(void)
{
    tp_evt        = osEventFlagsNew(NULL);
    tp_resp_cb    = NULL;
    tp_cleared_cb = NULL;
}

uint32_t scpi_transport_evt_wait(void)
{
    uint32_t evt =
        osEventFlagsWait(tp_evt, SCPI_TP_ALL, osFlagsWaitAny, osWaitForever);
    if (evt & osFlagsError) {
        return 0;
    }
    return evt;
}

size_t scpi_transport_req_get(const uint8_t** data, bool* end, bool* overrun)
{
    *data    = tud.msg;
    *end     = tud.msg_end;
    *overrun = tud.msg_overrun;
    return tud.msg_len;
}

void scpi_transport_resp_set(const uint8_t* resp, size_t len)
{
    scpi.resp      = resp;
    scpi.resp_len  = len;
    scpi.published = true;
}

void scpi_transport_clear_done(void)
{
    scpi.clear_handled = true;
}

void scpi_transport_stb_set(uint8_t stb)
{
    scpi.stb = stb;
}

//--------------------------------------------------------------------+
// USB class, TinyUSB thread
//--------------------------------------------------------------------+

void scpi_transport_req_set(
    const uint8_t* msg, size_t len, bool end, bool overrun
)
{
    tud.msg          = msg;
    tud.msg_len      = len;
    tud.msg_end      = end;
    tud.msg_overrun  = overrun;
    tud.wait_publish = true;
    osEventFlagsSet(tp_evt, SCPI_TP_MSG);
}

void scpi_transport_evt_set(ScpiTpEvt evt)
{
    switch (evt) {
        case SCPI_TP_EVT_RESP_DONE:
            osEventFlagsSet(tp_evt, SCPI_TP_RESP_DONE);
            break;
        case SCPI_TP_EVT_INTERRUPTED:
            osEventFlagsSet(tp_evt, SCPI_TP_INTERRUPTED);
            break;
        case SCPI_TP_EVT_UNTERMINATED:
            osEventFlagsSet(tp_evt, SCPI_TP_UNTERMINATED);
            break;
        case SCPI_TP_EVT_CLEAR:
            tud.wait_clear = true;
            osEventFlagsSet(tp_evt, SCPI_TP_CLEAR);
            break;
    }
}

uint8_t scpi_transport_stb_get(void)
{
    return scpi.stb;
}

void scpi_transport_cb_set(ScpiTpRespCb resp_cb, ScpiTpClearedCb cleared_cb)
{
    tp_resp_cb    = resp_cb;
    tp_cleared_cb = cleared_cb;
}

uint32_t scpi_transport_poll(void)
{
    // A publish pending together with a clear came first: the SCPI thread
    // publishes the message lent before the clear, then handles the clear.
    // In the other order the class would take a response from before the
    // clear.
    if (scpi.published) {
        scpi.published   = false;
        tud.wait_publish = false;
        if (tp_resp_cb != NULL) {
            tp_resp_cb(scpi.resp, scpi.resp_len);
        }
    }
    if (scpi.clear_handled) {
        scpi.clear_handled = false;
        tud.wait_clear     = false;
        // A message lent before the clear is dropped, not published
        tud.wait_publish = false;
        if (tp_cleared_cb != NULL) {
            tp_cleared_cb();
        }
    }

    return (tud.wait_publish || tud.wait_clear) ? 1U : UINT32_MAX;
}
