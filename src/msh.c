/*
 * microsh — Implementation.
 *
 * SPDX-License-Identifier: MIT
 * https://github.com/Vanderhell/microsh
 */

#include "msh.h"
#include <string.h>
#include <stdio.h>

/* ── Internal constants ────────────────────────────────────────────────── */

#define KEY_ENTER     '\r'
#define KEY_NEWLINE   '\n'
#define KEY_BACKSPACE '\b'
#define KEY_DEL       127
#define KEY_ESC       27
#define KEY_TAB       '\t'

/* ── Error strings ─────────────────────────────────────────────────────── */

const char *msh_err_str(msh_err_t err)
{
    switch (err) {
    case MSH_OK:            return "ok";
    case MSH_ERR_NULL:      return "null pointer";
    case MSH_ERR_FULL:      return "command table full";
    case MSH_ERR_NOT_FOUND: return "command not found";
    case MSH_ERR_ARGS:      return "wrong arguments";
    case MSH_ERR_INVALID:   return "invalid input";
    default:                return "unknown error";
    }
}

/* ── Output helpers ────────────────────────────────────────────────────── */

static void sh_print(msh_t *sh, const char *str)
{
    if (sh->print != NULL && str != NULL) {
        sh->print(str, sh->ctx);
    }
}

static void sh_putc(msh_t *sh, char c)
{
    char buf[2] = { c, '\0' };
    sh_print(sh, buf);
}

/* ── Line buffer helpers ───────────────────────────────────────────────── */

static void line_clear(msh_t *sh)
{
    sh->cursor = 0;
    sh->length = 0;
    sh->line[0] = '\0';
}

static void line_insert_char(msh_t *sh, char c)
{
    if (sh->length >= MSH_LINE_SIZE - 1) return;

    /* Shift right from cursor */
    for (uint8_t i = sh->length; i > sh->cursor; i--) {
        sh->line[i] = sh->line[i - 1];
    }
    sh->line[sh->cursor] = c;
    sh->length++;
    sh->cursor++;
    sh->line[sh->length] = '\0';
}

static void line_delete_back(msh_t *sh)
{
    if (sh->cursor == 0) return;

    /* Shift left over cursor-1 */
    for (uint8_t i = sh->cursor - 1; i < sh->length - 1; i++) {
        sh->line[i] = sh->line[i + 1];
    }
    sh->cursor--;
    sh->length--;
    sh->line[sh->length] = '\0';
}

/* ── History ───────────────────────────────────────────────────────────── */

#if MSH_ENABLE_HISTORY
static void history_push(msh_t *sh, const char *line)
{
    if (line[0] == '\0') return;

    /* Don't push duplicates of the last entry */
    if (sh->hist_count > 0) {
        uint8_t last = (sh->hist_write + MSH_HISTORY_DEPTH - 1) % MSH_HISTORY_DEPTH;
        if (strcmp(sh->history[last], line) == 0) return;
    }

    strncpy(sh->history[sh->hist_write], line, MSH_LINE_SIZE - 1);
    sh->history[sh->hist_write][MSH_LINE_SIZE - 1] = '\0';
    sh->hist_write = (sh->hist_write + 1) % MSH_HISTORY_DEPTH;
    if (sh->hist_count < MSH_HISTORY_DEPTH) {
        sh->hist_count++;
    }
}

static const char *history_get(msh_t *sh, uint8_t offset)
{
    if (offset == 0 || offset > sh->hist_count) return NULL;
    /* offset=1 → most recent, offset=hist_count → oldest */
    int idx = (int)sh->hist_write - (int)offset;
    if (idx < 0) idx += MSH_HISTORY_DEPTH;
    return sh->history[idx];
}

static void history_browse(msh_t *sh, int direction)
{
    int new_offset = sh->hist_browse + direction;
    if (new_offset < 0) new_offset = 0;
    if (new_offset > (int)sh->hist_count) new_offset = (int)sh->hist_count;

    sh->hist_browse = (int8_t)new_offset;

    /* Erase current line on terminal */
    sh_print(sh, "\r");
    if (sh->prompt) sh_print(sh, sh->prompt);
    /* Overwrite with spaces */
    for (uint8_t i = 0; i < sh->length; i++) sh_putc(sh, ' ');
    sh_print(sh, "\r");
    if (sh->prompt) sh_print(sh, sh->prompt);

    if (new_offset == 0) {
        line_clear(sh);
    } else {
        const char *entry = history_get(sh, (uint8_t)new_offset);
        if (entry != NULL) {
            strncpy(sh->line, entry, MSH_LINE_SIZE - 1);
            sh->line[MSH_LINE_SIZE - 1] = '\0';
            sh->length = (uint8_t)strlen(sh->line);
            sh->cursor = sh->length;
            sh_print(sh, sh->line);
        }
    }
}
#endif

/* ── Tab completion ────────────────────────────────────────────────────── */

#if MSH_ENABLE_COMPLETE
static void tab_complete(msh_t *sh)
{
    if (sh->length == 0) return;

    /* Find matching commands */
    const msh_cmd_t *match = NULL;
    int match_count = 0;

    for (uint8_t i = 0; i < sh->num_commands; i++) {
        if (strncmp(sh->commands[i].name, sh->line, sh->length) == 0) {
            match = &sh->commands[i];
            match_count++;
        }
    }

    if (match_count == 1 && match != NULL) {
        /* Single match — complete it */
        const char *name = match->name;
        uint8_t name_len = (uint8_t)strlen(name);

        /* Replace line with completed command + space */
        strncpy(sh->line, name, MSH_LINE_SIZE - 2);
        sh->line[name_len] = ' ';
        sh->line[name_len + 1] = '\0';
        sh->length = name_len + 1;
        sh->cursor = sh->length;

        /* Redraw */
        sh_print(sh, "\r");
        if (sh->prompt) sh_print(sh, sh->prompt);
        sh_print(sh, sh->line);
    } else if (match_count > 1) {
        /* Multiple matches — show them */
        sh_print(sh, "\r\n");
        for (uint8_t i = 0; i < sh->num_commands; i++) {
            if (strncmp(sh->commands[i].name, sh->line, sh->length) == 0) {
                sh_print(sh, "  ");
                sh_print(sh, sh->commands[i].name);
                sh_print(sh, "\r\n");
            }
        }
        /* Redraw prompt + current input */
        if (sh->prompt) sh_print(sh, sh->prompt);
        sh_print(sh, sh->line);
    }
}
#endif

/* ── Argument parsing ──────────────────────────────────────────────────── */

/**
 * Parse a line into argc/argv. Modifies the line in-place (inserts NULs).
 * Returns argc (0 if empty line).
 */
static int parse_args(char *line, const char **argv, int max_args)
{
    int argc = 0;
    char *p = line;

    while (*p != '\0' && argc < max_args) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        /* Check for quoted string */
        if (*p == '"') {
            p++;  /* skip opening quote */
            argv[argc++] = p;
            while (*p != '\0' && *p != '"') p++;
            if (*p == '"') *p++ = '\0';
        } else {
            argv[argc++] = p;
            while (*p != '\0' && *p != ' ' && *p != '\t') p++;
            if (*p != '\0') *p++ = '\0';
        }
    }

    return argc;
}

/* ── Built-in help command ─────────────────────────────────────────────── */

static int builtin_help(int argc, const char **argv, void *ctx)
{
    msh_t *sh = (msh_t *)ctx;
    (void)argc; (void)argv;

    sh_print(sh, "Available commands:\r\n");
    for (uint8_t i = 0; i < sh->num_commands; i++) {
        char buf[MSH_LINE_SIZE];
        int n = snprintf(buf, sizeof(buf), "  %-12s %s\r\n",
                         sh->commands[i].name,
                         sh->commands[i].help ? sh->commands[i].help : "");
        if (n > 0) sh_print(sh, buf);
    }

    return 0;
}

/* ── Execute a parsed line ─────────────────────────────────────────────── */

static int execute(msh_t *sh, char *line)
{
    const char *argv[MSH_MAX_ARGS];
    int argc = parse_args(line, argv, MSH_MAX_ARGS);

    if (argc == 0) return MSH_OK;  /* empty line */

    /* Find command */
    for (uint8_t i = 0; i < sh->num_commands; i++) {
        if (strcmp(sh->commands[i].name, argv[0]) == 0) {
            /* The help command gets the shell as ctx so it can enumerate */
            if (sh->commands[i].handler == builtin_help) {
                return sh->commands[i].handler(argc, argv, sh);
            }
            return sh->commands[i].handler(argc, argv, sh->ctx);
        }
    }

    sh_print(sh, "Unknown command: ");
    sh_print(sh, argv[0]);
    sh_print(sh, "\r\nType 'help' for available commands.\r\n");

    return MSH_ERR_NOT_FOUND;
}

/* ── Public API ────────────────────────────────────────────────────────── */

msh_err_t msh_init(msh_t *sh, msh_print_fn print, void *ctx)
{
    if (sh == NULL || print == NULL) return MSH_ERR_NULL;

    memset(sh, 0, sizeof(*sh));
    sh->print  = print;
    sh->ctx    = ctx;
    sh->prompt = "> ";
    sh->echo   = true;

#if MSH_ENABLE_HISTORY
    sh->hist_browse = 0;
#endif

    /* Register built-in help */
    msh_register(sh, "help", "Show available commands", builtin_help);

    return MSH_OK;
}

void msh_set_prompt(msh_t *sh, const char *prompt)
{
    if (sh != NULL) sh->prompt = prompt;
}

void msh_set_echo(msh_t *sh, bool echo)
{
    if (sh != NULL) sh->echo = echo;
}

msh_err_t msh_register(msh_t *sh, const char *name, const char *help,
                        msh_cmd_fn handler)
{
    if (sh == NULL || name == NULL || handler == NULL) return MSH_ERR_NULL;
    if (sh->num_commands >= MSH_MAX_COMMANDS) return MSH_ERR_FULL;

    msh_cmd_t *cmd = &sh->commands[sh->num_commands++];
    cmd->name    = name;
    cmd->help    = help;
    cmd->handler = handler;

    return MSH_OK;
}

void msh_prompt(msh_t *sh)
{
    if (sh == NULL) return;
    if (sh->prompt != NULL) {
        sh_print(sh, sh->prompt);
    }
}

void msh_feed(msh_t *sh, char c)
{
    if (sh == NULL) return;

    /* ── Escape sequence handling ──────────────────────────────────────── */
    if (sh->esc_state == 1) {
        if (c == '[') {
            sh->esc_state = 2;
            return;
        }
        sh->esc_state = 0;
        return;
    }

    if (sh->esc_state == 2) {
        sh->esc_state = 0;
        switch (c) {
        case 'A':  /* Up arrow */
#if MSH_ENABLE_HISTORY
            history_browse(sh, 1);
#endif
            return;
        case 'B':  /* Down arrow */
#if MSH_ENABLE_HISTORY
            history_browse(sh, -1);
#endif
            return;
        case 'C':  /* Right arrow */
            if (sh->cursor < sh->length) {
                sh->cursor++;
                sh_print(sh, "\033[C");
            }
            return;
        case 'D':  /* Left arrow */
            if (sh->cursor > 0) {
                sh->cursor--;
                sh_print(sh, "\033[D");
            }
            return;
        default:
            return;  /* ignore unknown sequences */
        }
    }

    /* ── Normal character handling ─────────────────────────────────────── */

    switch (c) {
    case KEY_ESC:
        sh->esc_state = 1;
        return;

    case KEY_ENTER:
    case KEY_NEWLINE:
        sh_print(sh, "\r\n");
        sh->line[sh->length] = '\0';

#if MSH_ENABLE_HISTORY
        history_push(sh, sh->line);
        sh->hist_browse = 0;
#endif

        if (sh->length > 0) {
            /* Copy line before execute (parse modifies it) */
            char exec_line[MSH_LINE_SIZE];
            memcpy(exec_line, sh->line, sh->length + 1);
            line_clear(sh);

            int result = execute(sh, exec_line);
            if (result != 0 && result != MSH_ERR_NOT_FOUND) {
                char buf[32];
                snprintf(buf, sizeof(buf), "error: %d\r\n", result);
                sh_print(sh, buf);
            }
        } else {
            line_clear(sh);
        }

        msh_prompt(sh);
        return;

    case KEY_BACKSPACE:
    case KEY_DEL:
        if (sh->cursor > 0) {
            line_delete_back(sh);
            if (sh->echo) {
                /* Redraw from cursor position */
                sh_print(sh, "\b");
                sh_print(sh, sh->line + sh->cursor);
                sh_print(sh, " \b");
                /* Move cursor back to position */
                for (uint8_t i = sh->cursor; i < sh->length; i++) {
                    sh_print(sh, "\b");
                }
            }
        }
        return;

    case KEY_TAB:
#if MSH_ENABLE_COMPLETE
        tab_complete(sh);
#endif
        return;

    default:
        /* Printable characters */
        if (c >= ' ' && c <= '~') {
            line_insert_char(sh, c);
            if (sh->echo) {
                if (sh->cursor == sh->length) {
                    /* Simple append */
                    sh_putc(sh, c);
                } else {
                    /* Inserted in middle — redraw from cursor */
                    sh_print(sh, sh->line + sh->cursor - 1);
                    for (uint8_t i = sh->cursor; i < sh->length; i++) {
                        sh_print(sh, "\b");
                    }
                }
            }
        }
        return;
    }
}

int msh_exec(msh_t *sh, const char *line)
{
    if (sh == NULL || line == NULL) return MSH_ERR_NULL;

    char buf[MSH_LINE_SIZE];
    strncpy(buf, line, MSH_LINE_SIZE - 1);
    buf[MSH_LINE_SIZE - 1] = '\0';

    return execute(sh, buf);
}

uint8_t msh_command_count(const msh_t *sh)
{
    if (sh == NULL) return 0;
    return sh->num_commands;
}

const msh_cmd_t *msh_command_at(const msh_t *sh, uint8_t index)
{
    if (sh == NULL || index >= sh->num_commands) return NULL;
    return &sh->commands[index];
}
