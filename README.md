# microsh

[![CI](https://github.com/Vanderhell/microsh/actions/workflows/ci.yml/badge.svg)](https://github.com/Vanderhell/microsh/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
![C99](https://img.shields.io/badge/language-C99-blue.svg)
![Embedded](https://img.shields.io/badge/target-embedded%20systems-orange.svg)
![Zero allocation](https://img.shields.io/badge/memory-zero%20allocation-brightgreen.svg)
![No dependencies](https://img.shields.io/badge/dependencies-none-lightgrey.svg)

Minimal debug shell for embedded systems.

`microsh` is a small C99 shell library for UART, RTT, USB CDC, or any byte-stream console. It is designed for firmware teams that need interactive inspection and control without dynamic allocation, external dependencies, or heavyweight command frameworks.

## Highlights

- Register commands with a name, help text, and handler callback.
- Parse `argc/argv` style arguments, including quoted strings.
- Handle character-by-character input with line editing.
- Support command history and tab completion without heap allocation.
- Fit embedded workflows: deterministic memory, portable C99, UART-friendly.
- Include tests and documentation for API, design decisions, and porting.

## Example

```text
> help
Available commands:
  help         Show available commands
  status       Show device status
  conf         Get/set config values
  fsm          State machine info
  log          Set log level
  reboot       Restart the device

> conf get mqtt_port
mqtt_port = 8883

> fsm state
Current: ONLINE

> log level warn
Log level set to WARN

> reboot
Rebooting...
```

## Repository Layout

| Path | Purpose |
|------|---------|
| `include/msh.h` | Public API |
| `src/msh.c` | Reference implementation |
| `tests/test_all.c` | Unit and behavior tests |
| `docs/API_REFERENCE.md` | API reference |
| `docs/DESIGN.md` | Design rationale |
| `docs/PORTING_GUIDE.md` | Platform integration notes |

## Quick Start

### 1. Add the library

Copy `include/msh.h` and `src/msh.c` into your firmware project, or vendor the repository as a library.

### 2. Register commands

```c
#include "msh.h"

static int cmd_status(int argc, const char **argv, void *ctx) {
    (void)argc;
    (void)argv;
    device_t *dev = (device_t *)ctx;

    printf("State: %s, Uptime: %lu s\n",
           mfsm_state_name(&dev->fsm),
           dev->uptime_s);
    return 0;
}

static int cmd_reboot(int argc, const char **argv, void *ctx) {
    (void)argc;
    (void)argv;
    (void)ctx;
    NVIC_SystemReset();
    return 0;
}

static msh_t shell;

void shell_init(device_t *dev) {
    msh_init(&shell, uart_print, dev);
    msh_register(&shell, "status", "Show device status", cmd_status);
    msh_register(&shell, "reboot", "Restart the device", cmd_reboot);
    msh_prompt(&shell);
}
```

### 3. Feed bytes from your console

```c
void USART2_IRQHandler(void) {
    if (USART2->SR & USART_SR_RXNE) {
        char c = (char)(USART2->DR & 0xFF);
        msh_feed(&shell, c);
    }
}
```

### 4. Provide an output callback

```c
static void uart_print(const char *str, void *ctx) {
    (void)ctx;
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), 100);
}
```

## Build and Test

The project ships with a minimal test target in `tests/Makefile`.
The CI badge above tracks the same test build on GitHub Actions with both GCC and Clang on Ubuntu.

Example with GCC or Clang on a POSIX-like environment:

```sh
cd tests
make
```

Or directly:

```sh
cc -std=c99 -Wall -Wextra -Wpedantic -Werror -I../include ../src/msh.c test_all.c -o test_all
./test_all
```

## Configuration

| Macro | Default | Description |
|------|---------|-------------|
| `MSH_MAX_COMMANDS` | `16` | Maximum registered commands |
| `MSH_LINE_SIZE` | `128` | Maximum input line length |
| `MSH_MAX_ARGS` | `8` | Maximum parsed arguments |
| `MSH_ENABLE_HISTORY` | `1` | Enable history navigation |
| `MSH_HISTORY_DEPTH` | `4` | Number of stored history entries |
| `MSH_ENABLE_COMPLETE` | `1` | Enable tab completion |

## API Overview

| Function | Description |
|------|-------------|
| `msh_init` | Initialize the shell and register built-in `help` |
| `msh_register` | Register a command |
| `msh_feed` | Process one incoming byte |
| `msh_exec` | Execute a line programmatically |
| `msh_prompt` | Print the configured prompt |
| `msh_set_prompt` | Change the prompt string |
| `msh_set_echo` | Enable or disable echo |
| `msh_command_count` | Return the number of registered commands |
| `msh_command_at` | Enumerate commands |

Full reference: [docs/API_REFERENCE.md](docs/API_REFERENCE.md)

## Documentation

| Document | Content |
|------|---------|
| [API Reference](docs/API_REFERENCE.md) | Public API and error codes |
| [Design Rationale](docs/DESIGN.md) | Core implementation tradeoffs |
| [Porting Guide](docs/PORTING_GUIDE.md) | Integration patterns for common targets |
| [Contributing Guide](CONTRIBUTING.md) | Scope and contribution expectations |
| [Changelog](CHANGELOG.md) | Release history |

## Ecosystem Integration

`microsh` is intended to pair well with small embedded runtime libraries:

| Library | Shell command | What it exposes |
|------|--------------|-----------------|
| [microfsm](https://github.com/Vanderhell/microfsm) | `fsm state` | Current state and transitions |
| [microres](https://github.com/Vanderhell/microres) | `breaker status` | Breaker state and reset hooks |
| [microconf](https://github.com/Vanderhell/microconf) | `conf get/set` | Runtime config inspection and mutation |
| [microlog](https://github.com/Vanderhell/microlog) | `log level` | Runtime log level control |

## Project Status

`microsh` is positioned as a small, production-oriented embedded utility library. The repository includes source, tests, changelog, contribution notes, and implementation docs suitable for an initial public GitHub release.

## License

Released under the MIT License. Copyright (c) 2026 Vanderhell.
See [LICENSE](LICENSE).
