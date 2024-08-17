# Create a library for the settings files
add_library(settings
    settings.c
)

# Add the include directory (for settings.h)
target_include_directories(settings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link necessary libraries
target_link_libraries(settings
    pico_stdlib    # Core Pico SDK library
    hardware_flash # Specific hardware flash library
)
