/**
 * @file Signals.c
 * @brief Signal handling implementation for the Sensor Data Handler System
 *
 * Implements signal handling, memory dumping, and graceful shutdown mechanisms
 * for the sensor data handling system. Includes facilities for process memory
 * dumps and pipe data preservation during crashes or shutdowns.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <src/Signals.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(Signals);

#include <src/Common/SensorDataPipe.h>
#include <src/DevUtils/TestConstants.h>

#define BACKTRACE_SIZE 50

/**
 * @brief Context structure for signal handling
 * This should be documented in the header file (Signals.h)
 */
static volatile sig_atomic_t GShutdownRequested = 0;
signal_handler_context       GSignalContext     = { 0 };

static pid_t
GetThreadId(void)
{
    return syscall(SYS_gettid);
}

static void
GetThreadName(char *Name, size_t Size)
{
    Name[0] = '\0';
    if(pthread_getname_np(pthread_self(), Name, Size) != 0) {
        snprintf(Name, Size, "Unknown");
    }
}


/**
 * @brief Creates a memory dump of the process state
 *
 * Generates a comprehensive dump including:
 * - Stack trace
 * - Thread information
 * - Memory regions
 * - Signal context
 *
 * @param SignalNum The signal that triggered the dump
 * @param Info Signal information structure
 */
static void
DumpProcessMemory(int SignalNum, siginfo_t *Info)
{
    if(mkdir("dumps", 0755) == -1 && errno != EEXIST) {
        SdbLogError("Failed to create dumps directory: %s", strerror(errno));
        return;
    }

    char  DumpFilename[256];
    char  ThreadName[32];
    pid_t ThreadId = GetThreadId();
    GetThreadName(ThreadName, sizeof(ThreadName));

    time_t     Now    = time(NULL);
    struct tm *TmInfo = localtime(&Now);

    snprintf(DumpFilename, sizeof(DumpFilename),
             "dumps/sdb_dump_%04d%02d%02d_%02d%02d%02d_sig%d_thread%d.dump", TmInfo->tm_year + 1900,
             TmInfo->tm_mon + 1, TmInfo->tm_mday, TmInfo->tm_hour, TmInfo->tm_min, TmInfo->tm_sec,
             SignalNum, ThreadId);

    int DumpFd = open(DumpFilename, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    if(DumpFd == -1) {
        SdbLogError("Failed to create memory dump file: %s", strerror(errno));
        return;
    }

    char Header[1024];
    snprintf(Header, sizeof(Header),
             "Memory Dump Information:\n"
             "Timestamp: %s"
             "Signal: %d\n"
             "Thread ID: %d\n"
             "Thread Name: %s\n"
             "Fault Address: %p\n"
             "Process ID: %d\n\n",
             ctime(&Now), SignalNum, ThreadId, ThreadName, Info->si_addr, getpid());

    write(DumpFd, Header, strlen(Header));

    void  *Backtrace[BACKTRACE_SIZE];
    int    BacktraceSize    = backtrace(Backtrace, BACKTRACE_SIZE);
    char **BacktraceSymbols = backtrace_symbols(Backtrace, BacktraceSize);

    if(BacktraceSymbols) {
        dprintf(DumpFd, "Stack Trace:\n");
        for(int i = 0; i < BacktraceSize; i++) {
            dprintf(DumpFd, "%s\n", BacktraceSymbols[i]);
        }
        free(BacktraceSymbols);
    }

    FILE *Maps = fopen("/proc/self/maps", "r");
    if(Maps) {
        char Line[256];
        dprintf(DumpFd, "\nMemory Regions:\n");

        while(fgets(Line, sizeof(Line), Maps)) {
            unsigned long Start, End;
            char          Perms[5];

            if(sscanf(Line, "%lx-%lx %4s", &Start, &End, Perms) == 3) {
                if(strchr(Perms, 'r')) {
                    write(DumpFd, "\n--- Region: ", 12);
                    write(DumpFd, Line, strlen(Line));

                    if((End - Start) < 1024 * 1024) { // 1MB limit per region
                        if(write(DumpFd, (void *)Start, End - Start) == -1) {
                            dprintf(DumpFd, "Failed to dump region: %s\n", strerror(errno));
                        }
                    } else {
                        dprintf(DumpFd, "Region too large to dump\n");
                    }
                }
            }
        }
        fclose(Maps);
    }

    close(DumpFd);
    SdbLogInfo("Memory dump completed to %s", DumpFilename);
}

/**
 * @brief Initiates system shutdown with proper synchronization
 *
 * Uses atomic operations to ensure thread-safe shutdown initiation
 */
static void
InitiateGracefulShutdown(void)
{
    __atomic_store_n(&GShutdownRequested, 1, __ATOMIC_SEQ_CST);
    SdbLogInfo("Shutdown requested - waiting for threads to complete...");
}

/**
 * @brief Handles internal pipe dump operations
 *
 * Creates timestamped dump files of pipe contents during signal handling.
 * Used by the signal handler to preserve data during crashes.
 *
 * @return true if dump successful, false on error
 */
static bool
HandlePipeDump(void)
{
    if(!GSignalContext.Pipe) {
        SdbLogError("Pipe is NULL during signal handling");
        return false;
    }

    char       DumpFilename[256];
    time_t     Now    = time(NULL);
    struct tm *TmInfo = localtime(&Now);

    snprintf(DumpFilename, sizeof(DumpFilename), "dumps/pipe_dump_%04d%02d%02d_%02d%02d%02d.bin",
             TmInfo->tm_year + 1900, TmInfo->tm_mon + 1, TmInfo->tm_mday, TmInfo->tm_hour,
             TmInfo->tm_min, TmInfo->tm_sec);

    if(!SdbDumpSensorDataPipe(GSignalContext.Pipe, DumpFilename)) {
        SdbLogError("Failed to dump pipe contents");
        return false;
    }

    return true;
}


/**
 * @brief Internal signal handler to process system signals
 *
 * Handles different types of signals:
 * - SIGINT/SIGTERM: Initiates graceful shutdown
 * - SIGSEGV: Handles segmentation faults
 * - SIGABRT: Handles abnormal terminations
 * - SIGFPE: Handles floating point exceptions
 * - SIGILL: Handles illegal instructions
 *
 * @param SignalNum Signal number received
 * @param Info Signal information structure
 * @param Context Signal context
 */
static void
SignalHandler(int SignalNum, siginfo_t *Info, void *Context)
{
    switch(SignalNum) {
        case SIGINT:
        case SIGTERM:
            {
                SdbLogWarning("Received termination signal %d", SignalNum);
                InitiateGracefulShutdown();
            }
            break;
        case SIGSEGV:
            {
                SdbLogError("Segmentation fault on thread at address %p", Info->si_addr);
                DumpProcessMemory(SignalNum, Info);
                HandlePipeDump();
                raise(SIGSEGV);
            }
            break;
        case SIGABRT:
            {
                SdbLogError("Abnormal termination");
                DumpProcessMemory(SignalNum, Info);
                raise(SIGABRT);
            }
            break;
        case SIGFPE:
            {
                SdbLogError("Floating point exception on code=%d", Info->si_code);
                DumpProcessMemory(SignalNum, Info);
                HandlePipeDump();
                raise(SIGFPE);
            }
            break;
        case SIGILL:
            {
                SdbLogError("Illegal instruction at address %p", Info->si_addr);
                DumpProcessMemory(SignalNum, Info);
                raise(SIGILL);
            }
            break;
        default:
            SdbLogWarning("Unhandled signal %d received", SignalNum);
            break;
    }
}

/**
 * @brief Sets up signal handlers for the application
 *
 * Sets up handlers for system signals including SIGINT, SIGTERM,
 * SIGSEGV, SIGABRT, SIGFPE, and SIGILL.
 *
 * @param Manager Thread group manager to be controlled by signals
 * @return 0 on success, -1 on failure
 */
int
SdbSetupSignalHandlers(struct tg_manager *Manager)
{
    GSignalContext.Manager = Manager;

    struct sigaction SignalAction;
    memset(&SignalAction, 0, sizeof(SignalAction));
    SignalAction.sa_sigaction = SignalHandler;
    SignalAction.sa_flags     = SA_SIGINFO | SA_RESTART;

    sigemptyset(&SignalAction.sa_mask);

    int SignalsToHandle[] = { SIGINT, SIGTERM, SIGSEGV, SIGABRT, SIGFPE, SIGILL };

    for(size_t i = 0; i < sizeof(SignalsToHandle) / sizeof(SignalsToHandle[0]); i++) {
        if(sigaction(SignalsToHandle[i], &SignalAction, NULL) == -1) {
            SdbLogError("Failed to set up signal handler for %d: %s", SignalsToHandle[i],
                        strerror(errno));
            return -1;
        }
    }

    return 0;
}

void
SdbSetMemoryDumpPath(const char *Path)
{
    if(GSignalContext.MemoryDumpPath) {
        free(GSignalContext.MemoryDumpPath);
    }
    GSignalContext.MemoryDumpPath = strdup(Path);
}

bool
SdbIsShutdownInProgress(void)
{
    return GSignalContext.ShutdownFlag == 1;
}

bool
SdbShouldShutdown(void)
{
    return __atomic_load_n(&GShutdownRequested, __ATOMIC_SEQ_CST) != 0;
}

/**
 * @brief Dumps sensor data pipe contents to a file
 *
 * Creates a binary dump of the pipe's current state including all
 * buffer contents and metadata.
 *
 * @param pipe Pointer to the sensor data pipe to dump
 * @param filename Path where the dump will be saved
 * @return true if successful, false if any error occurred
 */
bool
SdbDumpSensorDataPipe(sensor_data_pipe *Pipe, const char *Filename)
{
    if(!Pipe || !Filename) {
        SdbLogError("Invalid pipe or filename for dump");
        return false;
    }

    FILE *DumpFile = fopen(Filename, "wb");
    if(!DumpFile) {
        SdbLogError("Failed to create pipe dump file: %s", strerror(errno));
        return false;
    }

    fwrite(&Pipe->BufCount, sizeof(Pipe->BufCount), 1, DumpFile);
    fwrite(&Pipe->BufferMaxFill, sizeof(Pipe->BufferMaxFill), 1, DumpFile);
    fwrite(&Pipe->PacketSize, sizeof(Pipe->PacketSize), 1, DumpFile);
    fwrite(&Pipe->ItemMaxCount, sizeof(Pipe->ItemMaxCount), 1, DumpFile);

    for(u64 i = 0; i < Pipe->BufCount; i++) {
        sdb_arena *Buffer = Pipe->Buffers[i];
        fwrite(&Buffer->Cur, sizeof(Buffer->Cur), 1, DumpFile);

        if(Buffer->Cur > 0) {
            fwrite(Buffer->Mem, 1, Buffer->Cur, DumpFile);
        }
    }

    fclose(DumpFile);
    SdbLogInfo("Complete sensor data pipe dump saved to %s", Filename);
    return true;
}

void
ConvertDumpToCSV(const char *DumpFileName, const char *CsvFileName)
{
    FILE *DumpFile = fopen(DumpFileName, "rb");
    if(!DumpFile) {
        fprintf(stderr, "Failed to open dump file: %s\n", strerror(errno));
        return;
    }

    FILE *CsvFile = fopen(CsvFileName, "w");
    if(!CsvFile) {
        fprintf(stderr, "Failed to create CSV file: %s\n", strerror(errno));
        fclose(DumpFile);
        return;
    }

    fprintf(CsvFile, "PacketId,Time,Rpm,Torque,Power,PeakPeakPfs\n");

    shaft_power_data Packet;
    while(fread(&Packet, sizeof(shaft_power_data), 1, DumpFile) == 1) {
        fprintf(CsvFile, "%ld,%.2ld,%.2lf,%.2lf,%.2lf,%.2lf\n", Packet.PacketId, Packet.Time,
                Packet.Rpm, Packet.Torque, Packet.Power, Packet.PeakPeakPfs);
    }

    if(ferror(DumpFile)) {
        fprintf(stderr, "Error reading dump file: %s\n", strerror(errno));
    }

    fclose(DumpFile);
    fclose(CsvFile);

    printf("Dump file converted to CSV successfully: %s\n", CsvFileName);
}
