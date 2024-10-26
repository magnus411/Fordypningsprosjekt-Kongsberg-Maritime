#ifndef POSTGRES_H_
#define POSTGRES_H_

#include <libpq-fe.h>

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/CircularBuffer.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Libs/cJSON/cJSON.h>
#include <src/Modules/DatabaseModule.h>

#define POSTGRES_CONF_FS_PATH "./configs/postgres-conf"

// PostgreSQL Type OID  and Names
enum pg_oid
{
    PG_BOOL        = 16,
    PG_INT8        = 20,
    PG_INT4        = 23,
    PG_TEXT        = 25,
    PG_FLOAT8      = 701,
    PG_VARCHAR     = 1043,
    PG_DATE        = 1082,
    PG_TIME        = 1083,
    PG_TIMESTAMPTZ = 1184,
    PG_NUMERIC     = 1700,
};

typedef u32    pg_oid;
typedef i32    pg_int4;
typedef i64    pg_int8;
typedef float  pg_float4;
typedef double pg_float8;

#define PG_BOOL_NAME        "boolean";
#define PG_INT8_NAME        "bigint";
#define PG_INT4_NAME        "integer";
#define PG_TEXT_NAME        "text";
#define PG_FLOAT8_NAME      "double precision";
#define PG_VARCHAR_NAME     "character varying";
#define PG_DATE_NAME        "date";
#define PG_TIMESTAMPTZ_NAME "timestamp with time zone";
#define PG_TIME_NAME        "time without time zone";
#define PG_NUMERIC_NAME     "numeric";

#define PG_TABLE_METADATA_FULL_QUERY_FMT_LEN 609
#define PG_TABLE_METADATA_FULL_QUERY_FMT                                                           \
    "SELECT c.oid AS table_oid, c.relname AS table_name, a.attnum AS column_number, a.attname AS " \
    "column_name, t.oid AS type_oid, t.typname AS type_name, t.typlen AS type_length, t.typbyval " \
    "AS type_by_value, t.typalign AS type_alignment, t.typstorage AS type_storage, a.atttypmod "   \
    "AS type_modifier, a.attnotnull AS not_null, pg_catalog.format_type(a.atttypid, a.atttypmod) " \
    "AS full_data_type FROM pg_catalog.pg_class c JOIN pg_catalog.pg_attribute a ON a.attrelid = " \
    "c.oid JOIN pg_catalog.pg_type t ON a.atttypid = t.oid WHERE c.relname = "                     \
    "'%s' AND a.attnum > 0 AND NOT a.attisdropped ORDER BY a.attnum"

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
} pg_col_metadata_full;

typedef struct
{
    sdb_string ColumnName;
    pg_oid     TypeOid;
    i16        TypeLength;
    i32        TypeModifier;

} pg_col_metadata;

typedef struct
{
    PGconn *DbConn;

    sdb_string *TableNames;
    u64         TableCount;

    sdb_string *PreparedStatements;
    u64         StatementCount;

    size_t InsertBufSize;
    u8    *InsertBuf;

} postgres_ctx;

#define PG_CTX(pg) ((postgres_ctx *)pg->Ctx)

void                  DiagnoseConnectionAndTable(PGconn *DbConn, const char *TableName);
void                  PrintPGresult(const PGresult *Result);
char                 *PqTableMetaDataFull(const char *TableName, u64 TableNameLen);
void                  PrintColumnMetadataFull(const pg_col_metadata_full *Metadata);
sdb_errno             PrepareStatements(database_api *Pg);
pg_col_metadata_full *GetTableMetadataFull(PGconn *DbConn, const char *TableName, u64 TableNameLen,
                                           int *ColCount);
pg_col_metadata      *GetTableMetadata(PGconn *DbConn, sdb_string TableName, int *ColCount,
                                       sdb_arena *A);

void InsertSensorData(PGconn *DbConn, const char *TableName, u64 TableNameLen, const u8 *SensorData,
                      size_t DataSize);

sdb_errno ProcessTablesInConfig(database_api *Pg, cJSON *SchemaConf);
void      PgInitThreadArenas(void);


SDB_END_EXTERN_C

#endif
