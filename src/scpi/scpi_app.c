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

#include "scpi_app.h"
#include "scpi_app_tp.h"

#include <cmsis_os2.h>

// Written by the TinyUSB thread only
typedef struct
{
    ScpiTpState state;

    // Lent message, read by the SCPI thread after SCPI_TP_MSG. Valid while
    // parsing, and while clearing if the clear came during the parse.
    const uint8_t* msg;
    size_t         msg_len;
    bool           msg_end;
    bool           msg_overrun;

    // Response taken from the SCPI thread, valid in SCPI_TP_STATE_RESPONSE
    const uint8_t* resp;
    size_t         resp_len;
} TpTudState;

// Written by the SCPI thread, which then wakes the TinyUSB thread, read by
// scpi_tp_dispatch there. The dispatch also resets the flags once it has
// handled them; each flag is set again only after that: a new publish needs a
// new lent message, a new clear needs the previous one to complete.
typedef struct
{
    const uint8_t* volatile resp;
    volatile size_t   resp_len;
    volatile bool     published;      // resp / resp_len are set
    volatile bool     clear_handled;  // SCPI_TP_CLEAR is handled
    volatile uint16_t status;         // scpi_io_status, for the host
} TpScpiState;

static osEventFlagsId_t tp_evt;
static TpTudState       tud;
static TpScpiState      scpi;
static ScpiTpWakeFn     tp_wake;

//--------------------------------------------------------------------+
// SCPI thread
//--------------------------------------------------------------------+

void scpi_app_init(void)
{
    tp_evt = osEventFlagsNew(NULL);
}

uint32_t scpi_app_evt_wait(void)
{
    uint32_t evt =
        osEventFlagsWait(tp_evt, SCPI_TP_ALL, osFlagsWaitAny, osWaitForever);
    if (evt & osFlagsError) {
        return 0;
    }
    return evt;
}

size_t scpi_app_req_get(const uint8_t** data, bool* end, bool* overrun)
{
    *data    = tud.msg;
    *end     = tud.msg_end;
    *overrun = tud.msg_overrun;
    return tud.msg_len;
}

void scpi_app_resp_set(const uint8_t* resp, size_t len)
{
    scpi.resp      = resp;
    scpi.resp_len  = len;
    scpi.published = true;
}

void scpi_app_clear_done(void)
{
    scpi.clear_handled = true;
}

void scpi_app_status_set(uint16_t status)
{
    scpi.status = status;
}

void scpi_app_tud_wake(void)
{
    if (tp_wake != NULL) {
        tp_wake();
    }
}

//--------------------------------------------------------------------+
// USB class, TinyUSB thread
//--------------------------------------------------------------------+

ScpiTpState scpi_tp_state(void)
{
    return tud.state;
}

bool scpi_tp_msg_lend(const uint8_t* msg, size_t len, bool end, bool overrun)
{
    if (tud.state != SCPI_TP_STATE_IDLE) {
        return false;
    }
    tud.msg         = msg;
    tud.msg_len     = len;
    tud.msg_end     = end;
    tud.msg_overrun = overrun;
    tud.state       = SCPI_TP_STATE_PARSING;
    osEventFlagsSet(tp_evt, SCPI_TP_MSG);
    return true;
}

void scpi_tp_resp_done(void)
{
    if (tud.state != SCPI_TP_STATE_RESPONSE) {
        return;
    }
    tud.state = SCPI_TP_STATE_IDLE;
    osEventFlagsSet(tp_evt, SCPI_TP_RESP_DONE);
}

void scpi_tp_interrupted(void)
{
    if (tud.state != SCPI_TP_STATE_RESPONSE) {
        return;
    }
    tud.state = SCPI_TP_STATE_IDLE;
    osEventFlagsSet(tp_evt, SCPI_TP_INTERRUPTED);
}

void scpi_tp_unterminated(void)
{
    if (tud.state != SCPI_TP_STATE_IDLE) {
        return;
    }
    osEventFlagsSet(tp_evt, SCPI_TP_UNTERMINATED);
}

void scpi_tp_clear(void)
{
    if (tud.state == SCPI_TP_STATE_CLEARING) {
        // Nothing is lent or received while clearing, so one pending clear
        // covers the repeated ones
        return;
    }
    tud.state = SCPI_TP_STATE_CLEARING;
    osEventFlagsSet(tp_evt, SCPI_TP_CLEAR);
}

size_t scpi_tp_resp_get(const uint8_t** data)
{
    if (tud.state != SCPI_TP_STATE_RESPONSE) {
        *data = NULL;
        return 0;
    }
    *data = tud.resp;
    return tud.resp_len;
}

uint8_t scpi_tp_stb_get(void)
{
    // MAV: the output queue is the response the class holds
    bool mav = (tud.state == SCPI_TP_STATE_RESPONSE);

    return scpi_io_stb_compose(scpi.status, mav);
}

void scpi_tp_wake_set(ScpiTpWakeFn wake)
{
    tp_wake = wake;
}

bool scpi_tp_dispatch(void)
{
    bool changed = false;

    // The SCPI thread may publish the message lent before a clear, then
    // handle the clear. Read in the reverse order of the writes, so a clear
    // seen here brings the publish before it along: it is dropped in
    // SCPI_TP_STATE_CLEARING, never taken for a message lent after the clear.
    bool cleared   = scpi.clear_handled;
    bool published = scpi.published;

    if (published) {
        scpi.published = false;
        if (tud.state == SCPI_TP_STATE_PARSING) {
            tud.resp     = scpi.resp;
            tud.resp_len = scpi.resp_len;
            tud.state    = (tud.resp_len > 0) ? SCPI_TP_STATE_RESPONSE
                                              : SCPI_TP_STATE_IDLE;
            changed      = true;
        }
    }
    if (cleared) {
        scpi.clear_handled = false;
        if (tud.state == SCPI_TP_STATE_CLEARING) {
            tud.state = SCPI_TP_STATE_IDLE;
            changed   = true;
        }
    }
    return changed;
}
