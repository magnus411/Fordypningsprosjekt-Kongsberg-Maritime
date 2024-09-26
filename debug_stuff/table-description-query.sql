-- Replace 'your_table_name' with the name of the table you want to describe
WITH table_info AS (
    SELECT 
        c.table_schema,
        c.table_name,
        c.column_name,
        c.data_type,
        c.character_maximum_length,
        c.numeric_precision,
        c.numeric_scale,
        c.is_nullable,
        c.column_default,
        pgd.description AS column_description,
        (SELECT pg_catalog.obj_description(pg_class.oid, 'pg_class')
         FROM pg_catalog.pg_class
         WHERE pg_class.relname = c.table_name) AS table_description
    FROM information_schema.columns c
    LEFT JOIN pg_catalog.pg_description pgd ON 
        pgd.objoid = (SELECT oid FROM pg_catalog.pg_class WHERE relname = c.table_name) AND
        pgd.objsubid = c.ordinal_position
    WHERE c.table_name = 'your_table_name'
)
SELECT 
    table_schema || '.' || table_name AS "Table",
    table_description AS "Table Description",
    column_name AS "Column",
    data_type || 
        CASE 
            WHEN character_maximum_length IS NOT NULL THEN '(' || character_maximum_length || ')'
            WHEN numeric_precision IS NOT NULL AND numeric_scale IS NOT NULL THEN '(' || numeric_precision || ',' || numeric_scale || ')'
            WHEN numeric_precision IS NOT NULL THEN '(' || numeric_precision || ')'
            ELSE ''
        END AS "Data Type",
    is_nullable AS "Nullable",
    column_default AS "Default Value",
    column_description AS "Column Description"
FROM table_info
ORDER BY 
    table_schema, 
    table_name, 
    ordinal_position;
