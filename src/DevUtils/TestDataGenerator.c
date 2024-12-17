#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define SDB_H_IMPLEMENTATION
#include <src/Sdb.h>
#undef SDB_H_IMPLEMENTATION

SDB_LOG_REGISTER(TestDataGenerator);

#include <src/CommProtocols/Modbus.h>
#include <src/Common/Socket.h>
#include <src/Common/Thread.h>
#include <src/Common/Time.h>
#include <src/DatabaseSystems/Postgres.h>

#include <src/DevUtils/TestConstants.h>


static void
FillShaftPowerData(shaft_power_data *Data)
{
    time_t     CurrentTime = time(NULL);
    static i64 Id          = 0;

    Data->PacketId    = Id++;
    Data->Time        = CurrentTime;
    Data->Rpm         = ((rand() % 100) + 30.0) * sin(Id * 0.1); // RPM with variation
    Data->Torque      = ((rand() % 600) + 100.0) * 0.8;          // Torque (Nm)
    Data->Power       = Data->Rpm * Data->Torque / 9.5488;       // Derived power (kW)
    Data->PeakPeakPfs = ((rand() % 50) + 25.0) * cos(Id * 0.1);  // PFS variation
}

static void
MakeModbusTcpFrame(u8 *Buffer, u16 TransactionId, u16 ProtocolId, u16 Length, u8 UnitId,
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
    SdbMemcpy(&Buffer[9], Data, DataLength);
}

int
PrintTestDataSample(sdb_string Filename)
{
    FILE            *File;
    shaft_power_data Data;

    // Open the binary file
    File = fopen(Filename, "rb");
    if(File == NULL) {
        perror("Error opening file");
        return 1;
    }

    // Read and print 10 records
    printf("Reading 10 records from file:\n");
    printf("----------------------------------------\n");
    printf("Record | PacketId |     Time      |  RPM   | Torque |  Power  | PeakPeakPfs\n");
    printf("----------------------------------------\n");

    for(int i = 0; i < 10; i++) {
        size_t ReadResult = fread(&Data, sizeof(shaft_power_data), 1, File);

        if(ReadResult != 1) {
            if(feof(File)) {
                printf("End of file reached after %d records\n", i);
                break;
            } else {
                perror("Error reading file");
                fclose(File);
                return 1;
            }
        }

        // Convert time_t to a readable string
        char       TimeStr[26];
        struct tm *TmInfo = localtime((time_t *)(&Data + sizeof(pg_int8)));
        strftime(TimeStr, 26, "%Y-%m-%d %H:%M:%S", TmInfo);

        // Print the record
        printf("%6d | %8lu | %s | %6.2f | %6.2f | %7.2f | %10.2f\n", i + 1, Data.PacketId, TimeStr,
               Data.Rpm, Data.Torque, Data.Power, Data.PeakPeakPfs);
    }

    printf("----------------------------------------\n");

    fclose(File);
    return 0;
}

int
main(void)
{
    u64        SpdCount = 1e6;
    size_t     DataSize = SpdCount * sizeof(shaft_power_data);
    sdb_string Filename = "./data/testdata/TestData.sdb";

    sdb_mmap Map = { 0 };
    SdbMemMap(&Map, NULL, DataSize, PROT_READ | PROT_WRITE, MAP_SHARED, -2, 0, Filename,
              O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    u8 *Data = Map.Data;
    for(u64 i = 0; i < SpdCount; ++i) {
        shaft_power_data SpData = { 0 };
        FillShaftPowerData(&SpData);
        SdbMemcpy(Data + (i * sizeof(SpData)), &SpData, sizeof(SpData));
    }

    SdbMemUnmap(&Map);
    PrintTestDataSample(Filename);

    return 0;
}
