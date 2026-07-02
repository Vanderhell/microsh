# Design

## Goals

- Strict C99 implementation
- Fixed-size state and zero dynamic allocation
- Small API surface
- Deterministic memory use

## Input Model

`msh_feed()` owns one caller-provided `msh_t` line buffer and processes one byte at a time. Editing is intentionally limited to ASCII printable bytes and a small CSI subset:

- `ESC [ A`, `B`, `C`, `D`
- `ESC [ 3 ~`

This is enough for common ANSI/VT100-style serial terminals without attempting to implement a full terminal emulator.

## Parsing Model

The parser splits on spaces and tabs, preserves empty quoted arguments, and rejects:

- too many tokens
- unterminated quotes
- non-whitespace immediately after a closing quote

There is no shell expansion, escaping, globbing, pipeline, redirection, scripting, or variable interpolation.

## Ownership Model

- `msh_t` is caller-owned
- shell `ctx` is caller-owned
- prompt is caller-owned
- command names and help strings are caller-owned
- command argv storage is temporary borrowed storage during a handler call

## Execution Context Limits

- One `msh_t` instance must be owned by one execution context at a time.
- Shared-instance concurrency requires external synchronization.
- Recursive `msh_feed()` on the same instance from a print callback or handler is unsupported.
- ISR handlers should enqueue bytes and let the owning task or main loop call `msh_feed()`.

## Standard Library Requirements

`microsh` uses only standard C library facilities already typical in embedded or hosted C environments:

- `memcmp`/`memcpy`/`memset`-style byte operations via `<string.h>`
- `strcmp`/`strncmp`/`strchr`/`strlen`
- `snprintf`

There are no third-party runtime dependencies.
