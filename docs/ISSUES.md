# Issues and Troubleshooting

## No free command slots

`help` consumes one slot. User command capacity is `MSH_MAX_COMMANDS - 1`.

## Command registration fails

Registration rejects duplicate names, empty names, names with whitespace or quotes, names with control characters, and names that do not fit safely in the configured line buffer.

## Too many args

Input with more than `MSH_MAX_ARGS` tokens is rejected with `MSH_ERR_ARGS`.

## Unmatched quote

An unterminated quote returns `MSH_ERR_INVALID`.

## Closing quote followed by text

`"value"tail` is invalid and rejected.

## Overlong line rejected

Interactive lines longer than the buffer no longer execute a truncated prefix. They are rejected and cleared on Enter.

## CRLF prints twice

Immediate `CRLF` is handled as one Enter. If your terminal still shows odd output, inspect the transport layer for extra echoed bytes.

## Delete key inserts `~`

Only `ESC [ 3 ~` is treated as forward delete. Unsupported CSI sequences are consumed and ignored.

## Output callback retained a pointer

`msh_print_fn` receives borrowed storage. Copy the string before returning if you need to defer transmission.

## Handler retained argv

`argv` storage is temporary and valid only during the handler call.

## Recursive `msh_feed()`

Calling `msh_feed()` recursively on the same shell from a handler or print callback is unsupported.

## ISR direct feed

Queue the byte in the ISR and drain it later from the shell-owning loop or task.

## ABI config mismatch

Consumers and the library must use identical ABI-affecting macros. Use the exported CMake target or reference `MSH_ABI_GUARD_SYMBOL` in split builds.

## Feature-combination build issue

Build and test all four history/completion combinations; unsupported combinations indicate an integration or CI regression.

## Tool availability differences

Sanitizers, `clang-tidy`, `cppcheck`, or embedded cross-compilers may be unavailable on some hosts. Record those cases as `NOT VERIFIED`.
