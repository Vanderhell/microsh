# Porting Guide

## Requirements

- A strict C99 compiler
- `<string.h>` functions used by the implementation
- `snprintf`
- A caller-supplied output function

## Serial Integration

### Correct pattern

1. Receive bytes from UART, RTT, USB CDC, or another byte stream.
2. If reception happens in an ISR, push bytes into a caller-owned queue or ring buffer.
3. Drain that queue from the shell-owning main loop or task.
4. Call `msh_feed()` from that one execution context.

### Incorrect pattern

Calling `msh_feed()` directly from an ISR, or from multiple unsynchronized contexts, is unsupported.

## Feature Combinations

Supported build combinations:

- history off, completion off
- history on, completion off
- history off, completion on
- history on, completion on

## CMake Consumer

```cmake
find_package(microsh CONFIG REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE microsh::microsh)
```

The exported CMake target propagates ABI-affecting compile definitions as `PUBLIC` usage requirements so downstream translation units use the same `msh_t` layout as the library.

## C++ Consumer

The public header is C++-compatible. Compile `src/msh.c` as C and link it into a C++ consumer; C++ support here means header compatibility and link compatibility, not compiling the implementation itself as C++.
