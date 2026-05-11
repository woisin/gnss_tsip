cmake_minimum_required(VERSION 3.10)

# Cross-compilation configuration is only meaningful when this directory is the
# top-level project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(GNSS_DEFAULT_ZYNQ_COMPILER_DIR
        "/home/grand/cross-compiler/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin")

    option(GNSS_ENABLE_CROSS_AARCH64
        "Enable automatic ARM64 (aarch64) cross-compilation setup in auto mode"
        ON)

    set(GNSS_BUILD_TARGET "auto" CACHE STRING
        "Standalone build target: auto, x86_64, aarch64")
    set_property(CACHE GNSS_BUILD_TARGET PROPERTY STRINGS auto x86_64 aarch64)

    if(NOT GNSS_BUILD_TARGET STREQUAL "auto"
       AND NOT GNSS_BUILD_TARGET STREQUAL "x86_64"
       AND NOT GNSS_BUILD_TARGET STREQUAL "aarch64")
        message(FATAL_ERROR
            "Invalid GNSS_BUILD_TARGET='${GNSS_BUILD_TARGET}'. "
            "Use one of: auto, x86_64, aarch64")
    endif()

    set(GNSS_REQUIRE_AARCH64 OFF)
    if(GNSS_BUILD_TARGET STREQUAL "aarch64")
        set(GNSS_REQUIRE_AARCH64 ON)
    endif()

    set(GNSS_WANT_AARCH64 OFF)
    if(GNSS_BUILD_TARGET STREQUAL "aarch64")
        set(GNSS_WANT_AARCH64 ON)
    elseif(GNSS_BUILD_TARGET STREQUAL "auto" AND GNSS_ENABLE_CROSS_AARCH64)
        set(GNSS_WANT_AARCH64 ON)
    endif()

    if(GNSS_WANT_AARCH64 AND NOT DEFINED CMAKE_C_COMPILER)
        find_program(GNSS_AARCH64_CC
            NAMES aarch64-linux-gnu-gcc
            HINTS "${GNSS_DEFAULT_ZYNQ_COMPILER_DIR}")
        find_program(GNSS_AARCH64_CXX
            NAMES aarch64-linux-gnu-g++
            HINTS "${GNSS_DEFAULT_ZYNQ_COMPILER_DIR}")

        if(GNSS_AARCH64_CC)
            set(CMAKE_SYSTEM_NAME Linux CACHE STRING "" FORCE)
            set(CMAKE_SYSTEM_PROCESSOR aarch64 CACHE STRING "" FORCE)
            set(CMAKE_C_COMPILER "${GNSS_AARCH64_CC}" CACHE FILEPATH "" FORCE)
            if(GNSS_AARCH64_CXX)
                set(CMAKE_CXX_COMPILER "${GNSS_AARCH64_CXX}" CACHE FILEPATH "" FORCE)
            endif()
        elseif(GNSS_REQUIRE_AARCH64)
            message(FATAL_ERROR
                "GNSS_BUILD_TARGET=aarch64 but no aarch64 compiler was found. "
                "Install gcc-aarch64-linux-gnu or pass -DCMAKE_C_COMPILER=<aarch64-gcc>.")
        endif()
    endif()

    project(grand_gnss_tsip LANGUAGES C)

    set(GNSS_USING_AARCH64_COMPILER OFF)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$"
       OR CMAKE_C_COMPILER MATCHES "aarch64-linux-gnu-gcc")
        set(GNSS_USING_AARCH64_COMPILER ON)
    endif()

    if(GNSS_BUILD_TARGET STREQUAL "aarch64" AND NOT GNSS_USING_AARCH64_COMPILER)
        message(FATAL_ERROR
            "GNSS_BUILD_TARGET=aarch64 but selected compiler is not aarch64: ${CMAKE_C_COMPILER}")
    endif()

    if(GNSS_USING_AARCH64_COMPILER)
        message(STATUS "GNSS build target: ARM64 (aarch64)")
    else()
        message(STATUS "GNSS build target: native host (x86_64 or current host)")
    endif()
    message(STATUS "Using C compiler: ${CMAKE_C_COMPILER}")
else()
    project(grand_gnss_tsip LANGUAGES C)
endif()

add_library(grand_gnss_tsip STATIC
    src/gnss_tsip.c
)

target_include_directories(grand_gnss_tsip PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(gnss_set_overdetermined_position
    src/gnss_set_overdetermined_position.c
)

target_link_libraries(gnss_set_overdetermined_position
    grand_gnss_tsip
    m
)

install(TARGETS grand_gnss_tsip ARCHIVE DESTINATION lib)
install(TARGETS gnss_set_overdetermined_position RUNTIME DESTINATION bin)
