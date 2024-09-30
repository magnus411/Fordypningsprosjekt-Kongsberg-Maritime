#define SDB_LOG_LEVEL 4
#include "sdbExtern.h"

SDB_LOG_REGISTER(main);

static void
GeneratePowerShaftData(uint8_t *DataBuffer)
{
    // Simulated power shaft data (32-bit values split into two 16-bit registers)
    u32 Power  = rand() % 1000 + 100; //  Power in kW (random between 100 and 1000)
    u32 Torque = rand() % 500 + 50;   // Torque in Nm (random between 50 and 550)
    u32 Rpm    = rand() % 5000 + 500; // RPM (random between 500 and 5500)

    // Store data in 16-bit register format (big-endian)
    DataBuffer[0] = (Power >> 8) & 0xFF;  // Power high byte
    DataBuffer[1] = Power & 0xFF;         // Power low byte
    DataBuffer[2] = (Torque >> 8) & 0xFF; // Torque high byte
    DataBuffer[3] = Torque & 0xFF;        // Torque low byte
    DataBuffer[4] = (Rpm >> 8) & 0xFF;    // RPM high byte
    DataBuffer[5] = Rpm & 0xFF;           // RPM low byte
}

static void
GenerateModbusTcpFrame(u8 *Buffer, u16 TransactionId, u16 ProtocolId, u16 Length, u8 UnitId,
                       u16 FunctionCode, u8 *Data, u16 DataLength)
{
    Buffer[0] = (TransactionId >> 8) & 0xFF;
    Buffer[1] = TransactionId & 0xFF;
    Buffer[2] = (ProtocolId >> 8) & 0xFF;
    Buffer[3] = ProtocolId & 0xFF;
    Buffer[4] = (Length >> 8) & 0xFF;
    Buffer[5] = Length & 0xFF;
    Buffer[6] = UnitId;
    Buffer[7] = FunctionCode;
    Buffer[8] = DataLength;
    memcpy(&Buffer[9], Data, DataLength);
}
