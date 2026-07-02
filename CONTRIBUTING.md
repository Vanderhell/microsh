# Contributing

## Scope

In scope:

- bug fixes
- tests
- documentation
- packaging and CI improvements

Out of scope:

- dynamic allocation
- scripting layers
- dynamic command storage
- threads or RTOS-specific synchronization inside the library
- filesystem, networking, authentication, or logging frameworks
- package-manager coupling
- cleanup/defer abstractions unrelated to the shell

## Rules

- Keep the implementation strict C99.
- Preserve zero dynamic allocation.
- Do not weaken tests to hide regressions.
- Do not claim verification that has not been run.
- Do not create tags or releases unless explicitly requested.
