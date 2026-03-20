# API Reference

Header: `#include "msh.h"`  
Version: `1.0.0`

## Command Handler Signature

```c
typedef int (*msh_cmd_fn)(int argc, const char **argv, void *ctx);
```

`argc` is at least `1`, where `argv[0]` is the command name. Return `0` on success and a negative value on error.

## Output Callback

```c
typedef void (*msh_print_fn)(const char *str, void *ctx);
```

The callback receives NUL-terminated strings and can be wired to UART, RTT, USB CDC, or standard output.

## Functions

### `msh_init`

```c
msh_err_t msh_init(msh_t *sh, msh_print_fn print, void *ctx);
```

Initializes the shell instance and registers the built-in `help` command.

### `msh_register`

```c
msh_err_t msh_register(msh_t *sh, const char *name, const char *help, msh_cmd_fn handler);
```

Registers a command. `name` and `help` should point to static or otherwise long-lived strings.

### `msh_feed`

```c
void msh_feed(msh_t *sh, char c);
```

Processes one input byte. This is the primary entry point for UART RX handlers, polling loops, or console tasks.

### `msh_exec`

```c
int msh_exec(msh_t *sh, const char *line);
```

Executes a full command line programmatically. Useful for tests and scripted invocation.

### `msh_prompt`

```c
void msh_prompt(msh_t *sh);
```

Prints the current prompt, if one is configured.

### `msh_set_prompt`

```c
void msh_set_prompt(msh_t *sh, const char *prompt);
```

Updates the prompt string. Pass `NULL` to disable the prompt.

### `msh_set_echo`

```c
void msh_set_echo(msh_t *sh, bool echo);
```

Enables or disables echoing typed characters.

### `msh_command_count`

```c
uint8_t msh_command_count(const msh_t *sh);
```

Returns the number of currently registered commands.

### `msh_command_at`

```c
const msh_cmd_t *msh_command_at(const msh_t *sh, uint8_t index);
```

Returns a command descriptor for enumeration and inspection.

## Argument Parsing

Input is split on whitespace. Quoted strings are treated as single arguments.

```text
echo hello world        -> argc=3: "echo", "hello", "world"
set name "John Doe"     -> argc=3: "set", "name", "John Doe"
ping                    -> argc=1: "ping"
```

## Error Codes

| Code | Meaning |
|------|---------|
| `MSH_OK` | Success |
| `MSH_ERR_NULL` | NULL pointer argument |
| `MSH_ERR_FULL` | Command table full |
| `MSH_ERR_NOT_FOUND` | Command not found |
| `MSH_ERR_ARGS` | Wrong argument count |
| `MSH_ERR_INVALID` | Invalid input |

## Thread Safety

`microsh` is not thread-safe. If bytes arrive from an ISR, place them into a buffer and call `msh_feed()` from one execution context only.
