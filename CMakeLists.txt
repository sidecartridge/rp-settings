# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.26)

# Add the env variable PICO_SDK_PATH if it does not exist
if(NOT DEFINED ENV{PICO_SDK_PATH})
    set(ENV{PICO_SDK_PATH} ./pico-sdk)
endif()
# Show the value
message("PICO_SDK_PATH: $ENV{PICO_SDK_PATH}")

# Include build functions from Pico SDK
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

# Set name of project (as PROJECT_NAME) and C/C   standards
project(settings C CXX)
set(CMAKE_C_STANDARD 11)

# Include the Pico SDK (specific for Raspberry Pi Pico projects)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# if PICOTOOL_OVERRIDE_DIR system environment variable is set,
# then use that as the folder for the picotool executable
if (DEFINED ENV{PICOTOOL_OVERRIDE_DIR})
    message("PICOTOOL_OVERRIDE_DIR env var is set to '$ENV{PICOTOOL_OVERRIDE_DIR}'")
    add_executable(picotool IMPORTED GLOBAL)
    set_property(TARGET picotool PROPERTY IMPORTED_LOCATION $ENV{PICOTOOL_OVERRIDE_DIR}/picotool)
    # check the picotool version:
    execute_process(COMMAND $ENV{PICOTOOL_OVERRIDE_DIR}/picotool version
                    OUTPUT_VARIABLE PICOTOOL_VERSION
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX MATCH "^picotool v${picotool_VERSION_REQUIRED}" PICOTOOL_VERSION_MATCH ${PICOTOOL_VERSION})
    if (NOT PICOTOOL_VERSION_MATCH)
        message("picotool version response was: ${PICOTOOL_VERSION}")
        message(FATAL_ERROR "PICOTOOL_OVERRIDE_DIR is set to '$ENV{PICOTOOL_OVERRIDE_DIR}', but the version of picotool found is not ${picotool_VERSION_REQUIRED}")
    endif()
endif ()

# Initialize the SDK
pico_sdk_init()

# Add subdirectories
add_subdirectory(src)      # Source directory containing the core library or settings
add_subdirectory(examples) # Example programs that demonstrate usage of the src files

# Global flags (optimization, warnings, etc.)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections -Wall")

# Enable clang-tidy (you need to have clang-tidy installed on your system)
find_program(CLANG_TIDY_EXE NAMES clang-tidy)

if(CLANG_TIDY_EXE)
    set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}")
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")
else()
    message(WARNING "clang-tidy not found, skipping linting")
endif()

# Find clang-format executable
find_program(CLANG_FORMAT_BIN NAMES clang-format)

if(CLANG_FORMAT_BIN)
    message(STATUS "Found clang-format: ${CLANG_FORMAT_BIN}")

    # Add a custom target to run clang-format on all source files
    file(GLOB_RECURSE ALL_CXX_SOURCE_FILES
        "${CMAKE_SOURCE_DIR}/src/*.c"
        "${CMAKE_SOURCE_DIR}/src/*.h"
        "${CMAKE_SOURCE_DIR}/examples/*.c"
    )

    add_custom_target(clang-format
        COMMAND ${CLANG_FORMAT_BIN}
        -i
        -style=file
        ${ALL_CXX_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format on the source code..."
        VERBATIM
    )

else()
    message(WARNING "clang-format not found!")
endif()