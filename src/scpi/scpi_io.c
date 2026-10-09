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

#include "scpi_io.h"

#include "scpi/scpi.h"
#include "scpi_def.h"
#include <stdbool.h>
#include <string.h>

#define SCPI_ERROR_QUEUE_SIZE 17

static scpi_t       scpi_context;
static scpi_error_t scpi_error_queue_data[SCPI_ERROR_QUEUE_SIZE];
// libscpi line buffer, one byte is kept for the '\0' the parser needs
static char         scpi_input_buffer[SCPI_INPUT_BUFFER_LENGTH];

// Response to the host: written by SCPI_Write while parsing, read by the USB
// class until scpi_io_response_drop
static uint8_t response[SCPI_OUTPUT_BUFFER_LENGTH];
static size_t  response_len;
static bool    output_overflow;

static size_t SCPI_Write(scpi_t* context, const char* data, size_t len)
{
    size_t room = SCPI_OUTPUT_BUFFER_LENGTH - response_len;

    if (response_len == 0) {
        output_overflow = false;
    }
    if (len > room) {
        // Report once per response, the rest of it is dropped
        len = room;
        if (!output_overflow) {
            output_overflow = true;
            SCPI_ErrorPush(context, SCPI_ERROR_OUT_OF_MEMORY_FOR_REQ_OP);
        }
    }
    memcpy(response + response_len, data, len);
    response_len += len;
    return len;
}

static scpi_result_t SCPI_Flush(scpi_t* context)
{
    (void)context;
    return SCPI_RES_OK;
}

static int SCPI_Error(scpi_t* context, int_fast16_t err)
{
    (void)context;
    (void)err;
    return SCPI_RES_OK;
}

static scpi_result_t SCPI_Control(
    scpi_t* context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val
)
{
    // TODO: on SCPI_CTRL_SRQ send the USB488 SRQ notification on
    // interrupt-IN, blocked by a TinyUSB 0.19.0 bug (see tud_usbtmc_get_stb_cb)
    (void)context;
    (void)ctrl;
    (void)val;
    return SCPI_RES_OK;
}

static scpi_interface_t scpi_interface = {
    .error   = SCPI_Error,
    .write   = SCPI_Write,
    .control = SCPI_Control,
    .flush   = SCPI_Flush,
    .reset   = SCPI_Reset,
};

void scpi_io_init(void)
{
    SCPI_Init(
        &scpi_context,
        scpi_commands,
        &scpi_interface,
        scpi_units_def,
        SCPI_IDN1,
        SCPI_IDN2,
        SCPI_IDN3,
        SCPI_IDN4,
        scpi_input_buffer,
        SCPI_INPUT_BUFFER_LENGTH,
        scpi_error_queue_data,
        SCPI_ERROR_QUEUE_SIZE
    );
}

void scpi_io_input(const char* data, size_t len)
{
    response_len = 0;
    if (len > 0) {
        SCPI_Input(&scpi_context, data, (int)len);
    }
}

void scpi_io_input_end(void)
{
    // Empty if the message ended with a newline or was dropped on overrun
    if (scpi_context.buffer.position > 0) {
        SCPI_Input(&scpi_context, NULL, 0);
    }
}

void scpi_io_input_overrun(void)
{
    response_len = 0;
    SCPI_ErrorPush(&scpi_context, SCPI_ERROR_INPUT_BUFFER_OVERRUN);
}

size_t scpi_io_response(const uint8_t** data)
{
    *data = response;
    if (response_len > 0) {
        SCPI_RegSetBits(&scpi_context, SCPI_REG_STB, STB_MAV);
    }
    return response_len;
}

void scpi_io_response_drop(void)
{
    response_len = 0;
    SCPI_RegClearBits(&scpi_context, SCPI_REG_STB, STB_MAV);
}

void scpi_io_device_clear(void)
{
    scpi_context.buffer.position = 0;
    scpi_io_response_drop();
}

void scpi_io_query_interrupted(void)
{
    scpi_io_response_drop();
    SCPI_ErrorPush(&scpi_context, SCPI_ERROR_QUERY_INTERRUPTED);
}

void scpi_io_query_unterminated(void)
{
    SCPI_ErrorPush(&scpi_context, SCPI_ERROR_QUERY_UNTERMINATED);
}

uint8_t scpi_io_stb(void)
{
    return (uint8_t)SCPI_RegGet(&scpi_context, SCPI_REG_STB);
}
