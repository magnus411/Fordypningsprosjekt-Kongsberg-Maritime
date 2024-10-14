#ifndef DATABASE_SYSTEMS_H
#define DATABASE_SYSTEMS_H

typedef enum
{
    Dbs_Postgres = 1,

} Db_System_Id;

static inline const char *
DbsIdToName(Db_System_Id Id)
{
    switch(Id) {
        case Dbs_Postgres:
            {
                return "Postgres";
            }
            break;
        default:
            {
                return "Dbs does not exist";
            }
            break;
    }
}
#endif
