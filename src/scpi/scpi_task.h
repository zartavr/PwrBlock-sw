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

#pragma once

// SCPI parser thread: the IEEE 488.2 message exchange. Waits for transport
// events, parses messages with libscpi and hands responses back. The only
// thread that calls scpi_io and scpi_app.

// Initializes libscpi and the transport, call before the scheduler starts
void scpi_parser_app_init(void);
void scpi_parser_app_task(void* argument);
