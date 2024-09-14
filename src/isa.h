#ifndef ISA_H_
#define ISA_H_

// NOLINTBEGIN(misc-definitions-in-headers)

#if defined(_WIN32) || defined(_WIN64)

// NOTE(ingar): isa.h must be included before any std lib header for this to take effect
#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>

#endif // Windows

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //NOTE(ingar): Replace with our own functions?

#if !defined(__cplusplus)
#include <stdbool.h>
#endif // C/C++

////////////////////////////////////////
//              DEFINES               //
////////////////////////////////////////

#define ISA_BEGIN_EXTERN_C                                                                         \
    extern "C"                                                                                     \
    {

#define ISA_END_EXTERN_C }

#if defined(__cplusplus)
ISA_BEGIN_EXTERN_C
#endif

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float  f32;
typedef double f64;

#define ISA_EXPAND(x)       x
#define ISA__STRINGIFY__(x) #x
#define ISA_STRINGIFY(x)    ISA__STRINGIFY__(x)

#define ISA__NUM_ARGS__(X99, X98, X97, X96, X95, X94, X93, X92, X91, X90, X89, X88, X87, X86, X85, \
                        X84, X83, X82, X81, X80, X79, X78, X77, X76, X75, X74, X73, X72, X71, X70, \
                        X69, X68, X67, X66, X65, X64, X63, X62, X61, X60, X59, X58, X57, X56, X55, \
                        X54, X53, X52, X51, X50, X49, X48, X47, X46, X45, X44, X43, X42, X41, X40, \
                        X39, X38, X37, X36, X35, X34, X33, X32, X31, X30, X29, X28, X27, X26, X25, \
                        X24, X23, X22, X21, X20, X19, X18, X17, X16, X15, X14, X13, X12, X11, X10, \
                        X9, X8, X7, X6, X5, X4, X3, X2, X1, X0, N, ...)                            \
    N

// NOTE(ingar): ISA_NUM_ARGS() will result in 1... because C's peepee doesn't function quite right
#define ISA_NUM_ARGS(...)                                                                          \
    ISA_EXPAND(ISA__NUM_ARGS__(                                                                    \
        __VA_ARGS__, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82,  \
        81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60,    \
        59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38,    \
        37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,    \
        15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))

#define IsaKiloByte(Number) (Number * 1000ULL)
#define IsaMegaByte(Number) (IsaKiloByte(Number) * 1000ULL)
#define IsaGigaByte(Number) (IsaMegaByte(Number) * 1000ULL)
#define IsaTeraByte(Number) (IsaGigaByte(Number) * 1000ULL)

#define IsaKibiByte(Number) (Number * 1024ULL)
#define IsaMebiByte(Number) (IsaKibiByte(Number) * 1024ULL)
#define IsaGibiByte(Number) (IsaMebiByte(Number) * 1024ULL)
#define IsaTebiByte(Number) (IsaGibiByte(Number) * 1024ULL)

#define IsaArrayLen(Array) (sizeof(Array) / sizeof(Array[0]))

#define ISA__CONCAT2__(x, y) x##y
#define ISA_CONCAT2(x, y)    ISA__CONCAT2__(x, y)

#define ISA__CONCAT3__(x, y, z) x##y##z
#define ISA_CONCAT3(x, y, z)    ISA__CONCAT3__(x, y, z)

#define IsaMax(a, b) ((a > b) ? a : b)
#define IsaMin(a, b) ((a < b) ? a : b)

#define isa_internal static
#define isa_persist  static
#define isa_global   static

////////////////////////////////////////
//              LOGGING               //
////////////////////////////////////////
/* Based on the logging frontend I wrote for oec */

#if !defined(NDEBUG)

#if !defined(ISA_LOG_BUF_SIZE)
#define ISA_LOG_BUF_SIZE 128
#endif

typedef struct isa__log_module__
{
    const char *Name;
    u64         BufferSize;
    char       *Buffer;
} isa__log_module__;

#if defined(_WIN32) || defined(_WIN64)

// #define ISA_LOG_OUTPUTDEBUGSTRING

#if defined(ISA_LOG_OUTPUTDEBUGSTRING)
// TODO(ingar): OutputDebugString does not support formatting, so I need to find some way to do that
#define Isa__LogPrint__(string) OutputDebugStringA(string)
#else
#define Isa__LogPrint__(string) printf("%s", string)
#endif

u64
Isa__FormatTimeWin32__(char *__restrict Buffer, u64 BufferRemaining)
{
    SYSTEMTIME Time;
    GetLocalTime(&Time);

    int CharsWritten
        = snprintf(Buffer, BufferRemaining, "%04d-%02d-%02d %02d:%02d:%02d: ", Time.wYear,
                   Time.wMonth, Time.wDay, Time.wHour, Time.wMinute, Time.wSecond);

    return (CharsWritten < 0) ? -1 : CharsWritten;
}
#define FormatTime Isa__FormatTimeWin32__

#elif defined(__linux__)

#include <time.h>

#define Isa__LogPrint__(string) printf("%s", string)

size_t
Isa__FormatTimePosix__(char *__restrict Buffer, u64 BufferRemaining)
{
    time_t    PosixTime;
    struct tm TimeInfo;

    time(&PosixTime);
    localtime_r(&PosixTime, &TimeInfo);

    u64 CharsWritten = strftime(Buffer, BufferRemaining, "%T: ", &TimeInfo);

    return (0 == CharsWritten) ? -1 : CharsWritten;
}
#define FormatTime              Isa__FormatTimePosix__
#elif defined(__APPLE__) && defined(__MACH__)
#endif // Platform

i64
Isa__WriteLog__(isa__log_module__ *Module, const char *LogLevel, va_list VaArgs)
{
    u64 BufferRemaining = Module->BufferSize;
    u64 CharsWritten    = FormatTime(Module->Buffer, BufferRemaining);
    if((CharsWritten < 0) || (CharsWritten >= BufferRemaining))
    {
        return -1;
    }

    BufferRemaining -= CharsWritten;

    i64 Ret = snprintf(Module->Buffer + CharsWritten, BufferRemaining, "%s: %s: ", Module->Name,
                       LogLevel);
    if((Ret < 0) || (Ret >= (i64)BufferRemaining))
    {
        return -1;
    }

    CharsWritten += Ret;
    BufferRemaining -= Ret;

    const char *FormatString = va_arg(VaArgs, const char *);
    Ret = vsnprintf(Module->Buffer + CharsWritten, BufferRemaining, FormatString, VaArgs);

    if(Ret < 0)
    {
        return -1;
    }

    if(Ret >= (i64)BufferRemaining)
    {
        Module->Buffer[Module->BufferSize - 2] = '\n';
    }
    else
    {
        CharsWritten += Ret;
        if(CharsWritten < (Module->BufferSize - 1))
        {
            Module->Buffer[CharsWritten]     = '\n';
            Module->Buffer[CharsWritten + 1] = '\0';
        }
        else
        {
            Module->Buffer[Module->BufferSize - 2] = '\n';
            Module->Buffer[Module->BufferSize - 1] = '\0';
        }
    }

    Isa__LogPrint__(Module->Buffer);
    return 0;
}

i64
Isa__WriteLogIntermediate__(isa__log_module__ *Module, const char *LogLevel, ...)
{
    va_list VaArgs;
    va_start(VaArgs, LogLevel);
    i64 Ret = Isa__WriteLog__(Module, LogLevel, VaArgs);
    va_end(VaArgs);
    return Ret;
}

i64
Isa__WriteLogNoModule__(const char *LogLevel, const char *FunctionName, ...)
{
    char             *Buffer = (char *)calloc(ISA_LOG_BUF_SIZE, sizeof(char));
    isa__log_module__ Module = {
        .Name       = FunctionName,
        .BufferSize = ISA_LOG_BUF_SIZE,
        .Buffer     = Buffer,
    };

    va_list VaArgs;
    va_start(VaArgs, FunctionName);

    i64 Ret = Isa__WriteLog__(&Module, LogLevel, VaArgs);

    va_end(VaArgs);
    free(Buffer);

    return Ret;
}

#if !defined(ISA_LOG_LEVEL)
#define ISA_LOG_LEVEL 4
#endif

#define ISA_LOG_LEVEL_NONE (0U)
#define ISA_LOG_LEVEL_ERR  (1U)
#define ISA_LOG_LEVEL_WRN  (2U)
#define ISA_LOG_LEVEL_INF  (3U)
#define ISA_LOG_LEVEL_DBG  (4U)

#define ISA__LOG_LEVEL_CHECK__(level) (ISA_LOG_LEVEL >= ISA_LOG_LEVEL_##level ? 1 : 0)

#if !defined(ISA_LOG_OVERRIDE)
#define ISA_LOG_REGISTER(module_name)                                                              \
    isa_global char              Isa__LogBuffer__[ISA_LOG_BUF_SIZE];                               \
    isa_global isa__log_module__ ISA_CONCAT3(Isa__LogModule, module_name, __)                      \
        = { .Name = #module_name, .BufferSize = ISA_LOG_BUF_SIZE, .Buffer = Isa__LogBuffer__ };    \
    isa_global isa__log_module__ *Isa__LogInstance__ = &ISA_CONCAT3(Isa__LogModule, module_name, __)

#define ISA_LOG_DECLARE_EXTERN(name)                                                               \
    extern isa__log_module__      ISA_CONCAT3(Isa__LogModule, name, __);                           \
    isa_global isa__log_module__ *Isa__LogInstance__ = &ISA_CONCAT3(Isa__LogModule, name, __)

#else /* ISA_LOG_OVERRIDE */
/* This allows files with different module names to be included in the same TU by overriding their
 * module names */

#define ISA_LOG_REGISTER_OVERRIDE(module_name)                                                     \
    isa_global char              Isa__LogBuffer__[ISA_LOG_BUF_SIZE];                               \
    isa_global isa__log_module__ ISA_CONCAT3(Isa__LogModule, module_name, __)                      \
        = { .Name = #module_name, .BufferSize = ISA_LOG_BUF_SIZE, .Buffer = Isa__LogBuffer__ };    \
    isa_global isa__log_module__ *Isa__LogInstance__ = &ISA_CONCAT3(Isa__LogModule, module_name, __)

#define ISA_LOG_REGISTER(name)       extern isa__log_module__ *Isa__LogInstance__
#define ISA_LOG_DECLARE_EXTERN(name) extern isa__log_module__ *Isa__LogInstance__

#endif /* ISA_LOG_OVERRIDE */

#define ISA_LOG_DECLARE_SAME_TU extern struct isa__log_module__ *Isa__LogInstance__

#define ISA__LOG__(log_level, ...)                                                                 \
    do                                                                                             \
    {                                                                                              \
        if(ISA__LOG_LEVEL_CHECK__(log_level))                                                      \
        {                                                                                          \
            i64 Ret = Isa__WriteLogIntermediate__(Isa__LogInstance__, ISA_STRINGIFY(log_level),    \
                                                  __VA_ARGS__);                                    \
            if(Ret)                                                                                \
            {                                                                                      \
                Isa__LogPrint__("\n\nERROR WHILE LOGGING\n\n");                                    \
                assert(0);                                                                         \
            }                                                                                      \
        }                                                                                          \
    } while(0)

#define ISA__LOG_NO_MODULE__(log_level, ...)                                                       \
    do                                                                                             \
    {                                                                                              \
        if(ISA__LOG_LEVEL_CHECK__(log_level))                                                      \
        {                                                                                          \
            i64 Ret = Isa__WriteLogNoModule__(ISA_STRINGIFY(log_level), __func__, __VA_ARGS__);    \
            if(Ret)                                                                                \
            {                                                                                      \
                Isa__LogPrint__("\n\nERROR WHILE LOGGING\n\n");                                    \
                assert(0);                                                                         \
            }                                                                                      \
        }                                                                                          \
    } while(0)

#define IsaLogDebug(...)   ISA__LOG__(DBG, __VA_ARGS__)
#define IsaLogInfo(...)    ISA__LOG__(INF, __VA_ARGS__)
#define IsaLogWarning(...) ISA__LOG__(WRN, __VA_ARGS__)
#define IsaLogError(...)   ISA__LOG__(ERR, __VA_ARGS__)

#define IsaLogDebugNoModule(...)   ISA__LOG_NO_MODULE__(DBG, __VA_ARGS__)
#define IsaLogInfoNoModule(...)    ISA__LOG_NO_MODULE__(INF, __VA_ARGS__)
#define IsaLogWarningNoModule(...) ISA__LOG_NO_MODULE__(WRN, __VA_ARGS__)
#define IsaLogErrorNoModule(...)   ISA__LOG_NO_MODULE__(ERR, __VA_ARGS__)

#define IsaAssert(condition)                                                                       \
    do                                                                                             \
    {                                                                                              \
        if(!(condition))                                                                           \
        {                                                                                          \
            ISA__LOG_NO_MODULE__(ERR, "Assertion failed: " ISA_STRINGIFY(condition));              \
            assert(condition);                                                                     \
        }                                                                                          \
    } while(0)

#endif // NDEBUG

////////////////////////////////////////
//               MISC                 //
////////////////////////////////////////

bool
IsaDoubleEpsilonCompare(const double A, const double B)
{
    double GreatestValue = (fabs(A) < fabs(B)) ? fabs(B) : fabs(A);
    return fabs(A - B) <= (GreatestValue * DBL_EPSILON);
}

u64
IsaDoubleSignBit(double F)
{
    u64  Mask = 1ULL << 63;
    u64 *Comp = (u64 *)&F;

    return (*Comp) & Mask;
}

double
IsaRadiansFromDegrees(double Degrees)
{
    double Radians = 0.01745329251994329577f * Degrees;
    return Radians;
}

////////////////////////////////////////
//               MEMORY               //
////////////////////////////////////////

void
IsaMemZero(void *Mem, u64 Size)
{
    for(u64 i = 0; i < Size; ++i)
    {
        ((u8 *)Mem)[i] = 0;
    }
}

void /* From https://github.com/BLAKE2/BLAKE2/blob/master/ref/blake2-impl.h */
IsaMemZeroSecure(void *Mem, u64 Size)
{
    static void *(*const volatile memset_v)(void *, int, u64) = &memset;
    (void)memset_v(Mem, 0, Size);
}

#define IsaMemZeroStruct(struct)       IsaMemZero(struct, sizeof(*struct))
#define IsaMemZeroStructSecure(struct) IsaMemZeroSecure(struct, sizeof(*struct))

// TODO(ingar): Change all u64 instances with fixed size to ensure, well... fixed size?
typedef struct isa_arena
{
    u64 Cur;
    u64 Cap;
    // u64 Save; /* DOES NOT WORK!!! Makes it easier to use the arena as a stack */
    u8 *Mem; /* If it's last, the arena's memory can be contiguous with the struct itself */
} isa_arena;

typedef struct isa_slice
{
    u64 Len; /* Not size_t since I want the member size to be constant */
    u64 ESize;
    u8 *Mem;
} isa_slice;

void
IsaArenaInit(isa_arena *Arena, u8 *Mem, u64 Size)
{
    Arena->Cur = 0;
    Arena->Cap = Size;
    // Arena->Save = 0;
    Arena->Mem = Mem;
}

isa_arena *
IsaArenaCreateContiguous(u8 *Mem, u64 Size)
{
    IsaAssert(Size >= (sizeof(isa_arena) + 1));

    isa_arena *Arena = (isa_arena *)Mem;
    Arena->Cap       = Size - sizeof(isa_arena);
    Arena->Cur       = 0;
    // Arena->Save      = 0;
    Arena->Mem = Mem + sizeof(isa_arena);

    return Arena;
}

#if 0
u64
IsaArenaF5(isa_arena *Arena)
{
    Arena->Save = Arena->Cur;
    return Arena->Save;
}

u64
IsaArenaF9(isa_arena *Arena)
{
    Arena->Cur  = Arena->Save;
    Arena->Save = 0;

    return Arena->Cur;
}
#endif

// TODO(ingar): Create aligning pushes
void *
IsaArenaPush(isa_arena *Arena, u64 Size)
{
    if((Arena->Cur + Size) < Arena->Cap)
    {
        u8 *AllocedMem = Arena->Mem + Arena->Cur;
        Arena->Cur += Size;

        return (void *)AllocedMem;
    }

    return NULL;
}

void *
IsaArenaPushZero(isa_arena *Arena, u64 Size)
{
    if((Arena->Cur + Size) < Arena->Cap)
    {
        u8 *AllocedMem = Arena->Mem + Arena->Cur;
        Arena->Cur += Size;
        IsaMemZero(AllocedMem, Size);

        return (void *)AllocedMem;
    }

    return NULL;
}

void
IsaArenaPop(isa_arena *Arena, u64 Size)
{
    assert(Arena->Cur >= Size);
    Arena->Cur -= Size;
}

u64
IsaArenaGetPos(isa_arena *Arena)
{
    u64 Pos = Arena->Cur;
    return Pos;
}

void
IsaArenaSeek(isa_arena *Arena, u64 Pos)
{
    assert(0 <= Pos && Pos <= Arena->Cap);
    Arena->Cur = Pos;
}

void
IsaArenaClear(isa_arena *Arena)
{
    Arena->Cur = 0;
}

void
IsaArenaClearZero(isa_arena *Arena)
{
    IsaMemZero(Arena->Mem, Arena->Cap);
    Arena->Cur = 0;
}

void
IsaArrayShift(void *Mem, u64 From, u64 To, u64 Count, u64 ElementSize)
{
    if(From != To)
    {
        u8 *Array = (u8 *)Mem;
        u8 *Src   = Array + (From * ElementSize);
        u8 *Dst   = Array + (To * ElementSize);

        u64 NumElements = (Count > From) ? (Count - From) : 0;

        if(NumElements > 0)
        {
            u64 NBytes = NumElements * ElementSize;
            memmove(Dst, Src, NBytes);
        }
    }
}

#define IsaArrayDeleteAndShift(mem, i, count, esize) IsaArrayShift(mem, (i + 1), i, count, esize)

#define IsaPushArray(arena, type, count)     (type *)IsaArenaPush(arena, sizeof(type) * (count))
#define IsaPushArrayZero(arena, type, count) (type *)IsaArenaPushZero(arena, sizeof(type) * (count))

#define IsaPushStruct(arena, type)     IsaPushArray(arena, type, 1)
#define IsaPushStructZero(arena, type) IsaPushArrayZero(arena, type, 1)

#define IsaNewSlice(arena, len, esize)                                                             \
    {                                                                                              \
        len, esize, (u8 *)IsaArenaPushZero(arena, len *esize)                                      \
    }

#define ISA_DEFINE_POOL_ALLOCATOR(type_name, func_name)                                            \
    typedef struct type_name##_Pool                                                                \
    {                                                                                              \
        isa_arena *Arena;                                                                          \
        type_name *FirstFree;                                                                      \
    } type_name##_pool;                                                                            \
                                                                                                   \
    type_name *func_name##Alloc(type_name##_pool *Pool)                                            \
    {                                                                                              \
        type_name *Result = Pool->FirstFree;                                                       \
        if(Result)                                                                                 \
        {                                                                                          \
            Pool->FirstFree = Pool->FirstFree->Next;                                               \
            IsaMemZeroStruct(Result);                                                              \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            Result = IsaPushStructZero(Pool->Arena, type_name);                                    \
        }                                                                                          \
                                                                                                   \
        return Result;                                                                             \
    }                                                                                              \
                                                                                                   \
    void func_name##Release(type_name##_pool *Pool, type_name *Instance)                           \
    {                                                                                              \
        Instance->Next  = Pool->FirstFree;                                                         \
        Pool->FirstFree = Instance;                                                                \
    }

typedef struct isa_string
{
    u64         Len; /* Does not include the null terminator*/
    const char *S;   /* Will always be null-terminated for simplicity */
} isa_string;

void
IsaMemcpy(void *To, void *From, u64 Len)
{
    // NOTE(ingar): The compiler should replace this with memcpy if it's available
    for(u64 i = 0; i < Len; ++i)
    {
        ((u8 *)To)[i] = ((u8 *)From)[i];
    }
}

u64
IsaStrlen(const char *String)
{
    u64 Count = 0;
    while(*String++ != '\0')
    {
        ++Count;
    }

    return Count;
}

char *
IsaStrdup(char *String, isa_arena *Arena)
{
    u64   StringLength = IsaStrlen(String);
    char *NewString    = IsaPushArrayZero(Arena, char, StringLength + 1);
    IsaMemcpy(NewString, String, StringLength);
    NewString[StringLength] = '\0';

    return NewString;
}

////////////////////////////////////////
//            MEM TRACE               //
////////////////////////////////////////

typedef struct
{
    bool  *Occupied;
    void **Pointer;
    char **Function;
    int   *Line;
    char **File;
} Isa__allocation_collection_entry__;

typedef struct
{
    u64  End; // Final index in the arrays (Capacity - 1)
    bool Initialized;
    u64  AllocationCount;

    bool  *Occupied;
    void **Pointer;
    char **Function;
    int   *Line;
    char **File;
} Isa__global_allocation_collection__;

Isa__global_allocation_collection__ *
Isa__GetGlobalAllocationCollection__(void)
{
    isa_persist Isa__global_allocation_collection__ Collection = { 0 };
    return &Collection;
}

// NOTE(ingar): This will work even if the struct has never had its members
// allocated before
//  but the memory will not be zeroed the first time around, so we might want to
//  do something about that
bool
Isa__AllocGlobalPointerCollection__(u64 NewCapacity)
{
    Isa__global_allocation_collection__ *Collection = Isa__GetGlobalAllocationCollection__();
    u64                                  NewEnd     = NewCapacity - 1;
    if(Collection->End >= NewEnd)
    {
        // TODO(ingar): Error handling
        return false;
    }

    Collection->End = NewEnd;

    void *OccupiedRealloc = realloc(Collection->Occupied, NewCapacity);
    void *PointerRealloc  = realloc(Collection->Pointer, NewCapacity);
    void *FunctionRealloc = realloc(Collection->Function, NewCapacity);
    void *LineRealloc     = realloc(Collection->Line, NewCapacity);
    void *FileRealloc     = realloc(Collection->File, NewCapacity);

    if(!OccupiedRealloc || !PointerRealloc || !FunctionRealloc || !LineRealloc || !FileRealloc)
    {
        // TODO(ingar): Error handling
        return false;
    }

    Collection->Occupied = (bool *)OccupiedRealloc;
    Collection->Pointer  = (void **)PointerRealloc;
    Collection->Function = (char **)FunctionRealloc;
    Collection->Line     = (int *)LineRealloc;
    Collection->File     = (char **)FileRealloc;

    return true;
}

Isa__allocation_collection_entry__
Isa__GetGlobalAllocationCollectionEntry__(void *Pointer)
{
    Isa__global_allocation_collection__ *Collection = Isa__GetGlobalAllocationCollection__();

    u64 Idx = 0;
    for(; Idx <= Collection->End; ++Idx)
    {
        if(Collection->Pointer[Idx] == Pointer)
        {
            break;
        }
    }

    if(Idx > Collection->End)
    {
        // TODO(ingar): Error handling
        Isa__allocation_collection_entry__ Entry = { 0 };
        return Entry;
    }

    Isa__allocation_collection_entry__ Entry;

    Entry.Occupied = &Collection->Occupied[Idx];
    Entry.Pointer  = &Collection->Pointer[Idx];
    Entry.Function = &Collection->Function[Idx];
    Entry.Line     = &Collection->Line[Idx];
    Entry.File     = &Collection->File[Idx];

    return Entry;
}

void
Isa__RegisterNewAllocation__(void *Pointer, const char *Function, int Line, const char *File)
{
    Isa__global_allocation_collection__ *Collection = Isa__GetGlobalAllocationCollection__();

    // TODO(ingar): This loop should never fail if we don't run out of memory
    //  but I should still add some error handling at some point
    u64 EntryIdx = 0;
    for(u64 i = 0; i <= Collection->End; ++i)
    {
        if(i > Collection->End)
        {
            u64 NewCapacity = (u64)(1.5 * (double)Collection->End);
            if(NewCapacity <= Collection->End)
            {
                // TODO(ingar): Handle wrapping
            }
            Isa__AllocGlobalPointerCollection__(NewCapacity);
        }

        if(!Collection->Occupied[i])
        {
            EntryIdx = i;
            break;
        }
    }

    u64 FunctionNameLength = IsaStrlen(Function) + 1;
    u64 FileNameLength     = IsaStrlen(File) + 1;

    char *FunctionNameString = (char *)malloc(FunctionNameLength);
    char *FileNameString     = (char *)malloc(FileNameLength);

    if(!FunctionNameString || !FileNameString)
    {
        // TODO(ingar): Error handling
    }

    strcpy(FunctionNameString, Function);
    strcpy(FileNameString, File);

    Collection->Occupied[EntryIdx] = true;
    Collection->Pointer[EntryIdx]  = Pointer;
    Collection->Function[EntryIdx] = FunctionNameString;
    Collection->Line[EntryIdx]     = Line;
    Collection->File[EntryIdx]     = FileNameString;

    Collection->AllocationCount++;
}

/**
 * @note Assumes that Pointer is not null
 */
void
Isa__RemoveAllocationFromGlobalCollection__(void *Pointer)
{
    Isa__allocation_collection_entry__ Entry = Isa__GetGlobalAllocationCollectionEntry__(Pointer);
    if(!Entry.Pointer)
    {
        // TODO(ingar): Error handling
    }

    *Entry.Occupied = false;
    *Entry.Line     = 0;
    free(*Entry.Function);
    free(*Entry.File);

    Isa__GetGlobalAllocationCollection__()->AllocationCount--;
}

void
Isa__UpdateRegisteredAllocation__(void *Original, void *New)
{
    Isa__global_allocation_collection__ *Collection = Isa__GetGlobalAllocationCollection__();

    u64 Idx = 0;
    for(; Idx <= Collection->End; ++Idx)
    {
        if(Collection->Pointer[Idx] == Original)
        {
            break;
        }
    }

    if(Idx > Collection->End)
    {
        // TODO(ingar): Error handling
        return;
    }

    Collection->Pointer[Idx] = New;
}

void *
Isa__MallocTrace__(u64 Size, const char *Function, int Line, const char *File)
{
    void *Pointer = malloc(Size);

    printf("MALLOC: In %s on line %d in %s:\n\n", Function, Line, File);
#if MEM_LOG
    Isa__RegisterNewAllocation(Pointer, Function, Line, File);
#endif

    return Pointer;
}

void *
Isa__CallocTrace__(u64 ElementCount, u64 ElementSize, const char *Function, int Line,
                   const char *File)
{
    void *Pointer = calloc(ElementCount, ElementSize);

    printf("CALLOC: In %s on line %d in %s\n\n", Function, Line, File);
#if MEM_LOG
    if(!Pointer)
    {
        return NULL;
    }
    Isa__RegisterNewAllocation(Pointer, Function, Line, File);
#endif

    return Pointer;
}

void *
Isa__ReallocTrace__(void *Pointer, u64 Size, const char *Function, int Line, const char *File)
{
    if(!Pointer)
    {
        return NULL;
    }

    printf("REALLOC: In %s on line %d in %s\n", Function, Line, File);
#if MEM_LOG
    Isa__allocation_collection_entry Entry = Isa__GetGlobalAllocationCollectionEntry(Pointer);
    if(!Entry.Pointer)
    {
        // TODO(ingar): Error handling
    }
    printf("         Previously allocated in %s on line %d in %s\n\n", *Entry.Function, *Entry.Line,
           *Entry.File);
    Isa__RemoveAllocationFromGlobalCollection(Pointer);
#endif

    void *PointerRealloc = realloc(Pointer, Size);
    if(!PointerRealloc)
    {
        return NULL;
    }
    Isa__RegisterNewAllocation__(PointerRealloc, Function, Line, File);

    return PointerRealloc;
}

bool
Isa__FreeTrace__(void *Pointer, const char *Function, int Line, const char *File)
{
    if(!Pointer)
    {
        return false;
    }

    printf("FREE: In %s on line %d in %s:\n", Function, Line, File);
#if MEM_LOG
    Isa__allocation_collection_entry Entry = Isa__GetGlobalAllocationCollectionEntry(Pointer);
    if(!Entry.Pointer)
    {
        // TODO(ingar): Error handling
    }
    printf("      Allocated in %s on line %d in %s\n\n", *Entry.Function, *Entry.Line, *Entry.File);
    Isa__RemoveAllocationFromGlobalCollection(Pointer);
#endif

    free(Pointer);
    return true;
}

// TODO(ingar): Pretty sure this code is busted. Considering I'm mostly going to be using arenas
// from now on, fixing this isn't a priority
#if 0
bool
IsaInitAllocationCollection(u64 Capacity)
{
    Isa__global_allocation_collection__ *Collection = Isa__GetGlobalAllocationCollection__();

    void *OccupiedRealloc = calloc(Capacity, sizeof(bool));
    void *PointerRealloc  = calloc(Capacity, sizeof(void *));
    void *FunctionRealloc = calloc(Capacity, sizeof(void *));
    void *LineRealloc     = calloc(Capacity, sizeof(void *));
    void *FileRealloc     = calloc(Capacity, sizeof(void *));

    if(!OccupiedRealloc || !PointerRealloc || !FunctionRealloc || !LineRealloc || !FileRealloc)
    {
        // TODO(ingar): Error handling
        return false;
    }

    Collection->Occupied = (bool *)OccupiedRealloc;
    Collection->Pointer  = (void **)PointerRealloc;
    Collection->Function = (char **)FunctionRealloc;
    Collection->Line     = (int *)LineRealloc;
    Collection->File     = (char **)FileRealloc;

    Collection->Initialized     = true;
    Collection->AllocationCount = 0;

    return true;
}

void
IsaPrintAllAllocations(void)
{
    Isa__global_allocation_collection__ *Collection = Isa__GetGlobalAllocationCollection__();
    if(Collection->AllocationCount > 0)
    {
        printf("DEBUG: Printing remaining allocations:\n");
        for(u64 i = 0; i <= Collection->End; ++i)
        {
            if(Collection->Occupied[i])
            {
                printf("\n\tIn %s on line %d in %s\n", Collection->Function[i], Collection->Line[i],
                       Collection->File[i]);
            }
        }
    }

    printf("\nDBEUG: There are %llu remaining allocations\n\n", Collection->AllocationCount);
}
#endif

#if MEM_TRACE
#define malloc(Size)           Isa__MallocTrace__(Size, __func__, __LINE__, __FILE__)
#define calloc(Count, Size)    Isa__CallocTrace__(Count, Size, __func__, __LINE__, __FILE__)
#define realloc(Pointer, Size) Isa__ReallocTrace__(Pointer, Size, __func__, __LINE__, __FILE__)
#define free(Pointer)          Isa__FreeTrace__(Pointer, __func__, __LINE__, __FILE__)

#else // MEM_TRACE

#define malloc(Size)           malloc(Size)
#define calloc(Count, Size)    calloc(Count, Size)
#define realloc(Pointer, Size) realloc(Pointer, Size)
#define free(Pointer)          free(Pointer)
#endif // MEM_TRACE

////////////////////////////////////////
//               RANDOM               //
////////////////////////////////////////

u32 *
Isa__GetPCGState__(void)
{
    isa_persist uint32_t Isa__PCGState = 0;
    return &Isa__PCGState;
}

// Implementation of the PCG algorithm (https://www.pcg-random.org)
// It's the caller's responsibilites to have called SeedRandPCG before use
u32
IsaRandPCG(void)
{
    u32 State             = *Isa__GetPCGState__();
    *Isa__GetPCGState__() = State * 747796405u + 2891336453u;
    u32 Word              = ((State >> ((State >> 28u) + 4u)) ^ State) * 277803737u;
    return (Word >> 22u) ^ Word;
}

void
IsaSeedRandPCG(uint32_t Seed)
{
    *Isa__GetPCGState__() = Seed;
}

/////////////////////////////////////////
//              FILE IO                //
/////////////////////////////////////////

typedef struct isa_file_data
{
    u64 Size;
    u8 *Data;
} isa_file_data;

isa_file_data *
IsaLoadFileIntoMemory(const char *Filename, isa_arena *Arena)
{
    FILE *fd = fopen(Filename, "rb");
    if(!fd)
    {
        return NULL;
    }

    if(fseek(fd, 0L, SEEK_END) != 0)
    {
        fclose(fd);
        return NULL;
    }

    u64 FileSize = (u64)ftell(fd);
    rewind(fd);

    isa_file_data *FileData = IsaPushStructZero(Arena, isa_file_data);
    FileData->Data          = IsaPushArray(Arena, u8, FileSize + 1);
    /*
    if(!FileData)
    {
        fprintf(stderr, "Could not allocate memory for file!\n");
        fclose(fd);
        return NULL;
    }
    */

    FileData->Size = FileSize;
    u64 BytesRead  = fread(FileData->Data, 1, FileSize, fd);
    if(BytesRead != FileSize)
    {
        fclose(fd);
        IsaArenaPop(Arena, sizeof(isa_file_data) + FileData->Size + 1);
        return NULL;
    }

    fclose(fd);

    FileData->Data[FileData->Size] = '\0';
    return FileData;
}

bool
IsaWriteBufferToFile(void *Buffer, u64 ElementSize, u64 ElementCount, const char *Filename)
{
    FILE *fd = fopen(Filename, "wb");
    if(!fd)
    {
        fprintf(stderr, "Unable to open file %s!\n", Filename);
        return false;
    }

    bool WriteSuccessful = fwrite(Buffer, ElementSize, ElementCount, fd) == ElementCount;
    fclose(fd);
    return WriteSuccessful;
}

bool
IsaWrite_isa_file_data_ToFile(isa_file_data *FileData, const char *Filename)
{
    FILE *fd = fopen(Filename, "wb");
    if(!fd)
    {
        fprintf(stderr, "Failed to open file during file_data write!\n");
        return false;
    }

    bool WriteSuccessful
        = fwrite(FileData->Data, sizeof(uint8_t), FileData->Size, fd) == FileData->Size;
    fclose(fd);
    return WriteSuccessful;
}

////////////////////////////////////////
//             TOKENIZER              //
////////////////////////////////////////

// TODO(ingar): This is not a general purpose tokenizer,
// but it is an example of an implementation
typedef struct isa_token
{
    char *Start;
    u64   Len;
} isa_token;

isa_token
IsaGetNextToken(char **Cursor)
{
    while('\t' != **Cursor)
    {
        (*Cursor)++;
    }

    (*Cursor)++; // Skips to start of hex number

    isa_token Token;
    Token.Start = *Cursor;
    Token.Len   = 0;

    while('\n' != **Cursor && '\r' != **Cursor)
    {
        (*Cursor)++;
        ++Token.Len;
    }

    if('\0' != **Cursor)
    {
        **Cursor = '\0';
    }

    return Token;
}

#if defined(__cplusplus)
ISA_END_EXTERN_C
#endif

// NOLINTEND(misc-definitions-in-headers)

#endif // ISA_H_
