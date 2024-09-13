#define ISA_LOG_LEVEL    4
#define ISA_LOG_BUF_SIZE 256
#include "isa.h"

ISA_LOG_REGISTER(main);

int
main(void)
{
    IsaLogInfo("Hello, Kongsberg!");
    return 0;
}
