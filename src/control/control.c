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

#include "control.h"

#include "monitor/monitor_task.h"
#include "regulator/regulator_task.h"

#include "ntc/ntc.h"

// Capability text string
const char CAPABILITY[] = "DCPSUPPLY";

// Internals of power supply
bool is_local_block = false;
bool is_rw_block    = false;

scpi_result_t SCPI_stub_callback(scpi_t* context)
{
    (void)context;
    return SCPI_RES_OK;
}

scpi_result_t SCPI_Output(scpi_t* context)
{
    bool state = false;

    // Read first parameter if present
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }
    // Update sate of DC-DC driver via thread
    regulator_output_set(state);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_OutputQ(scpi_t* context)
{
    bool val = regulator_output_get();
    // Return output state
    SCPI_ResultBool(context, val);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SystemCapabilityQ(scpi_t* context)
{
    // Return capability text info
    SCPI_ResultCharacters(context, CAPABILITY, sizeof(CAPABILITY));
    return SCPI_RES_OK;
}

scpi_result_t SCPI_Current(scpi_t* context)
{
    // Read first parameter if present
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (param.unit != SCPI_UNIT_AMPER && param.unit != SCPI_UNIT_NONE) {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
        return SCPI_RES_ERR;
    }

    // Convert from base units: amperes, to integer value in milliamperes
    uint32_t val_ma = (uint32_t)((float)param.content.value * 1000.0F);

    // Update Ilim of DC-DC driver via thread
    regulator_i_set(val_ma);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CurrentQ(scpi_t* context)
{
    uint32_t val = 0;

    // Read first parameter if present: map to scpi_special_numbers_def
    scpi_number_t par;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &par, FALSE)) {
        // If no parameter, than reply current val of current
        val = regulator_i_get();
    }
    else {
        // Select by special number descriptor
        switch (par.content.tag) {
            case SCPI_NUM_MIN: {
                val = regulator_i_min_get();
                break;
            }
            case SCPI_NUM_MAX: {
                val = regulator_i_max_get();
                break;
            }
            default: {
                return SCPI_RES_ERR;
            }
        }
    }

    // Conver from internal units: milliamperes, to base unit: amperes
    // Reply to query
    SCPI_ResultFloat(context, (float)val / 1000.0F);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_Voltage(scpi_t* context)
{
    // Read first parameter if present
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (param.unit != SCPI_UNIT_VOLT && param.unit != SCPI_UNIT_NONE) {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
        return SCPI_RES_ERR;
    }

    // Convert from base units: volts, to integer value in millivolts
    uint32_t val_mv = (uint32_t)((float)param.content.value * 1000.0F);

    // Update Vout of DC-DC driver via thread
    regulator_v_set(val_mv);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_VoltageQ(scpi_t* context)
{
    uint32_t val = 0;

    // Read first parameter if present: map to scpi_special_numbers_def
    scpi_number_t par;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &par, FALSE)) {
        // If no parameter, than reply current val of voltage
        val = regulator_v_get();
    }
    else {
        // Select by special number descriptor
        switch (par.content.tag) {
            case SCPI_NUM_MIN: {
                val = regulator_v_min_get();
                break;
            }
            case SCPI_NUM_MAX: {
                val = regulator_v_max_get();
                break;
            }
            default: {
                return SCPI_RES_ERR;
            }
        }
    }

    // Conver from internal units: millivolts, to base unit: volts
    // Reply to query
    SCPI_ResultFloat(context, (float)val / 1000.0F);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_MeasureCurrentQ(scpi_t* context)
{
    uint32_t val_ma = monitor_current_get();
    // Conver from internal units: milliamperes, to base unit: amperes
    // Reply to query
    SCPI_ResultFloat(context, (float)val_ma / 1000.0F);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_MeasurePowerQ(scpi_t* context)
{
    uint32_t val_mw = monitor_power_get();
    // Conver from internal units: milliwatts, to base unit: watts
    // Reply to query
    SCPI_ResultFloat(context, (float)val_mw / 1000.0F);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_MeasureVoltageQ(scpi_t* context)
{
    uint32_t val_mv = monitor_vsbus_get();
    // Conver from internal units: millivolts, to base unit: volts
    // Reply to query
    SCPI_ResultFloat(context, (float)val_mv / 1000.0F);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_MeasureTemperatureShuntQ(scpi_t* context)
{
    uint32_t val_mc = monitor_dietemp_get();
    // Conver from units: millidegrees Celsius, to base unit: degree Celsius
    // Reply to query
    SCPI_ResultFloat(context, (float)val_mc / 1000.0F);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_MeasureTemperatureTerminalQ(scpi_t* context)
{
    uint32_t val_mc = ntc_temp_get();
    // Reply to query
    SCPI_ResultFloat(context, (float)val_mc / 1000.0F);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SystemLocal(scpi_t* context)
{
    (void)context;

    // TODO: implement in future
    // Update control state
    is_local_block = false;
    is_rw_block    = false;
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SystemRemote(scpi_t* context)
{
    (void)context;

    // TODO: implement in future
    // Update control state
    is_local_block = true;
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SystemRWlock(scpi_t* context)
{
    (void)context;

    // TODO: implement in future
    // Update control state
    is_local_block = true;
    is_rw_block    = true;
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SaveState(scpi_t* context)
{
    uint32_t cell_id;

    // Read first parameter if present
    if (!SCPI_ParamUInt32(context, &cell_id, TRUE)) {
        return SCPI_RES_ERR;
    }

    // Process save sate to selected memory cell
    regulator_preset_store(cell_id);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_RecallState(scpi_t* context)
{
    uint32_t cell_id;

    // Read first parameter if present
    if (!SCPI_ParamUInt32(context, &cell_id, TRUE)) {
        return SCPI_RES_ERR;
    }

    // Process restore sate from selected memory cell
    regulator_preset_recall(cell_id);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CurrentProtectionState(scpi_t* context)
{
    bool state = false;

    // Read first parameter if present
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    // Update sate of fuse mode monitor
    monitor_fuse_state_set(state);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CurrentProtectionStateQ(scpi_t* context)
{
    // Get current fuse mode of monitor
    bool val = monitor_fuse_state_get();
    // Return output state
    SCPI_ResultBool(context, val);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CurrentProtectionLevel(scpi_t* context)
{
    // Read first parameter if present
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (param.unit != SCPI_UNIT_AMPER && param.unit != SCPI_UNIT_NONE) {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
        return SCPI_RES_ERR;
    }

    // Convert from base units: amperes, to integer value in milliamperes
    uint32_t val_ma = (uint32_t)((float)param.content.value * 1000.0F);

    // Update limit of fuse mode in monitor
    monitor_fuse_current_set(val_ma);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CurrentProtectionLevelQ(scpi_t* context)
{
    uint32_t val = 0;

    // Read first parameter if present: map to scpi_special_numbers_def
    scpi_number_t par;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &par, FALSE)) {
        // If no parameter, than reply actual current limit of fuse mode
        val = monitor_fuse_current_get();
    }
    else {
        // Select by special number descriptor
        switch (par.content.tag) {
            case SCPI_NUM_MIN: {
                val = monitor_fuse_current_min_get();
                break;
            }
            case SCPI_NUM_MAX: {
                val = monitor_fuse_current_max_get();
                break;
            }
            default: {
                return SCPI_RES_ERR;
            }
        }
    }

    // Conver from internal units: milliamperes, to base unit: amperes
    // Reply to query
    SCPI_ResultFloat(context, (float)val / 1000.0F);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CurrentProtectionClear(scpi_t* context)
{
    (void)context;
    // Clear sate of fuse mode monitor (drop alert)
    monitor_fuse_clear();
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SourceStatusQ(scpi_t* context)
{
    uint8_t status = regulator_status_get();
    // Return output state
    SCPI_ResultInt32(context, (int32_t)status);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SourceStatusClear(scpi_t* context)
{
    (void)context;
    regulator_status_clear();
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CalibrationSecureState(scpi_t* context)
{
    bool state = false;

    // Read first parameter if present
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    // Update write protection sate for calibration coeffs
    regulator_calib_wp_set(state);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CalibrationSecureStateQ(scpi_t* context)
{
    // Get current WP mode of calibration coeffs of regulator
    bool val = regulator_calib_wp_get();

    // Return output state
    SCPI_ResultBool(context, val);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CalibrationVoltageSlope(scpi_t* context)
{
    // Read first parameter if present
    float param;
    if (!SCPI_ParamFloat(context, &param, TRUE)) {
        return SCPI_RES_ERR;
    }

    // Update calibration const
    regulator_calib_v_slope_set(param);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CalibrationVoltageSlopeQ(scpi_t* context)
{
    float val = regulator_calib_v_slope_get();

    // Reply to query
    SCPI_ResultFloat(context, val);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CalibrationVoltageOffset(scpi_t* context)
{
    // Read first parameter if present
    float param;
    if (!SCPI_ParamFloat(context, &param, TRUE)) {
        return SCPI_RES_ERR;
    }

    // Update calibration const
    regulator_calib_v_offset_set(param);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CalibrationVoltageOffsetQ(scpi_t* context)
{
    float val = regulator_calib_v_offset_get();

    // Reply to query
    SCPI_ResultFloat(context, val);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CalibrationCurrentSlope(scpi_t* context)
{
    // Read first parameter if present
    float param;
    if (!SCPI_ParamFloat(context, &param, TRUE)) {
        return SCPI_RES_ERR;
    }

    // Update calibration const
    regulator_calib_i_slope_set(param);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CalibrationCurrentSlopeQ(scpi_t* context)
{
    float val = regulator_calib_i_slope_get();

    // Reply to query
    SCPI_ResultFloat(context, val);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CalibrationCurrentOffset(scpi_t* context)
{
    // Read first parameter if present
    float param;
    if (!SCPI_ParamFloat(context, &param, TRUE)) {
        return SCPI_RES_ERR;
    }

    // Update calibration const
    regulator_calib_i_offset_set(param);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CalibrationCurrentOffsetQ(scpi_t* context)
{
    float val = regulator_calib_i_offset_get();

    // Reply to query
    SCPI_ResultFloat(context, val);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CalibrationStore(scpi_t* context)
{
    (void)context;
    // Process storing calibration consts
    regulator_calib_store();
    return SCPI_RES_OK;
}
