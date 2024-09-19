#ifndef POSTGRES_TYPES_H_
#define POSTGRES_TYPES_H_
// PostgreSQL Type _OIDs and Names

// BIGSERIAL (for 'id' column)
// Note: BIGSERIAL is actually a BIGINT with a default sequence
#define BIGINT_OID 20
static const char *BIGINT_NAME = "bigint";

// TIMESTAMPTZ (for 'timestamp' column)
#define TIMESTAMPTZ_OID 1184
static const char *TIMESTAMPTZ_NAME = "timestamp with time zone";

// INTEGER (for 'rpm' column)
#define INT4_OID 23
static const char *INT4_NAME = "integer";

// DOUBLE PRECISION (for most measurement columns)
#define FLOAT8_OID 701
static const char *FLOAT8_NAME = "double precision";

// VARCHAR (for 'sensor_id' column)
#define VARCHAR_OID 1043
static const char *VARCHAR_NAME = "character varying";

// Additional types that might be useful:

// TEXT
#define TEXT_OID 25
static const char *TEXT_NAME = "text";

// BOOLEAN
#define BOOL_OID 16
static const char *BOOL_NAME = "boolean";

// DATE
#define DATE_OID 1082
static const char *DATE_NAME = "date";

// TIME
#define TIME_OID 1083
static const char *TIME_NAME = "time without time zone";

// NUMERIC
#define NUMERIC_OID 1700
static const char *NUMERIC_NAME = "numeric";

#endif
