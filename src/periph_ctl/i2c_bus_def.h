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

/*
 * Limits of the settings of the I2C bus.
 *
 * Internal to the module: include it from the .c files of the bus only, the
 * public interface of the backend is i2c_bus.h.
 */

/// Supported range of the transaction timeout in milliseconds
#define I2C_BUS_TIMEOUT_MIN 1
#define I2C_BUS_TIMEOUT_MAX 10000

/// Max unshifted address of a slave, per width of the address
#define I2C_BUS_ADDR_MAX_7BIT  0x7F
#define I2C_BUS_ADDR_MAX_10BIT 0x3FF
