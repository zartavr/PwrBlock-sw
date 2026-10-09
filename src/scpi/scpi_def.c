#include "scpi_def.h"
#include "control/control.h"
#include "scpi/bus_format.h"
#include "scpi/ext_conn/digital_ctl.h"
#include "scpi/ext_conn/i2c_ctl.h"
#include "scpi/ext_conn/uart_ctl.h"
#include "scpi/scpi.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const scpi_command_t scpi_commands[] = {
    /* IEEE Mandated Commands (SCPI std V1999.0 4.1.1) */
    {
     .pattern  = "*CLS",
     .callback = SCPI_CoreCls,
     },
    {
     .pattern  = "*ESE",
     .callback = SCPI_CoreEse,
     },
    {
     .pattern  = "*ESE?",
     .callback = SCPI_CoreEseQ,
     },
    {
     .pattern  = "*ESR?",
     .callback = SCPI_CoreEsrQ,
     },
    {
     .pattern  = "*IDN?",
     .callback = SCPI_CoreIdnQ,
     },
    {
     .pattern  = "*OPC",
     .callback = SCPI_CoreOpc,
     },
    {
     .pattern  = "*OPC?",
     .callback = SCPI_CoreOpcQ,
     },
    {
     .pattern  = "*RST",
     .callback = SCPI_CoreRst,
     },
    {
     .pattern  = "*SRE",
     .callback = SCPI_CoreSre,
     },
    {
     .pattern  = "*SRE?",
     .callback = SCPI_CoreSreQ,
     },
    {
     .pattern  = "*STB?",
     .callback = SCPI_CoreStbQ,
     },
    {
     .pattern  = "*TST?",
     .callback = SCPI_CoreTstQ,
     },
    {
     .pattern  = "*WAI",
     .callback = SCPI_CoreWai,
     },
    {
     .pattern  = "*SAV",
     .callback = SCPI_SaveState,
     },
    {
     .pattern  = "*RCL",
     .callback = SCPI_RecallState,
     },

    /* Required SCPI commands (SCPI std V1999.0 4.2.1) */
    {
     .pattern  = "SYSTem:ERRor[:NEXT]?",
     .callback = SCPI_SystemErrorNextQ,
     },
    {
     .pattern  = "SYSTem:ERRor:COUNt?",
     .callback = SCPI_SystemErrorCountQ,
     },
    {
     .pattern  = "SYSTem:VERSion?",
     .callback = SCPI_SystemVersionQ,
     },
    /* Base functions */
    {
     .pattern  = "OUTPut[:STATe]",
     .callback = SCPI_Output,
     },
    {
     .pattern  = "OUTPut[:STATe]?",
     .callback = SCPI_OutputQ,
     },
    /* SYSTem Subsystem */
    {
     .pattern  = "SYSTem:CAPability?",
     .callback = SCPI_SystemCapabilityQ,
     },
    {
     .pattern  = "SYSTem:LOCal",
     .callback = SCPI_SystemLocal,
     },
    {
     .pattern  = "SYSTem:REMote",
     .callback = SCPI_SystemRemote,
     },
    {
     .pattern  = "SYSTem:RWLock",
     .callback = SCPI_SystemRWlock,
     },
    /* SOURce Subsystem */
    {
     .pattern  = "[SOURce]:CURRent[:LEVel][:IMMediate][:AMPLitude]",
     .callback = SCPI_Current,
     },
    {
     .pattern  = "[SOURce]:CURRent[:LEVel][:IMMediate][:AMPLitude]?",
     .callback = SCPI_CurrentQ,
     },
    {
     .pattern  = "[SOURce]:VOLTage[:LEVel][:IMMediate][:AMPLitude]",
     .callback = SCPI_Voltage,
     },
    {
     .pattern  = "[SOURce]:VOLTage[:LEVel][:IMMediate][:AMPLitude]?",
     .callback = SCPI_VoltageQ,
     },
    {
     .pattern  = "[SOURce]:CURRent:PROTection:STATe",
     .callback = SCPI_CurrentProtectionState,
     },
    {
     .pattern  = "[SOURce]:CURRent:PROTection:STATe?",
     .callback = SCPI_CurrentProtectionStateQ,
     },
    {
     .pattern  = "[SOURce]:CURRent:PROTection:LEVel",
     .callback = SCPI_CurrentProtectionLevel,
     },
    {
     .pattern  = "[SOURce]:CURRent:PROTection:LEVel?",
     .callback = SCPI_CurrentProtectionLevelQ,
     },
    {
     .pattern  = "[SOURce]:CURRent:PROTection:CLEar",
     .callback = SCPI_CurrentProtectionClear,
     },
    /* STATus subsystem */
    {
     .pattern  = "STATus:OPERation[:EVENt]?",
     .callback = SCPI_StatusOperationEventQ,
     },
    {
     .pattern  = "STATus:OPERation:CONDition?",
     .callback = SCPI_StatusOperationConditionQ,
     },
    {
     .pattern  = "STATus:OPERation:ENABle",
     .callback = SCPI_StatusOperationEnable,
     },
    {
     .pattern  = "STATus:OPERation:ENABle?",
     .callback = SCPI_StatusOperationEnableQ,
     },
    {
     .pattern  = "STATus:QUEStionable[:EVENt]?",
     .callback = SCPI_StatusQuestionableEventQ,
     },
    {
     .pattern  = "STATus:QUEStionable:CONDition?",
     .callback = SCPI_StatusQuestionableConditionQ,
     },
    {
     .pattern  = "STATus:QUEStionable:ENABle",
     .callback = SCPI_StatusQuestionableEnable,
     },
    {
     .pattern  = "STATus:QUEStionable:ENABle?",
     .callback = SCPI_StatusQuestionableEnableQ,
     },
    {
     .pattern  = "STATus:PRESet",
     .callback = SCPI_StatusPreset,
     },
    /* MEASure subsystem */
    {
     .pattern  = "MEASure[:SCALar]:VOLTage[:DC]?",
     .callback = SCPI_MeasureVoltageQ,
     },
    {
     .pattern  = "MEASure[:SCALar]:CURRent[:DC]?",
     .callback = SCPI_MeasureCurrentQ,
     },
    {
     .pattern  = "MEASure[:SCALar]:POWer[:DC]?",
     .callback = SCPI_MeasurePowerQ,
     },
    {
     .pattern  = "MEASure[:SCALar]:TEMPerature:SHUNt?",
     .callback = SCPI_MeasureTemperatureShuntQ,
     },
    {
     .pattern  = "MEASure[:SCALar]:TEMPerature:TERMinal?",
     .callback = SCPI_MeasureTemperatureTerminalQ,
     },
    /* CALibration subsystem */
    {.pattern  = "CALibration:SECUre:STATe",
     .callback = SCPI_CalibrationSecureState},
    {.pattern  = "CALibration:SECUre:STATe?",
     .callback = SCPI_CalibrationSecureStateQ},
    {.pattern  = "CALibration:VOLTage:SLOPe",
     .callback = SCPI_CalibrationVoltageSlope},
    {.pattern  = "CALibration:VOLTage:SLOPe?",
     .callback = SCPI_CalibrationVoltageSlopeQ},
    {.pattern  = "CALibration:VOLTage:OFFSet",
     .callback = SCPI_CalibrationVoltageOffset},
    {.pattern  = "CALibration:VOLTage:OFFSet?",
     .callback = SCPI_CalibrationVoltageOffsetQ},
    {.pattern  = "CALibration:CURRent:SLOPe",
     .callback = SCPI_CalibrationCurrentSlope},
    {.pattern  = "CALibration:CURRent:SLOPe?",
     .callback = SCPI_CalibrationCurrentSlopeQ},
    {.pattern  = "CALibration:CURRent:OFFSet",
     .callback = SCPI_CalibrationCurrentOffset},
    {.pattern  = "CALibration:CURRent:OFFSet?",
     .callback = SCPI_CalibrationCurrentOffsetQ},
    {.pattern = "CALibration:STORe", .callback = SCPI_CalibrationStore},

    /* DIGital subsystem */
    {
     .pattern  = "[SOURce]:DIGital:COUNt?",
     .callback = SCPI_DigitalCountQ,
     },
    {
     .pattern  = "[SOURce]:DIGital:PIN#:FUNCtion?",
     .callback = SCPI_DigitalPinFunctionQ,
     },
    {
     .pattern  = "[SOURce]:DIGital:PIN#:DIRection",
     .callback = SCPI_DigitalPinDirection,
     },
    {
     .pattern  = "[SOURce]:DIGital:PIN#:DIRection?",
     .callback = SCPI_DigitalPinDirectionQ,
     },
    {
     .pattern  = "[SOURce]:DIGital:PIN#:MODE",
     .callback = SCPI_DigitalPinMode,
     },
    {
     .pattern  = "[SOURce]:DIGital:PIN#:MODE?",
     .callback = SCPI_DigitalPinModeQ,
     },
    {
     .pattern  = "[SOURce]:DIGital:PIN#:PULL",
     .callback = SCPI_DigitalPinPull,
     },
    {
     .pattern  = "[SOURce]:DIGital:PIN#:PULL?",
     .callback = SCPI_DigitalPinPullQ,
     },
    {
     .pattern  = "[SOURce]:DIGital:PIN#[:LEVel]",
     .callback = SCPI_DigitalPinLevel,
     },
    {
     .pattern  = "[SOURce]:DIGital:PIN#[:LEVel]?",
     .callback = SCPI_DigitalPinLevelQ,
     },

    /* FORMat subsystem */
    {
     .pattern  = "FORMat[:DATA]",
     .callback = SCPI_BusFormat,
     },
    {
     .pattern  = "FORMat[:DATA]?",
     .callback = SCPI_BusFormatQ,
     },

    /* BUS subsystem */
    {
     .pattern  = "BUS:I2C:STATe",
     .callback = SCPI_I2cState,
     },
    {
     .pattern  = "BUS:I2C:STATe?",
     .callback = SCPI_I2cStateQ,
     },
    {
     .pattern  = "BUS:I2C:ADDRess:WIDTh",
     .callback = SCPI_I2cAddressWidth,
     },
    {
     .pattern  = "BUS:I2C:ADDRess:WIDTh?",
     .callback = SCPI_I2cAddressWidthQ,
     },
    {
     .pattern  = "BUS:I2C:WRITe",
     .callback = SCPI_I2cWrite,
     },
    {
     .pattern  = "BUS:I2C:READ?",
     .callback = SCPI_I2cReadQ,
     },
    {
     .pattern  = "BUS:UART:STATe",
     .callback = SCPI_UartState,
     },
    {
     .pattern  = "BUS:UART:STATe?",
     .callback = SCPI_UartStateQ,
     },
    {
     .pattern  = "BUS:UART:BAUD",
     .callback = SCPI_UartBaud,
     },
    {
     .pattern  = "BUS:UART:BAUD?",
     .callback = SCPI_UartBaudQ,
     },
    {
     .pattern  = "BUS:UART:FRAMe",
     .callback = SCPI_UartFrame,
     },
    {
     .pattern  = "BUS:UART:FRAMe?",
     .callback = SCPI_UartFrameQ,
     },
    {
     .pattern  = "BUS:UART:WRITe",
     .callback = SCPI_UartWrite,
     },
    {
     .pattern  = "BUS:UART:READ?",
     .callback = SCPI_UartReadQ,
     },
    {
     .pattern  = "BUS:UART:TRANsfer?",
     .callback = SCPI_UartTransferQ,
     },
    {
     .pattern  = "BUS:UART:BUFFer:COUNt?",
     .callback = SCPI_UartBufferCountQ,
     },
    {
     .pattern  = "BUS:UART:BUFFer:CLEar",
     .callback = SCPI_UartBufferClear,
     },

    SCPI_CMD_LIST_END
};

scpi_result_t SCPI_Reset(scpi_t* context)
{
    (void)context;

    SCPI_DigitalReset(context);
    SCPI_BusFormatReset(context);
    SCPI_I2cReset(context);
    SCPI_UartReset(context);

    return SCPI_RES_OK;
}
