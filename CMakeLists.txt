cmake_minimum_required(VERSION 3.21)

project(microsh VERSION 0.1.0 LANGUAGES C CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(MICROSH_BUILD_TESTS "Build microsh tests and consumer checks" OFF)

set(MSH_MAX_COMMANDS 16 CACHE STRING "Total command slots, including built-in help")
set(MSH_LINE_SIZE 128 CACHE STRING "Input line capacity including NUL")
set(MSH_MAX_ARGS 8 CACHE STRING "Maximum parsed argc")
set(MSH_ENABLE_HISTORY 1 CACHE STRING "Enable history: 0 or 1")
set(MSH_HISTORY_DEPTH 4 CACHE STRING "History depth when history is enabled")
set(MSH_ENABLE_COMPLETE 1 CACHE STRING "Enable completion: 0 or 1")

function(microsh_require_range name min max)
    if(NOT ${name} MATCHES "^-?[0-9]+$")
        message(FATAL_ERROR "${name} must be an integer")
    endif()
    if(${name} LESS ${min} OR ${name} GREATER ${max})
        message(FATAL_ERROR "${name} must be in range ${min}..${max}")
    endif()
endfunction()

microsh_require_range(MSH_MAX_COMMANDS 1 255)
microsh_require_range(MSH_LINE_SIZE 5 256)
microsh_require_range(MSH_MAX_ARGS 1 255)
microsh_require_range(MSH_ENABLE_HISTORY 0 1)
microsh_require_range(MSH_ENABLE_COMPLETE 0 1)
if(MSH_ENABLE_HISTORY)
    microsh_require_range(MSH_HISTORY_DEPTH 1 255)
endif()

add_library(microsh src/msh.c)
add_library(microsh::microsh ALIAS microsh)

target_compile_features(microsh PUBLIC c_std_99)
set_target_properties(microsh PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

target_include_directories(microsh
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(microsh
    PUBLIC
        MSH_MAX_COMMANDS=${MSH_MAX_COMMANDS}
        MSH_LINE_SIZE=${MSH_LINE_SIZE}
        MSH_MAX_ARGS=${MSH_MAX_ARGS}
        MSH_ENABLE_HISTORY=${MSH_ENABLE_HISTORY}
        MSH_HISTORY_DEPTH=${MSH_HISTORY_DEPTH}
        MSH_ENABLE_COMPLETE=${MSH_ENABLE_COMPLETE}
)

if(MSVC)
    target_compile_options(microsh PRIVATE /W4 /WX)
else()
    target_compile_options(microsh PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

install(TARGETS microsh
    EXPORT microshTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES include/msh.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT microshTargets
    FILE microshTargets.cmake
    NAMESPACE microsh::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/microsh
)

configure_package_config_file(
    cmake/microshConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/microshConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/microsh
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/microshConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/microshConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/microshConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/microsh
)

if(MICROSH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
