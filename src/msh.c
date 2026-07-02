/*
 * microsh - implementation
 *
 * SPDX-License-Identifier: MIT
 * https://github.com/Vanderhell/microsh
 */

#include "msh.h"

#include <stdio.h>
#include <string.h>

#define MSH_KEY_CR '\r'
#define MSH_KEY_LF '\n'
#define MSH_KEY_BS '\b'
#define MSH_KEY_DEL 127
#define MSH_KEY_ESC 27
#define MSH_KEY_TAB '\t'

#define MSH_ESC_NONE 0u
#define MSH_ESC_SEEN_ESC 1u
#define MSH_ESC_SEEN_CSI 2u

typedef enum {
    MSH_PARSE_OK = 0,
    MSH_PARSE_TOO_MANY_ARGS,
    MSH_PARSE_INVALID
} msh_parse_status_t;

typedef struct {
    int result;
    bool lookup_failed;
} msh_exec_result_t;

const unsigned char MSH_ABI_GUARD_SYMBOL = 0;

static void msh_print_str(msh_t *sh, const char *str)
{
    if ((sh != NULL) && (sh->print != NULL) && (str != NULL)) {
        sh->print(str, sh->ctx);
    }
}

static void msh_print_char(msh_t *sh, char c)
{
    char buf[2];
    buf[0] = c;
    buf[1] = '\0';
    msh_print_str(sh, buf);
}

static void msh_write_prefix(msh_t *sh, uint8_t count)
{
    char saved;

    if (count == 0u) {
        return;
    }

    saved = sh->line[count];
    sh->line[count] = '\0';
    msh_print_str(sh, sh->line);
    sh->line[count] = saved;
}

static void msh_redraw_line(msh_t *sh, uint8_t previous_length)
{
    uint8_t i;

    if ((sh == NULL) || !sh->echo) {
        return;
    }

    msh_print_str(sh, "\r");
    if (sh->prompt != NULL) {
        msh_print_str(sh, sh->prompt);
    }
    msh_print_str(sh, sh->line);

    for (i = sh->length; i < previous_length; ++i) {
        msh_print_char(sh, ' ');
    }

    msh_print_str(sh, "\r");
    if (sh->prompt != NULL) {
        msh_print_str(sh, sh->prompt);
    }
    msh_write_prefix(sh, sh->cursor);
}

static void msh_line_reset(msh_t *sh)
{
    sh->cursor = 0u;
    sh->length = 0u;
    sh->line_overflow = false;
    sh->line[0] = '\0';
}

#if MSH_ENABLE_HISTORY
static void msh_line_set(msh_t *sh, const char *text)
{
    size_t len;

    len = strlen(text);
    if (len >= (size_t)MSH_LINE_SIZE) {
        len = (size_t)MSH_LINE_SIZE - 1u;
    }

    memcpy(sh->line, text, len);
    sh->line[len] = '\0';
    sh->length = (uint8_t)len;
    sh->cursor = (uint8_t)len;
    sh->line_overflow = false;
}
#endif

static bool msh_line_insert_char(msh_t *sh, char c)
{
    uint8_t i;

    if (sh->length >= (uint8_t)(MSH_LINE_SIZE - 1u)) {
        sh->line_overflow = true;
        return false;
    }

    for (i = sh->length; i > sh->cursor; --i) {
        sh->line[i] = sh->line[i - 1u];
    }

    sh->line[sh->cursor] = c;
    sh->cursor++;
    sh->length++;
    sh->line[sh->length] = '\0';
    return true;
}

static bool msh_line_delete_back(msh_t *sh)
{
    uint8_t i;

    if (sh->cursor == 0u) {
        return false;
    }

    for (i = sh->cursor; i <= sh->length; ++i) {
        sh->line[i - 1u] = sh->line[i];
    }

    sh->cursor--;
    sh->length--;
    return true;
}

static bool msh_line_delete_forward(msh_t *sh)
{
    uint8_t i;

    if (sh->cursor >= sh->length) {
        return false;
    }

    for (i = sh->cursor; i < sh->length; ++i) {
        sh->line[i] = sh->line[i + 1u];
    }

    sh->length--;
    return true;
}

#if MSH_ENABLE_HISTORY
static void msh_history_push(msh_t *sh, const char *line)
{
    uint8_t last_index;

    if (line[0] == '\0') {
        return;
    }

    if (sh->hist_count > 0u) {
        last_index = (uint8_t)((sh->hist_write + MSH_HISTORY_DEPTH - 1u) %
                               MSH_HISTORY_DEPTH);
        if (strcmp(sh->history[last_index], line) == 0) {
            return;
        }
    }

    memcpy(sh->history[sh->hist_write], line, (size_t)MSH_LINE_SIZE);
    sh->hist_write = (uint8_t)((sh->hist_write + 1u) % MSH_HISTORY_DEPTH);
    if (sh->hist_count < (uint8_t)MSH_HISTORY_DEPTH) {
        sh->hist_count++;
    }
}

static const char *msh_history_get(const msh_t *sh, uint8_t offset)
{
    int index;

    if ((offset == 0u) || (offset > sh->hist_count)) {
        return NULL;
    }

    index = (int)sh->hist_write - (int)offset;
    while (index < 0) {
        index += MSH_HISTORY_DEPTH;
    }

    return sh->history[index];
}

static void msh_history_browse(msh_t *sh, int direction)
{
    int new_offset;
    const char *entry;
    uint8_t previous_length;

    previous_length = sh->length;
    new_offset = (int)sh->hist_browse + direction;
    if (new_offset < 0) {
        new_offset = 0;
    }
    if (new_offset > (int)sh->hist_count) {
        new_offset = (int)sh->hist_count;
    }

    sh->hist_browse = (uint8_t)new_offset;
    if (sh->hist_browse == 0u) {
        msh_line_reset(sh);
    } else {
        entry = msh_history_get(sh, sh->hist_browse);
        if (entry != NULL) {
            msh_line_set(sh, entry);
        }
    }

    msh_redraw_line(sh, previous_length);
}
#endif

static bool msh_name_is_valid(const char *name)
{
    size_t i;
    size_t len;
    unsigned char ch;

    len = strlen(name);
    if ((len == 0u) || (len >= (size_t)MSH_LINE_SIZE)) {
        return false;
    }

    for (i = 0; i < len; ++i) {
        ch = (unsigned char)name[i];
        if ((ch < 33u) || (ch > 126u) || (ch == '"') || (ch == '\'')) {
            return false;
        }
    }

    return true;
}

#if MSH_ENABLE_COMPLETE
static void msh_tab_complete(msh_t *sh)
{
    const msh_cmd_t *match;
    size_t name_len;
    uint8_t i;
    uint8_t match_count;

    if ((sh->length == 0u) || (sh->cursor != sh->length) ||
        (strchr(sh->line, ' ') != NULL) || (strchr(sh->line, '\t') != NULL)) {
        return;
    }

    match = NULL;
    match_count = 0u;
    for (i = 0u; i < sh->num_commands; ++i) {
        if (strncmp(sh->commands[i].name, sh->line, (size_t)sh->length) == 0) {
            match = &sh->commands[i];
            match_count++;
        }
    }

    if ((match_count == 1u) && (match != NULL)) {
        uint8_t previous_length;

        previous_length = sh->length;
        name_len = strlen(match->name);
        memcpy(sh->line, match->name, name_len);
        sh->length = (uint8_t)name_len;
        sh->cursor = sh->length;
        if ((name_len + 2u) <= (size_t)MSH_LINE_SIZE) {
            sh->line[sh->length] = ' ';
            sh->length++;
            sh->cursor = sh->length;
        }
        sh->line[sh->length] = '\0';
        msh_redraw_line(sh, previous_length);
        return;
    }

    if (match_count > 1u) {
        msh_print_str(sh, "\r\n");
        for (i = 0u; i < sh->num_commands; ++i) {
            if (strncmp(sh->commands[i].name, sh->line,
                        (size_t)sh->length) == 0) {
                msh_print_str(sh, "  ");
                msh_print_str(sh, sh->commands[i].name);
                msh_print_str(sh, "\r\n");
            }
        }
        msh_redraw_line(sh, sh->length);
    }
}
#endif

static msh_parse_status_t msh_parse_args(char *line, const char *argv[],
                                         uint8_t *argc_out)
{
    uint8_t argc;
    char *p;

    argc = 0u;
    p = line;

    while (*p != '\0') {
        while ((*p == ' ') || (*p == '\t')) {
            ++p;
        }
        if (*p == '\0') {
            break;
        }

        if (argc >= (uint8_t)MSH_MAX_ARGS) {
            return MSH_PARSE_TOO_MANY_ARGS;
        }

        if (*p == '"') {
            argv[argc++] = ++p;
            while ((*p != '\0') && (*p != '"')) {
                ++p;
            }
            if (*p != '"') {
                return MSH_PARSE_INVALID;
            }
            *p = '\0';
            ++p;
            if ((*p != '\0') && (*p != ' ') && (*p != '\t')) {
                return MSH_PARSE_INVALID;
            }
            continue;
        }

        argv[argc++] = p;
        while ((*p != '\0') && (*p != ' ') && (*p != '\t')) {
            ++p;
        }
        if (*p != '\0') {
            *p = '\0';
            ++p;
        }
    }

    *argc_out = argc;
    return MSH_PARSE_OK;
}

static int msh_builtin_help(int argc, const char *const *argv, void *ctx)
{
    msh_t *sh;
    uint8_t i;

    (void)argc;
    (void)argv;

    sh = (msh_t *)ctx;
    msh_print_str(sh, "Available commands:\r\n");
    for (i = 0u; i < sh->num_commands; ++i) {
        char buf[MSH_LINE_SIZE];
        int written;

        written = snprintf(buf, sizeof(buf), "  %-12s %s\r\n",
                           sh->commands[i].name,
                           (sh->commands[i].help != NULL) ? sh->commands[i].help
                                                          : "");
        if (written > 0) {
            msh_print_str(sh, buf);
        }
    }

    return MSH_OK;
}

static void msh_print_result(msh_t *sh, int result)
{
    char buf[48];
    int written;

    written = snprintf(buf, sizeof(buf), "error: %d (%s)\r\n", result,
                       msh_err_str((msh_err_t)result));
    if (written > 0) {
        msh_print_str(sh, buf);
    }
}

static void msh_print_not_found(msh_t *sh, const char *name)
{
    msh_print_str(sh, "Unknown command: ");
    msh_print_str(sh, name);
    msh_print_str(sh, "\r\nType 'help' for available commands.\r\n");
}

static msh_exec_result_t msh_execute_line(msh_t *sh, char *line, bool emit_lookup_error)
{
    const char *argv[MSH_MAX_ARGS];
    uint8_t argc;
    uint8_t i;
    msh_exec_result_t exec_result;
    msh_parse_status_t parse_status;

    exec_result.result = MSH_OK;
    exec_result.lookup_failed = false;

    parse_status = msh_parse_args(line, argv, &argc);
    if (parse_status == MSH_PARSE_TOO_MANY_ARGS) {
        exec_result.result = MSH_ERR_ARGS;
        return exec_result;
    }
    if (parse_status == MSH_PARSE_INVALID) {
        exec_result.result = MSH_ERR_INVALID;
        return exec_result;
    }
    if (argc == 0u) {
        return exec_result;
    }

    for (i = 0u; i < sh->num_commands; ++i) {
        if (strcmp(sh->commands[i].name, argv[0]) == 0) {
            if (sh->commands[i].handler == msh_builtin_help) {
                exec_result.result = sh->commands[i].handler((int)argc, argv, sh);
            } else {
                exec_result.result =
                    sh->commands[i].handler((int)argc, argv, sh->ctx);
            }
            return exec_result;
        }
    }

    exec_result.result = MSH_ERR_NOT_FOUND;
    exec_result.lookup_failed = true;
    if (emit_lookup_error) {
        msh_print_not_found(sh, argv[0]);
    }
    return exec_result;
}

static void msh_handle_enter(msh_t *sh)
{
    char exec_line[MSH_LINE_SIZE];
    msh_exec_result_t exec_result;

    msh_print_str(sh, "\r\n");

    if (sh->line_overflow) {
        msh_line_reset(sh);
        msh_print_result(sh, MSH_ERR_INPUT_TOO_LONG);
        msh_prompt(sh);
        return;
    }

#if MSH_ENABLE_HISTORY
    msh_history_push(sh, sh->line);
    sh->hist_browse = 0u;
#endif

    memcpy(exec_line, sh->line, (size_t)sh->length + 1u);
    msh_line_reset(sh);

    exec_result = msh_execute_line(sh, exec_line, true);
    if ((exec_result.result != MSH_OK) && !exec_result.lookup_failed) {
        msh_print_result(sh, exec_result.result);
    }

    msh_prompt(sh);
}

const char *msh_err_str(msh_err_t err)
{
    switch (err) {
    case MSH_OK:
        return "ok";
    case MSH_ERR_NULL:
        return "null pointer";
    case MSH_ERR_FULL:
        return "command table full";
    case MSH_ERR_NOT_FOUND:
        return "command not found";
    case MSH_ERR_ARGS:
        return "wrong arguments";
    case MSH_ERR_INVALID:
        return "invalid input";
    case MSH_ERR_INPUT_TOO_LONG:
        return "input too long";
    default:
        return "unknown error";
    }
}

msh_err_t msh_init(msh_t *sh, msh_print_fn print, void *ctx)
{
    msh_err_t init_result;

    if ((sh == NULL) || (print == NULL)) {
        return MSH_ERR_NULL;
    }

    *sh = (msh_t){0};
    sh->print = print;
    sh->ctx = ctx;
    sh->prompt = "> ";
    sh->echo = true;

    init_result = msh_register(sh, MSH_HELP_NAME, "Show available commands",
                               msh_builtin_help);
    if (init_result != MSH_OK) {
        return init_result;
    }

    return MSH_OK;
}

void msh_set_prompt(msh_t *sh, const char *prompt)
{
    if (sh != NULL) {
        sh->prompt = prompt;
    }
}

void msh_set_echo(msh_t *sh, bool echo)
{
    if (sh != NULL) {
        sh->echo = echo;
    }
}

msh_err_t msh_register(msh_t *sh, const char *name, const char *help,
                       msh_cmd_fn handler)
{
    uint8_t i;
    msh_cmd_t *slot;

    if ((sh == NULL) || (name == NULL) || (handler == NULL)) {
        return MSH_ERR_NULL;
    }
    if (!msh_name_is_valid(name)) {
        return MSH_ERR_INVALID;
    }
    for (i = 0u; i < sh->num_commands; ++i) {
        if (strcmp(sh->commands[i].name, name) == 0) {
            return MSH_ERR_INVALID;
        }
    }
    if (sh->num_commands >= (uint8_t)MSH_MAX_COMMANDS) {
        return MSH_ERR_FULL;
    }

    slot = &sh->commands[sh->num_commands];
    *slot = (msh_cmd_t){name, help, handler};
    sh->num_commands++;
    return MSH_OK;
}

void msh_prompt(msh_t *sh)
{
    if ((sh != NULL) && (sh->prompt != NULL)) {
        msh_print_str(sh, sh->prompt);
    }
}

void msh_feed(msh_t *sh, char c)
{
    uint8_t previous_length;
    unsigned char byte;

    if (sh == NULL) {
        return;
    }

    byte = (unsigned char)c;

    if (sh->esc_state == MSH_ESC_SEEN_ESC) {
        sh->esc_state = MSH_ESC_NONE;
        if (c == '[') {
            sh->esc_state = MSH_ESC_SEEN_CSI;
            sh->csi_param = 0u;
            return;
        }
    } else if (sh->esc_state == MSH_ESC_SEEN_CSI) {
        if ((byte >= (unsigned char)'0') && (byte <= (unsigned char)'9')) {
            if (sh->csi_param < 25u) {
                sh->csi_param = (uint8_t)(sh->csi_param * 10u + (byte - '0'));
            }
            return;
        }
        if (c == ';' || c == '?') {
            return;
        }

        sh->esc_state = MSH_ESC_NONE;
        switch (c) {
#if MSH_ENABLE_HISTORY
        case 'A':
            msh_history_browse(sh, 1);
            return;
        case 'B':
            msh_history_browse(sh, -1);
            return;
#endif
        case 'C':
            if (sh->cursor < sh->length) {
                sh->cursor++;
                if (sh->echo) {
                    msh_print_str(sh, "\033[C");
                }
            }
            return;
        case 'D':
            if (sh->cursor > 0u) {
                sh->cursor--;
                if (sh->echo) {
                    msh_print_str(sh, "\033[D");
                }
            }
            return;
        case '~':
            if (sh->csi_param == 3u) {
                previous_length = sh->length;
                if (msh_line_delete_forward(sh)) {
                    msh_redraw_line(sh, previous_length);
                }
            }
            return;
        default:
            return;
        }
    }

    if (c != MSH_KEY_LF) {
        sh->saw_cr = false;
    }

    switch (c) {
    case MSH_KEY_ESC:
        sh->esc_state = MSH_ESC_SEEN_ESC;
        return;

    case MSH_KEY_CR:
        sh->saw_cr = true;
        msh_handle_enter(sh);
        return;

    case MSH_KEY_LF:
        if (sh->saw_cr) {
            sh->saw_cr = false;
            return;
        }
        msh_handle_enter(sh);
        return;

    case MSH_KEY_BS:
        previous_length = sh->length;
        if (msh_line_delete_back(sh)) {
            msh_redraw_line(sh, previous_length);
        }
        return;

    case MSH_KEY_DEL:
        previous_length = sh->length;
        if (msh_line_delete_back(sh)) {
            msh_redraw_line(sh, previous_length);
        }
        return;

    case MSH_KEY_TAB:
#if MSH_ENABLE_COMPLETE
        msh_tab_complete(sh);
#endif
        return;

    default:
        if ((byte >= 32u) && (byte <= 126u)) {
            previous_length = sh->length;
            if (msh_line_insert_char(sh, c)) {
                msh_redraw_line(sh, previous_length);
            }
        }
        return;
    }
}

int msh_exec(msh_t *sh, const char *line)
{
    char buf[MSH_LINE_SIZE];
    size_t len;
    msh_exec_result_t exec_result;

    if ((sh == NULL) || (line == NULL)) {
        return MSH_ERR_NULL;
    }

    len = strlen(line);
    if (len >= (size_t)MSH_LINE_SIZE) {
        return MSH_ERR_INPUT_TOO_LONG;
    }

    memcpy(buf, line, len + 1u);
    exec_result = msh_execute_line(sh, buf, true);
    return exec_result.result;
}

uint8_t msh_command_count(const msh_t *sh)
{
    if (sh == NULL) {
        return 0u;
    }
    return sh->num_commands;
}

const msh_cmd_t *msh_command_at(const msh_t *sh, uint8_t index)
{
    if ((sh == NULL) || (index >= sh->num_commands)) {
        return NULL;
    }
    return &sh->commands[index];
}
