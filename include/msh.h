/*
 * microsh — Minimal debug shell for embedded systems.
 *
 * C99 · Zero dependencies · Zero allocations · UART-friendly · Portable
 *
 * SPDX-License-Identifier: MIT
 * https://github.com/Vanderhell/microsh
 */

#ifndef MSH_H
#define MSH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Configuration ─────────────────────────────────────────────────────── */

/** Maximum registered commands. */
#ifndef MSH_MAX_COMMANDS
#define MSH_MAX_COMMANDS 16
#endif

/** Maximum input line length (including NUL). */
#ifndef MSH_LINE_SIZE
#define MSH_LINE_SIZE 128
#endif

/** Maximum arguments per command (including command name). */
#ifndef MSH_MAX_ARGS
#define MSH_MAX_ARGS 8
#endif

/** Enable command history (ring buffer of previous lines). */
#ifndef MSH_ENABLE_HISTORY
#define MSH_ENABLE_HISTORY 1
#endif

/** Number of history entries. */
#ifndef MSH_HISTORY_DEPTH
#define MSH_HISTORY_DEPTH 4
#endif

/** Enable tab-completion of command names. */
#ifndef MSH_ENABLE_COMPLETE
#define MSH_ENABLE_COMPLETE 1
#endif

/* ── Error codes ───────────────────────────────────────────────────────── */

typedef enum {
    MSH_OK            =  0,   /**< Success.                              */
    MSH_ERR_NULL      = -1,   /**< NULL pointer argument.                */
    MSH_ERR_FULL      = -2,   /**< Command table is full.                */
    MSH_ERR_NOT_FOUND = -3,   /**< Command not found.                    */
    MSH_ERR_ARGS      = -4,   /**< Wrong number of arguments.            */
    MSH_ERR_INVALID   = -5,   /**< Invalid input.                        */
} msh_err_t;

const char *msh_err_str(msh_err_t err);

/* ── Platform callback ─────────────────────────────────────────────────── */

/**
 * Output callback — write a string to the terminal.
 *
 * @param str  NUL-terminated string to output.
 * @param ctx  User context.
 */
typedef void (*msh_print_fn)(const char *str, void *ctx);

/* ── Command callback ──────────────────────────────────────────────────── */

/**
 * Command handler.
 *
 * @param argc  Argument count (>= 1, argv[0] is the command name).
 * @param argv  Argument array (NUL-terminated strings).
 * @param ctx   User context from the shell instance.
 * @return 0 on success, negative on error. The return value is displayed
 *         to the user if non-zero.
 */
typedef int (*msh_cmd_fn)(int argc, const char **argv, void *ctx);

/* ── Command descriptor ────────────────────────────────────────────────── */

typedef struct {
    const char  *name;     /**< Command name (e.g., "conf").             */
    const char  *help;     /**< One-line help text.                      */
    msh_cmd_fn   handler;  /**< Command handler function.                */
} msh_cmd_t;

/* ── Shell instance ────────────────────────────────────────────────────── */

typedef struct {
    /* Command table */
    msh_cmd_t    commands[MSH_MAX_COMMANDS];
    uint8_t      num_commands;

    /* Input buffer */
    char         line[MSH_LINE_SIZE];
    uint8_t      cursor;           /**< Current position in line.         */
    uint8_t      length;           /**< Current line length.              */

    /* History */
#if MSH_ENABLE_HISTORY
    char         history[MSH_HISTORY_DEPTH][MSH_LINE_SIZE];
    uint8_t      hist_count;       /**< Number of stored entries.         */
    uint8_t      hist_write;       /**< Next write position (ring).       */
    int8_t       hist_browse;      /**< Current browse offset (-1=none).  */
#endif

    /* Escape sequence state machine */
    uint8_t      esc_state;        /**< 0=normal, 1=got ESC, 2=got '['.   */

    /* Output */
    msh_print_fn print;
    void        *ctx;              /**< User context passed to callbacks.  */
    const char  *prompt;           /**< Prompt string (default: "> ").     */
    bool         echo;             /**< Echo typed characters.             */
} msh_t;

/* ── Init / config ─────────────────────────────────────────────────────── */

/**
 * Initialise shell instance.
 *
 * @param sh     Shell instance (caller-allocated).
 * @param print  Output callback (required).
 * @param ctx    User context passed to all callbacks.
 * @return MSH_OK on success.
 */
msh_err_t msh_init(msh_t *sh, msh_print_fn print, void *ctx);

/**
 * Set prompt string (default: "> "). The string must stay valid
 * (static or const). Pass NULL for no prompt.
 */
void msh_set_prompt(msh_t *sh, const char *prompt);

/** Enable/disable echo of typed characters. Default: true. */
void msh_set_echo(msh_t *sh, bool echo);

/* ── Command registration ──────────────────────────────────────────────── */

/**
 * Register a command.
 *
 * @param sh       Shell instance.
 * @param name     Command name (e.g., "reboot"). Must be static/const.
 * @param help     One-line help text. Must be static/const. May be NULL.
 * @param handler  Command handler function.
 * @return MSH_OK or MSH_ERR_FULL.
 */
msh_err_t msh_register(msh_t *sh, const char *name, const char *help,
                        msh_cmd_fn handler);

/* ── Input processing ──────────────────────────────────────────────────── */

/**
 * Feed one byte from UART/terminal into the shell.
 *
 * Call this from your UART RX interrupt or polling loop. The shell
 * handles line editing (backspace, arrows), history navigation, tab
 * completion, and executes commands on Enter.
 *
 * @param sh  Shell instance.
 * @param c   Received byte.
 */
void msh_feed(msh_t *sh, char c);

/**
 * Feed a complete line (without newline) and execute.
 * Useful for testing or scripted input.
 *
 * @param sh    Shell instance.
 * @param line  NUL-terminated command line.
 * @return Command return value, or MSH_ERR_NOT_FOUND.
 */
int msh_exec(msh_t *sh, const char *line);

/**
 * Print the prompt. Call once after init and after each command.
 * (Also called internally by msh_feed after command execution.)
 */
void msh_prompt(msh_t *sh);

/* ── Query ─────────────────────────────────────────────────────────────── */

/** Get number of registered commands. */
uint8_t msh_command_count(const msh_t *sh);

/** Get command descriptor by index (for enumeration). */
const msh_cmd_t *msh_command_at(const msh_t *sh, uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* MSH_H */
