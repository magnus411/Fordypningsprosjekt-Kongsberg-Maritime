#define _GNU_SOURCE

#include <src/Sdb.h>
SDB_LOG_REGISTER(Signals);


#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <src/Common/SensorDataPipe.h>
#include <src/DevUtils/ModbusDataParser.h>
#include <src/Signals.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#define BACKTRACE_SIZE 50
#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/DevUtils/ModbusDataParser.h>

#include <src/DevUtils/ModbusDataParser.h>

static volatile sig_atomic_t g_shutdown_requested = 0;
signal_handler_context       g_signal_context     = { 0 };

static pid_t
GetThreadId(void)
{
    return syscall(SYS_gettid);
}

static void
GetThreadName(char *name, size_t size)
{
    name[0] = '\0';
    if(pthread_getname_np(pthread_self(), name, size) != 0) {
        snprintf(name, size, "Unknown");
    }
}


static void
DumpProcessMemory(int signum, siginfo_t *info)
{
    if(mkdir("dumps", 0755) == -1 && errno != EEXIST) {
        SdbLogError("Failed to create dumps directory: %s", strerror(errno));
        return;
    }

    char  dump_filename[256];
    char  thread_name[32];
    pid_t thread_id = GetThreadId();
    GetThreadName(thread_name, sizeof(thread_name));

    time_t     now     = time(NULL);
    struct tm *tm_info = localtime(&now);

    snprintf(dump_filename, sizeof(dump_filename),
             "dumps/sdb_dump_%04d%02d%02d_%02d%02d%02d_sig%d_thread%d.dump",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday, tm_info->tm_hour,
             tm_info->tm_min, tm_info->tm_sec, signum, thread_id);

    int dump_fd = open(dump_filename, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    if(dump_fd == -1) {
        SdbLogError("Failed to create memory dump file: %s", strerror(errno));
        return;
    }

    char header[1024];
    snprintf(header, sizeof(header),
             "Memory Dump Information:\n"
             "Timestamp: %s"
             "Signal: %d\n"
             "Thread ID: %d\n"
             "Thread Name: %s\n"
             "Fault Address: %p\n"
             "Process ID: %d\n\n",
             ctime(&now), signum, thread_id, thread_name, info->si_addr, getpid());

    write(dump_fd, header, strlen(header));

    void  *bt[BACKTRACE_SIZE];
    int    bt_size = backtrace(bt, BACKTRACE_SIZE);
    char **bt_syms = backtrace_symbols(bt, bt_size);

    if(bt_syms) {
        dprintf(dump_fd, "Stack Trace:\n");
        for(int i = 0; i < bt_size; i++) {
            dprintf(dump_fd, "%s\n", bt_syms[i]);
        }
        free(bt_syms);
    }

    FILE *maps = fopen("/proc/self/maps", "r");
    if(maps) {
        char line[256];
        dprintf(dump_fd, "\nMemory Regions:\n");

        while(fgets(line, sizeof(line), maps)) {
            unsigned long start, end;
            char          perms[5];

            if(sscanf(line, "%lx-%lx %4s", &start, &end, perms) == 3) {
                if(strchr(perms, 'r')) {
                    write(dump_fd, "\n--- Region: ", 12);
                    write(dump_fd, line, strlen(line));

                    // Only try to dump small regions to avoid massive files
                    if((end - start) < 1024 * 1024) { // 1MB limit per region
                        if(write(dump_fd, (void *)start, end - start) == -1) {
                            dprintf(dump_fd, "Failed to dump region: %s\n", strerror(errno));
                        }
                    } else {
                        dprintf(dump_fd, "Region too large to dump\n");
                    }
                }
            }
        }
        fclose(maps);
    }


    close(dump_fd);
    SdbLogInfo("Memory dump completed to %s", dump_filename);
}


static void
InitiateGracefulShutdown(void)
{
    __atomic_store_n(&g_shutdown_requested, 1, __ATOMIC_SEQ_CST);
    SdbLogInfo("Shutdown requested - waiting for threads to complete...");
}


static void
SignalHandler(int signum, siginfo_t *info, void *context)
{
    char  thread_name[32];
    pid_t thread_id = GetThreadId();
    GetThreadName(thread_name, sizeof(thread_name));

    switch(signum) {
        case SIGINT:
        case SIGTERM:
            SdbLogWarning("Received termination signal %d on thread %d (%s)", signum, thread_id,
                          thread_name);

            // Safely dump and parse pipe content
            if(g_signal_context.pipe) {
                char       dump_filename[256];
                char       parsed_filename[256];
                time_t     now     = time(NULL);
                struct tm *tm_info = localtime(&now);

                snprintf(dump_filename, sizeof(dump_filename),
                         "dumps/pipe_dump_%04d%02d%02d_%02d%02d%02d.bin", tm_info->tm_year + 1900,
                         tm_info->tm_mon + 1, tm_info->tm_mday, tm_info->tm_hour, tm_info->tm_min,
                         tm_info->tm_sec);


                // Dump pipe contents to file
                if(!SdbDumpSensorDataPipe(g_signal_context.pipe, dump_filename)) {
                    SdbLogError("Failed to dump pipe contents");
                    break;
                }


            } else {
                SdbLogError("Pipe is NULL during signal handling");
            }
            InitiateGracefulShutdown();
            break;


        case SIGSEGV:
            SdbLogError("Segmentation fault on thread %d (%s) at address %p", thread_id,
                        thread_name, info->si_addr);
            DumpProcessMemory(signum, info);
            raise(SIGSEGV);
            break;


        case SIGABRT:
            SdbLogError("Abnormal termination on thread %d (%s)", thread_id, thread_name);
            DumpProcessMemory(signum, info);
            raise(SIGABRT);
            break;


        case SIGFPE:
            SdbLogError("Floating point exception on thread %d (%s) code=%d", thread_id,
                        thread_name, info->si_code);
            DumpProcessMemory(signum, info);
            raise(SIGFPE);
            break;

        case SIGILL:
            SdbLogError("Illegal instruction on thread %d (%s) at address %p", thread_id,
                        thread_name, info->si_addr);
            DumpProcessMemory(signum, info);
            raise(SIGILL);
            break;


        default:
            SdbLogWarning("Unhandled signal %d received on thread %d (%s)", signum, thread_id,
                          thread_name);
            break;
    }
}

int
SdbSetupSignalHandlers(struct tg_manager *manager)
{
    g_signal_context.manager = manager;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = SignalHandler;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART;

    sigemptyset(&sa.sa_mask);

    int signals_to_handle[] = { SIGINT, SIGTERM, SIGSEGV, SIGABRT, SIGFPE, SIGILL };

    // Register signal handlers
    for(size_t i = 0; i < sizeof(signals_to_handle) / sizeof(signals_to_handle[0]); i++) {
        if(sigaction(signals_to_handle[i], &sa, NULL) == -1) {
            SdbLogError("Failed to set up signal handler for %d: %s", signals_to_handle[i],
                        strerror(errno));
            return -1;
        }
    }

    return 0;
}


void
SdbSetMemoryDumpPath(const char *path)
{
    if(g_signal_context.memory_dump_path) {
        free(g_signal_context.memory_dump_path);
    }
    g_signal_context.memory_dump_path = strdup(path);
}

bool
SdbIsShutdownInProgress(void)
{
    return g_signal_context.shutdown_flag == 1;
}


bool
SdbShouldShutdown(void)
{
    return __atomic_load_n(&g_shutdown_requested, __ATOMIC_SEQ_CST) != 0;
}


bool
SdbDumpSensorDataPipe(sensor_data_pipe *pipe, const char *filename)
{
    if(!pipe || !filename) {
        SdbLogError("Invalid pipe or filename for dump");
        return false;
    }

    FILE *dumpFile = fopen(filename, "wb");
    if(!dumpFile) {
        SdbLogError("Failed to create pipe dump file: %s", strerror(errno));
        return false;
    }

    // Write pipe metadata
    fwrite(&pipe->BufCount, sizeof(pipe->BufCount), 1, dumpFile);
    fwrite(&pipe->BufferMaxFill, sizeof(pipe->BufferMaxFill), 1, dumpFile);
    fwrite(&pipe->PacketSize, sizeof(pipe->PacketSize), 1, dumpFile);
    fwrite(&pipe->ItemMaxCount, sizeof(pipe->ItemMaxCount), 1, dumpFile);

    // Iterate through all buffers and write their full content
    for(u64 i = 0; i < pipe->BufCount; i++) {
        sdb_arena *buffer = pipe->Buffers[i];

        // Write buffer's current used size
        fwrite(&buffer->Cur, sizeof(buffer->Cur), 1, dumpFile);

        // Write entire buffer content if it has data
        if(buffer->Cur > 0) {
            fwrite(buffer->Mem, 1, buffer->Cur, dumpFile);
        }
    }

    fclose(dumpFile);
    SdbLogInfo("Complete sensor data pipe dump saved to %s", filename);
    return true;
}


typedef struct
{
    int64_t packet_id;
    double  time; // Use UNIX timestamp for simplicity
    double  rpm;
    double  torque;
    double  power;
    double  peak_peak_pfs;
} DataPacket;

// Function to convert binary dump to CSV
void
ConvertDumpToCSV(const char *dumpFileName, const char *csvFileName)
{
    FILE *dumpFile = fopen(dumpFileName, "rb");
    if(!dumpFile) {
        fprintf(stderr, "Failed to open dump file: %s\n", strerror(errno));
        return;
    }

    FILE *csvFile = fopen(csvFileName, "w");
    if(!csvFile) {
        fprintf(stderr, "Failed to create CSV file: %s\n", strerror(errno));
        fclose(dumpFile);
        return;
    }

    // Write CSV header
    fprintf(csvFile, "packet_id,time,rpm,torque,power,peak_peak_pfs\n");

    DataPacket packet;
    while(fread(&packet, sizeof(DataPacket), 1, dumpFile) == 1) {
        // Convert binary data to human-readable CSV format
        fprintf(csvFile, "%ld,%.2lf,%.2lf,%.2lf,%.2lf,%.2lf\n", packet.packet_id, packet.time,
                packet.rpm, packet.torque, packet.power, packet.peak_peak_pfs);
    }

    if(ferror(dumpFile)) {
        fprintf(stderr, "Error reading dump file: %s\n", strerror(errno));
    }

    fclose(dumpFile);
    fclose(csvFile);

    printf("Dump file converted to CSV successfully: %s\n", csvFileName);
}