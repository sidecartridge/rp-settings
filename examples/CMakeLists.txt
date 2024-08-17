# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Include build functions from Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Set name of project (as PROJECT_NAME) and C/C   standards
project(examples C CXX ASM)
set(CMAKE_C_STANDARD 11)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

# Tell CMake where to find the executable source file
add_executable(${PROJECT_NAME} 
        main.c
)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_sources(${PROJECT_NAME} PRIVATE  ${CMAKE_SOURCE_DIR}/src/settings.c)

# Link to necessary libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
        pico_stdlib              # for core functionality
        hardware_flash # Specific hardware flash library
        settings                # for settings library
        )


# Use reduced space for ROMs
pico_set_binary_type(${PROJECT_NAME} copy_to_ram)

# Fetch the values from the environment variables
set(RELEASE_VERSION $ENV{RELEASE_VERSION})
set(RELEASE_DATE $ENV{RELEASE_DATE})
set(_DEBUG $ENV{DEBUG_MODE})

# If the environment variables are not set, use default values
if(NOT RELEASE_VERSION)

    # Determine the file name based on RELEASE_TYPE
    set(VERSION_FILE_NAME "version.txt")
    
    # Remove newline character if present
    string(STRIP "${RELEASE_VERSION}" RELEASE_VERSION)

    # In case version.txt is empty or does not exist, use a default version
    if("${RELEASE_VERSION}" STREQUAL "")
        set(RELEASE_VERSION "v0.0.1dev")
    endif()
endif()

if(NOT RELEASE_DATE)
        string(TIMESTAMP CURRENT_DATE_TIME "%Y-%m-%d %H:%M:%S")
        set(RELEASE_DATE ${CURRENT_DATE_TIME})
endif()

if (NOT _DEBUG)
        set(_DEBUG 1)
endif()


# Debug outputs
pico_enable_stdio_usb(${PROJECT_NAME} 0)
# Workaround to disable USB output in release builds
if(${_DEBUG} STREQUAL "0")
    pico_enable_stdio_uart(${PROJECT_NAME} 0)
else()
    pico_enable_stdio_uart(${PROJECT_NAME} 1)
endif()

# Print the variables on screen
message("RELEASE_VERSION: " ${RELEASE_VERSION})
message("RELEASE_DATE: " ${RELEASE_DATE})
message("DEBUG_MODE: " ${_DEBUG})

# Pass these values to the C compiler
add_definitions(-DRELEASE_VERSION="${RELEASE_VERSION}")
add_definitions(-DRELEASE_DATE="${RELEASE_DATE}")

# Pass the _DEBUG flag to the C compiler
add_definitions(-D_DEBUG=${_DEBUG})

# Optimization Flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -ffunction-sections -fdata-sections")
