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

#include "scpi/scpi.h"

/**
 * @brief Empty callback (works as NOP)
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_stub_callback(scpi_t* context);

/**
 * SYSTem:CAPability?
 * @brief This query returns an <instrument_specifier>. Syntax described
 * at 1.4.1 SCPI-99.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_SystemCapabilityQ(scpi_t* context);

/**
 * SYSTem:LOCal
 * @brief Sets the system to front panel control (front panel keyboard is
 * unlocked). ref by example: NGE100 User Manual.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_SystemLocal(scpi_t* context);

/**
 * SYSTem:REMote
 * @brief Sets the system to remote state (front panel keyboard is locked). By
pressing button "Remote", the front panel control will be activated. ref by
example: NGE100 User Manual.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_SystemRemote(scpi_t* context);

/**
 * SYSTem:RWLock
 * @brief Sets the system to remote state (front panel keyboard is locked). You
are only able to unlock the front panel control via SCPI command SYSTem:LOCal.
ref by example: NGE100 User Manual.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_SystemRWlock(scpi_t* context);

/**
 * OUTPut[:STATe] {OFF | ON | 0 | 1}
 * @brief Turn off and on output of power supply.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_Output(scpi_t* context);

/**
 * OUTPut[:STATe]?
 * @brief This query returns state of output.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_OutputQ(scpi_t* context);

/**
 * [SOURce]:CURRent[:LEVel][:IMMediate][:AMPLitude] {<Current>}
 * @brief Set up output value of current.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_Current(scpi_t* context);

/**
 * [SOURce]:CURRent[:LEVel][:IMMediate][:AMPLitude]? [MIN | MAX]
 * @brief This query returns target, min or max value of current.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CurrentQ(scpi_t* context);

/**
 * [SOURce]:VOLTage[:LEVel][:IMMediate][:AMPLitude] {<Voltage>}
 * @brief Set up output value of voltage.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_Voltage(scpi_t* context);

/**
 * [SOURce]:VOLTage[:LEVel][:IMMediate][:AMPLitude]? [MIN | MAX]
 * @brief This query returns target, min or max value of voltage.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_VoltageQ(scpi_t* context);

/**
 * MEASure[:SCALar]:CURRent[:DC]?
 * @brief This query returns measured value of current value on shunt.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_MeasureCurrentQ(scpi_t* context);

/**
 * MEASure[:SCALar]:VOLTage[:DC]?
 * @brief This query returns measured value of voltage value on output bus.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_MeasureVoltageQ(scpi_t* context);

/**
 * MEASure[:SCALar]:POWer[:DC]?
 * @brief This query returns measured value of average output power (DC).
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_MeasurePowerQ(scpi_t* context);

/**
 * MEASure[:SCALar]:TEMPerature:SHUNt?
 * @brief This query returns measured value of die temperature of monitor shunt.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_MeasureTemperatureShuntQ(scpi_t* context);

/**
 * MEASure[:SCALar]:TEMPerature:TERMinal?
 * @brief This query returns measured value close to terminals (NTC sensor).
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_MeasureTemperatureTerminalQ(scpi_t* context);

/**
 * *SAV {<Mem_cell>}
 * @brief Store current state to internal memory cell.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_SaveState(scpi_t* context);

/**
 * *RCL {<Mem_cell>}
 * @brief Recall state from internal memory cell.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_RecallState(scpi_t* context);

/**
 * [SOURce]:CURRent:PROTection:STATe {OFF | ON | 0 | 1}
 * @brief Set up overcurrent protection module work state.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CurrentProtectionState(scpi_t* context);

/**
 * [SOURce]:CURRent:PROTection:STATe?
 * @brief This query returns overcurrent module work state.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CurrentProtectionStateQ(scpi_t* context);

/**
 * [SOURce]:CURRent:PROTection:LEVel {<Current>}
 * @brief Set up current limit for overcurren protection module.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CurrentProtectionLevel(scpi_t* context);

/**
 * [SOURce]:CURRent:PROTection:LEVel?
 * @brief This query returns current limit for overcurrent module.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CurrentProtectionLevelQ(scpi_t* context);

/**
 * [SOURce]:CURRent:PROTection:CLEar
 * @brief Clear alarm flag in overcurrent protection system.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CurrentProtectionClear(scpi_t* context);


/**
 * SOURce:STATus?
 * @brief Read latched status byte of LM51772
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_SourceStatusQ(scpi_t* context);

/**
 * SOURce:STATus:CLEar
 * @brief Clear faults in status byte of LM51772
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_SourceStatusClear(scpi_t* context);

/**
 * CALibration:SECUre:STATe {OFF | ON | 0 | 1}
 * @brief Set up write protection secure flag for calibration consts.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CalibrationSecureState(scpi_t* context);

/**
 * CALibration:SECUre:STATe?
 * @brief This query returns write protection secure flag for calibration
 * consts.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CalibrationSecureStateQ(scpi_t* context);

/**
 * CALibration:VOLTage:SLOPe {<Slope>}
 * @brief Set up voltage slope calibration const.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CalibrationVoltageSlope(scpi_t* context);

/**
 * CALibration:VOLTage:SLOPe?
 * @brief This query returns voltage slope calibration const.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CalibrationVoltageSlopeQ(scpi_t* context);

/**
 * CALibration:VOLTage:OFFSet {<Offset>}
 * @brief Set up voltage offset calibration const.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CalibrationVoltageOffset(scpi_t* context);

/**
 * CALibration:VOLTage:OFFSet?
 * @brief This query returns voltage offset calibration const.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CalibrationVoltageOffsetQ(scpi_t* context);

/**
 * CALibration:CURRent:SLOPe {<Slope>}
 * @brief Set up current slope calibration const.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CalibrationCurrentSlope(scpi_t* context);

/**
 * CALibration:CURRent:SLOPe?
 * @brief This query returns current slope calibration const.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CalibrationCurrentSlopeQ(scpi_t* context);

/**
 * CALibration:CURRent:OFFSet {<Offset>}
 * @brief Set up current offset calibration const.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CalibrationCurrentOffset(scpi_t* context);

/**
 * CALibration:CURRent:OFFSet?
 * @brief This query returns current offset calibration const.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CalibrationCurrentOffsetQ(scpi_t* context);

/**
 * CALibration:STORe
 * @brief Store all calibration consts to nonvolatile memory.
 *
 * @param context
 * @return scpi_result_t
 */
scpi_result_t SCPI_CalibrationStore(scpi_t* context);
