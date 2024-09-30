#ifndef SDB_EXTERN_H_
#define SDB_EXTERN_H_

#ifndef SDB_H_

// NOLINTBEGIN(misc-definitions-in-headers)

#include <assert.h>
#include <errno.h>
#include <float.h>
#include <math.h>
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

////////////////////////////////////////
//              DEFINES               //
////////////////////////////////////////

#define SDB_BEGIN_EXTERN_C                                                                         \
    extern "C"                                                                                     \
    {
#define SDB_END_EXTERN_C }

// NOTE(ingar): Prevents the formatter from indenting everything inside the extern "C" block
#if defined(__cplusplus)
SDB_BEGIN_EXTERN_C
#endif

#define sdb_internal static
#define sdb_persist  static
#define sdb_global   static

// TODO(ingar): Add support for custom errno codes
/* 0 is defined as success; negative errno code otherwise. */
typedef int_least32_t sdb_errno;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// NOTE(ingar): DO NOT USE THESE OUTSIDE OF THIS FILE! Since we are working with embedded systems
// floats and doubles may not necessarily be 32-bit and 64-bit respectively
typedef float  f32;
typedef double f64;

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

////////////////////////////////////////
//               MISC                 //
////////////////////////////////////////

extern bool   SdbDoubleEpsilonCompare(const double A, const double B);
extern u64    SdbDoubleSignBit(double F);
extern double SdbRadiansFromDegrees(double Degrees);

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

#if SDB_LOG_LEVEL >= 4
#define SdbPrintfDebug(...) printf(__VA_ARGS__);
#else
#define SdbPrintfDebug(...)
#endif

typedef struct sdb__log_module__
{
    const char *Name;
    u64         BufferSize;
    char       *Buffer;
} sdb__log_module__;

extern i64       Sdb__WriteLog__(sdb__log_module__ *Module, const char *LogLevel, va_list VaArgs);
extern sdb_errno Sdb__WriteLogIntermediate__(sdb__log_module__ *Module, const char *LogLevel, ...);
extern sdb_errno Sdb__WriteLogNoModule__(const char *LogLevel, const char *FunctionName, ...);

#define SDB__LOG_LEVEL_CHECK__(level) (SDB_LOG_LEVEL >= SDB_LOG_LEVEL_##level)

#define SDB_LOG_REGISTER(module_name)                                                              \
    sdb_global char SDB_CONCAT3(Sdb__LogModule, module_name, Buffer__)[SDB_LOG_BUF_SIZE];          \
    sdb_global sdb__log_module__ SDB_CONCAT3(Sdb__LogModule, module_name, __)                      \
        __attribute__((used))                                                                      \
        = { .Name       = #module_name,                                                            \
            .BufferSize = SDB_LOG_BUF_SIZE,                                                        \
            .Buffer     = SDB_CONCAT3(Sdb__LogModule, module_name, Buffer__) };                        \
    sdb_global sdb__log_module__ *Sdb__LogInstance__ __attribute__((used))                         \
    = &SDB_CONCAT3(Sdb__LogModule, module_name, __)

#define SDB_LOG_DECLARE_EXTERN(name)                                                               \
    extern sdb__log_module__      SDB_CONCAT3(Sdb__LogModule, name, __) __attribute__((used));     \
    sdb_global sdb__log_module__ *Sdb__LogInstance__ __attribute__((used))                         \
    = &SDB_CONCAT3(Sdb__LogModule, name, __)

#define SDB_LOG_DECLARE_SAME_TU                                                                    \
    extern struct sdb__log_module__ *Sdb__LogInstance__ __attribute__((used));

#define SDB__LOG__(log_level, ...)                                                                 \
    do                                                                                             \
    {                                                                                              \
        if(SDB__LOG_LEVEL_CHECK__(log_level))                                                      \
        {                                                                                          \
            sdb_errno LogRet = Sdb__WriteLogIntermediate__(Sdb__LogInstance__,                     \
                                                           SDB_STRINGIFY(log_level), __VA_ARGS__); \
            assert(LogRet >= 0);                                                                   \
        }                                                                                          \
    } while(0)

#define SDB__LOG_NO_MODULE__(log_level, ...)                                                       \
    do                                                                                             \
    {                                                                                              \
        if(SDB__LOG_LEVEL_CHECK__(log_level))                                                      \
        {                                                                                          \
            sdb_errno LogRet                                                                       \
                = Sdb__WriteLogNoModule__(SDB_STRINGIFY(log_level), __func__, __VA_ARGS__);        \
            assert(LogRet >= 0);                                                                   \
        }                                                                                          \
    } while(0)

#define SdbLogDebug(...)   SDB__LOG__(DBG, __VA_ARGS__)
#define SdbLogInfo(...)    SDB__LOG__(INF, __VA_ARGS__)
#define SdbLogWarning(...) SDB__LOG__(WRN, __VA_ARGS__)
#define SdbLogError(...)   SDB__LOG__(ERR, __VA_ARGS__)

// WARN: Uses calloc!
#define SdbLogDebugNoModule(...)   SDB__LOG_NO_MODULE__(DBG, __VA_ARGS__)
#define SdbLogInfoNoModule(...)    SDB__LOG_NO_MODULE__(INF, __VA_ARGS__)
#define SdbLogWarningNoModule(...) SDB__LOG_NO_MODULE__(WRN, __VA_ARGS__)
#define SdbLogErrorNoModule(...)   SDB__LOG_NO_MODULE__(ERR, __VA_ARGS__)

#define SdbAssert(condition)                                                                       \
    do                                                                                             \
    {                                                                                              \
        if(!(condition))                                                                           \
        {                                                                                          \
            SdbLogError("Assertion failed: " SDB_STRINGIFY(condition));                            \
            assert(condition);                                                                     \
        }                                                                                          \
    } while(0)

////////////////////////////////////////
//             ALLOCATORS             //
////////////////////////////////////////

extern void SdbMemZero(void *Mem, u64 Size);
extern void SdbMemZeroSecure(void *Mem, u64 Size);
#define SdbMemZeroStruct(struct)       SdbMemZero(struct, sizeof(*struct))
#define SdbMemZeroStructSecure(struct) SdbMemZeroSecure(struct, sizeof(*struct))

typedef struct sdb_arena
{
    u64 Cur;
    u64 Cap;
    // u64 Save; /* DOES NOT WORK!!! Makes it easier to use the arena as a stack */
    u8 *Mem; /* If it's last, the arena's memory can be contiguous with the struct itself */
} sdb_arena;

typedef struct sdb_slice
{
    u64 Len; /* Not size_t since I want the member size to be constant */
    u64 ESize;
    u8 *Mem;
} sdb_slice;

extern void       SdbArenaInit(sdb_arena *Arena, u8 *Mem, u64 Size);
extern sdb_arena *SdbArenaCreateContiguous(u8 *Mem, u64 Size);

#if 0
u64
SdbArenaF5(sdb_arena *Arena)
{
    Arena->Save = Arena->Cur;
    return Arena->Save;
}

u64
SdbArenaF9(sdb_arena *Arena)
{
    Arena->Cur  = Arena->Save;
    Arena->Save = 0;

    return Arena->Cur;
}
#endif

extern void *SdbArenaPush(sdb_arena *Arena, u64 Size);

extern void *SdbArenaPushZero(sdb_arena *Arena, u64 Size);

extern void SdbArenaPop(sdb_arena *Arena, u64 Size);

extern u64 SdbArenaGetPos(sdb_arena *Arena);

extern void SdbArenaSeek(sdb_arena *Arena, u64 Pos);

extern void SdbArenaClear(sdb_arena *Arena);

extern void SdbArenaClearZero(sdb_arena *Arena);

extern void SdbArrayShift(void *Mem, u64 From, u64 To, u64 Count, u64 ElementSize);

#define SdbArrayDeleteAndShift(mem, i, count, esize) SdbArrayShift(mem, (i + 1), i, count, esize)

#define SdbPushArray(arena, type, count)     (type *)SdbArenaPush(arena, sizeof(type) * (count))
#define SdbPushArrayZero(arena, type, count) (type *)SdbArenaPushZero(arena, sizeof(type) * (count))

#define SdbPushStruct(arena, type)     SdbPushArray(arena, type, 1)
#define SdbPushStructZero(arena, type) SdbPushArrayZero(arena, type, 1)

#define SDB_DEFINE_POOL_ALLOCATOR(type_name, func_name)                                            \
    typedef struct type_name##_Pool                                                                \
    {                                                                                              \
        sdb_arena *Arena;                                                                          \
        type_name *FirstFree;                                                                      \
    } type_name##_pool;                                                                            \
                                                                                                   \
    type_name *func_name##Alloc(type_name##_pool *Pool)                                            \
    {                                                                                              \
        type_name *Result = Pool->FirstFree;                                                       \
        if(Result)                                                                                 \
        {                                                                                          \
            Pool->FirstFree = Pool->FirstFree->Next;                                               \
            SdbMemZeroStruct(Result);                                                              \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
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

typedef struct sdb_string
{
    u64         Len; /* Does not include the null terminator*/
    const char *S;   /* Will always be null-terminated for simplicity */
} sdb_string;

extern void SdbMemcpy(void *To, void *From, u64 Len);
extern u64  SdbStrnlen(const char *String, u64 Max);
extern u64  SdbStrlen(const char *String);

extern char *SdbStrdup(char *String, sdb_arena *Arena);

////////////////////////////////////////
//                RNG                 //
////////////////////////////////////////

extern u32 *Sdb__GetPCGState__(void);

// Implementation of the PCG algorithm (https://www.pcg-random.org)
// It's the caller's responsibilites to have called SeedRandPCG before use
extern u32 SdbRandPCG(void);

extern void SdbSeedRandPCG(uint32_t Seed);

/////////////////////////////////////////
//              FILE IO                //
/////////////////////////////////////////

typedef struct sdb_file_data
{
    u64 Size;
    u8 *Data;
} sdb_file_data;

extern sdb_file_data *SdbLoadFileIntoMemory(const char *Filename, sdb_arena *Arena);

extern bool SdbWriteBufferToFile(void *Buffer, u64 ElementSize, u64 ElementCount,
                                 const char *Filename);

extern bool SdbWrite_sdb_file_data_ToFile(sdb_file_data *FileData, const char *Filename);

////////////////////////////////////////
//             TOKENIZER              //
////////////////////////////////////////

// TODO(ingar): This is not a general purpose tokenizer,
// but it is an example of an implementation
typedef struct sdb_token
{
    char *Start;
    u64   Len;
} sdb_token;

extern sdb_token SdbGetNextToken(char **Cursor);

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
} Sdb__allocation_collection_entry__;

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
} Sdb__global_allocation_collection__;

extern Sdb__global_allocation_collection__ *Sdb__GetGlobalAllocationCollection__(void);

// NOTE(ingar): This will work even if the struct has never had its members
// allocated before
//  but the memory will not be zeroed the first time around, so we might want to
//  do something about that
extern bool Sdb__AllocGlobalPointerCollection__(u64 NewCapacity);

extern Sdb__allocation_collection_entry__ Sdb__GetGlobalAllocationCollectionEntry__(void *Pointer);

extern void Sdb__RegisterNewAllocation__(void *Pointer, const char *Function, int Line,
                                         const char *File);

/**
 * @note Assumes that Pointer is not null
 */
extern void Sdb__RemoveAllocationFromGlobalCollection__(void *Pointer);

extern void Sdb__UpdateRegisteredAllocation__(void *Original, void *New);

extern void *Sdb__MallocTrace__(u64 Size, const char *Function, int Line, const char *File);

extern void *Sdb__CallocTrace__(u64 ElementCount, u64 ElementSize, const char *Function, int Line,
                                const char *File);

extern void *Sdb__ReallocTrace__(void *Pointer, u64 Size, const char *Function, int Line,
                                 const char *File);

extern bool Sdb__FreeTrace__(void *Pointer, const char *Function, int Line, const char *File);

// TODO(ingar): Pretty sure this code is busted. Considering I'm mostly going to be
// using arenas from now on, fixing this isn't a priority
#if 0
bool
SdbInitAllocationCollection(u64 Capacity)
{
    Sdb__global_allocation_collection__ *Collection = Sdb__GetGlobalAllocationCollection__();

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
SdbPrintAllAllocations(void)
{
    Sdb__global_allocation_collection__ *Collection = Sdb__GetGlobalAllocationCollection__();
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
#define malloc(Size)           Sdb__MallocTrace__(Size, __func__, __LINE__, __FILE__)
#define calloc(Count, Size)    Sdb__CallocTrace__(Count, Size, __func__, __LINE__, __FILE__)
#define realloc(Pointer, Size) Sdb__ReallocTrace__(Pointer, Size, __func__, __LINE__, __FILE__)
#define free(Pointer)          Sdb__FreeTrace__(Pointer, __func__, __LINE__, __FILE__)

#else // MEM_TRACE

#define malloc(Size)           malloc(Size)
#define calloc(Count, Size)    calloc(Count, Size)
#define realloc(Pointer, Size) realloc(Pointer, Size)
#define free(Pointer)          free(Pointer)
#endif // MEM_TRACE

#if defined(__cplusplus)
SDB_END_EXTERN_C
#endif

#endif // SDB_H_

#endif // SDB_EXTERN_H_
