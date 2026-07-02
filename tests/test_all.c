/*
 * microsh unit tests
 */

#include "msh.h"

#include <stdio.h>
#include <string.h>

#define TEST(name) static void name(void)

static int tests_run;
static int tests_passed;
static int tests_failed;

#define RUN_TEST(name)                                                          \
    do {                                                                        \
        int failures_before_ = tests_failed;                                    \
        ++tests_run;                                                            \
        printf("  %-48s ", #name);                                              \
        name();                                                                 \
        if (tests_failed == failures_before_) {                                 \
            ++tests_passed;                                                     \
            printf("PASS\n");                                                   \
        }                                                                       \
    } while (0)

#define ASSERT_EQ(expected_expr, actual_expr)                                   \
    do {                                                                        \
        int expected_ = (expected_expr);                                        \
        int actual_ = (actual_expr);                                            \
        if (expected_ != actual_) {                                             \
            printf("FAIL\n    %s:%d expected %d got %d\n", __FILE__, __LINE__,  \
                   expected_, actual_);                                         \
            ++tests_failed;                                                     \
            return;                                                             \
        }                                                                       \
    } while (0)

#define ASSERT_TRUE(expr)                                                       \
    do {                                                                        \
        int value_ = !!(expr);                                                  \
        if (!value_) {                                                          \
            printf("FAIL\n    %s:%d assertion failed: %s\n", __FILE__,          \
                   __LINE__, #expr);                                            \
            ++tests_failed;                                                     \
            return;                                                             \
        }                                                                       \
    } while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_STR_EQ(expected_expr, actual_expr)                               \
    do {                                                                        \
        const char *expected_ = (expected_expr);                                \
        const char *actual_ = (actual_expr);                                    \
        if ((expected_ == NULL) || (actual_ == NULL) ||                         \
            (strcmp(expected_, actual_) != 0)) {                                \
            printf("FAIL\n    %s:%d expected \"%s\" got \"%s\"\n",              \
                   __FILE__, __LINE__,                                          \
                   (expected_ != NULL) ? expected_ : "(null)",                  \
                   (actual_ != NULL) ? actual_ : "(null)");                     \
            ++tests_failed;                                                     \
            return;                                                             \
        }                                                                       \
    } while (0)

#define ASSERT_STR_CONTAINS(haystack_expr, needle_expr)                         \
    do {                                                                        \
        const char *haystack_ = (haystack_expr);                                \
        const char *needle_ = (needle_expr);                                    \
        if ((haystack_ == NULL) || (needle_ == NULL) ||                         \
            (strstr(haystack_, needle_) == NULL)) {                             \
            printf("FAIL\n    %s:%d missing \"%s\"\n", __FILE__, __LINE__,      \
                   (needle_ != NULL) ? needle_ : "(null)");                     \
            ++tests_failed;                                                     \
            return;                                                             \
        }                                                                       \
    } while (0)

#define OUTPUT_CAPACITY 4096

static char output_buf[OUTPUT_CAPACITY];
static size_t output_len;
static const char *last_print_ptr;

static int handler_calls;
static int handler_argc;
static int handler_result;
static char handler_argv[MSH_MAX_ARGS][64];

static msh_t shell_instance;

static void capture_print(const char *str, void *ctx)
{
    size_t len;

    (void)ctx;
    last_print_ptr = str;
    if (str == NULL) {
        return;
    }

    len = strlen(str);
    if ((output_len + len) >= (OUTPUT_CAPACITY - 1u)) {
        len = (OUTPUT_CAPACITY - 1u) - output_len;
    }
    memcpy(output_buf + output_len, str, len);
    output_len += len;
    output_buf[output_len] = '\0';
}

static void reset_output(void)
{
    memset(output_buf, 0, sizeof(output_buf));
    output_len = 0u;
    last_print_ptr = NULL;
}

static void reset_handler_state(void)
{
    handler_calls = 0;
    handler_argc = 0;
    handler_result = 0;
    memset(handler_argv, 0, sizeof(handler_argv));
}

static int recording_handler(int argc, const char *const *argv, void *ctx)
{
    int i;

    (void)ctx;
    ++handler_calls;
    handler_argc = argc;
    for (i = 0; (i < argc) && (i < MSH_MAX_ARGS); ++i) {
        strncpy(handler_argv[i], argv[i], sizeof(handler_argv[i]) - 1u);
        handler_argv[i][sizeof(handler_argv[i]) - 1u] = '\0';
    }
    return handler_result;
}

static void setup_shell(void)
{
    reset_output();
    reset_handler_state();
    ASSERT_EQ(MSH_OK, msh_init(&shell_instance, capture_print, NULL));
}

static void type_bytes(const char *text)
{
    while (*text != '\0') {
        msh_feed(&shell_instance, *text);
        ++text;
    }
}

static void press_up(void)
{
    msh_feed(&shell_instance, 27);
    msh_feed(&shell_instance, '[');
    msh_feed(&shell_instance, 'A');
}

static void press_left(void)
{
    msh_feed(&shell_instance, 27);
    msh_feed(&shell_instance, '[');
    msh_feed(&shell_instance, 'D');
}

static void press_delete(void)
{
    msh_feed(&shell_instance, 27);
    msh_feed(&shell_instance, '[');
    msh_feed(&shell_instance, '3');
    msh_feed(&shell_instance, '~');
}

TEST(test_init_registers_help)
{
    setup_shell();
    ASSERT_EQ(1, msh_command_count(&shell_instance));
    ASSERT_STR_EQ("help", msh_command_at(&shell_instance, 0)->name);
}

TEST(test_register_rejects_invalid_names)
{
    setup_shell();
    ASSERT_EQ(MSH_ERR_INVALID,
              msh_register(&shell_instance, "", NULL, recording_handler));
    ASSERT_EQ(MSH_ERR_INVALID,
              msh_register(&shell_instance, "two words", NULL,
                           recording_handler));
    ASSERT_EQ(MSH_ERR_INVALID,
              msh_register(&shell_instance, "bad\tname", NULL,
                           recording_handler));
    ASSERT_EQ(MSH_ERR_INVALID,
              msh_register(&shell_instance, "\"quote\"", NULL,
                           recording_handler));
}

TEST(test_register_rejects_duplicate_names)
{
    setup_shell();
    ASSERT_EQ(MSH_OK,
              msh_register(&shell_instance, "echo", NULL, recording_handler));
    ASSERT_EQ(MSH_ERR_INVALID,
              msh_register(&shell_instance, "echo", NULL, recording_handler));
}

TEST(test_exec_preserves_empty_quoted_arg)
{
    setup_shell();
    ASSERT_EQ(MSH_OK,
              msh_register(&shell_instance, "set", NULL, recording_handler));
    ASSERT_EQ(MSH_OK, msh_exec(&shell_instance, "set \"\" tail"));
    ASSERT_EQ(1, handler_calls);
    ASSERT_EQ(3, handler_argc);
    ASSERT_STR_EQ("", handler_argv[1]);
    ASSERT_STR_EQ("tail", handler_argv[2]);
}

TEST(test_exec_rejects_too_many_args)
{
    setup_shell();
    ASSERT_EQ(MSH_OK,
              msh_register(&shell_instance, "cmd", NULL, recording_handler));
    ASSERT_EQ(MSH_ERR_ARGS,
              msh_exec(&shell_instance,
                       "cmd a b c d e f g h i j k l m n"));
    ASSERT_EQ(0, handler_calls);
}

TEST(test_exec_rejects_unterminated_quote)
{
    setup_shell();
    ASSERT_EQ(MSH_OK,
              msh_register(&shell_instance, "cmd", NULL, recording_handler));
    ASSERT_EQ(MSH_ERR_INVALID, msh_exec(&shell_instance, "cmd \"oops"));
}

TEST(test_exec_rejects_text_after_quote)
{
    setup_shell();
    ASSERT_EQ(MSH_OK,
              msh_register(&shell_instance, "cmd", NULL, recording_handler));
    ASSERT_EQ(MSH_ERR_INVALID, msh_exec(&shell_instance, "cmd \"x\"tail"));
}

TEST(test_exec_rejects_overlong_input)
{
    char too_long[MSH_LINE_SIZE + 8];
    size_t i;

    setup_shell();
    for (i = 0; i < sizeof(too_long) - 1u; ++i) {
        too_long[i] = 'a';
    }
    too_long[sizeof(too_long) - 1u] = '\0';
    ASSERT_EQ(MSH_ERR_INPUT_TOO_LONG, msh_exec(&shell_instance, too_long));
}

TEST(test_interactive_crlf_runs_once)
{
    setup_shell();
    ASSERT_EQ(MSH_OK,
              msh_register(&shell_instance, "go", NULL, recording_handler));
    type_bytes("go");
    msh_feed(&shell_instance, '\r');
    msh_feed(&shell_instance, '\n');
    ASSERT_EQ(1, handler_calls);
    ASSERT_STR_CONTAINS(output_buf, "> ");
}

TEST(test_interactive_lf_runs_once)
{
    setup_shell();
    ASSERT_EQ(MSH_OK,
              msh_register(&shell_instance, "go", NULL, recording_handler));
    type_bytes("go");
    msh_feed(&shell_instance, '\n');
    ASSERT_EQ(1, handler_calls);
}

TEST(test_interactive_overflow_rejected)
{
    size_t i;

    setup_shell();
    ASSERT_EQ(MSH_OK,
              msh_register(&shell_instance, "go", NULL, recording_handler));
    for (i = 0; i < (size_t)MSH_LINE_SIZE; ++i) {
        msh_feed(&shell_instance, 'x');
    }
    msh_feed(&shell_instance, '\r');
    ASSERT_EQ(0, handler_calls);
    ASSERT_STR_CONTAINS(output_buf, "input too long");
}

TEST(test_full_line_insert_at_zero_rejected_cleanly)
{
    size_t i;
    uint8_t old_length;

    setup_shell();
    for (i = 0; i < (size_t)(MSH_LINE_SIZE - 1); ++i) {
        msh_feed(&shell_instance, 'a');
    }
    old_length = shell_instance.length;
    while (shell_instance.cursor > 0u) {
        press_left();
    }
    reset_output();
    msh_feed(&shell_instance, 'b');
    ASSERT_EQ(old_length, shell_instance.length);
    ASSERT_EQ(0, shell_instance.cursor);
    ASSERT_TRUE(output_buf[0] == '\0');
}

TEST(test_delete_sequence_removes_char_at_cursor)
{
    setup_shell();
    type_bytes("abcd");
    press_left();
    press_left();
    press_delete();
    ASSERT_STR_EQ("abd", shell_instance.line);
}

TEST(test_unknown_csi_is_consumed)
{
    setup_shell();
    type_bytes("ab");
    msh_feed(&shell_instance, 27);
    msh_feed(&shell_instance, '[');
    msh_feed(&shell_instance, '9');
    msh_feed(&shell_instance, '~');
    ASSERT_STR_EQ("ab", shell_instance.line);
    ASSERT_EQ(2, shell_instance.length);
}

TEST(test_handler_not_found_result_is_reported)
{
    setup_shell();
    handler_result = MSH_ERR_NOT_FOUND;
    ASSERT_EQ(MSH_OK,
              msh_register(&shell_instance, "wrap", NULL, recording_handler));
    type_bytes("wrap");
    msh_feed(&shell_instance, '\r');
    ASSERT_EQ(1, handler_calls);
    ASSERT_STR_CONTAINS(output_buf, "error: -3");
}

#if MSH_ENABLE_HISTORY
TEST(test_history_wraps_and_recalls_latest)
{
    unsigned int i;

    setup_shell();
    ASSERT_EQ(MSH_OK,
              msh_register(&shell_instance, "cmd", NULL, recording_handler));
    for (i = 0u; i < (unsigned int)(MSH_HISTORY_DEPTH + 2u); ++i) {
        char line[16];
        snprintf(line, sizeof(line), "cmd %u", i);
        type_bytes(line);
        msh_feed(&shell_instance, '\r');
    }
    reset_handler_state();
    press_up();
    msh_feed(&shell_instance, '\r');
    ASSERT_EQ(1, handler_calls);
    ASSERT_STR_EQ("cmd", handler_argv[0]);
}
#endif

#if MSH_ENABLE_COMPLETE
TEST(test_completion_long_name_stays_bounded)
{
    static char long_name[MSH_LINE_SIZE];
    size_t i;

    setup_shell();
    for (i = 0; i < (sizeof(long_name) - 2u); ++i) {
        long_name[i] = 'a';
    }
    long_name[sizeof(long_name) - 2u] = 'z';
    long_name[sizeof(long_name) - 1u] = '\0';
    ASSERT_EQ(MSH_OK,
              msh_register(&shell_instance, long_name, NULL, recording_handler));
    type_bytes("aaa");
    msh_feed(&shell_instance, '\t');
    ASSERT_TRUE(shell_instance.length < MSH_LINE_SIZE);
    ASSERT_TRUE(shell_instance.line[shell_instance.length] == '\0');
}
#endif

TEST(test_help_uses_synchronous_output)
{
    setup_shell();
    ASSERT_EQ(MSH_OK,
              msh_register(&shell_instance, "echo", "desc", recording_handler));
    ASSERT_EQ(MSH_OK, msh_exec(&shell_instance, "help"));
    ASSERT_TRUE(last_print_ptr != NULL);
    ASSERT_STR_CONTAINS(output_buf, "Available commands:");
}

int main(void)
{
    printf("microsh unit tests\n");

    RUN_TEST(test_init_registers_help);
    RUN_TEST(test_register_rejects_invalid_names);
    RUN_TEST(test_register_rejects_duplicate_names);
    RUN_TEST(test_exec_preserves_empty_quoted_arg);
    RUN_TEST(test_exec_rejects_too_many_args);
    RUN_TEST(test_exec_rejects_unterminated_quote);
    RUN_TEST(test_exec_rejects_text_after_quote);
    RUN_TEST(test_exec_rejects_overlong_input);
    RUN_TEST(test_interactive_crlf_runs_once);
    RUN_TEST(test_interactive_lf_runs_once);
    RUN_TEST(test_interactive_overflow_rejected);
    RUN_TEST(test_full_line_insert_at_zero_rejected_cleanly);
    RUN_TEST(test_delete_sequence_removes_char_at_cursor);
    RUN_TEST(test_unknown_csi_is_consumed);
    RUN_TEST(test_handler_not_found_result_is_reported);
#if MSH_ENABLE_HISTORY
    RUN_TEST(test_history_wraps_and_recalls_latest);
#endif
#if MSH_ENABLE_COMPLETE
    RUN_TEST(test_completion_long_name_stays_bounded);
#endif
    RUN_TEST(test_help_uses_synchronous_output);

    printf("\nResults: %d/%d passed, %d failed\n", tests_passed, tests_run,
           tests_failed);
    return (tests_failed == 0) ? 0 : 1;
}
