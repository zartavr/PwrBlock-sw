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

#include "regulator_task.h"

#include "regulator.h"
#include "sys_status/sys_status.h"

#include "storage/preset.h"

#include <cmsis_os2.h>

#define REGULATOR_QUEUE_READ_TIMEOUT      (100)
#define REGULATOR_QUEUE_SIZE              (5U)
#define REGULATOR_DEFAULT_MEM_CELL_RECALL (1U)

typedef enum
{
    REGULATOR_CMD_INIT,
    REGULATOR_CMD_DEINIT,
    REGULATOR_CMD_SET_OUT_EN,
    REGULATOR_CMD_SET_V,
    REGULATOR_CMD_SET_I,
    REGULATOR_CMD_PRESET_RECALL,
    REGULATOR_CMD_PRESET_STORE,
    REGULATOR_CMD_V_SLOPE_SET,
    REGULATOR_CMD_V_OFFSET_SET,
    REGULATOR_CMD_I_SLOPE_SET,
    REGULATOR_CMD_I_OFFSET_SET,
    REGULATOR_CMD_CALIB_CONST_WP_SET,
    REGULATOR_CMD_CALIB_CONST_STORE,
} RegulatorCmd;

typedef struct
{
    RegulatorCmd cmd;

    union
    {
        uint32_t u;
        float    f;
    } value;
} RegulatorQueueMessage;

typedef enum
{
    REGULATOR_STATE_CHECK_QUEUE,
    REGULATOR_STATE_POLL_DRV,
    REGULATOR_STATE_ERR,
} RegulatorState;

static osMessageQueueId_t regulator_q;
static RegulatorState     fsm_state;

// Private prototypes
static void regulator_fsm_check_queue(void);
static void regulator_fsm_poll_drv(void);
static void regulator_fsm_err(void);
static void regulator_on_mode_change_cb(void);

void regulator_app_task(void* argument)
{
    (void)argument;

    // Init task queue
    regulator_q = osMessageQueueNew(
        REGULATOR_QUEUE_SIZE, sizeof(RegulatorQueueMessage), NULL
    );

    // Init driver instance
    RegulatorCallbacks callbacks = {
        .on_mode_change = regulator_on_mode_change_cb,
    };
    regulator_drv_init(&callbacks);

    // Init storage of presets
    preset_init();

    // Init FSM
    fsm_state = REGULATOR_STATE_CHECK_QUEUE;

    while (true) {
        switch (fsm_state) {
            case REGULATOR_STATE_CHECK_QUEUE: {
                regulator_fsm_check_queue();
                break;
            }

            case REGULATOR_STATE_POLL_DRV: {
                regulator_fsm_poll_drv();
                break;
            }

            case REGULATOR_STATE_ERR: {
                regulator_fsm_err();
                break;
            }
            default: break;
        }
    }
}

void regulator_init(void)
{
    RegulatorQueueMessage msg = {.cmd = REGULATOR_CMD_INIT, .value.u = 0};
    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

void regulator_deinit(void)
{
    RegulatorQueueMessage msg = {.cmd = REGULATOR_CMD_DEINIT, .value.u = 0};
    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

void regulator_v_set(uint32_t val_mv)
{
    RegulatorQueueMessage msg = {.cmd = REGULATOR_CMD_SET_V, .value.u = val_mv};
    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

void regulator_preset_recall(uint8_t preset_id)
{
    RegulatorQueueMessage msg = {
        .cmd = REGULATOR_CMD_PRESET_RECALL, .value.u = preset_id
    };
    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

void regulator_preset_store(uint8_t preset_id)
{
    RegulatorQueueMessage msg = {
        .cmd = REGULATOR_CMD_PRESET_STORE, .value.u = preset_id
    };
    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

uint32_t regulator_v_get(void)
{
    return regulator_drv_vout_get();
}

uint32_t regulator_v_max_get(void)
{
    return regulator_drv_v_max_get();
}

uint32_t regulator_v_min_get(void)
{
    return regulator_drv_v_min_get();
}

uint8_t regulator_status_get(void)
{
    return regulator_drv_status_get();
}

uint8_t regulator_status_clear(void)
{
    return regulator_drv_clear_faults();
}

void regulator_i_set(uint32_t val_ma)
{
    RegulatorQueueMessage msg = {.cmd = REGULATOR_CMD_SET_I, .value.u = val_ma};
    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

uint32_t regulator_i_get(void)
{
    return regulator_drv_ilim_get();
}

uint32_t regulator_i_max_get(void)
{
    return regulator_drv_i_max_get();
}

uint32_t regulator_i_min_get(void)
{
    return regulator_drv_i_min_get();
}

void regulator_output_set(bool val)
{
    RegulatorQueueMessage msg = {
        .cmd = REGULATOR_CMD_SET_OUT_EN, .value.u = val
    };
    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

bool regulator_output_get(void)
{
    return regulator_drv_output_get();
}

void regulator_output_toggle(void)
{
    RegulatorQueueMessage msg;
    msg.cmd     = REGULATOR_CMD_SET_OUT_EN;
    msg.value.u = regulator_drv_output_get() ? false : true;

    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

bool regulator_calib_wp_get(void)
{
    return regulator_drv_calib_is_write_prot_get();
}

void regulator_calib_wp_set(bool state)
{
    RegulatorQueueMessage msg = {
        .cmd = REGULATOR_CMD_CALIB_CONST_WP_SET, .value.u = state
    };
    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

void regulator_calib_v_slope_set(float val)
{
    RegulatorQueueMessage msg = {
        .cmd = REGULATOR_CMD_V_SLOPE_SET, .value.f = val
    };
    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

float regulator_calib_v_slope_get(void)
{
    return regulator_drv_calib_v_slope_get();
}

void regulator_calib_v_offset_set(float val)
{
    RegulatorQueueMessage msg = {
        .cmd = REGULATOR_CMD_V_OFFSET_SET, .value.f = val
    };
    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

float regulator_calib_v_offset_get(void)
{
    return regulator_drv_calib_v_offset_get();
}

void regulator_calib_i_slope_set(float val)
{
    RegulatorQueueMessage msg = {
        .cmd = REGULATOR_CMD_I_SLOPE_SET, .value.f = val
    };
    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

float regulator_calib_i_slope_get(void)
{
    return regulator_drv_calib_i_slope_get();
}

void regulator_calib_i_offset_set(float val)
{
    RegulatorQueueMessage msg = {
        .cmd = REGULATOR_CMD_I_OFFSET_SET, .value.f = val
    };
    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

float regulator_calib_i_offset_get(void)
{
    return regulator_drv_calib_i_offset_get();
}

void regulator_calib_store(void)
{
    RegulatorQueueMessage msg = {
        .cmd = REGULATOR_CMD_CALIB_CONST_STORE, .value.u = 0
    };
    osMessageQueuePut(regulator_q, &msg, 0U, 0U);
}

// FSM states
static void regulator_fsm_check_queue(void)
{
    RegulatorQueueMessage msg;
    osStatus_t            status = osMessageQueueGet(
        regulator_q, &msg, NULL, REGULATOR_QUEUE_READ_TIMEOUT
    );
    if (status == osOK) {
        switch (msg.cmd) {
            case REGULATOR_CMD_INIT: {
                // Soft start delay
                osDelay(500);

                // Recall preset upfront so setup can soft-start straight to
                // its targets instead of DVS-ramping to them right after the
                // power stage is enabled
                Preset_t preset;
                preset_read(&preset, REGULATOR_DEFAULT_MEM_CELL_RECALL);

                // Default setup of DC-DC regulator
                if (regulator_drv_setup(preset.cell.v_out, preset.cell.i_out) ==
                    osOK) {
                    // Drop own queue
                    osMessageQueueReset(regulator_q);
                    sys_status_set(SYS_STATUS_SRC_REGULATOR, SYS_STATUS_OK);
                }
                break;
            }
            case REGULATOR_CMD_DEINIT: {
                // Drop instance
                regulator_drv_reset();
                // Status update
                sys_status_set(SYS_STATUS_SRC_REGULATOR, SYS_STATUS_OFF);
                break;
            }
            case REGULATOR_CMD_SET_OUT_EN: {
                if (sys_status_get(SYS_STATUS_SRC_MONITOR) !=
                    SYS_STATUS_FAULT) {
                    regulator_drv_output_set((bool)msg.value.u);
                }
                break;
            };
            case REGULATOR_CMD_SET_V: {
                regulator_drv_vout_set(msg.value.u);
                break;
            }
            case REGULATOR_CMD_SET_I: {
                regulator_drv_ilim_set(msg.value.u);
                break;
            }
            case REGULATOR_CMD_PRESET_RECALL: {
                Preset_t preset;
                preset_read(&preset, msg.value.u);
                // Update regulator
                regulator_v_set(preset.cell.v_out);
                regulator_i_set(preset.cell.i_out);
                break;
            }
            case REGULATOR_CMD_PRESET_STORE: {
                Preset_t preset;
                preset.cell.v_out = regulator_drv_vout_get();
                preset.cell.i_out = regulator_drv_ilim_get();
                preset_write(&preset, msg.value.u);
                break;
            }
            case REGULATOR_CMD_V_SLOPE_SET: {
                regulator_drv_calib_v_slope_set(msg.value.f);
                break;
            }
            case REGULATOR_CMD_V_OFFSET_SET: {
                regulator_drv_calib_v_offset_set(msg.value.f);
                break;
            }
            case REGULATOR_CMD_I_SLOPE_SET: {
                regulator_drv_calib_i_slope_set(msg.value.f);
                break;
            }
            case REGULATOR_CMD_I_OFFSET_SET: {
                regulator_drv_calib_i_offset_set(msg.value.f);
                break;
            }
            case REGULATOR_CMD_CALIB_CONST_STORE: {
                regulator_drv_calib_store();
                break;
            }
            case REGULATOR_CMD_CALIB_CONST_WP_SET: {
                regulator_drv_calib_is_write_prot_set((bool)msg.value.u);
                break;
            }
            default: break;
        }

        fsm_state = REGULATOR_STATE_POLL_DRV;
    }
    else if (status == osErrorTimeout) {
        // Empty queue
        fsm_state = REGULATOR_STATE_POLL_DRV;
    }
    else {
        // Any error
        fsm_state = REGULATOR_STATE_ERR;
    }
}

static void regulator_fsm_poll_drv(void)
{
    // No power-off flag
    // Poll status register from DC-DC driver
    if (regulator_drv_mode_poll() == osError) {
        fsm_state = REGULATOR_STATE_ERR;
    }

    fsm_state = REGULATOR_STATE_CHECK_QUEUE;
}

static void regulator_fsm_err(void)
{
    sys_status_set(SYS_STATUS_SRC_REGULATOR, SYS_STATUS_FAULT);
}

// Callbacks
static void regulator_on_mode_change_cb(void)
{
    // Update system status state
    switch (regulator_drv_mode_get()) {
        case REGULATOR_DRV_OUT_MODE_OFF: {
            sys_status_set(SYS_STATUS_SRC_REGULATOR, SYS_STATUS_OFF);
            break;
        }
        case REGULATOR_DRV_OUT_MODE_CC: {
            sys_status_set(SYS_STATUS_SRC_REGULATOR, SYS_STATUS_CC);
            break;
        }
        case REGULATOR_DRV_OUT_MODE_CV: {
            sys_status_set(SYS_STATUS_SRC_REGULATOR, SYS_STATUS_CV);
            break;
        }
        default: break;
    }
}
