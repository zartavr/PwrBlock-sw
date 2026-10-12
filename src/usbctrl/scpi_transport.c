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

// SCPI transport over TinyUSB, shared by the USB classes. The SCPI thread
// waits on event flags and reads the lent message; what it hands back is
// deferred to the TinyUSB thread, where the class owns all transport state.

#include "scpi_transport_usb.h"

#include "tusb.h"
// After tusb.h: relies on its types
#include "device/usbd_pvt.h" /* usbd_defer_func */
#include <cmsis_os2.h>

static osEventFlagsId_t tp_evt;

// Lent message: set by the TinyUSB thread before SCPI_TP_MSG, read by the
// SCPI thread. The class receives nothing more until the message is returned,
// so they never overlap.
static const uint8_t* msg_data;
static size_t         msg_len;
static bool           msg_end;
static bool           msg_overrun;

// Response passed to the deferred publish: set by the SCPI thread, read by
// the TinyUSB thread before the next message can be lent
static const uint8_t* pub_resp;
static size_t         pub_len;

static volatile uint8_t stb_snapshot;

//--------------------------------------------------------------------+
// scpi_transport.h, SCPI thread
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

static void published(void* param)
{
    (void)param;
    scpi_transport_published_cb(pub_resp, pub_len);
}

void scpi_transport_publish(const uint8_t* resp, size_t len)
{
    pub_resp = resp;
    pub_len  = len;
    usbd_defer_func(published, NULL, false);
}

static void cleared(void* param)
{
    (void)param;
    scpi_transport_cleared_cb();
}

void scpi_transport_clear_done(void)
{
    usbd_defer_func(cleared, NULL, false);
}

void scpi_transport_stb(uint8_t stb)
{
    stb_snapshot = stb;
}

//--------------------------------------------------------------------+
// scpi_transport_usb.h, TinyUSB thread
//--------------------------------------------------------------------+

void scpi_transport_lend(const uint8_t* msg, size_t len, bool end, bool overrun)
{
    msg_data    = msg;
    msg_len     = len;
    msg_end     = end;
    msg_overrun = overrun;
    osEventFlagsSet(tp_evt, SCPI_TP_MSG);
}

void scpi_transport_post(uint32_t evt)
{
    osEventFlagsSet(tp_evt, evt);
}

uint8_t scpi_transport_host_stb(void)
{
    return stb_snapshot;
}
