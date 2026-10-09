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

#include <class/cdc/cdc_device.h>
#include <stdint.h>

#include "scpi/scpi_def.h"

// Host-centric directions out: from PC, in: to device
#define BUF_OUT_SIZE 256
#define BUF_IN_SIZE  SCPI_OUTPUT_BUFFER_LENGTH
uint8_t buffer_in[BUF_IN_SIZE];
size_t  buffer_in_len;

void usbcdc_app_task_iter(void)
{
    if (tud_cdc_connected()) {
        // connected and there are data available
        if (buffer_in_len > 0) {
            // TODO: Fix data loss in case buffer_in_len >
            // CFG_TUD_CDC_TX_BUFSIZE
            tud_cdc_write(buffer_in, buffer_in_len);
            tud_cdc_write_flush();
            buffer_in_len = 0;
        }
    }
}

void tud_cdc_rx_cb(uint8_t itf)
{
    (void)itf;
    uint8_t buffer_out[BUF_OUT_SIZE];
    size_t  buffer_out_len = 0;

    buffer_out_len = tud_cdc_read(buffer_out, BUF_IN_SIZE);
    // TODO: Should split tud thread and parser->handler call
    SCPI_Input(&scpi_context, (char*)buffer_out, buffer_out_len);
}
