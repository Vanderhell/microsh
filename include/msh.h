/*
 * microsh - Minimal debug shell for embedded systems.
 *
 * C99, zero third-party runtime dependencies, zero dynamic allocation.
 *
 * SPDX-License-Identifier: MIT
 * https://github.com/Vanderhell/microsh
 */

#ifndef MSH_H
#define MSH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Configuration defaults */

#ifndef MSH_MAX_COMMANDS
#define MSH_MAX_COMMANDS 16
#endif

#ifndef MSH_LINE_SIZE
#define MSH_LINE_SIZE 128
#endif

#ifndef MSH_MAX_ARGS
#define MSH_MAX_ARGS 8
#endif

#ifndef MSH_ENABLE_HISTORY
#define MSH_ENABLE_HISTORY 1
#endif

#ifndef MSH_HISTORY_DEPTH
#define MSH_HISTORY_DEPTH 4
#endif

#ifndef MSH_ENABLE_COMPLETE
#define MSH_ENABLE_COMPLETE 1
#endif

/* Configuration validation */

#define MSH_HELP_NAME "help"
#define MSH_HELP_NAME_LEN 4u

#if (MSH_MAX_COMMANDS) < 1
#error "microsh: MSH_MAX_COMMANDS must be at least 1 and includes the built-in help command"
#endif

#if (MSH_MAX_COMMANDS) > 255
#error "microsh: MSH_MAX_COMMANDS must be <= 255"
#endif

#if (MSH_LINE_SIZE) < (MSH_HELP_NAME_LEN + 1u)
#error "microsh: MSH_LINE_SIZE must fit the built-in help command plus NUL"
#endif

#if (MSH_LINE_SIZE) > 256
#error "microsh: MSH_LINE_SIZE must be <= 256 because msh_t cursor and length are byte-sized"
#endif

#if (MSH_MAX_ARGS) < 1
#error "microsh: MSH_MAX_ARGS must be at least 1"
#endif

#if (MSH_MAX_ARGS) > 255
#error "microsh: MSH_MAX_ARGS must be <= 255"
#endif

#if ((MSH_ENABLE_HISTORY) != 0) && ((MSH_ENABLE_HISTORY) != 1)
#error "microsh: MSH_ENABLE_HISTORY must be exactly 0 or 1"
#endif

#if ((MSH_ENABLE_COMPLETE) != 0) && ((MSH_ENABLE_COMPLETE) != 1)
#error "microsh: MSH_ENABLE_COMPLETE must be exactly 0 or 1"
#endif

#if MSH_ENABLE_HISTORY
#if (MSH_HISTORY_DEPTH) < 1
#error "microsh: MSH_HISTORY_DEPTH must be at least 1 when history is enabled"
#endif
#if (MSH_HISTORY_DEPTH) > 255
#error "microsh: MSH_HISTORY_DEPTH must be <= 255"
#endif
#endif

/* ABI-affecting configuration guard */

#define MSH_DETAIL_CAT_IMPL(a, b) a##b
#define MSH_DETAIL_CAT(a, b) MSH_DETAIL_CAT_IMPL(a, b)
#define MSH_DETAIL_CAT3(a, b, c) MSH_DETAIL_CAT(MSH_DETAIL_CAT(a, b), c)

#define MSH_ABI_GUARD_STEP1 MSH_DETAIL_CAT3(msh_abi_guard_, MSH_MAX_COMMANDS, _)
#define MSH_ABI_GUARD_STEP2 MSH_DETAIL_CAT3(MSH_ABI_GUARD_STEP1, MSH_LINE_SIZE, _)
#define MSH_ABI_GUARD_STEP3 MSH_DETAIL_CAT3(MSH_ABI_GUARD_STEP2, MSH_MAX_ARGS, _)
#define MSH_ABI_GUARD_STEP4 MSH_DETAIL_CAT3(MSH_ABI_GUARD_STEP3, MSH_ENABLE_HISTORY, _)
#define MSH_ABI_GUARD_STEP5 MSH_DETAIL_CAT3(MSH_ABI_GUARD_STEP4, MSH_HISTORY_DEPTH, _)
#define MSH_ABI_GUARD_SYMBOL MSH_DETAIL_CAT3(MSH_ABI_GUARD_STEP5, MSH_ENABLE_COMPLETE, _end)

extern const unsigned char MSH_ABI_GUARD_SYMBOL;

typedef enum {
    MSH_OK = 0,
    MSH_ERR_NULL = -1,
    MSH_ERR_FULL = -2,
    MSH_ERR_NOT_FOUND = -3,
    MSH_ERR_ARGS = -4,
    MSH_ERR_INVALID = -5,
    MSH_ERR_INPUT_TOO_LONG = -6
} msh_err_t;

const char *msh_err_str(msh_err_t err);

/*
 * Output callback contract:
 * - called synchronously by microsh
 * - receives a borrowed NUL-terminated string
 * - the pointer may reference stack-backed temporary storage
 * - the callback must consume or copy the bytes before returning
 * - output failures cannot be propagated through the current API
 */
typedef void (*msh_print_fn)(const char *str, void *ctx);

/*
 * Command handler contract:
 * - argc >= 1 and argv[0] is the command name
 * - argv and the pointed-to strings are borrowed temporary storage
 * - argv contents are valid only for the duration of the call
 * - copy anything needed after the handler returns
 */
typedef int (*msh_cmd_fn)(int argc, const char *const *argv, void *ctx);

typedef struct {
    const char *name;
    const char *help;
    msh_cmd_fn handler;
} msh_cmd_t;

typedef struct {
    msh_cmd_t commands[MSH_MAX_COMMANDS];
    uint8_t num_commands;

    char line[MSH_LINE_SIZE];
    uint8_t cursor;
    uint8_t length;
    bool line_overflow;
    bool saw_cr;

#if MSH_ENABLE_HISTORY
    char history[MSH_HISTORY_DEPTH][MSH_LINE_SIZE];
    uint8_t hist_count;
    uint8_t hist_write;
    uint8_t hist_browse;
#endif

    uint8_t esc_state;
    uint8_t csi_param;

    msh_print_fn print;
    void *ctx;
    const char *prompt;
    bool echo;
} msh_t;

/*
 * ABI-affecting configuration macros:
 * - MSH_MAX_COMMANDS
 * - MSH_LINE_SIZE
 * - MSH_MAX_ARGS
 * - MSH_ENABLE_HISTORY
 * - MSH_HISTORY_DEPTH
 * - MSH_ENABLE_COMPLETE
 *
 * Every translation unit that touches msh_t or links against a separately
 * built microsh library must use identical values for those macros.
 * Conflicting translation units are unsupported.
 */

msh_err_t msh_init(msh_t *sh, msh_print_fn print, void *ctx);
void msh_set_prompt(msh_t *sh, const char *prompt);
void msh_set_echo(msh_t *sh, bool echo);

/*
 * Registration rules:
 * - MSH_MAX_COMMANDS includes the built-in help command
 * - user command capacity is MSH_MAX_COMMANDS - 1
 * - name and help are borrowed and must stay valid while registered
 * - name must be non-empty, unique, printable ASCII, and contain no
 *   whitespace, quotes, or control characters
 */
msh_err_t msh_register(msh_t *sh, const char *name, const char *help,
                       msh_cmd_fn handler);

/*
 * Feed one received byte.
 *
 * Call msh_feed() from a single non-ISR execution context. If bytes arrive in
 * an ISR, push them into a caller-owned queue or ring buffer and drain that
 * queue from the owning task or main loop.
 */
void msh_feed(msh_t *sh, char c);

/*
 * Execute a complete command line without CR/LF.
 *
 * Returns the command handler result or a microsh error code. Overlong input
 * is rejected with MSH_ERR_INPUT_TOO_LONG instead of being truncated.
 */
int msh_exec(msh_t *sh, const char *line);

void msh_prompt(msh_t *sh);
uint8_t msh_command_count(const msh_t *sh);
const msh_cmd_t *msh_command_at(const msh_t *sh, uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* MSH_H */
