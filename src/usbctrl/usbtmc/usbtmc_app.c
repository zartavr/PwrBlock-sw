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
 * Copyright (c) 2019 Nathan Conrad
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

#include "usbtmc_app.h"

#include "scpi/scpi_def.h"
#include "tusb.h"
// After tusb.h: relies on its types
#include "device/usbd_pvt.h" /* usbd_defer_func */
#include <cmsis_os2.h>
#include <string.h>

#if (CFG_TUD_USBTMC_ENABLE_488)
static const usbtmc_response_capabilities_488_t
#else
static const usbtmc_response_capabilities_t
#endif
    tud_usbtmc_app_capabilities = {
        .USBTMC_status = USBTMC_STATUS_SUCCESS,
        .bcdUSBTMC     = USBTMC_VERSION,
        .bmIntfcCapabilities =
            {.listenOnly = 0, .talkOnly = 0, .supportsIndicatorPulse = 1},
        .bmDevCapabilities = {.canEndBulkInOnTermChar = 0},

#if (CFG_TUD_USBTMC_ENABLE_488)
        .bcdUSB488 = USBTMC_488_VERSION,
        .bmIntfcCapabilities488 =
            {.supportsTrigger = 0, .supportsREN_GTL_LLO = 0, .is488_2 = 1},
        .bmDevCapabilities488 =
            {
                                  .SCPI = 1,
                                  .SR1  = 0,
                                  .RL1  = 0,
                                  .DT1  = 0,
                                  }
#endif
};

// Events from the TinyUSB thread to the SCPI thread. libscpi (context,
// registers, error queue) is touched only by the SCPI thread, TinyUSB only by
// the TinyUSB thread.
#define EVT_RX           (1u << 0)  // a bulk-OUT transfer is in buffer_out
#define EVT_TX_DONE      (1u << 1)  // the response was sent or dropped
#define EVT_INTERRUPTED  (1u << 2)  // a command came before the response read
#define EVT_UNTERMINATED (1u << 3)  // the host asked for a missing response
#define EVT_CLEAR        (1u << 4)  // INITIATE_CLEAR
#define EVT_ABORT_OUT    (1u << 5)  // INITIATE_ABORT_BULK_OUT
#define EVT_ALL                                                                \
    (EVT_RX | EVT_TX_DONE | EVT_INTERRUPTED | EVT_UNTERMINATED | EVT_CLEAR |   \
     EVT_ABORT_OUT)

static osEventFlagsId_t tmc_evt;

// host centric naming: out - PC to device, in - device to PC
// Command: filled by the TinyUSB thread, parsed by the SCPI thread after
// EVT_RX. Bulk-OUT stays NAKed while parsing, so they never overlap.
static uint8_t buffer_out[SCPI_INPUT_BUFFER_LENGTH];
static size_t  buffer_out_len;
static bool    rx_eom;

// Response: written by SCPI_Write while parsing, sent by the TinyUSB thread
// afterwards. out_len is the response length fixed after parsing, 0 if there
// is no response to send.
uint8_t                buffer_in[SCPI_OUTPUT_BUFFER_LENGTH];
size_t                 buffer_in_len;
static volatile size_t out_len;
static size_t          tx_ix;     // bytes of the response already sent
static size_t          tx_chunk;  // bytes in the bulk-IN transfer in flight

static volatile uint8_t stb_snapshot;  // STB for READ_STATUS_BYTE
static volatile bool    clear_done;

void usbtmc_app_init(void)
{
    tmc_evt = osEventFlagsNew(NULL);
}

// TinyUSB thread: the command is parsed, accept the next one
static void after_parse(void* param)
{
    (void)param;
    tud_usbtmc_start_bus_read();
}

static void response_drop(void)
{
    out_len       = 0;
    buffer_in_len = 0;
    SCPI_RegClearBits(&scpi_context, SCPI_REG_STB, STB_MAV);
}

static void command_parse(void)
{
    buffer_in_len = 0;
    if (buffer_out_len > 0) {
        SCPI_Input(&scpi_context, (const char*)buffer_out, (int)buffer_out_len);
    }
    // EOM is the IEEE 488.2 END: it terminates a message without a newline
    if (rx_eom && (scpi_context.buffer.position > 0)) {
        SCPI_Input(&scpi_context, NULL, 0);
    }

    if (osEventFlagsGet(tmc_evt) & EVT_CLEAR) {
        // Device clear came while parsing: the next iteration drops the
        // response, and the clear sequence re-arms bulk-OUT
        return;
    }

    out_len = buffer_in_len;
    if (out_len > 0) {
        SCPI_RegSetBits(&scpi_context, SCPI_REG_STB, STB_MAV);
    }
    usbd_defer_func(after_parse, NULL, false);
}

void usbtmc_app_task_iter(void)
{
    uint32_t evt =
        osEventFlagsWait(tmc_evt, EVT_ALL, osFlagsWaitAny, osWaitForever);
    if (evt & osFlagsError) {
        return;
    }

    if (evt & EVT_CLEAR) {
        // IEEE 488.2 device clear: input and output are dropped, status
        // registers and the error queue are kept
        scpi_context.buffer.position = 0;
        response_drop();
        evt &= ~EVT_RX;
    }
    if (evt & EVT_ABORT_OUT) {
        // Drop the part of a message received in earlier transfers
        scpi_context.buffer.position = 0;
    }
    if (evt & EVT_TX_DONE) {
        response_drop();
    }
    if (evt & EVT_INTERRUPTED) {
        response_drop();
        SCPI_ErrorPush(&scpi_context, SCPI_ERROR_QUERY_INTERRUPTED);
    }
    if (evt & EVT_UNTERMINATED) {
        SCPI_ErrorPush(&scpi_context, SCPI_ERROR_QUERY_UNTERMINATED);
    }
    if (evt & EVT_RX) {
        command_parse();
    }

    stb_snapshot = (uint8_t)SCPI_RegGet(&scpi_context, SCPI_REG_STB);
    if (evt & EVT_CLEAR) {
        clear_done = true;
    }
}

void tud_usbtmc_open_cb(uint8_t interface_id)
{
    (void)interface_id;
    buffer_out_len = 0;
    out_len        = 0;
    tx_ix          = 0;
    tx_chunk       = 0;
    osEventFlagsSet(tmc_evt, EVT_ABORT_OUT | EVT_TX_DONE);
    tud_usbtmc_start_bus_read();
}

#if (CFG_TUD_USBTMC_ENABLE_488)
const usbtmc_response_capabilities_488_t*
#else
const usbtmc_response_capabilities_t*
#endif
tud_usbtmc_get_capabilities_cb()
{
    return &tud_usbtmc_app_capabilities;
}

bool tud_usbtmc_msgBulkOut_start_cb(
    const usbtmc_msg_request_dev_dep_out* msgHeader
)
{
    if (msgHeader->TransferSize > sizeof(buffer_out)) {
        return false;
    }
    buffer_out_len = 0;
    rx_eom         = msgHeader->bmTransferAttributes.EOM;

    if (out_len > 0) {
        // A new message before the response is read discards the response
        // and reports Query INTERRUPTED (IEEE 488.2)
        out_len  = 0;
        tx_ix    = 0;
        tx_chunk = 0;
        osEventFlagsSet(tmc_evt, EVT_INTERRUPTED);
    }
    return true;
}

bool tud_usbtmc_msg_data_cb(void* data, size_t len, bool transfer_complete)
{
    if (len > (sizeof(buffer_out) - buffer_out_len)) {
        return false;  // buffer overflow!
    }
    memcpy(&buffer_out[buffer_out_len], data, len);
    buffer_out_len += len;

    if (transfer_complete) {
        // Bulk-OUT stays NAKed until the SCPI thread has parsed the command
        osEventFlagsSet(tmc_evt, EVT_RX);
    }
    else {
        tud_usbtmc_start_bus_read();
    }
    return true;
}

bool tud_usbtmc_msgBulkIn_request_cb(
    const usbtmc_msg_request_dev_dep_in* request
)
{
    if (out_len == 0) {
        // Nothing to send: Query UNTERMINATED (IEEE 488.2). The request
        // stays NAKed until the host times out and aborts it (USBTMC).
        osEventFlagsSet(tmc_evt, EVT_UNTERMINATED);
        return true;
    }

    tx_chunk = tu_min32(out_len - tx_ix, request->TransferSize);
    return tud_usbtmc_transmit_dev_msg_data(
        &buffer_in[tx_ix], tx_chunk, (tx_ix + tx_chunk) == out_len, false
    );
}

bool tud_usbtmc_msgBulkIn_complete_cb()
{
    tx_ix += tx_chunk;
    tx_chunk = 0;
    if (tx_ix >= out_len) {
        out_len = 0;
        tx_ix   = 0;
        osEventFlagsSet(tmc_evt, EVT_TX_DONE);
    }
    tud_usbtmc_start_bus_read();

    return true;
}

bool tud_usbtmc_initiate_clear_cb(uint8_t* tmcResult)
{
    *tmcResult     = USBTMC_STATUS_SUCCESS;
    buffer_out_len = 0;
    out_len        = 0;
    tx_ix          = 0;
    tx_chunk       = 0;
    clear_done     = false;
    osEventFlagsSet(tmc_evt, EVT_CLEAR);
    return true;
}

bool tud_usbtmc_check_clear_cb(usbtmc_get_clear_status_rsp_t* rsp)
{
    // Pending until the SCPI thread finishes the command it is parsing
    rsp->USBTMC_status =
        clear_done ? USBTMC_STATUS_SUCCESS : USBTMC_STATUS_PENDING;
    rsp->bmClear.BulkInFifoBytes = 0u;
    return true;
}

bool tud_usbtmc_initiate_abort_bulk_in_cb(uint8_t* tmcResult)
{
    *tmcResult = USBTMC_STATUS_SUCCESS;
    out_len    = 0;
    tx_ix      = 0;
    tx_chunk   = 0;
    osEventFlagsSet(tmc_evt, EVT_TX_DONE);
    return true;
}

bool tud_usbtmc_check_abort_bulk_in_cb(usbtmc_check_abort_bulk_rsp_t* rsp)
{
    (void)rsp;
    tud_usbtmc_start_bus_read();
    return true;
}

bool tud_usbtmc_initiate_abort_bulk_out_cb(uint8_t* tmcResult)
{
    *tmcResult     = USBTMC_STATUS_SUCCESS;
    buffer_out_len = 0;
    osEventFlagsSet(tmc_evt, EVT_ABORT_OUT);
    return true;
}

bool tud_usbtmc_check_abort_bulk_out_cb(usbtmc_check_abort_bulk_rsp_t* rsp)
{
    (void)rsp;
    tud_usbtmc_start_bus_read();
    return true;
}

void tud_usbtmc_bulkIn_clearFeature_cb(void) {}

void tud_usbtmc_bulkOut_clearFeature_cb(void)
{
    tud_usbtmc_start_bus_read();
}

// Return status byte, but put the transfer result status code in the rspResult
// argument.
uint8_t tud_usbtmc_get_stb_cb(uint8_t* tmcResult)
{
    *tmcResult = USBTMC_STATUS_SUCCESS;
    // TODO: send the USB488 SRQ notification (bNotify1 = 0x81, STB) on
    // interrupt-IN when RQS rises (SCPI_CTRL_SRQ), and return RQS instead of
    // MSS in bit 6. tud_usbtmc_transmit_notification_data() in TinyUSB 0.19.0
    // checks usbd_edpt_busy() without negation and never sends, so for now
    // the host has to poll READ_STATUS_BYTE.
    return stb_snapshot;
}

bool tud_usbtmc_indicator_pulse_cb(
    const tusb_control_request_t* msg, uint8_t* tmcResult
)
{
    (void)msg;
    *tmcResult = USBTMC_STATUS_SUCCESS;
    return true;
}
