#ifndef SDB_H_INCLUDE
#define SDB_H_INCLUDE

// NOLINTBEGIN(misc-definitions-in-headers)

// WARN: Do not remove any includes without first checking if they are used in the implementation
// part by uncommenting #define SDB_H_IMPLEMENTATION (and remember to comment it out again
// afterwards,
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if !defined(__cplusplus)
#include <stdbool.h>
#endif // C/C++

#include <src/Common/Errno.h>

////////////////////////////////////////
//              DEFINES               //
////////////////////////////////////////

// NOTE(ingar): Prevents the formatter from indenting everything inside the "C" block
#if defined(__cplusplus)
#define SDB_BEGIN_EXTERN_C                                                                         \
    extern "C"                                                                                     \
    {
#define SDB_END_EXTERN_C }
#else
#define SDB_BEGIN_EXTERN_C
#define SDB_END_EXTERN_C
#endif


#if defined(__cplusplus)
SDB_BEGIN_EXTERN_C
#endif

#define sdb_internal static
#define sdb_persist  static
#define sdb_global   static

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define SDB_EXPAND(x)       x
#define SDB__STRINGIFY__(x) #x
#define SDB_STRINGIFY(x)    SDB__STRINGIFY__(x)

#define SDB__CONCAT2__(x, y) x##y
#define SDB_CONCAT2(x, y)    SDB__CONCAT2__(x, y)

#define SDB__CONCAT3__(x, y, z) x##y##z
#define SDB_CONCAT3(x, y, z)    SDB__CONCAT3__(x, y, z)

#define SdbKiloByte(Number) (Number * 1000ULL)
#define SdbMegaByte(Number) (SdbKiloByte(Number) * 1000ULL)
#define SdbGigaByte(Number) (SdbMegaByte(Number) * 1000ULL)
#define SdbTeraByte(Number) (SdbGigaByte(Number) * 1000ULL)

#define SdbKibiByte(Number) (Number * 1024ULL)
#define SdbMebiByte(Number) (SdbKibiByte(Number) * 1024ULL)
#define SdbGibiByte(Number) (SdbMebiByte(Number) * 1024ULL)
#define SdbTebiByte(Number) (SdbGibiByte(Number) * 1024ULL)

#define SdbArrayLen(Array) (sizeof(Array) / sizeof(Array[0]))

#define SdbMax(a, b) ((a > b) ? a : b)
#define SdbMin(a, b) ((a < b) ? a : b)
bool   SdbDoubleEpsilonCompare(const double A, const double B);
u64    SdbDoubleSignBit(double F);
double SdbRadiansFromDegrees(double Degrees);

////////////////////////////////////////
//              LOGGING               //
////////////////////////////////////////
/* Based on the logging frontend I wrote for oec */

#if !defined(SDB_LOG_BUF_SIZE)
#define SDB_LOG_BUF_SIZE 2048
#endif

static_assert(SDB_LOG_BUF_SIZE >= 128, "SDB_LOG_BUF_SIZE must greater than or equal to 128!");

#define SDB_LOG_LEVEL_NONE (0U)
#define SDB_LOG_LEVEL_ERR  (1U)
#define SDB_LOG_LEVEL_WRN  (2U)
#define SDB_LOG_LEVEL_INF  (3U)
#define SDB_LOG_LEVEL_DBG  (4U)

#if !defined(SDB_LOG_LEVEL)
#define SDB_LOG_LEVEL 3
#endif

#if SDB_LOG_LEVEL >= 4 && SDB_PRINTF_DEBUG_ENABLE == 1
#define SdbPrintfDebug(...) printf(__VA_ARGS__);
#else
#define SdbPrintfDebug(...)
#endif

typedef struct sdb__log_module__
{
    const char     *Name;
    u64             BufferSize;
    char           *Buffer;
    pthread_mutex_t Lock;
} sdb__log_module__;

i64 Sdb__WriteLog__(sdb__log_module__ *Module, const char *LogLevel, const char *Fmt, ...)
    __attribute__((format(printf, 3, 4)));

#define SDB__LOG_LEVEL_CHECK__(level) (SDB_LOG_LEVEL >= SDB_LOG_LEVEL_##level)

#define SDB_LOG_REGISTER(module_name)                                                              \
    static char              SDB_CONCAT3(Sdb__LogModule, module_name, Buffer__)[SDB_LOG_BUF_SIZE]; \
    static sdb__log_module__ SDB_CONCAT3(Sdb__LogModule, module_name, __) __attribute__((used))    \
    = { .Name       = SDB_STRINGIFY(module_name),                                                  \
        .BufferSize = SDB_LOG_BUF_SIZE,                                                            \
        .Buffer     = SDB_CONCAT3(Sdb__LogModule, module_name, Buffer__),                          \
        .Lock       = PTHREAD_MUTEX_INITIALIZER };                                                       \
    static sdb__log_module__ *Sdb__LogInstance__ __attribute__((used))                             \
    = &SDB_CONCAT3(Sdb__LogModule, module_name, __)

#define SDB_LOG_DECLARE(name)                                                                      \
    sdb__log_module__         SDB_CONCAT3(Sdb__LogModule, name, __);                               \
    static sdb__log_module__ *Sdb__LogInstance__ __attribute__((used))                             \
    = &SDB_CONCAT3(Sdb__LogModule, name, __)

#define SDB__LOG__(log_level, fmt, ...)                                                            \
    do {                                                                                           \
        if(SDB__LOG_LEVEL_CHECK__(log_level)) {                                                    \
            sdb_errno LogRet = Sdb__WriteLog__(Sdb__LogInstance__, SDB_STRINGIFY(log_level), fmt,  \
                                               ##__VA_ARGS__);                                     \
            assert(LogRet >= 0);                                                                   \
        }                                                                                          \
    } while(0)

#define SdbLogDebug(...)   SDB__LOG__(DBG, ##__VA_ARGS__)
#define SdbLogInfo(...)    SDB__LOG__(INF, ##__VA_ARGS__)
#define SdbLogWarning(...) SDB__LOG__(WRN, ##__VA_ARGS__)
#define SdbLogError(...)   SDB__LOG__(ERR, ##__VA_ARGS__)

#define SdbAssert(condition, fmt, ...)                                                             \
    do {                                                                                           \
        if(!(condition)) {                                                                         \
            Sdb__WriteLog__(Sdb__LogInstance__, "DBG", "ASSERT (%s,%d): " fmt, __func__, __LINE__, \
                            ##__VA_ARGS__);                                                        \
            assert(condition);                                                                     \
        }                                                                                          \
    } while(0)

////////////////////////////////////////
//               MEMORY               //
////////////////////////////////////////

void SdbMemZero(void *Mem, u64 Size);
void SdbMemZeroSecure(void *Mem, u64 Size);
#define SdbMemZeroStruct(struct)       SdbMemZero(struct, sizeof(*struct))
#define SdbMemZeroStructSecure(struct) SdbMemZeroSecure(struct, sizeof(*struct))

typedef struct sdb_slice
{
    u64 Len;
    u64 ESize;
    u8 *Mem;
} sdb_slice;

// TODO(ingar): Make stack support, maybe by having an array which you can set the size of that
// contains positions
typedef struct sdb_arena
{
    u64 Cur;
    u64 Cap;
    u8 *Mem;
} sdb_arena;

typedef struct
{

    sdb_arena *Arena;
    u64        F5;
} sdb_scratch_arena;

void *SdbMemcpy(void *__restrict To, const void *__restrict From, size_t Len);
void *SdbMemset(void *__restrict Data, int SetTo, size_t Len);
bool  SdbMemcmp(const void *Lhs, const void *Rhs, size_t Len);

void       SdbArenaInit(sdb_arena *Arena, u8 *Mem, u64 Size);
sdb_arena *SdbArenaBootstrap(sdb_arena *Arena, sdb_arena *NewArena_, u64 Size);

sdb_scratch_arena SdbScratchArena(sdb_arena **Conflicts, u64 ConflictCount);
void              SdbScratchRelease(sdb_scratch_arena ScratchArena);

void *SdbArenaPush(sdb_arena *Arena, u64 Size);
void *SdbArenaPushZero(sdb_arena *Arena, u64 Size);
void *SdbArenaPop(sdb_arena *Arena, u64 Size);

u64   SdbArenaRemaining(sdb_arena *Arena);
u64   SdbArenaGetPos(sdb_arena *Arena);
void *SdbArenaSeek(sdb_arena *Arena, u64 Pos);
u64   SdbArenaReserve(sdb_arena *Arena, u64 Size);

void SdbArenaClear(sdb_arena *Arena);
void SdbArenaClearZero(sdb_arena *Arena);

#define SdbPushArray(arena, type, count)     (type *)SdbArenaPush(arena, sizeof(type) * (count))
#define SdbPushArrayZero(arena, type, count) (type *)SdbArenaPushZero(arena, sizeof(type) * (count))

#define SdbPushStruct(arena, type)     SdbPushArray(arena, type, 1)
#define SdbPushStructZero(arena, type) SdbPushArrayZero(arena, type, 1)

void SdbArrayShift(void *Mem, u64 From, u64 To, u64 Count, u64 ElementSize);
#define SdbArrayDeleteAndShift(mem, i, count, esize) SdbArrayShift(mem, (i + 1), i, count, esize)


#define SDB_POOL_ALLOCATOR_DEFINE(type_name, func_name)                                            \
    typedef struct type_name##_pool                                                                \
    {                                                                                              \
        sdb_arena *Arena;                                                                          \
        type_name *FirstFree;                                                                      \
    } type_name##_pool;                                                                            \
                                                                                                   \
    type_name *func_name##Alloc(type_name##_pool *Pool)                                            \
    {                                                                                              \
        type_name *Result = Pool->FirstFree;                                                       \
        if(Result) {                                                                               \
            Pool->FirstFree = Pool->FirstFree->Next;                                               \
            SdbMemZeroStruct(Result);                                                              \
        } else {                                                                                   \
            Result = SdbPushStructZero(Pool->Arena, type_name);                                    \
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

////////////////////////////////////////
//              STRINGS               //
////////////////////////////////////////

// NOTE(ingar): These are libc equivalents and are intended to be used with regular C-strings, not
// sdb_strings
u64   SdbStrnlen(const char *String, u64 Max);
u64   SdbStrlen(const char *String);
char *SdbStrdup(char *String, sdb_arena *Arena);


// Based on ginger bills gbString implementation. A few assumptions being made:
// The strings are not intended to be reallocated. If something has been pushed onto the arena, and
// you use a function that increases the strings length or capacity, it will *NOT* work (atm).
// Therefore, you should ideally use a scratch allocator to build a string and then copy it to
// permanent storage once it's finished and will not be altered further
typedef char *sdb_string;
typedef struct
{
    sdb_arena *Arena;
    u64        Len;
    u64        Cap;

} sdb_string_header;

#define SDB_STRING_HEADER(str) ((sdb_string_header *)(str)-1)

u64        SdbStringLen(sdb_string String);
u64        SdbStringCap(sdb_string String);
u64        SdbStringAvailableSpace(sdb_string String);
u64        SdbStringAllocSize(sdb_string String);
sdb_string SdbStringMakeSpace(sdb_string String, u64 AddLen);

sdb_string SdbStringMake(sdb_arena *A, const char *String);
void       SdbStringFree(sdb_string String);

sdb_string SdbStringDuplicate(sdb_arena *A, const sdb_string String);
void       SdbStringClear(sdb_string String);
sdb_string SdbStringSet(sdb_string String, const char *CString);
void       SdbStringBackspace(sdb_string String, u64 N);

sdb_string SdbStringAppend(sdb_string String, sdb_string Other);
sdb_string SdbStringAppendC(sdb_string String, const char *Other);
sdb_string SdbStringAppendFmt(sdb_string String, const char *Fmt, ...);

bool SdbStringsAreEqual(sdb_string Lhs, sdb_string Rhs);


typedef struct
{
    u64 Len; /* Does not include the null terminator*/
    u64 Cap;
    u64 ArenaPos;

    sdb_arena *Arena;
    char      *Str; /* Will always be null-terminated for simplicity */
} sdb_string_builder;

void  SdbStrBuilderInit(sdb_string_builder *Builder, sdb_arena *Arena);
void  SdbStrBuilderAppend(sdb_string_builder *Builder, char *String);
char *SdbStrBuilderGetString(sdb_string_builder *Builder);

////////////////////////////////////////
//                RNG                 //
////////////////////////////////////////

// Implementation of the PCG algorithm (https://www.pcg-random.org)
// It's the caller's responsibilites to have called SeedRandPCG before use

u32 *Sdb__GetPCGState__(void);
u32  SdbRandPCG(void);
void SdbSeedRandPCG(uint32_t Seed);

/////////////////////////////////////////
//              FILE IO                //
/////////////////////////////////////////

typedef struct
{
    u64 Size;
    u8 *Data;
} sdb_file_data;

sdb_file_data *SdbLoadFileIntoMemory(const char *Filename, sdb_arena *Arena);
bool SdbWriteBufferToFile(void *Buffer, u64 ElementSize, u64 ElementCount, const char *Filename);
bool SdbWrite_sdb_file_data_ToFile(sdb_file_data *FileData, const char *Filename);

////////////////////////////////////////
//             TOKENIZER              //
////////////////////////////////////////

typedef struct
{
    char *Start;
    u64   Len;
} sdb_token;

sdb_token SdbGetNextToken(char **Cursor);

////////////////////////////////////////
//            MEM TRACE               //
////////////////////////////////////////

void *Sdb__MallocTrace__(size_t Size, int Line, const char *Func, sdb__log_module__ *Module);
void *Sdb__CallocTrace__(u64 ElementCount, u64 ElementSize, int Line, const char *Func,
                         sdb__log_module__ *Module);
void *Sdb__ReallocTrace__(void *Pointer, size_t Size, int Line, const char *Func,
                          sdb__log_module__ *Module);
void  Sdb__FreeTrace__(void *Pointer, int Line, const char *Func, sdb__log_module__ *Module);

#if SDB_MEM_TRACE == 1
#define malloc(Size)        Sdb__MallocTrace__(Size, __LINE__, __func__, Sdb__LogInstance__)
#define calloc(Count, Size) Sdb__CallocTrace__(Count, Size, __LINE__, __func__, Sdb__LogInstance__)
#define realloc(Pointer, Size)                                                                     \
    Sdb__ReallocTrace__(Pointer, Size, __LINE__, __func__, Sdb__LogInstance__)
#define free(Pointer) Sdb__FreeTrace__(Pointer, __LINE__, __func__, Sdb__LogInstance__)
#endif

#if defined(__cplusplus)
SDB_END_EXTERN_C
#endif

#endif // SDB_H_INCLUDE

// WARN: Only one file in a program should define SDB_H_IMPLEMENTATION, otherwise you will get
// redefintion errors
// #define SDB_H_IMPLEMENTATION
#ifdef SDB_H_IMPLEMENTATION

// NOTE(ingar): The reason this is done is so that functions inside Sdb.h don't use the trace
// macros. The macros are redefined at the bottom
#if SDB_MEM_TRACE == 1
#undef malloc
#undef calloc
#undef realloc
#undef free
#endif

////////////////////////////////////////
//               MISC                 //
////////////////////////////////////////

bool
SdbDoubleEpsilonCompare(const double A, const double B)
{
    double GreatestValue = (fabs(A) < fabs(B)) ? fabs(B) : fabs(A);
    return fabs(A - B) <= (GreatestValue * DBL_EPSILON);
}

u64
SdbDoubleSignBit(double F)
{
    u64  Mask = 1ULL << 63;
    u64 *Comp = (u64 *)&F;

    return (*Comp) & Mask;
}

double
SdbRadiansFromDegrees(double Degrees)
{
    double Radians = 0.01745329251994329577f * Degrees;
    return Radians;
}

////////////////////////////////////////
//              LOGGING               //
////////////////////////////////////////
/* Based on the logging frontend I wrote for oec */

i64
Sdb__WriteLog__(sdb__log_module__ *Module, const char *LogLevel, const char *Fmt, ...)
{
    pthread_mutex_lock(&Module->Lock);

    time_t    PosixTime;
    struct tm TimeInfo;

    if((time_t)(-1) == time(&PosixTime)) {
        return -errno;
    }

    if(NULL == localtime_r(&PosixTime, &TimeInfo)) {
        return -errno;
    }

    u64 CharsWritten    = 0;
    u64 BufferRemaining = Module->BufferSize;

    int FormatRet = strftime(Module->Buffer, BufferRemaining, "%T: ", &TimeInfo);
    if(0 == FormatRet) {
        // NOTE(ingar): Since the buffer size is at least 128, this should never happen
        assert(FormatRet);
        return -ENOMEM;
    }

    CharsWritten = FormatRet;
    BufferRemaining -= FormatRet;

    FormatRet = snprintf(Module->Buffer + CharsWritten, BufferRemaining, "%s: %s: ", Module->Name,
                         LogLevel);
    if(FormatRet < 0) {
        return -errno;
    } else if((u64)FormatRet >= BufferRemaining) {
        // NOTE(ingar): If the log module name is so long that it does not fit in 128 bytes - the
        // time stamp, it should be changed
        assert(FormatRet);
        return -ENOMEM;
    }

    CharsWritten += FormatRet;
    BufferRemaining -= FormatRet;

    va_list VaArgs;
    va_start(VaArgs, Fmt);
    FormatRet = vsnprintf(Module->Buffer + CharsWritten, BufferRemaining, Fmt, VaArgs);
    va_end(VaArgs);

    if(FormatRet < 0) {
        return -errno;
    } else if((u64)FormatRet >= BufferRemaining) {
        (void)SdbMemset(Module->Buffer + CharsWritten, 0, BufferRemaining);
        FormatRet = snprintf(Module->Buffer + CharsWritten, BufferRemaining, "%s",
                             "Message dropped; too big");
        if(FormatRet < 0) {
            return -errno;
        } else if((u64)FormatRet >= BufferRemaining) {
            assert(FormatRet);
            return -ENOMEM;
        }
    }

    CharsWritten += FormatRet;
    Module->Buffer[CharsWritten++] = '\n';

    int OutFd;
    if('E' == LogLevel[0]) {
        OutFd = STDERR_FILENO;
    } else {
        OutFd = STDOUT_FILENO;
    }

    if((ssize_t)(-1) == write(OutFd, Module->Buffer, CharsWritten)) {
        return -errno;
    }

    pthread_mutex_unlock(&Module->Lock);

    return 0;
}

////////////////////////////////////////
//               MEMORY               //
////////////////////////////////////////

void *
SdbMemcpy(void *__restrict To, const void *__restrict From, size_t Len)
{
    // NOTE(ingar): The compiler should replace this with memcpy if it's available
    for(size_t i = 0; i < Len; ++i) {
        ((u8 *)To)[i] = ((u8 *)From)[i];
    }

    return To;
}

void *
SdbMemset(void *__restrict Data, int SetTo, size_t Len)
{
    // NOTE(ingar): The compiler should replace this with memset if it's available. Don't ask me why
    // you cast it to a u8* and not an int*, but that must be done for it to be replaced
    for(size_t i = 0; i < Len; ++i) {
        ((u8 *)Data)[i] = SetTo;
    }
    return Data;
}

bool
SdbMemcmp(const void *Lhs, const void *Rhs, size_t Len)
{
    // NOTE(ingar): The compiler (probably) won't replace this one with memcmp, but GCC didn't
    // replace their own memcmp implementation with a call to memcmp, so that's just on them
    for(size_t i = 0; i < Len; ++i) {
        if(((u8 *)Lhs)[i] != ((u8 *)Rhs)[i]) {
            return false;
        }
    }
    return true;
}

void
SdbMemZero(void *__restrict Mem, u64 Size)
{
    (void)SdbMemset(Mem, 0, Size);
}

void /* From https://github.com/BLAKE2/BLAKE2/blob/master/ref/blake2-impl.h */
SdbMemZeroSecure(void *Mem, u64 Size)
{
    static void *(*const volatile memset_v)(void *, int, u64) = &memset;
    (void)memset_v(Mem, 0, Size);
}

void
SdbArenaInit(sdb_arena *Arena, u8 *Mem, u64 Size)
{
    Arena->Cur = 0;
    Arena->Cap = Size;
    Arena->Mem = Mem;
}

sdb_arena *
SdbArenaBootstrap(sdb_arena *Arena, sdb_arena *NewArena_, u64 Size)
{
    sdb_arena *NewArena;
    if(NewArena_ == NULL) {
        NewArena = SdbPushStruct(Arena, sdb_arena);
    } else {
        NewArena = NewArena_;
    }
    u8 *NewArenaMem = SdbPushArray(Arena, u8, Size);

    if(NULL == NewArena && NULL == NewArenaMem) {
        return NULL;
    }

    SdbArenaInit(NewArena, NewArenaMem, Size);
    return NewArena;
}

// TODO(ingar): Create aligning pushes
void *
SdbArenaPush(sdb_arena *Arena, u64 Size)
{
    if(Arena->Cur + Size <= Arena->Cap) {
        u8 *AllocedMem = Arena->Mem + Arena->Cur;
        Arena->Cur += Size;
        return AllocedMem;
    }

    return NULL;
}

void *
SdbArenaPushZero(sdb_arena *Arena, u64 Size)
{
    if(Arena->Cur + Size <= Arena->Cap) {
        u8 *AllocedMem = Arena->Mem + Arena->Cur;
        Arena->Cur += Size;
        SdbMemZero(AllocedMem, Size);
        return AllocedMem;
    }

    return NULL;
}

u64
SdbArenaRemaining(sdb_arena *Arena)
{
    return Arena->Cap - Arena->Cur;
}

// TODO(ingar): Does returning the position add overhead/is it necessary?
void *
SdbArenaPop(sdb_arena *Arena, u64 Size)
{
    if(Arena->Cur >= Size) {
        Arena->Cur -= Size;
        return Arena->Mem + Arena->Cur;
    }

    return NULL;
}

u64
SdbArenaGetPos(sdb_arena *Arena)
{
    u64 Pos = Arena->Cur;
    return Pos;
}

void *
SdbArenaSeek(sdb_arena *Arena, u64 Pos)
{
    if(0 <= Pos && Pos <= Arena->Cap) {
        Arena->Cur = Pos;
        return Arena->Mem + Arena->Cur;
    }

    return NULL;
}

u64
SdbArenaReserve(sdb_arena *Arena, u64 Size)
{
    if(Arena->Cur + Size <= Arena->Cap) {
        Arena->Cur += Size;
        return Size;
    } else {
        return SdbArenaRemaining(Arena);
    }
}

void
SdbArenaClear(sdb_arena *Arena)
{
    Arena->Cur = 0;
}

void
SdbArenaClearZero(sdb_arena *Arena)
{
    SdbMemZero(Arena->Mem, Arena->Cap);
    Arena->Cur = 0;
}

void
SdbArrayShift(void *Mem, u64 From, u64 To, u64 Count, u64 ElementSize)
{
    if(From != To) {
        u8 *Array = (u8 *)Mem;
        u8 *Src   = Array + (From * ElementSize);
        u8 *Dst   = Array + (To * ElementSize);

        u64 NumElements = (Count > From) ? (Count - From) : 0;

        if(NumElements > 0) {
            u64 NBytes = NumElements * ElementSize;
            memmove(Dst, Src, NBytes);
        }
    }
}

////////////////////////////////////////
//              STRINGS               //
////////////////////////////////////////


u64
SdbStrnlen(const char *String, u64 Max)
{
    u64 Count = 0;
    while(*String++ != '\0' && Count < Max) {
        ++Count;
    }

    return Count;
}

u64
SdbStrlen(const char *String)
{
    u64 Count = 0;
    while(*String++ != '\0') {
        ++Count;
    }

    return Count;
}

char *
SdbStrdup(char *String, sdb_arena *Arena)
{
    u64   StringLength = SdbStrlen(String);
    char *NewString    = SdbPushArray(Arena, char, StringLength + 1);
    SdbMemcpy(NewString, String, StringLength);
    NewString[StringLength] = '\0';

    return NewString;
}

//
// SDB String
//

u64
SdbStringLen(sdb_string String)
{
    return SDB_STRING_HEADER(String)->Len;
}
u64 SdbStringCap(sdb_string String);

u64
SdbStringCap(sdb_string String)
{
    return SDB_STRING_HEADER(String)->Cap;
}

u64
SdbStringAvailableSpace(sdb_string String)
{
    sdb_string_header *Header = SDB_STRING_HEADER(String);
    return Header->Cap - Header->Len;
}

u64
SdbStringAllocSize(sdb_string String)
{
    u64 Size = sizeof(sdb_string_header) + SdbStringCap(String);
    return Size;
}

void
Sdb__StringSetLen__(sdb_string String, u64 Len)
{
    SDB_STRING_HEADER(String)->Len = Len;
}

void
Sdb__StringSetCap__(sdb_string String, u64 Cap)
{
    SDB_STRING_HEADER(String)->Cap = Cap;
}

sdb_string
Sdb__StringMake__(sdb_arena *A, const void *InitString, u64 Len)
{
    size_t HeaderSize = sizeof(sdb_string_header);
    void  *Ptr        = SdbArenaPush(A, HeaderSize + Len + 1);
    if(Ptr == NULL) {
        return NULL;
    }
    if(InitString == NULL) {
        SdbMemset(Ptr, 0, HeaderSize + Len + 1);
    }

    sdb_string         String;
    sdb_string_header *Header;

    String = (char *)Ptr + HeaderSize;
    Header = SDB_STRING_HEADER(String);

    Header->Arena = A;
    Header->Len   = Len;
    Header->Cap   = Len;
    if(Len > 0 && (InitString != NULL)) {
        SdbMemcpy(String, InitString, Len);
        String[Len] = '\0';
    }

    return String;
}

sdb_string
SdbStringMake(sdb_arena *A, const char *String)
{
    u64 Len = (String != NULL) ? SdbStrlen(String) : 0;
    return Sdb__StringMake__(A, String, Len);
}

void
SdbStringFree(sdb_string String)
{
    if(String != NULL) {
        sdb_string_header *H       = SDB_STRING_HEADER(String);
        u64                PopSize = sizeof(sdb_string_header) + H->Cap + 1;
        SdbArenaPop(H->Arena, PopSize);
    }
}

sdb_string
SdbStringDuplicate(sdb_arena *A, const sdb_string String)
{
    return Sdb__StringMake__(A, String, SdbStringLen(String));
}

void
SdbStringClear(sdb_string String)
{
    Sdb__StringSetLen__(String, 0);
    String[0] = '\0';
}

void
SdbStringBackspace(sdb_string String, u64 N)
{
    u64 NewLen = SdbStringLen(String) - N;
    Sdb__StringSetLen__(String, NewLen);
    String[NewLen] = '\0';
}

sdb_string
SdbStringMakeSpace(sdb_string String, u64 AddLen)
{
    u64 Available = SdbStringAvailableSpace(String);
    if(Available < AddLen) {
        sdb_arena *A        = SDB_STRING_HEADER(String)->Arena;
        u64        Reserved = SdbArenaReserve(A, AddLen + 1);
        if(Reserved < AddLen) {
            return NULL;
        }

        u64 NewCap = SdbStringCap(String) + AddLen;
        Sdb__StringSetCap__(String, NewCap);
    }
    return String;
}

sdb_string
Sdb__StringAppend__(sdb_string String, const void *Other, u64 OtherLen)
{
    String = SdbStringMakeSpace(String, OtherLen);
    if(String == NULL) {
        return NULL;
    }

    u64 CurLen = SdbStringLen(String);
    SdbMemcpy(String + CurLen, Other, OtherLen);
    String[CurLen + OtherLen] = '\0';
    return String;
}

sdb_string
SdbStringAppend(sdb_string String, sdb_string Other)
{
    return Sdb__StringAppend__(String, Other, SdbStringLen(Other));
}

sdb_string
SdbStringAppendC(sdb_string String, const char *Other)
{
    return Sdb__StringAppend__(String, Other, SdbStrlen(Other));
}

sdb_string
SdbStringAppendFmt(sdb_string String, const char *Fmt, ...)
{
    // TODO(ingar): Make arena mandatory or use String's arena?
    char    Buf[1024] = { 0 };
    va_list Va;
    va_start(Va, Fmt);
    u64 FmtLen = snprintf(Buf, SdbArrayLen(Buf), Fmt, Va); // TODO(ingar): stb_sprintf?
    va_end(Va);
    return Sdb__StringAppend__(String, Buf, FmtLen);
}

sdb_string
SdbStringSet(sdb_string String, const char *CString)
{
    u64 Len = SdbStrlen(CString);
    String  = SdbStringMakeSpace(String, Len);
    if(String == NULL) {
        return NULL;
    }

    SdbMemcpy(String, CString, Len);
    String[Len] = '\0';
    Sdb__StringSetLen__(String, Len);

    return String;
}

bool
SdbStringsAreEqual(sdb_string Lhs, sdb_string Rhs)
{
    u64 LStringLen = SdbStringLen(Lhs);
    u64 RStringLen = SdbStringLen(Rhs);

    if(LStringLen != RStringLen) {
        return false;
    } else {
        return SdbMemcmp(Lhs, Rhs, LStringLen);
    }
}

////////////////////////////////////////
//                RNG                 //
////////////////////////////////////////

u32 *
Sdb__GetPCGState__(void)
{
    static uint32_t Sdb__PCGState = 0;
    return &Sdb__PCGState;
}

// Implementation of the PCG algorithm (https://www.pcg-random.org)
// It's the caller's responsibilites to have called SeedRandPCG before use
u32
SdbRandPCG(void)
{
    u32 State             = *Sdb__GetPCGState__();
    *Sdb__GetPCGState__() = State * 747796405u + 2891336453u;
    u32 Word              = ((State >> ((State >> 28u) + 4u)) ^ State) * 277803737u;
    return (Word >> 22u) ^ Word;
}

void
SdbSeedRandPCG(uint32_t Seed)
{
    *Sdb__GetPCGState__() = Seed;
}

/////////////////////////////////////////
//              FILE IO                //
/////////////////////////////////////////

sdb_file_data *
SdbLoadFileIntoMemory(const char *Filename, sdb_arena *Arena)
{
    FILE *File = fopen(Filename, "rb");
    if(!File) {
        return NULL;
    }

    if(fseek(File, 0L, SEEK_END) != 0) {
        fclose(File);
        return NULL;
    }

    u64 FileSize = (u64)ftell(File);
    rewind(File);

    sdb_file_data *FileData;
    if(NULL != Arena) {
        FileData       = SdbPushStruct(Arena, sdb_file_data);
        FileData->Data = SdbPushArray(Arena, u8, FileSize + 1);
    } else {
        u64 FileDataSize = sizeof(sdb_file_data) + FileSize + 1;
        FileData         = calloc(1, FileDataSize);
        if(!FileData) {
            fclose(File);
            return NULL;
        }
    }

    FileData->Size = FileSize;
    u64 BytesRead  = fread(FileData->Data, 1, FileSize, File);
    if(BytesRead != FileSize) {
        fclose(File);
        if(Arena != NULL) {
            SdbArenaPop(Arena, sizeof(sdb_file_data) + FileData->Size + 1);
        } else {
            free(FileData);
        }
        return NULL;
    }

    fclose(File);
    return FileData;
}

bool
SdbWriteBufferToFile(void *Buffer, u64 ElementSize, u64 ElementCount, const char *Filename)
{
    FILE *fd = fopen(Filename, "wb");
    if(!fd) {
        fprintf(stderr, "Unable to open file %s!\n", Filename);
        return false;
    }

    bool WriteSuccessful = fwrite(Buffer, ElementSize, ElementCount, fd) == ElementCount;
    fclose(fd);
    return WriteSuccessful;
}

////////////////////////////////////////
//            "TOKENIZER"             //
////////////////////////////////////////

sdb_token
SdbGetNextToken(char **Cursor)
{
    while('\t' != **Cursor) {
        (*Cursor)++;
    }

    (*Cursor)++; // Skips to start of hex number

    sdb_token Token;
    Token.Start = *Cursor;
    Token.Len   = 0;

    while('\n' != **Cursor && '\r' != **Cursor) {
        (*Cursor)++;
        ++Token.Len;
    }

    if('\0' != **Cursor) {
        **Cursor = '\0';
    }

    return Token;
}
////////////////////////////////////////
//            MEM TRACE               //
////////////////////////////////////////

void *
Sdb__MallocTrace__(size_t Size, int Line, const char *Func, sdb__log_module__ *Module)
{
    void *Pointer = malloc(Size);
    Sdb__WriteLog__(Module, "DBG", "MALLOC (%s,%d): %p (%zd B)", Func, Line, Pointer, Size);
    return Pointer;
}

void *
Sdb__CallocTrace__(u64 ElementCount, u64 ElementSize, int Line, const char *Func,
                   sdb__log_module__ *Module)
{
    void *Pointer = calloc(ElementCount, ElementSize);
    Sdb__WriteLog__(Module, "DBG", "CALLOC (%s,%d): %p (%lu * %luB = %zd B)", Func, Line, Pointer,
                    ElementCount, ElementSize, (size_t)(ElementCount * ElementSize));
    return Pointer;
}

void *
Sdb__ReallocTrace__(void *Pointer, size_t Size, int Line, const char *Func,
                    sdb__log_module__ *Module)
{
    void *Original = Pointer;
    void *Realloc  = NULL;
    if(Pointer != NULL) {
        Realloc = realloc(Pointer, Size);
    }
    Sdb__WriteLog__(Module, "DBG", "REALLOC (%s,%d): %p -> %p (%zd B)", Func, Line, Original,
                    Realloc, Size);
    return Realloc;
}

void
Sdb__FreeTrace__(void *Pointer, int Line, const char *Func, sdb__log_module__ *Module)
{
    Sdb__WriteLog__(Module, "DBG", "FREE (%s,%d): %p", Func, Line, Pointer);
    if(Pointer != NULL) {
        free(Pointer);
    }
}

#if(SDB_MEM_TRACE == 1) && (SDB_LOG_LEVEL >= SDB_LOG_LEVEL_DBG)
#define malloc(Size)        Sdb__MallocTrace__(Size, __LINE__, __func__, Sdb__LogInstance__)
#define calloc(Count, Size) Sdb__CallocTrace__(Count, Size, __LINE__, __func__, Sdb__LogInstance__)
#define realloc(Pointer, Size)                                                                     \
    Sdb__ReallocTrace__(Pointer, Size, __LINE__, __func__, Sdb__LogInstance__)
#define free(Pointer) Sdb__FreeTrace__(Pointer, __LINE__, __func__, Sdb__LogInstance__)
#endif

// NOLINTEND(misc-definitions-in-headers)

#endif // SDB_H_IMPLEMENTATION
