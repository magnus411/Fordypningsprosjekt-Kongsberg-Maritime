/**
 * @file Postgres.h
 * @brief PostgreSQL interface for sensor data storage
 * @details Provides functionality for storing sensor data in PostgreSQL databases,
 * including data type mappings, timestamp conversions, and bulk data operations.
 */
#ifndef POSTGRES_H_
#define POSTGRES_H_

#include <libpq-fe.h>

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>
#include <src/Libs/cJSON/cJSON.h>

/** @brief Path to PostgreSQL configuration file */
#define POSTGRES_CONF_FS_PATH "./configs/postgres-conf"

/**
 * @name PostgreSQL Epoch Constants
 * @{
 */
#define POSTGRES_EPOCH_JDATE 2451545 /**< Julian date for 2000-01-01 (PostgreSQL epoch) */
#define UNIX_EPOCH_JDATE     2440588 /**< Julian date for 1970-01-01 (Unix epoch) */
#define USECS_PER_DAY        INT64_C(86400000000)
#define USECS_PER_SECOND     INT64_C(1000000)
#define USECS_PER_MSEC       INT64_C(1000)
#define NSECS_PER_USEC       INT64_C(1000)
/** @} */


/**
 * @brief PostgreSQL data types
 */
enum
{
    /**< Boolean */
    PG_BOOL = 16, /**< Boolean type */


    /**< Numbers */
    PG_INT2    = 21,   /**< smallint, int2 */
    PG_INT4    = 23,   /**< integer, int4 */
    PG_INT8    = 20,   /**< bigint, int8 */
    PG_FLOAT4  = 700,  /**< real, float4 */
    PG_FLOAT8  = 701,  /**< double precision, float8 */
    PG_NUMERIC = 1700, /**< numeric, decimal */

    /**< Character types */
    PG_CHAR    = 18,   /**< single char */
    PG_NAME    = 19,   /**< internal type for object names */
    PG_TEXT    = 25,   /**< variable-length text */
    PG_BPCHAR  = 1042, /**< blank-padded char(n) */
    PG_VARCHAR = 1043, /**< variable-length character string */

    /**< Date/Time */
    PG_DATE        = 1082, /**< date */
    PG_TIME        = 1083, /**< time without timezone */
    PG_TIMETZ      = 1266, /**< time with timezone */
    PG_TIMESTAMP   = 1114, /**< timestamp without timezone */
    PG_TIMESTAMPTZ = 1184, /**< timestamp with timezone */
    PG_INTERVAL    = 1186, /**< time interval */

    /**< Binary data */
    PG_BYTEA = 17, /**< variable-length binary string */

    /**< Network address types */
    PG_INET     = 869, /**< IPv4 or IPv6 address */
    PG_CIDR     = 650, /**< IPv4 or IPv6 network */
    PG_MACADDR  = 829, /**< MAC address */
    PG_MACADDR8 = 774, /**< MAC address (EUI-64 format) */

    /**< Bit strings */
    PG_BIT    = 1560, /**< fixed-length bit string */
    PG_VARBIT = 1562, /**< variable-length bit string */

    /**< Geometric types */
    PG_POINT   = 600, /**< geometric point (x,y) */
    PG_LINE    = 628, /**< geometric line */
    PG_LSEG    = 601, /**< geometric line segment */
    PG_BOX     = 603, /**< geometric box */
    PG_PATH    = 602, /**< geometric path */
    PG_POLYGON = 604, /**< geometric polygon */
    PG_CIRCLE  = 718, /**< geometric circle */

    /**< JSON types */
    PG_JSON  = 114,  /**< text JSON */
    PG_JSONB = 3802, /**< binary JSON */

    /**< Arrays */
    PG_BOOL_ARRAY        = 1000,
    PG_INT2_ARRAY        = 1005,
    PG_INT4_ARRAY        = 1007,
    PG_INT8_ARRAY        = 1016,
    PG_FLOAT4_ARRAY      = 1021,
    PG_FLOAT8_ARRAY      = 1022,
    PG_CHAR_ARRAY        = 1002,
    PG_TEXT_ARRAY        = 1009,
    PG_VARCHAR_ARRAY     = 1015,
    PG_BPCHAR_ARRAY      = 1014,
    PG_BYTEA_ARRAY       = 1001,
    PG_TIMESTAMP_ARRAY   = 1115,
    PG_TIMESTAMPTZ_ARRAY = 1185,
    PG_DATE_ARRAY        = 1182,
    PG_TIME_ARRAY        = 1183,
    PG_TIMETZ_ARRAY      = 1270,
    PG_INTERVAL_ARRAY    = 1187,
    PG_NUMERIC_ARRAY     = 1231,

    /**< UUID */
    PG_UUID = 2950, /**< universally unique identifier */

    /**< XML */
    PG_XML = 142, /**< XML data */

    /**< Money */
    PG_MONEY = 790, /**< currency amount */

    /**< Range types */
    PG_INT4RANGE = 3904,
    PG_INT8RANGE = 3926,
    PG_NUMRANGE  = 3906,
    PG_TSRANGE   = 3908,
    PG_TSTZRANGE = 3910,
    PG_DATERANGE = 3912,

    /**< Object identifier types */
    PG_OID      = 26,   /**< object identifier */
    PG_REGPROC  = 24,   /**< registered procedure  */
    PG_REGCLASS = 2205, /**< registered class */
    PG_REGTYPE  = 2206, /**< registered type */

    /**< Pseudo-types */
    PG_VOID    = 2278,
    PG_UNKNOWN = 705
};


typedef u32    pg_oid;
typedef i16    pg_int2;
typedef i32    pg_int4;
typedef i64    pg_int8;
typedef float  pg_float4;
typedef double pg_float8;
typedef i64    pg_timestamp; // microseconds from 2000-01-01

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

#define PQ_ALL_TABLE_METADATA_QUERY_FMT_LEN 609
#define PQ_ALL_TABLE_METADATA_QUERY_FMT                                                            \
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
} pq_col_all_metadata;

#define PQ_TABLE_METADATA_QUERY_FMT                                                                \
    "SELECT "                                                                                      \
    "a.attname AS column_name, "                                                                   \
    "t.oid AS type_oid, "                                                                          \
    "t.typlen AS type_length, "                                                                    \
    "a.atttypmod AS type_modifier, "                                                               \
    "EXISTS ( "                                                                                    \
    "SELECT 1 "                                                                                    \
    "FROM pg_constraint pc "                                                                       \
    "WHERE pc.conrelid = c.oid "                                                                   \
    "AND pc.contype = 'p' "                                                                        \
    "AND a.attnum = ANY(pc.conkey) "                                                               \
    ") AS is_primary_key, "                                                                        \
    "EXISTS ( "                                                                                    \
    "SELECT 1 "                                                                                    \
    "FROM pg_attrdef ad "                                                                          \
    "WHERE ad.adrelid = c.oid "                                                                    \
    "AND ad.adnum = a.attnum "                                                                     \
    "AND pg_get_expr(ad.adbin, ad.adrelid) LIKE 'nextval%%' "                                      \
    ") AS is_auto_increment "                                                                      \
    "FROM pg_catalog.pg_class c "                                                                  \
    "JOIN pg_catalog.pg_attribute a ON a.attrelid = c.oid "                                        \
    "JOIN pg_catalog.pg_type t ON a.atttypid = t.oid "                                             \
    "WHERE c.relname = '%s' "                                                                      \
    "AND a.attnum > 0 "                                                                            \
    "AND NOT a.attisdropped "                                                                      \
    "ORDER BY a.attnum"


/**
 * @struct pg_col_metadata
 * @brief PostgreSQL column metadata
 */
typedef struct
{
    pg_oid     TypeOid;
    i32        TypeLength;
    i32        Offset;
    i32        TypeModifier;
    sdb_string ColumnName;
    bool       IsPrimaryKey; // TODO(ingar): Might not need this
    bool       IsAutoIncrement;
} pg_col_metadata;

/**
 * @struct pg_table_info
 * @brief Table information for PostgreSQL operations
 */
typedef struct
{
    i16              ColCount;
    i16              ColCountNoAutoIncrements; // NOTE(ingar): Used for copy command
    size_t           RowSize;
    sdb_string       TableName;
    sdb_string       CopyCommand;
    pg_col_metadata *ColMetadata;

} pg_table_info;

/**
 * @struct postgres_ctx
 * @brief PostgreSQL context for database operations
 */
typedef struct
{
    PGconn         *DbConn; // TODO(ingar): One connection per table?
    pg_table_info **TablesInfo;

} postgres_ctx;

#ifndef PG_SCRATCH_COUNT
#define PG_SCRATCH_COUNT 2
#endif

void             DiagnoseConnectionAndTable(PGconn *DbConn, const char *TableName);
void             PrintPGresult(const PGresult *Result);
pg_col_metadata *GetTableMetadata(PGconn *DbConn, sdb_string TableName, i16 *ColCount,
                                  i16 *ColCountNoAutoIncrements, size_t *RowSize, sdb_arena *A);

/**
 * @brief Initializes thread arenas for PostgreSQL operations
 */
void PgInitThreadArenas(void);

/**
 * @brief Prepares PostgreSQL context from configuration
 *
 * @param PgArena Memory arena for allocations
 * @param Pipe Sensor data pipe
 * @return Initialized context or NULL on failure
 */
postgres_ctx *PgPrepareCtx(sdb_arena *PgArena, sensor_data_pipe *Pipe);

pg_timestamp UnixToPgTimestamp(time_t UnixTime);
pg_timestamp TimevalToPgTimestamp(struct timeval Tv);
pg_timestamp TimespecToPgTimestamp(struct timespec Ts);
sdb_errno    PgInsertData(PGconn *Conn, pg_table_info *Ti, const char *Data, u64 ItemCount);

SDB_END_EXTERN_C

#endif
