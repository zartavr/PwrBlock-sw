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

#include "monitor_task.h"

#include "regulator/regulator_task.h"

#include "bsp/drivers/exti/exti.h"
#include "bsp/drivers/ina229/ina229.h"

#include "sys_status/sys_status.h"

#include <cmsis_os2.h>

#define MONITOR_FUSE_CURRENT_MIN     (1U)
#define MONITOR_FUSE_CURRENT_MAX     (5000U)
#define MONITOR_FUSE_CURRENT_DEFAULT (100U)

#define MONITOR_SHUNT_OHM         (0.030F)
#define MONITOR_SHUNT_MAX_CURRENT (5.0F)

#define MONITOR_SPI_TIMEOUT_MS   (100)
#define MONITOR_MUTEX_TIMEOUT_MS (5)
#define MONITOR_MEASURE_DELAY_MS (60)

typedef enum
{
    MONITOR_STATE_CHECK_ALERT,
    MONITOR_STATE_MEASURE,
} MonitorState;

typedef struct
{
    uint32_t vbus_mv;
    uint32_t vshunt_mv;
    uint32_t current_ma;
    uint32_t power_mw;
    uint32_t fuse_current_ma;
    uint32_t dietemp_mc;
    bool     is_fuse_en;
} Monitor;

static Monitor      mon;
static MonitorState fsm_state;

// Written from EXTI ISR context, read from the monitor task loop
static volatile bool is_alert;

static osSemaphoreId_t monitor_sem;
static osMutexId_t     monitor_mtx;

// Private prototypes
static void fsm_check_alert(void);
static void fsm_measure(void);

static bool init(void);
static bool read(Ina229RegisterName reg, uint8_t* rx_data);
static bool write(Ina229RegisterName reg, uint8_t* tx_data);

static void spi_txrx_cplt_cb(void);

static void alert_set_cb(void);
static void alert_clear_cb(void);

void monitor_app_task(void* argument)
{
    (void)argument;

    monitor_sem = osSemaphoreNew(1, 0, NULL);
    monitor_mtx = osMutexNew(NULL);

    // Init driver
    init();

    // Register alert pin callbacks
    exti_register_cb(EXTI_CONSUMER_INA229, alert_clear_cb, alert_set_cb);

    // Left only for debug purposes
    uint8_t mnf_id[2] = {0};
    uint8_t dev_id[2] = {0};
    read(INA229_REG_MANUFACTURER_ID, mnf_id);
    read(INA229_REG_DEVICE_ID, dev_id);

    // Init FSM
    fsm_state = MONITOR_STATE_CHECK_ALERT;

    while (true) {
        switch (fsm_state) {
            case MONITOR_STATE_CHECK_ALERT: {
                fsm_check_alert();
                break;
            }
            case MONITOR_STATE_MEASURE: {
                fsm_measure();
                osDelay(MONITOR_MEASURE_DELAY_MS);
                break;
            }
            default: break;
        }
    }
}

uint32_t monitor_vsbus_get(void)
{
    uint32_t val = 0;
    if (osMutexAcquire(monitor_mtx, MONITOR_MUTEX_TIMEOUT_MS) == osOK) {
        val = mon.vbus_mv;
        osMutexRelease(monitor_mtx);
    }
    return val;
}

uint32_t monitor_current_get(void)
{
    uint32_t val = 0;
    if (osMutexAcquire(monitor_mtx, MONITOR_MUTEX_TIMEOUT_MS) == osOK) {
        val = mon.current_ma;
        osMutexRelease(monitor_mtx);
    }
    return val;
}

uint32_t monitor_vshunt_get(void)
{
    uint32_t val = 0;
    if (osMutexAcquire(monitor_mtx, MONITOR_MUTEX_TIMEOUT_MS) == osOK) {
        val = mon.vshunt_mv;
        osMutexRelease(monitor_mtx);
    }
    return val;
}

uint32_t monitor_power_get(void)
{
    uint32_t val = 0;
    if (osMutexAcquire(monitor_mtx, MONITOR_MUTEX_TIMEOUT_MS) == osOK) {
        val = mon.power_mw;
        osMutexRelease(monitor_mtx);
    }
    return val;
}

uint32_t monitor_dietemp_get(void)
{
    uint32_t val = 0;
    if (osMutexAcquire(monitor_mtx, MONITOR_MUTEX_TIMEOUT_MS) == osOK) {
        val = mon.dietemp_mc;
        osMutexRelease(monitor_mtx);
    }
    return val;
}

void monitor_fuse_state_set(bool state)
{
    if (osMutexAcquire(monitor_mtx, MONITOR_MUTEX_TIMEOUT_MS) == osOK) {
        mon.is_fuse_en = state;
        osMutexRelease(monitor_mtx);
    }
}

bool monitor_fuse_state_get(void)
{
    bool val = 0;
    if (osMutexAcquire(monitor_mtx, MONITOR_MUTEX_TIMEOUT_MS) == osOK) {
        val = mon.is_fuse_en;
        osMutexRelease(monitor_mtx);
    }
    return val;
}

void monitor_fuse_current_set(uint32_t val)
{
    // Validating
    if (val > MONITOR_FUSE_CURRENT_MAX) {
        val = MONITOR_FUSE_CURRENT_MAX;
    }
    else if (val < MONITOR_FUSE_CURRENT_MIN) {
        val = MONITOR_FUSE_CURRENT_MIN;
    }

    if (osMutexAcquire(monitor_mtx, MONITOR_MUTEX_TIMEOUT_MS) == osOK) {
        mon.fuse_current_ma = val;
        osMutexRelease(monitor_mtx);

        // Calc overvoltage on shunt for selected current
        float vshunt_thr = ((float)val / 1000.0F) * MONITOR_SHUNT_OHM;
        // Update monitor driver threshold
        uint8_t sovl[2];
        ina229_serialize_sovl(vshunt_thr, sovl);
        write(INA229_REG_SOVL, sovl);
    }
}

uint32_t monitor_fuse_current_get(void)
{
    uint32_t val = 0;
    if (osMutexAcquire(monitor_mtx, MONITOR_MUTEX_TIMEOUT_MS) == osOK) {
        val = mon.fuse_current_ma;
        osMutexRelease(monitor_mtx);
    }
    return val;
}

uint32_t monitor_fuse_current_min_get(void)
{
    return MONITOR_FUSE_CURRENT_MIN;
}

uint32_t monitor_fuse_current_max_get(void)
{
    return MONITOR_FUSE_CURRENT_MAX;
}

void monitor_fuse_clear(void)
{
    sys_status_set(SYS_STATUS_SRC_MONITOR, SYS_STATUS_OK);
}

static bool init(void)
{
    // Config internals of driver for correct calculation
    ina229_init(MONITOR_SHUNT_MAX_CURRENT, spi_txrx_cplt_cb);

    uint8_t ctrl[2];

    // Send soft-reset command
    if (write(INA229_REG_CONFIG, ctrl) == false) {
        return false;
    }

    // Wait for driver reset
    osDelay(20);

    // Adc config
    Ina229ADCcfg adc_cfg = {
        .avg    = INA229_ADC_AVG_16,
        .mode   = INA229_ADC_MODE_CONT_VBUS_VSUNT_DIETEMP,
        .vbusct = INA229_ADC_CONV_T_1052_US,
        .vshct  = INA229_ADC_CONV_T_1052_US,
        .vtct   = INA229_ADC_CONV_T_1052_US,
    };
    ina229_serialize_adc_cfg(adc_cfg, ctrl);
    if (write(INA229_REG_ADC_CONFIG, ctrl) == false) {
        return false;
    }

    // Send SHUNT_CAL value
    ina229_serialize_shunt_cal(MONITOR_SHUNT_OHM, ctrl);
    if (write(INA229_REG_SHUNT_CAL, ctrl) == false) {
        return false;
    }

    // Default fuse config
    monitor_fuse_state_set(false);
    monitor_fuse_current_set(MONITOR_FUSE_CURRENT_MAX);

    // Default alert state
    is_alert = false;

    return true;
}

static bool read(Ina229RegisterName reg, uint8_t* rx_data)
{
    if (ina229_reg_read(reg, rx_data) == false) {
        // Fail on communication transaction start
        return false;
    }

    // Suspen current thread until semaphore ready: waiting spi transaction done
    if (osSemaphoreAcquire(monitor_sem, MONITOR_SPI_TIMEOUT_MS) != osOK) {
        // Fail on timeout
        ina229_abort();
        return false;
    }

    // If spi transaction done -> data ready at rx_data
    return true;
}

static bool write(Ina229RegisterName reg, uint8_t* tx_data)
{
    if (ina229_reg_write(reg, tx_data) == false) {
        // Fail on communication transaction start
        return false;
    }

    if (osSemaphoreAcquire(monitor_sem, MONITOR_SPI_TIMEOUT_MS) != osOK) {
        // Fail on timeout
        ina229_abort();
        return false;
    }

    return true;
}

static void spi_txrx_cplt_cb(void)
{
    osSemaphoreRelease(monitor_sem);
}

static void fsm_check_alert(void)
{
    uint8_t raw_diag_alrt[2];
    if (is_alert) {

        bool read_complete = read(INA229_REG_DIAG_ALRT, raw_diag_alrt);

        if (read_complete) {
            // Check shunt overvoltage threshold alert
            if (mon.is_fuse_en) {
                if (ina229_diag_check(raw_diag_alrt, INA229_DIAG_SHNTOL)) {
                    // System status update
                    sys_status_set(SYS_STATUS_SRC_MONITOR, SYS_STATUS_FAULT);
                }
            }
        }
    }

    fsm_state = MONITOR_STATE_MEASURE;
}

static void fsm_measure(void)
{
    uint8_t raw_vshunt[3];
    uint8_t raw_vbus[3];
    uint8_t raw_curr[3];
    uint8_t raw_power[3];
    uint8_t raw_dietemp[2];

    bool read_complete = true;
    read_complete &= read(INA229_REG_VBUS, raw_vbus);
    read_complete &= read(INA229_REG_VSHUNT, raw_vshunt);
    read_complete &= read(INA229_REG_CURRENT, raw_curr);
    read_complete &= read(INA229_REG_POWER, raw_power);
    read_complete &= read(INA229_REG_DIETEMP, raw_dietemp);

    if (read_complete) {
        osMutexAcquire(monitor_mtx, osWaitForever);

        mon.vbus_mv   = (uint32_t)ina229_deserialize_vbus(raw_vbus);
        mon.vshunt_mv = (uint32_t)ina229_deserialize_vshunt(raw_vshunt);
        mon.current_ma =
            (uint32_t)(ina229_deserialize_current(raw_curr) * 1000);
        mon.power_mw   = (uint32_t)(ina229_deserialize_power(raw_power) * 1000);
        mon.dietemp_mc = (uint32_t)(ina229_deserialize_dietemp(raw_dietemp));

        osMutexRelease(monitor_mtx);
    }

    fsm_state = MONITOR_STATE_CHECK_ALERT;
}

static void alert_set_cb(void)
{
    is_alert = true;
}

static void alert_clear_cb(void)
{
    is_alert = false;
}
