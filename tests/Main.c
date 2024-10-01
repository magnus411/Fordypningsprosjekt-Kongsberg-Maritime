#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "database_systems/PostgresTest.h"
#include "comm_protocols/ModbusTest.h"

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN   "\033[36m"
#define COLOR_RED    "\033[31m"

//! Btw, using printf here ingar, since its interface
void
PrintUsage()
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
    printf(COLOR_YELLOW "  -h, --help       " COLOR_RESET "Show this help message\n");
    printf(COLOR_CYAN "----------------------------------------------------------\n" COLOR_RESET);
}

int
main(int argc, char **argv)
{
    if(argc < 2) {
        PrintUsage();
        return 1;
    }

    int         opt;
    int         option_index    = 0;
    const char *postgres_config = NULL;

    static struct option long_options[] = {
        { "postgres", required_argument, 0, 'p' },
        {   "modbus",       no_argument, 0, 'm' },
        {   "client",       no_argument, 0,   1 },
        {   "server",       no_argument, 0,   2 },
        {     "help",       no_argument, 0, 'h' },
        {          0,                 0, 0,   0 }
    };

    int modbus_test = 0;

    while((opt = getopt_long(argc, argv, "p:mh", long_options, &option_index)) != -1) {
        switch(opt) {
            case 'p':
                postgres_config = optarg;
                break;
            case 'm':
                modbus_test = 1;
                break;
            case 1:
                if(modbus_test) {
                    printf(COLOR_GREEN "Running Modbus Client Test...\n" COLOR_RESET);
                    RunModbusModuleClient();
                }
                return 0;
            case 2:
                if(modbus_test) {
                    printf(COLOR_GREEN "Running Modbus Server Test...\n" COLOR_RESET);
                    RunModbusServer(argc, argv);
                }
                return 0;
            case 'h':
            default:
                PrintUsage();
                return 0;
        }
    }

    if(postgres_config) {
        printf(COLOR_GREEN "Running Postgres Test...\n" COLOR_RESET);
        RunPostgresTest(postgres_config);
    } else if(modbus_test) {
        PrintUsage();
    } else {
        PrintUsage();
    }

    return 0;
}
