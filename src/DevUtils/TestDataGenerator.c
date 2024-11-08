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

typedef struct __attribute__((packed, aligned(1)))
{
    pg_int8 PacketId;
    time_t  Time; // NOTE(ingar): The insert function assumes a time_t comes in and converts it to a
                  // pg_timestamp
    pg_float8 Rpm;
    pg_float8 Torque;
    pg_float8 Power;
    pg_float8 PeakPeakPfs;

} shaft_power_data;

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

void *
CreateMemoryMappedFile(const char *FileName, size_t FileSize)
{
    // Open file with create, read/write permissions
    int FileDescriptor = open(FileName, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if(FileDescriptor == -1) {
        fprintf(stderr, "Error opening file: %s\n", strerror(errno));
        return NULL;
    }

    // Set the file size to desired mapping size
    if(ftruncate(FileDescriptor, FileSize) == -1) {
        fprintf(stderr, "Error setting file size: %s\n", strerror(errno));
        close(FileDescriptor);
        return NULL;
    }

    // Create the memory mapping
    void *MappedData = mmap(NULL, FileSize, PROT_READ | PROT_WRITE, MAP_SHARED, FileDescriptor, 0);

    if(MappedData == MAP_FAILED) {
        fprintf(stderr, "Error mapping file: %s\n", strerror(errno));
        close(FileDescriptor);
        return NULL;
    }

    // Close the file descriptor (mapping remains valid)
    close(FileDescriptor);

    return MappedData;
}

int
PrintTestDataSample()
{
    FILE            *file;
    shaft_power_data data;
    int              i;

    // Open the binary file
    file = fopen("./data/TestData.sdb", "rb");
    if(file == NULL) {
        perror("Error opening file");
        return 1;
    }

    // Read and print 10 records
    printf("Reading 10 records from file:\n");
    printf("----------------------------------------\n");
    printf("Record | PacketId |     Time      |  RPM   | Torque |  Power  | PeakPeakPfs\n");
    printf("----------------------------------------\n");

    for(i = 0; i < 10; i++) {
        size_t read_result = fread(&data, sizeof(shaft_power_data), 1, file);

        if(read_result != 1) {
            if(feof(file)) {
                printf("End of file reached after %d records\n", i);
                break;
            } else {
                perror("Error reading file");
                fclose(file);
                return 1;
            }
        }

        // Convert time_t to a readable string
        char       time_str[26];
        struct tm *tm_info = localtime(&data.Time);
        strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);

        // Print the record
        printf("%6d | %8lu | %s | %6.2f | %6.2f | %7.2f | %10.2f\n", i + 1, data.PacketId, time_str,
               data.Rpm, data.Torque, data.Power, data.PeakPeakPfs);
    }

    printf("----------------------------------------\n");

    fclose(file);
    return 0;
}

int
main(void)
{
    u64    SpdCount = 1e6;
    size_t DataSize = SpdCount * sizeof(shaft_power_data);
    u8    *Data     = CreateMemoryMappedFile("./data/TestData.sdb", DataSize);

    for(u64 i = 0; i < SpdCount; ++i) {
        shaft_power_data SpData = { 0 };
        FillShaftPowerData(&SpData);
        SdbMemcpy(Data + (i * sizeof(SpData)), &SpData, sizeof(SpData));
    }

    if(munmap(Data, DataSize) == -1) {
        fprintf(stderr, "Error unmapping file: %s\n", strerror(errno));
    }

    PrintTestDataSample();

    return 0;
}
