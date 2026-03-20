# Design Rationale

## 1. Character-by-character input

Decision: `msh_feed()` processes one byte at a time instead of receiving full lines.

Why: UART interfaces naturally deliver bytes incrementally. Handling input at byte granularity allows line editing, history navigation, and tab completion without requiring the application to build a separate line buffer first.

## 2. Static command table

Decision: Commands are stored in a fixed-size array.

Why: This keeps the implementation deterministic, allocation-free, and small. For the intended command counts, linear lookup is acceptable and simpler than dynamic structures.

## 3. `argc/argv` command interface

Decision: Command handlers use `(int argc, const char **argv, void *ctx)`.

Why: The shape is familiar to C developers, supports subcommands naturally, and keeps the public API minimal.

## 4. Built-in help registration

Decision: `msh_init()` automatically registers a `help` command.

Why: Every shell benefits from discoverability. Auto-registering help keeps the developer experience consistent and avoids documentation drift between code and command listings.

## 5. History and completion as compile-time features

Decision: History and tab completion are enabled by default, but can be disabled with `MSH_ENABLE_HISTORY=0` and `MSH_ENABLE_COMPLETE=0`.

Why: These features are useful on most targets, but some constrained systems may need the RAM back. Compile-time switches remove both storage and code paths.

## 6. Quoted string support

Decision: Double-quoted spans are parsed as a single argument.

Why: Embedded configuration values often contain spaces, such as SSIDs or labels. Quoted parsing improves usability without introducing a heavy parser.

## Summary

| Decision | Gains | Costs |
|----------|-------|-------|
| Byte-wise input | Real-time editing and immediate processing | Slightly more stateful implementation |
| Static command table | Deterministic memory and zero allocation | Fixed command capacity |
| `argc/argv` API | Familiar and flexible | No typed argument schema |
| Auto `help` | Better discoverability | Consumes one command slot |
| Compile-time features | Easy RAM and code-size trimming | Requires rebuild to change behavior |
| Quoted strings | Better UX for values with spaces | Slightly more parsing logic |
