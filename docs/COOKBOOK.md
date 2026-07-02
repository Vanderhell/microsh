# Cookbook

## 1. Minimal shell init

```c
msh_t shell;
msh_init(&shell, uart_print, NULL);
msh_prompt(&shell);
```

## 2. Registering commands

```c
msh_register(&shell, "status", "Show status", cmd_status);
```

## 3. Built-in help slot capacity

`MSH_MAX_COMMANDS` includes the built-in `help` command, so user capacity is `MSH_MAX_COMMANDS - 1`.

## 4. Command with ctx

`ctx` passed to `msh_init()` is forwarded to normal handlers.

## 5. Printing from a command

Use the platform output path owned by your application. `msh_print_fn` does not propagate output failures.

## 6. `msh_exec()` example

```c
int rc = msh_exec(&shell, "status");
```

## 7. Interactive `msh_feed()` loop

Drain bytes from the shell-owning loop or task and call `msh_feed()` one byte at a time.

## 8. CR, LF, and CRLF

Interactive input accepts `CR`, `LF`, and immediate `CRLF`. A `CRLF` pair is treated as one Enter.

## 9. History enabled or disabled

Set `MSH_ENABLE_HISTORY` to `0` or `1` consistently across all translation units.

## 10. Completion enabled or disabled

Set `MSH_ENABLE_COMPLETE` to `0` or `1` consistently across all translation units.

## 11. Parser quoting

- `set name "John Doe"`
- `set empty ""`
- `"x"tail` is rejected

## 12. Overflow handling

Interactive input beyond capacity is rejected, not truncated. Pressing Enter reports `MSH_ERR_INPUT_TOO_LONG` and clears the line.

## 13. ANSI editing assumption

Only a small ANSI/VT100-style CSI subset is supported.

## 14. ISR ring-buffer handoff

Queue bytes in the ISR and drain them in the owning loop before calling `msh_feed()`.

## 15. External synchronization

If multiple contexts can touch one shell instance, synchronize outside the library.

## 16. `find_package` consumer

See `tests/consumer_package/` for standalone CMake package consumers.

## 17. C++ consumer

See `tests/consumers/cpp_consumer.cpp`.

## 18. Minimum configuration

Smallest valid settings:

- `MSH_MAX_COMMANDS=1`
- `MSH_LINE_SIZE=5`
- `MSH_MAX_ARGS=1`

## 19. Maximum-boundary cautions

`MSH_LINE_SIZE` is limited to `256` because cursor and line length fields are byte-sized.

## 20. Abnormal termination

Power loss, resets, watchdog events, `abort`, or `longjmp` out of callbacks can leave terminal output incomplete. `microsh` does not provide durable execution or delivery guarantees.
