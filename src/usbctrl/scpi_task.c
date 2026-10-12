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

#include "scpi/scpi_io.h"
#include "usb_class.h"

#include <stdbool.h>

void scpi_parser_app_init(void)
{
    scpi_io_init();
}

void scpi_parser_app_task(void* argument)
{
    (void)argument;

    while (true) {
        usb_class_scpi_iter();
    }
}
