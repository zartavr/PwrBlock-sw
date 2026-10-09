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

#include "scpi/scpi_io.h"
#include "tusb.h"
#include "usbctrl/usb_class.h"
#include <cmsis_os2.h>
#include <stdint.h>

// Event from the TinyUSB thread to the SCPI thread. The CDC FIFOs are
// mutex-protected, so the SCPI thread reads and writes them directly.
#define EVT_RX (1u << 0)  // data in the CDC RX FIFO

static osEventFlagsId_t cdc_evt;

void usb_class_init(void)
{
    cdc_evt = osEventFlagsNew(NULL);
    scpi_io_init();
}

// Sends the response in portions as the TX FIFO frees up, drops it if the
// terminal disconnects
static void response_send(void)
{
    const uint8_t* data;
    size_t         len = scpi_io_response(&data);
    size_t         ix  = 0;

    while ((ix < len) && tud_cdc_connected()) {
        uint32_t n = tud_cdc_write(&data[ix], len - ix);
        tud_cdc_write_flush();
        ix += n;
        if (n == 0) {
            osDelay(1);
        }
    }
    scpi_io_response_drop();
}

void usb_class_scpi_iter(void)
{
    uint8_t  buffer_out[CFG_TUD_CDC_RX_BUFSIZE];
    uint32_t len;

    uint32_t evt =
        osEventFlagsWait(cdc_evt, EVT_RX, osFlagsWaitAny, osWaitForever);
    if (evt & osFlagsError) {
        return;
    }

    while ((len = tud_cdc_read(buffer_out, sizeof(buffer_out))) > 0) {
        scpi_io_input((const char*)buffer_out, len);
        response_send();
    }
}

void tud_cdc_rx_cb(uint8_t itf)
{
    (void)itf;
    osEventFlagsSet(cdc_evt, EVT_RX);
}
