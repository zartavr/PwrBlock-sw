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

// Every field has a single writer. Each job for the SCPI thread (a lent
// message, a clear) gets a number, and the SCPI thread returns its result
// with the number of the job. The dispatch takes a result only if it belongs
// to the current job, so a late one, like the response to a message lent
// before a clear, is never taken for a newer job, whenever it is seen.

// Written by the TinyUSB thread only. The SCPI thread reads the message and
// the job numbers after the event flag of the job, which orders them.
typedef struct
{
    ScpiTpState state;

    // Lent message, read by the SCPI thread after SCPI_JOB_MSG. Valid while
    // parsing, and while clearing if the clear came during the parse.
    const uint8_t* msg;
    size_t         msg_len;
    bool           msg_end;
    bool           msg_overrun;

    uint32_t msg_seq;    // number of the last lent message
    uint32_t clear_seq;  // number of the last clear

    // Response taken from the SCPI thread, valid in SCPI_TP_STATE_RESPONSE
    const uint8_t* resp;
    size_t         resp_len;
} TpTudState;

// Written by the SCPI thread only, read by scpi_tp_dispatch in the TinyUSB
// thread. A job number is stored last: volatile keeps the stores in order, so
// on the single core the data before it is complete once the number is seen.
typedef struct
{
    const uint8_t* volatile resp;
    volatile size_t   resp_len;
    volatile uint32_t resp_seq;     // msg_seq of the message resp belongs to
    volatile uint32_t cleared_seq;  // clear_seq of the last handled clear
    volatile uint16_t status;       // scpi_io_status, for the host

    uint32_t parse_seq;  // msg_seq of the message being parsed
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
        osEventFlagsWait(tp_evt, SCPI_JOB_ALL, osFlagsWaitAny, osWaitForever);
    if (evt & osFlagsError) {
        return 0;
    }
    return evt;
}

size_t scpi_app_req_get(const uint8_t** data, bool* end, bool* overrun)
{
    scpi.parse_seq = tud.msg_seq;
    *data          = tud.msg;
    *end           = tud.msg_end;
    *overrun       = tud.msg_overrun;
    return tud.msg_len;
}

void scpi_app_resp_set(const uint8_t* resp, size_t len)
{
    scpi.resp     = resp;
    scpi.resp_len = len;
    scpi.resp_seq = scpi.parse_seq;
}

void scpi_app_clear_done(void)
{
    scpi.cleared_seq = tud.clear_seq;
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
    tud.msg_seq++;
    tud.state = SCPI_TP_STATE_PARSING;
    osEventFlagsSet(tp_evt, SCPI_JOB_MSG);
    return true;
}

void scpi_tp_resp_done(void)
{
    if (tud.state != SCPI_TP_STATE_RESPONSE) {
        return;
    }
    tud.state = SCPI_TP_STATE_IDLE;
    osEventFlagsSet(tp_evt, SCPI_JOB_RESP_DONE);
}

void scpi_tp_interrupted(void)
{
    if (tud.state != SCPI_TP_STATE_RESPONSE) {
        return;
    }
    tud.state = SCPI_TP_STATE_IDLE;
    osEventFlagsSet(tp_evt, SCPI_JOB_INTERRUPTED);
}

void scpi_tp_unterminated(void)
{
    if (tud.state != SCPI_TP_STATE_IDLE) {
        return;
    }
    osEventFlagsSet(tp_evt, SCPI_JOB_UNTERMINATED);
}

void scpi_tp_clear(void)
{
    if (tud.state == SCPI_TP_STATE_CLEARING) {
        // Nothing is lent or received while clearing, so one pending clear
        // covers the repeated ones
        return;
    }
    tud.clear_seq++;
    tud.state = SCPI_TP_STATE_CLEARING;
    osEventFlagsSet(tp_evt, SCPI_JOB_CLEAR);
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
    // The SCPI thread may still publish the message lent before a clear: its
    // number never matches in CLEARING, nor after the next lend
    switch (tud.state) {
        case SCPI_TP_STATE_PARSING:
            if (scpi.resp_seq != tud.msg_seq) {
                return false;
            }
            tud.resp     = scpi.resp;
            tud.resp_len = scpi.resp_len;
            tud.state    = (tud.resp_len > 0) ? SCPI_TP_STATE_RESPONSE
                                              : SCPI_TP_STATE_IDLE;
            return true;

        case SCPI_TP_STATE_CLEARING:
            if (scpi.cleared_seq != tud.clear_seq) {
                return false;
            }
            tud.state = SCPI_TP_STATE_IDLE;
            return true;

        default:
            return false;
    }
}
