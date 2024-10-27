#ifndef POSTGRES_H_
#define POSTGRES_H_

#include <libpq-fe.h>

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/CircularBuffer.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>
#include <src/Libs/cJSON/cJSON.h>
#include <src/Modules/DatabaseModule.h>

#define POSTGRES_CONF_FS_PATH "./configs/postgres-conf"

// PostgreSQL Type OID  and Names
enum
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

#define PQ_TABLE_METADATA_QUERY_FMT                                                                \
    "SELECT a.attname AS column_name, t.oid AS type_oid, t.typlen AS type_length, "                \
    "a.atttypmod AS type_modifier FROM pg_catalog.pg_class c "                                     \
    "JOIN pg_catalog.pg_attribute a ON a.attrelid = c.oid "                                        \
    "JOIN pg_catalog.pg_type t ON a.atttypid = t.oid "                                             \
    "WHERE c.relname = '%s' AND a.attnum > 0 AND NOT a.attisdropped "                              \
    "AND a.attname != 'id' ORDER BY a.attnum"

typedef struct
{
    sdb_string ColumnName;
    pg_oid     TypeOid;
    i16        TypeLength;
    i16        Offset;
    i32        TypeModifier;

} pg_col_metadata;

typedef struct
{
    sdb_string       TableName;
    sdb_string       Statement;
    u64              ColCount;
    pg_col_metadata *ColMetadata;

} pg_table_info;

typedef struct
{
    PGconn   *DbConn;
    sdb_mutex ConnLock;

    pg_table_info *TablesInfo;
    u64            TablesCount;

    // TODO(ingar): We're probably just gonna use the pipe. Fill up a buffer, signal to db insert,
    // and switch to another buffer for writing while the previous is written to db
    size_t InsertBufSize;
    u8    *InsertBuf;

} postgres_ctx;

#define PG_CTX(pg) ((postgres_ctx *)pg->Ctx)

void             DiagnoseConnectionAndTable(PGconn *DbConn, const char *TableName);
void             PrintPGresult(const PGresult *Result);
pg_col_metadata *GetTableMetadata(PGconn *DbConn, sdb_string TableName, int *ColCount,
                                  sdb_arena *A);
void             InsertSensorData(database_api *Pg);
void             PgInitThreadArenas(void);
sdb_errno        PgPrepareTablesAndStatements(database_api *Pg, cJSON *SchemaConf);

SDB_END_EXTERN_C

#endif
