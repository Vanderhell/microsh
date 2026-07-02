# API Reference

Header:
`#include "msh.h"`

## Configuration

ABI-affecting macros must match across every translation unit that touches `msh_t` or links against a separately built library:

- `MSH_MAX_COMMANDS`
- `MSH_LINE_SIZE`
- `MSH_MAX_ARGS`
- `MSH_ENABLE_HISTORY`
- `MSH_HISTORY_DEPTH`
- `MSH_ENABLE_COMPLETE`

`MSH_MAX_COMMANDS` is the total table capacity, including the built-in `help` command. User capacity is `MSH_MAX_COMMANDS - 1`.

## Error Codes

- `MSH_OK`
- `MSH_ERR_NULL`
- `MSH_ERR_FULL`
- `MSH_ERR_NOT_FOUND`
- `MSH_ERR_ARGS`
- `MSH_ERR_INVALID`
- `MSH_ERR_INPUT_TOO_LONG`

## Callback Contracts

`msh_print_fn` is synchronous. The callback receives a borrowed NUL-terminated string and must consume or copy it before returning. The pointer may refer to stack-backed temporary storage. Output failures cannot be reported through the current API.

`msh_cmd_fn` receives borrowed `argv` storage that is valid only during the handler call. Copy any argument data needed after return.

## Functions

### `msh_init`

Initializes a caller-owned `msh_t`, sets prompt to `"> "`, enables echo, and registers the built-in `help` command. Initialization fails if `help` cannot be registered.

### `msh_set_prompt`

Sets a borrowed prompt pointer. Pass `NULL` to disable prompt output.

### `msh_set_echo`

Enables or disables local echo.

### `msh_register`

Registers a command by value in the shell table. The command name:

- must be unique
- must be non-empty
- must fit within `MSH_LINE_SIZE - 1`
- must be printable ASCII
- must not contain whitespace, quotes, or control characters

The `name` and `help` pointers remain caller-owned and must stay valid and unchanged while registered.

### `msh_feed`

Processes one byte of interactive input. Supported control handling:

- `CR`
- `LF`
- immediate `CRLF` as a single Enter
- backspace
- DEL as delete-before-cursor
- left/right arrows
- up/down history
- `ESC [ 3 ~` forward delete

Unsupported complete CSI sequences are consumed and ignored. Input is ASCII-oriented; there is no Unicode editing contract.

### `msh_exec`

Executes a full line without trailing newline characters. Overlong input is rejected with `MSH_ERR_INPUT_TOO_LONG`; it is not truncated.

### `msh_prompt`

Prints the current prompt if configured.

### `msh_command_count`

Returns the current registered command count, including built-in `help`.

### `msh_command_at`

Returns a pointer to a registered command descriptor for enumeration.
