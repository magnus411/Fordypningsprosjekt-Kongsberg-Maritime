#ifndef POSTGRES_TYPES_H_
#define POSTGRES_TYPES_H_

#include "SdbExtern.h"

// PostgreSQL Type OIDs and Names
enum pq_oid : unsigned int
{
    BOOL        = 16,
    BIGINT      = 20,
    INT4        = 23,
    TEXT        = 25,
    FLOAT8      = 701,
    VARCHAR     = 1043,
    DATE        = 1082,
    TIME        = 1083,
    TIMESTAMPTZ = 1184,
    NUMERIC     = 1700,
};

static const char *PQ_BOOL_NAME        = "boolean";
static const char *PQ_BIGINT_NAME      = "bigint";
static const char *PQ_INT4_NAME        = "integer";
static const char *PQ_TEXT_NAME        = "text";
static const char *PQ_FLOAT8_NAME      = "double precision";
static const char *PQ_VARCHAR_NAME     = "character varying";
static const char *PQ_DATE_NAME        = "date";
static const char *PQ_TIMESTAMPTZ_NAME = "timestamp with time zone";
static const char *PQ_TIME_NAME        = "time without time zone";
static const char *PQ_NUMERIC_NAME     = "numeric";

typedef struct
{
    u32   TableOid;
    char *TableName;
    i16   ColumnNumber;
    char *ColumnName;
    u32   TypeOid;
    char *TypeName;
    i16   TypeLength;
    bool  TypeByValue;
    char  TypeAlignment;
    char  TypeStorage;
    i32   TypeModifier;
    bool  NotNull;
    char *FullDataType;
} pq_col_metadata;

char *GetSqlQueryFromFile(const char *FileName, sdb_arena *Arena);

#endif
