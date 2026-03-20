# Changelog

## [1.0.0] - 2026-03-20

### Added

- Command registration with name, help text, and handler callback.
- Argument parsing with whitespace splitting, quoted strings, and extra-space handling.
- Character-by-character input processing via `msh_feed()`.
- Line editing with backspace, delete, and left/right arrow cursor movement.
- Command history with up/down arrows, ring buffer storage, and duplicate suppression.
- Tab completion with single-match completion and multi-match listing.
- Built-in `help` command generated from registered commands.
- Programmatic execution via `msh_exec()` for testing and scripted input.
- ANSI escape sequence handling for common arrow key navigation.
- Configurable prompt and echo control.
- Full repository documentation: API reference, design rationale, and porting guide.
- Test suite covering command registration, execution, editing, history, completion, and error handling.
- Platform integration recipes for STM32, ESP32, Segger RTT, and Linux/POSIX.
