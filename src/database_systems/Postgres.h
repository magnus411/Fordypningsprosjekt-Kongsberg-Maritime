#ifndef POSTGRES_H_
#define POSTGRES_H_

#include <libpq-fe.h>

#include <SdbExtern.h>
#include <common/CircularBuffer.h>
#include <modules/DatabaseModule.h>

#define POSTGRES_CONF_FS_PATH "./postgres-conf"

// PostgreSQL Type OIDs and Names
enum pq_oid
{
    BOOL        = 16,
    INT8        = 20,
    INT4        = 23,
    TEXT        = 25,
    FLOAT8      = 701,
    VARCHAR     = 1043,
    DATE        = 1082,
    TIME        = 1083,
    TIMESTAMPTZ = 1184,
    NUMERIC     = 1700,
};

#define PQ_BOOL_NAME        "boolean";
#define PQ_INT8_NAME        "bigint";
#define PQ_INT4_NAME        "integer";
#define PQ_TEXT_NAME        "text";
#define PQ_FLOAT8_NAME      "double precision";
#define PQ_VARCHAR_NAME     "character varying";
#define PQ_DATE_NAME        "date";
#define PQ_TIMESTAMPTZ_NAME "timestamp with time zone";
#define PQ_TIME_NAME        "time without time zone";
#define PQ_NUMERIC_NAME     "numeric";

#define PQ_TABLE_METADATA_QUERY_FMT_LEN 609
#define PQ_TABLE_METADATA_QUERY_FMT                                                                \
    "SELECT c.oid AS table_oid, c.relname AS table_name, a.attnum AS column_number, a.attname AS " \
    "column_name, t.oid AS type_oid, t.typname AS type_name, t.typlen AS type_length, t.typbyval " \
    "AS type_by_value, t.typalign AS type_alignment, t.typstorage AS type_storage, a.atttypmod "   \
    "AS type_modifier, a.attnotnull AS not_null, pg_catalog.format_type(a.atttypid, a.atttypmod) " \
    "AS full_data_type FROM pg_catalog.pg_class c JOIN pg_catalog.pg_attribute a ON a.attrelid = " \
    "c.oid JOIN pg_catalog.pg_type t ON a.atttypid = t.oid WHERE c.relname = "                     \
    "'%s' AND a.attnum > 0 AND NOT a.attisdropped ORDER BY a.attnum"

// TODO(ingar): Create a minimal structure that only contains the necessary data to perform inserts
// from a byte stream
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

void PrintPGresult(const PGresult *Result);

void DiagnoseConnectionAndTable(PGconn *DbConn, const char *TableName);

pq_col_metadata *GetTableMetadata(PGconn *DbConn, const char *TableName, u64 TableNameLen,
                                  int *ColCount);

char *PqTableMetaDataQuery(const char *TableName, u64 TableNameLen);

void PrintColumnMetadata(const pq_col_metadata *Metadata);

void InsertSensorData(PGconn *DbConn, const char *TableName, u64 TableNameLen, const u8 *SensorData,
                      size_t DataSize);

sdb_errno PgInit(database_api *Pg);
sdb_errno PgRun(database_api *Pg);

typedef struct
{
    PGconn *DbConn;
    char  **TableNames;
    u64    *TableNameLengths;

    circular_buffer *Cb;
    void            *PgInsertBuf;
    size_t           PgInsertBufSize;
} pg_ctx;

#endif
