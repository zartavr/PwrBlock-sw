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

#include "scpi_task.h"

#include "scpi_io.h"
#include "transport_scpi.h"

#include <stdbool.h>

void scpi_parser_app_init(void)
{
    scpi_io_init();
    scpi_transport_init();
}

static void message_parse(void)
{
    const uint8_t* msg;
    bool           end;
    bool           overrun;
    size_t         len = scpi_transport_req_get(&msg, &end, &overrun);

    if (overrun) {
        scpi_io_input_overrun();
        return;
    }
    scpi_io_input((const char*)msg, len);
    if (end) {
        // EOM is the IEEE 488.2 END: it terminates a message without a
        // newline
        scpi_io_input_end();
    }
}

void scpi_parser_app_task(void* argument)
{
    (void)argument;

    while (true) {
        uint32_t evt = scpi_transport_evt_wait();

        if (evt & SCPI_TP_CLEAR) {
            // IEEE 488.2 device clear: input and output are dropped, status
            // registers and the error queue are kept. A message lent before
            // the clear is dropped too, the transport takes its buffer back.
            scpi_io_device_clear();
            evt &= ~SCPI_TP_MSG;
        }
        if (evt & SCPI_TP_RESP_DONE) {
            scpi_io_response_drop();
        }
        if (evt & SCPI_TP_INTERRUPTED) {
            scpi_io_query_interrupted();
        }
        if (evt & SCPI_TP_UNTERMINATED) {
            scpi_io_query_unterminated();
        }
        if (evt & SCPI_TP_MSG) {
            const uint8_t* resp;

            message_parse();
            size_t len = scpi_io_response(&resp);
            scpi_transport_resp_set(resp, len);
        }

        scpi_transport_stb_set(scpi_io_stb());
        if (evt & SCPI_TP_CLEAR) {
            scpi_transport_clear_done();
        }
    }
}
