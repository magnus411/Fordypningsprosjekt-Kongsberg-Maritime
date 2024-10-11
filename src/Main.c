#include <stdlib.h>
#include <libpq-fe.h>

#define SDB_LOG_LEVEL 4
#include <Sdb.h>

SDB_LOG_REGISTER(Main);

#include <modules/DatabaseModule.h>

#define DATABASE_SYSTEM DATABASE_SYSTEM_POSTGRES
#include <database_systems/DatabaseSystems.h>

int
main(int ArgCount, char **ArgV)
{
    if(ArgCount <= 1) {

        SdbLogError("Please provide the path to the configuration.sdb file!\nRelative paths start "
                    "from where YOU launch the executable from.");
        return 1;
    }

    sdb_arena SdbArena;
    u64       SdbArenaSize = SdbMebiByte(32);
    u8       *SdbArenaMem  = malloc(SdbArenaSize);

    if(NULL == SdbArenaMem) {
        SdbLogError("Failed to allocate memory for arena!");
        exit(EXIT_FAILURE);
    } else {
        SdbArenaInit(&SdbArena, SdbArenaMem, SdbArenaSize);
    }

    exit(EXIT_SUCCESS);
}
