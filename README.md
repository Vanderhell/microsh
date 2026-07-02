# microsh

[![CI](https://github.com/Vanderhell/microsh/actions/workflows/ci.yml/badge.svg)](https://github.com/Vanderhell/microsh/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

`microsh` is a small embedded shell library for byte-stream consoles such as UART, RTT, USB CDC, and test harnesses. It is written in strict C99, uses no dynamic allocation, and has no third-party runtime dependencies.

The current repository state is documented in [docs/VERIFICATION.md](docs/VERIFICATION.md). Releases are tag-based only; see [CHANGELOG.md](CHANGELOG.md) and [.github/workflows/release.yml](.github/workflows/release.yml).

## Highlights

- Fixed-size command table with built-in `help`
- `argc`/`argv` command handlers with quoted-argument support
- Optional history and tab completion
- Deterministic RAM use configured at compile time
- CMake package export and install support
- C and C++ consumer examples

## Constraints

- `MSH_MAX_COMMANDS` is the total slot count, including built-in `help`
- User command capacity is `MSH_MAX_COMMANDS - 1`
- Input is ASCII-oriented; there is no Unicode line-editing claim
- `msh_feed()` is not ISR-safe or reentrant on a shared `msh_t`
- Output callbacks are synchronous and receive borrowed strings that may be stack-backed

## Minimal Example

```c
#include "msh.h"

static void uart_print(const char *str, void *ctx) {
    (void)ctx;
    uart_write_blocking(str);
}

static int cmd_status(int argc, const char *const *argv, void *ctx) {
    (void)argc;
    (void)argv;
    (void)ctx;
    uart_write_blocking("ok\r\n");
    return 0;
}

static msh_t shell;

void shell_init(void) {
    msh_init(&shell, uart_print, NULL);
    msh_register(&shell, "status", "Show status", cmd_status);
    msh_prompt(&shell);
}
```

## ISR Handoff Pattern

Do not call `msh_feed()` directly from an ISR. Push received bytes into a caller-owned ring buffer in the interrupt handler, then drain that buffer from the owning main loop or task:

```c
void uart_rx_isr(void) {
    ring_push(&rx_ring, uart_read_byte());
}

void shell_poll(void) {
    uint8_t byte;
    while (ring_pop(&rx_ring, &byte)) {
        msh_feed(&shell, (char)byte);
    }
}
```

## Build

### CMake

```sh
cmake -S . -B build -DMICROSH_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

### Makefile Test Path

```sh
cd tests
make
```

## Documentation

- [docs/API_REFERENCE.md](docs/API_REFERENCE.md)
- [docs/COOKBOOK.md](docs/COOKBOOK.md)
- [docs/DESIGN.md](docs/DESIGN.md)
- [docs/ISSUES.md](docs/ISSUES.md)
- [docs/PORTING_GUIDE.md](docs/PORTING_GUIDE.md)
- [docs/VERIFICATION.md](docs/VERIFICATION.md)
- [CONTRIBUTING.md](CONTRIBUTING.md)
- [SECURITY.md](SECURITY.md)
- [CHANGELOG.md](CHANGELOG.md)

## Status

This repository contains the implementation, tests, packaging metadata, CI definitions, and documentation for a small embedded utility library. Verification evidence and any remaining gaps are tracked in [docs/VERIFICATION.md](docs/VERIFICATION.md).
