# Porting Guide

`microsh` needs `msh.h`, `msh.c`, a C99 compiler, and one output callback.

## Platform Recipes

### STM32 HAL

```c
static void uart_print(const char *str, void *ctx) {
    (void)ctx;
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), 100);
}

static msh_t shell;

void shell_init(void) {
    msh_init(&shell, uart_print, NULL);
    msh_register(&shell, "reboot", "Restart", cmd_reboot);
    msh_prompt(&shell);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    msh_feed(&shell, rx_byte);
    HAL_UART_Receive_IT(huart, &rx_byte, 1);
}
```

### ESP32 UART

```c
static void esp_print(const char *str, void *ctx) {
    (void)ctx;
    uart_write_bytes(UART_NUM_0, str, strlen(str));
}

void shell_task(void *param) {
    msh_t shell;
    msh_init(&shell, esp_print, NULL);
    msh_prompt(&shell);

    uint8_t c;
    while (1) {
        if (uart_read_bytes(UART_NUM_0, &c, 1, portMAX_DELAY) == 1) {
            msh_feed(&shell, (char)c);
        }
    }
}
```

### Segger RTT

```c
#include "SEGGER_RTT.h"

static void rtt_print(const char *str, void *ctx) {
    (void)ctx;
    SEGGER_RTT_WriteString(0, str);
}

while (1) {
    char c;
    if (SEGGER_RTT_Read(0, &c, 1) == 1) {
        msh_feed(&shell, c);
    }
}
```

### Linux / POSIX

```c
#include <stdio.h>

static void stdout_print(const char *str, void *ctx) {
    (void)ctx;
    fputs(str, stdout);
    fflush(stdout);
}

while (1) {
    char c = getchar();
    msh_feed(&shell, c);
}
```

## CMake Example

```cmake
add_library(microsh STATIC src/msh.c)
target_include_directories(microsh PUBLIC include)

target_compile_definitions(microsh PUBLIC
    MSH_ENABLE_HISTORY=0
    MSH_ENABLE_COMPLETE=0
    MSH_MAX_COMMANDS=8
)
```

## Integration Checklist

1. Use a C99-capable compiler.
2. Provide an output callback for your transport.
3. Forward incoming bytes to `msh_feed()`.
4. Tune compile-time macros if RAM is constrained.
5. If input arrives in an ISR, drain it from one consumer context.
