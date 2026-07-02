#include "msh.h"

static void sink(const char *str, void *ctx)
{
    (void)str;
    (void)ctx;
}

static int hello_cmd(int argc, const char *const *argv, void *ctx)
{
    (void)argc;
    (void)argv;
    (void)ctx;
    return 0;
}

int main(void)
{
    msh_t shell;
    (void)&MSH_ABI_GUARD_SYMBOL;
    if (msh_init(&shell, sink, NULL) != MSH_OK) {
        return 1;
    }
    if (msh_register(&shell, "hello", "demo", hello_cmd) != MSH_OK) {
        return 1;
    }
    return msh_exec(&shell, "hello");
}
