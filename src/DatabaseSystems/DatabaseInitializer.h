/**
 * @file DatabaseInitializer.h
 * @brief Database configuration initialization interface
 * @details Provides functionality for loading and parsing database configuration
 * files in JSON format. Supports both arena-based and traditional memory management.
 */


#ifndef DATABASE_INITIALIZER_H
#define DATABASE_INITIALIZER_H

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Libs/cJSON/cJSON.h>


/**
 * @brief Loads and parses a database configuration file
 *
 * Reads a JSON configuration file into memory and parses it into a cJSON structure.
 * Supports both arena-based and traditional memory allocation depending on whether
 * an arena is provided.
 *
 * @param Filename Path to the configuration file
 * @param A Memory arena for allocations (can be NULL for heap allocation)
 * @return Parsed cJSON structure, NULL on failure
 *
 * @note If arena is NULL, caller is responsible for freeing the returned cJSON structure
 * @note If arena is provided, memory will be managed through the arena
 */
cJSON *DbInitGetConfFromFile(const char *Filename, sdb_arena *A);

SDB_END_EXTERN_C

#endif // DATABASE_INITIALIZER_H
