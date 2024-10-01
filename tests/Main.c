#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <MQTTClient.h>
#include <SdbExtern.h>
#include <common/CircularBuffer.h>
#include <comm_protocols/MQTT.h>
#include "database_systems/PostgresTest.h"
#include "comm_protocols/UNIXSocket/ModbusSocketTest.h"
#include "comm_protocols/MQTT/Subscriber/MQTTSubscriber.h"
#include "comm_protocols/MQTT/Publisher/MQTTPublisher.h"

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN   "\033[36m"
#define COLOR_RED    "\033[31m"

static circular_buffer Cb = { 0 };

void
PrintUsage(void)
{
    printf(COLOR_CYAN "----------------------------------------------------------\n" COLOR_RESET);
    printf(COLOR_GREEN "Usage: TestApp [OPTIONS]\n" COLOR_RESET);
    printf(COLOR_CYAN "----------------------------------------------------------\n" COLOR_RESET);
    printf(COLOR_BLUE "Options:\n" COLOR_RESET);
    printf(COLOR_YELLOW "  -p, --postgres   " COLOR_RESET
                        "Run the Postgres test (requires config file path)\n");
    printf(COLOR_YELLOW "  -m, --modbus     " COLOR_RESET "Run the Modbus test\n");
    printf(COLOR_YELLOW "                   Modbus options:\n" COLOR_RESET);
    printf(COLOR_YELLOW "                     --client        " COLOR_RESET
                        "Run the Modbus client test\n");
    printf(COLOR_YELLOW "                     --server        " COLOR_RESET
                        "Run the Modbus server test (optional: -p port, -u unitId, -s speed)\n");
    printf(COLOR_YELLOW "  -s, --mqtt-sub   " COLOR_RESET "Run the MQTT Subscriber test\n");
    printf(COLOR_YELLOW "  -t, --mqtt-pub   " COLOR_RESET
                        "Run the MQTT Publisher test (requires rate in Hz)\n");
    printf(COLOR_YELLOW "  -h, --help       " COLOR_RESET "Show this help message\n");
    printf(COLOR_CYAN "----------------------------------------------------------\n" COLOR_RESET);
}

int
main(int Argc, char **Argv)
{
    if(Argc < 2) {
        PrintUsage();
        return 1;
    }

    int         Opt;
    int         OptionIndex    = 0;
    const char *PostgresConfig = NULL;

    static struct option LongOptions[] = {
        { "postgres", required_argument, 0, 'p' },
        {   "modbus",       no_argument, 0, 'm' },
        {   "client",       no_argument, 0,   1 },
        {   "server",       no_argument, 0,   2 },
        { "mqtt-sub",       no_argument, 0, 's' },
        { "mqtt-pub", required_argument, 0, 't' },
        {     "help",       no_argument, 0, 'h' },
        {          0,                 0, 0,   0 }
    };

    int ModbusTest = 0;
    while((Opt = getopt_long(Argc, Argv, "p:mst:h", LongOptions, &OptionIndex)) != -1) {
        switch(Opt) {
            case 'p':
                PostgresConfig = optarg;
                break;
            case 'm':
                ModbusTest = 1;
                break;
            case 1:
                if(ModbusTest) {
                    printf(COLOR_GREEN "Running Modbus Client Test...\n" COLOR_RESET);
                    RunModbusModuleClient();
                }
                return 0;
            case 2:
                if(ModbusTest) {
                    printf(COLOR_GREEN "Running Modbus Server Test...\n" COLOR_RESET);
                    RunModbusServer(Argc, Argv);
                }
                return 0;
            case 's':
                printf(COLOR_GREEN "Running MQTT Subscriber Test...\n" COLOR_RESET);
                MQTTSubscriber("tcp://localhost:1883", "ModbusSub", "MODBUS");
                return 0;
            case 't':
                printf(COLOR_GREEN "Running MQTT Publisher Test...\n" COLOR_RESET);
                MQTTPublisher(2, (char *[]){ Argv[0], optarg });
                return 0;
            case 'h':
            default:
                PrintUsage();
                return 0;
        }
    }

    if(PostgresConfig) {
        printf(COLOR_GREEN "Running Postgres Test...\n" COLOR_RESET);
        RunPostgresTest(PostgresConfig);
    } else if(ModbusTest) {
        PrintUsage();
    } else {
        PrintUsage();
    }

    return 0;
}
