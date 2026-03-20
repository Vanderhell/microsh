/*
 * microsh test suite.
 *
 * Build: gcc -std=c99 -Wall -Wextra -I../include ../src/msh.c test_all.c -o test_all
 * Run:   ./test_all
 */

#include "msh.h"
#include <stdio.h>
#include <string.h>

/* ── Minimal test framework ────────────────────────────────────────────── */

static int tests_run = 0, tests_passed = 0, tests_failed = 0;

#define TEST(name) static void name(void)
#define RUN_TEST(name) do {                                     \
    tests_run++;                                                \
    printf("  %-55s ", #name);                                  \
    name();                                                     \
    printf("PASS\n");                                           \
    tests_passed++;                                             \
} while (0)

#define ASSERT_EQ(expected, actual) do {                        \
    if ((expected) != (actual)) {                               \
        printf("FAIL\n    %s:%d: expected %d, got %d\n",       \
               __FILE__, __LINE__, (int)(expected), (int)(actual)); \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

#define ASSERT_TRUE(expr) do {                                  \
    if (!(expr)) {                                              \
        printf("FAIL\n    %s:%d: expected true\n",              \
               __FILE__, __LINE__);                             \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

#define ASSERT_FALSE(expr) do {                                 \
    if ((expr)) {                                               \
        printf("FAIL\n    %s:%d: expected false\n",             \
               __FILE__, __LINE__);                             \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

#define ASSERT_STR_EQ(expected, actual) do {                    \
    if (strcmp((expected), (actual)) != 0) {                     \
        printf("FAIL\n    %s:%d:\n      expected: \"%s\"\n      got:      \"%s\"\n", \
               __FILE__, __LINE__, (expected), (actual));       \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

#define ASSERT_STR_CONTAINS(haystack, needle) do {              \
    if (strstr((haystack), (needle)) == NULL) {                  \
        printf("FAIL\n    %s:%d: \"%s\" not found in output\n", \
               __FILE__, __LINE__, (needle));                   \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

/* ── Output capture ────────────────────────────────────────────────────── */

#define OUTPUT_SIZE 2048
static char output_buf[OUTPUT_SIZE];
static int output_pos = 0;

static void capture_print(const char *str, void *ctx)
{
    (void)ctx;
    int len = (int)strlen(str);
    if (output_pos + len < OUTPUT_SIZE - 1) {
        memcpy(output_buf + output_pos, str, (size_t)len);
        output_pos += len;
        output_buf[output_pos] = '\0';
    }
}

static void capture_reset(void)
{
    memset(output_buf, 0, OUTPUT_SIZE);
    output_pos = 0;
}

/* ── Command call tracking ─────────────────────────────────────────────── */

static int cmd_called = 0;
static int cmd_argc = 0;
static char cmd_argv_copy[MSH_MAX_ARGS][64];

static void reset_tracking(void)
{
    cmd_called = 0;
    cmd_argc = 0;
    memset(cmd_argv_copy, 0, sizeof(cmd_argv_copy));
}

static int cmd_echo(int argc, const char **argv, void *ctx)
{
    (void)ctx;
    cmd_called++;
    cmd_argc = argc;
    for (int i = 0; i < argc && i < MSH_MAX_ARGS; i++) {
        strncpy(cmd_argv_copy[i], argv[i], 63);
    }
    return 0;
}

static int cmd_fail(int argc, const char **argv, void *ctx)
{
    (void)argc; (void)argv; (void)ctx;
    cmd_called++;
    return -42;
}

static int cmd_add(int argc, const char **argv, void *ctx)
{
    (void)ctx; (void)argv;
    cmd_called++;
    if (argc != 3) return MSH_ERR_ARGS;
    cmd_argc = argc;
    return 0;
}

/* ── Helper ────────────────────────────────────────────────────────────── */

static msh_t sh;

static void setup(void)
{
    capture_reset();
    reset_tracking();
    msh_init(&sh, capture_print, NULL);
}

/** Feed a string char-by-char, as if typed. */
static void type_line(const char *str)
{
    while (*str) {
        msh_feed(&sh, *str++);
    }
}

/** Type a line and press Enter. */
static void type_and_enter(const char *str)
{
    type_line(str);
    msh_feed(&sh, '\r');
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Init
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_init) {
    setup();
    ASSERT_EQ(1, sh.num_commands);  /* built-in help */
    ASSERT_TRUE(sh.echo);
    ASSERT_STR_EQ("> ", sh.prompt);
}

TEST(test_init_null) {
    ASSERT_EQ(MSH_ERR_NULL, msh_init(NULL, capture_print, NULL));
    ASSERT_EQ(MSH_ERR_NULL, msh_init(&sh, NULL, NULL));
}

TEST(test_set_prompt) {
    setup();
    msh_set_prompt(&sh, "$ ");
    ASSERT_STR_EQ("$ ", sh.prompt);

    msh_set_prompt(&sh, NULL);
    ASSERT_TRUE(sh.prompt == NULL);
}

TEST(test_set_echo) {
    setup();
    msh_set_echo(&sh, false);
    ASSERT_FALSE(sh.echo);
    msh_set_echo(&sh, true);
    ASSERT_TRUE(sh.echo);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Command registration
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_register) {
    setup();
    ASSERT_EQ(MSH_OK, msh_register(&sh, "test", "A test command", cmd_echo));
    ASSERT_EQ(2, sh.num_commands);  /* help + test */
}

TEST(test_register_null) {
    setup();
    ASSERT_EQ(MSH_ERR_NULL, msh_register(NULL, "x", "y", cmd_echo));
    ASSERT_EQ(MSH_ERR_NULL, msh_register(&sh, NULL, "y", cmd_echo));
    ASSERT_EQ(MSH_ERR_NULL, msh_register(&sh, "x", "y", NULL));
}

TEST(test_register_full) {
    setup();
    /* help is already registered, fill the rest */
    for (int i = 1; i < MSH_MAX_COMMANDS; i++) {
        char name[8];
        snprintf(name, sizeof(name), "c%d", i);
        /* Using static name would be needed in real code, but for test it's ok */
        ASSERT_EQ(MSH_OK, msh_register(&sh, "cmd", NULL, cmd_echo));
    }
    ASSERT_EQ(MSH_ERR_FULL, msh_register(&sh, "extra", NULL, cmd_echo));
}

TEST(test_command_count) {
    setup();
    ASSERT_EQ(1, msh_command_count(&sh));
    msh_register(&sh, "a", NULL, cmd_echo);
    msh_register(&sh, "b", NULL, cmd_echo);
    ASSERT_EQ(3, msh_command_count(&sh));
    ASSERT_EQ(0, msh_command_count(NULL));
}

TEST(test_command_at) {
    setup();
    msh_register(&sh, "mytest", "desc", cmd_echo);
    const msh_cmd_t *c = msh_command_at(&sh, 1);
    ASSERT_TRUE(c != NULL);
    ASSERT_STR_EQ("mytest", c->name);
    ASSERT_STR_EQ("desc", c->help);

    ASSERT_TRUE(msh_command_at(&sh, 99) == NULL);
    ASSERT_TRUE(msh_command_at(NULL, 0) == NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Command execution (msh_exec)
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_exec_basic) {
    setup();
    msh_register(&sh, "echo", NULL, cmd_echo);
    int result = msh_exec(&sh, "echo hello world");
    ASSERT_EQ(0, result);
    ASSERT_EQ(1, cmd_called);
    ASSERT_EQ(3, cmd_argc);
    ASSERT_STR_EQ("echo", cmd_argv_copy[0]);
    ASSERT_STR_EQ("hello", cmd_argv_copy[1]);
    ASSERT_STR_EQ("world", cmd_argv_copy[2]);
}

TEST(test_exec_no_args) {
    setup();
    msh_register(&sh, "ping", NULL, cmd_echo);
    msh_exec(&sh, "ping");
    ASSERT_EQ(1, cmd_called);
    ASSERT_EQ(1, cmd_argc);
    ASSERT_STR_EQ("ping", cmd_argv_copy[0]);
}

TEST(test_exec_not_found) {
    setup();
    int result = msh_exec(&sh, "nonexistent");
    ASSERT_EQ(MSH_ERR_NOT_FOUND, result);
}

TEST(test_exec_empty) {
    setup();
    int result = msh_exec(&sh, "");
    ASSERT_EQ(MSH_OK, result);
    ASSERT_EQ(0, cmd_called);
}

TEST(test_exec_whitespace_only) {
    setup();
    int result = msh_exec(&sh, "   ");
    ASSERT_EQ(MSH_OK, result);
    ASSERT_EQ(0, cmd_called);
}

TEST(test_exec_extra_whitespace) {
    setup();
    msh_register(&sh, "echo", NULL, cmd_echo);
    msh_exec(&sh, "  echo   hello   world  ");
    ASSERT_EQ(1, cmd_called);
    ASSERT_EQ(3, cmd_argc);
    ASSERT_STR_EQ("hello", cmd_argv_copy[1]);
    ASSERT_STR_EQ("world", cmd_argv_copy[2]);
}

TEST(test_exec_quoted_args) {
    setup();
    msh_register(&sh, "set", NULL, cmd_echo);
    msh_exec(&sh, "set name \"John Doe\"");
    ASSERT_EQ(3, cmd_argc);
    ASSERT_STR_EQ("set", cmd_argv_copy[0]);
    ASSERT_STR_EQ("name", cmd_argv_copy[1]);
    ASSERT_STR_EQ("John Doe", cmd_argv_copy[2]);
}

TEST(test_exec_command_error) {
    setup();
    msh_register(&sh, "fail", NULL, cmd_fail);
    int result = msh_exec(&sh, "fail");
    ASSERT_EQ(-42, result);
    ASSERT_EQ(1, cmd_called);
}

TEST(test_exec_null) {
    setup();
    ASSERT_EQ(MSH_ERR_NULL, msh_exec(NULL, "test"));
    ASSERT_EQ(MSH_ERR_NULL, msh_exec(&sh, NULL));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Built-in help
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_help_command) {
    setup();
    msh_register(&sh, "reboot", "Restart the device", cmd_echo);
    msh_register(&sh, "status", "Show device status", cmd_echo);

    capture_reset();
    msh_exec(&sh, "help");

    ASSERT_STR_CONTAINS(output_buf, "help");
    ASSERT_STR_CONTAINS(output_buf, "reboot");
    ASSERT_STR_CONTAINS(output_buf, "Restart the device");
    ASSERT_STR_CONTAINS(output_buf, "status");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Interactive input (msh_feed)
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_feed_basic) {
    setup();
    msh_register(&sh, "ping", NULL, cmd_echo);
    capture_reset();
    type_and_enter("ping");
    ASSERT_EQ(1, cmd_called);
    ASSERT_STR_EQ("ping", cmd_argv_copy[0]);
}

TEST(test_feed_with_args) {
    setup();
    msh_register(&sh, "add", NULL, cmd_add);
    type_and_enter("add 1 2");
    ASSERT_EQ(1, cmd_called);
    ASSERT_EQ(3, cmd_argc);
}

TEST(test_feed_backspace) {
    setup();
    msh_register(&sh, "ping", NULL, cmd_echo);

    /* Type "pixx", backspace twice, type "ng" → "ping" */
    type_line("pixx");
    msh_feed(&sh, '\b');
    msh_feed(&sh, '\b');
    type_line("ng");
    msh_feed(&sh, '\r');

    ASSERT_EQ(1, cmd_called);
    ASSERT_STR_EQ("ping", cmd_argv_copy[0]);
}

TEST(test_feed_delete_key) {
    setup();
    msh_register(&sh, "ping", NULL, cmd_echo);

    type_line("pingg");
    msh_feed(&sh, 127);  /* DEL */
    msh_feed(&sh, '\r');

    ASSERT_EQ(1, cmd_called);
    ASSERT_STR_EQ("ping", cmd_argv_copy[0]);
}

TEST(test_feed_empty_enter) {
    setup();
    capture_reset();
    msh_feed(&sh, '\r');
    ASSERT_EQ(0, cmd_called);
    /* Should still print prompt */
    ASSERT_STR_CONTAINS(output_buf, "> ");
}

TEST(test_feed_echo_off) {
    setup();
    msh_set_echo(&sh, false);
    msh_register(&sh, "ping", NULL, cmd_echo);

    capture_reset();
    type_and_enter("ping");

    /* Characters should NOT be echoed, but command still executes */
    ASSERT_EQ(1, cmd_called);
    /* The only output should be newline + not-found/error or prompt, no 'p','i','n','g' */
    ASSERT_TRUE(output_buf[0] == '\r' || output_buf[0] == '>');
}

TEST(test_feed_unknown_command_message) {
    setup();
    capture_reset();
    type_and_enter("bogus");
    ASSERT_STR_CONTAINS(output_buf, "Unknown command");
    ASSERT_STR_CONTAINS(output_buf, "bogus");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: History
 * ═══════════════════════════════════════════════════════════════════════════ */

#if MSH_ENABLE_HISTORY
TEST(test_history_up_arrow) {
    setup();
    msh_register(&sh, "aaa", NULL, cmd_echo);
    msh_register(&sh, "bbb", NULL, cmd_echo);

    type_and_enter("aaa");
    type_and_enter("bbb");

    reset_tracking();
    capture_reset();

    /* Press up arrow → should recall "bbb" */
    msh_feed(&sh, 27);   /* ESC */
    msh_feed(&sh, '[');
    msh_feed(&sh, 'A');   /* Up */
    msh_feed(&sh, '\r');

    ASSERT_EQ(1, cmd_called);
    ASSERT_STR_EQ("bbb", cmd_argv_copy[0]);
}

TEST(test_history_up_up_arrow) {
    setup();
    msh_register(&sh, "first", NULL, cmd_echo);
    msh_register(&sh, "second", NULL, cmd_echo);

    type_and_enter("first");
    type_and_enter("second");

    reset_tracking();

    /* Up, Up → should recall "first" */
    msh_feed(&sh, 27); msh_feed(&sh, '['); msh_feed(&sh, 'A');
    msh_feed(&sh, 27); msh_feed(&sh, '['); msh_feed(&sh, 'A');
    msh_feed(&sh, '\r');

    ASSERT_EQ(1, cmd_called);
    ASSERT_STR_EQ("first", cmd_argv_copy[0]);
}

TEST(test_history_no_duplicate) {
    setup();
    msh_register(&sh, "ping", NULL, cmd_echo);

    type_and_enter("ping");
    type_and_enter("ping");  /* duplicate — should not be added */

    /* Up should give "ping", Up again should not go further */
    reset_tracking();
    msh_feed(&sh, 27); msh_feed(&sh, '['); msh_feed(&sh, 'A');
    msh_feed(&sh, 27); msh_feed(&sh, '['); msh_feed(&sh, 'A');
    msh_feed(&sh, '\r');

    ASSERT_EQ(1, cmd_called);
    ASSERT_STR_EQ("ping", cmd_argv_copy[0]);
}

TEST(test_history_down_arrow_restore) {
    setup();
    msh_register(&sh, "aaa", NULL, cmd_echo);

    type_and_enter("aaa");
    reset_tracking();

    /* Up → recall, Down → back to empty, Enter → empty line */
    msh_feed(&sh, 27); msh_feed(&sh, '['); msh_feed(&sh, 'A');
    msh_feed(&sh, 27); msh_feed(&sh, '['); msh_feed(&sh, 'B');
    msh_feed(&sh, '\r');

    ASSERT_EQ(0, cmd_called);  /* empty line */
}

TEST(test_history_empty_not_stored) {
    setup();
    msh_register(&sh, "aaa", NULL, cmd_echo);

    msh_feed(&sh, '\r');  /* empty enter */
    type_and_enter("aaa");
    reset_tracking();

    /* Only "aaa" should be in history, not the empty line */
    ASSERT_EQ(1, (int)sh.hist_count);
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Tab completion
 * ═══════════════════════════════════════════════════════════════════════════ */

#if MSH_ENABLE_COMPLETE
TEST(test_tab_single_match) {
    setup();
    msh_register(&sh, "reboot", "Restart", cmd_echo);

    capture_reset();
    type_line("reb");
    msh_feed(&sh, '\t');

    /* Line should now be "reboot " */
    ASSERT_STR_EQ("reboot ", sh.line);
}

TEST(test_tab_multiple_matches) {
    setup();
    msh_register(&sh, "read", NULL, cmd_echo);
    msh_register(&sh, "reboot", NULL, cmd_echo);

    capture_reset();
    type_line("re");
    msh_feed(&sh, '\t');

    /* Should show both options */
    ASSERT_STR_CONTAINS(output_buf, "read");
    ASSERT_STR_CONTAINS(output_buf, "reboot");
    /* Line should still be "re" */
    ASSERT_EQ(2, (int)sh.length);
}

TEST(test_tab_no_match) {
    setup();
    capture_reset();
    type_line("xyz");
    int old_pos = output_pos;
    msh_feed(&sh, '\t');
    /* No output change */
    ASSERT_EQ(old_pos, output_pos);
}

TEST(test_tab_empty) {
    setup();
    capture_reset();
    int old_pos = output_pos;
    msh_feed(&sh, '\t');
    /* No output for empty line */
    ASSERT_EQ(old_pos, output_pos);
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Arrow keys (cursor movement)
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_left_right_arrows) {
    setup();
    /* Just verify no crash — cursor movement is visual */
    type_line("abc");
    /* Left arrow */
    msh_feed(&sh, 27); msh_feed(&sh, '['); msh_feed(&sh, 'D');
    /* Right arrow */
    msh_feed(&sh, 27); msh_feed(&sh, '['); msh_feed(&sh, 'C');
    /* Should not crash, cursor state should be valid */
    ASSERT_EQ(3, (int)sh.cursor);
    ASSERT_EQ(3, (int)sh.length);
}

TEST(test_left_arrow_boundary) {
    setup();
    /* Left arrow on empty line — should not go below 0 */
    msh_feed(&sh, 27); msh_feed(&sh, '['); msh_feed(&sh, 'D');
    ASSERT_EQ(0, (int)sh.cursor);
}

TEST(test_right_arrow_boundary) {
    setup();
    type_line("ab");
    /* Right arrow past end */
    msh_feed(&sh, 27); msh_feed(&sh, '['); msh_feed(&sh, 'C');
    ASSERT_EQ(2, (int)sh.cursor);  /* should not exceed length */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Error strings
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_err_str) {
    ASSERT_STR_EQ("ok",                msh_err_str(MSH_OK));
    ASSERT_STR_EQ("null pointer",      msh_err_str(MSH_ERR_NULL));
    ASSERT_STR_EQ("command table full", msh_err_str(MSH_ERR_FULL));
    ASSERT_STR_EQ("command not found",  msh_err_str(MSH_ERR_NOT_FOUND));
    ASSERT_STR_EQ("wrong arguments",    msh_err_str(MSH_ERR_ARGS));
    ASSERT_STR_EQ("unknown error",      msh_err_str((msh_err_t)99));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Edge cases
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_backspace_on_empty) {
    setup();
    /* Should not crash */
    msh_feed(&sh, '\b');
    msh_feed(&sh, 127);
    ASSERT_EQ(0, (int)sh.cursor);
}

TEST(test_unknown_escape_sequence) {
    setup();
    /* ESC [ Z — unknown, should be ignored */
    msh_feed(&sh, 27); msh_feed(&sh, '['); msh_feed(&sh, 'Z');
    ASSERT_EQ(0, (int)sh.length);
}

TEST(test_incomplete_escape) {
    setup();
    /* ESC followed by non-[ */
    msh_feed(&sh, 27); msh_feed(&sh, 'x');
    ASSERT_EQ(0, (int)sh.length);
}

TEST(test_prompt_display) {
    setup();
    capture_reset();
    msh_prompt(&sh);
    ASSERT_STR_CONTAINS(output_buf, "> ");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== microsh test suite ===\n\n");

    printf("[Init]\n");
    RUN_TEST(test_init);
    RUN_TEST(test_init_null);
    RUN_TEST(test_set_prompt);
    RUN_TEST(test_set_echo);

    printf("\n[Registration]\n");
    RUN_TEST(test_register);
    RUN_TEST(test_register_null);
    RUN_TEST(test_register_full);
    RUN_TEST(test_command_count);
    RUN_TEST(test_command_at);

    printf("\n[Exec]\n");
    RUN_TEST(test_exec_basic);
    RUN_TEST(test_exec_no_args);
    RUN_TEST(test_exec_not_found);
    RUN_TEST(test_exec_empty);
    RUN_TEST(test_exec_whitespace_only);
    RUN_TEST(test_exec_extra_whitespace);
    RUN_TEST(test_exec_quoted_args);
    RUN_TEST(test_exec_command_error);
    RUN_TEST(test_exec_null);

    printf("\n[Built-in Help]\n");
    RUN_TEST(test_help_command);

    printf("\n[Interactive Input]\n");
    RUN_TEST(test_feed_basic);
    RUN_TEST(test_feed_with_args);
    RUN_TEST(test_feed_backspace);
    RUN_TEST(test_feed_delete_key);
    RUN_TEST(test_feed_empty_enter);
    RUN_TEST(test_feed_echo_off);
    RUN_TEST(test_feed_unknown_command_message);

#if MSH_ENABLE_HISTORY
    printf("\n[History]\n");
    RUN_TEST(test_history_up_arrow);
    RUN_TEST(test_history_up_up_arrow);
    RUN_TEST(test_history_no_duplicate);
    RUN_TEST(test_history_down_arrow_restore);
    RUN_TEST(test_history_empty_not_stored);
#endif

#if MSH_ENABLE_COMPLETE
    printf("\n[Tab Completion]\n");
    RUN_TEST(test_tab_single_match);
    RUN_TEST(test_tab_multiple_matches);
    RUN_TEST(test_tab_no_match);
    RUN_TEST(test_tab_empty);
#endif

    printf("\n[Arrow Keys]\n");
    RUN_TEST(test_left_right_arrows);
    RUN_TEST(test_left_arrow_boundary);
    RUN_TEST(test_right_arrow_boundary);

    printf("\n[Error Strings]\n");
    RUN_TEST(test_err_str);

    printf("\n[Edge Cases]\n");
    RUN_TEST(test_backspace_on_empty);
    RUN_TEST(test_unknown_escape_sequence);
    RUN_TEST(test_incomplete_escape);
    RUN_TEST(test_prompt_display);

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) printf(", %d FAILED", tests_failed);
    printf(" ===\n\n");

    return tests_failed > 0 ? 1 : 0;
}
