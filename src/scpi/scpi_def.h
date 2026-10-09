#ifndef __SCPI_DEF_H_
#define __SCPI_DEF_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "scpi/scpi.h"

#define SCPI_IDN1 "EVERYPINIO"
#define SCPI_IDN2 "POWERBLOCK"
#define SCPI_IDN3 NULL

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.0.0-dev"
#endif

#define SCPI_IDN4 FIRMWARE_VERSION

extern const scpi_command_t scpi_commands[];

scpi_result_t SCPI_Reset(scpi_t* context);

scpi_result_t SCPI_SystemCommTcpipControlQ(scpi_t* context);

#ifdef __cplusplus
}
#endif
#endif /* __SCPI_DEF_H_ */
