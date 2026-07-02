#include "msh.h"

const void *microsh_consumer_abi_guard(void)
{
    return &MSH_ABI_GUARD_SYMBOL;
}
