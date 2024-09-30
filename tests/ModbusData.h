#ifndef Modbus_Data_H
#define Modbus_Data_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>


// Function prototypes
void GeneratePowerShaftData(uint8_t *DataBuffer);
void GenerateModbusTcpFrame(uint8_t *Buffer, uint16_t TransactionId, uint16_t ProtocolId, uint16_t Length,
                            uint8_t UnitId, uint16_t FunctionCode, uint8_t *Data, uint16_t DataLength);

#endif // Modbus_Data_H
