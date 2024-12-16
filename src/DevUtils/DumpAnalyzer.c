/**
 * @file DumpAnalyzer.c
 * @brief Analyzes a memory dump file for debugging purposes
 *
 * @note This file is not part of the final system, but is used for debugging
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if 0
#define MAX_LINE 1024

void
analyze_dump(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if(!file) {
        fprintf(stderr, "Failed to open dump file: %s\n", filename);
        return;
    }

    char line[MAX_LINE];
    int  section = 0; // 0=header, 1=stack trace, 2=memory regions

    printf("\n=== Dump File Analysis ===\n\n");

    while(fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;

        // Detect sections
        if(strstr(line, "Stack Trace:")) {
            section = 1;
            printf("\n=== Stack Trace ===\n");
            continue;
        } else if(strstr(line, "Memory Regions:")) {
            section = 2;
            printf("\n=== Memory Regions ===\n");
            continue;
        }

        // Process based on section
        switch(section) {
            case 0: // Header
                if(strlen(line) > 0) {
                    printf("%s\n", line);
                }
                break;

            case 1: // Stack Trace
                if(strlen(line) > 0) {
                    // Try to make stack trace more readable
                    char *addr = strstr(line, "0x");
                    if(addr) {
                        printf("  %s\n", line);
                    }
                }
                break;

            case 2: // Memory Regions
                if(strstr(line, "Region:")) {
                    printf("\n%s\n", line);
                } else if(strstr(line, "Failed to dump region")) {
                    printf("  %s\n", line);
                }
                break;
        }
    }

    fclose(file);
}

int
main(int argc, char **argv)
{
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <dump_file>\n", argv[0]);
        return 1;
    }

    analyze_dump(argv[1]);
    return 0;
}
#endif
