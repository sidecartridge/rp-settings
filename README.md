# RP2040 and RP235x Microcontrollers Settings Tool

## Introduction

This tool is used to create and manage one or more settings sections in the FLASH memory of RP2040 and RP235x microcontrollers. The settings are stored in a dedicated area of the FLASH memory and can be read by the application at runtime.

## Installation

1. Copy the files `CMakeLists.txt`, `settings.h` and `settings.c` into the `/src` folder of your project. For better organization, you can create a folder called `settings` and put the files inside it, then reference them in your project’s `CMakeLists.txt`:

   ```
   +-- /src
   |   +-- /settings
   |   |   +-- CMakeLists.txt
   |   |   +-- settings.h
   |   |   +-- settings.c
   |   +-- CMakeLists.txt
   |   +-- main.c
   ```

2. In your main project `CMakeLists.txt`, add a subdirectory for the settings folder:

```cmake
   # Add the 'settings' subdirectory
   add_subdirectory(settings)

   # Then add the settings.c file to your build target
   target_sources(yourproject PRIVATE ${CMAKE_SOURCE_DIR}/settings/settings.c)
```

3. Modify your `target_link_libraries` to include the `settings` library, `pico_stdlib`, and `hardware_flash`:

```cmake
   target_link_libraries(yourproject PRIVATE
       pico_stdlib      # For core functionality
       hardware_flash   # For writing/erasing flash on RP2040
       settings         # Our settings library
   )
```

Ensure that you also have the **Pico SDK** properly set up, including `pico_stdlib` and `hardware_flash`.

## Usage

### Overview

The settings are stored in one (or more) dedicated region(s) of FLASH memory. Each region must be a multiple of 4096 bytes (the minimum erase/program size on RP2040). Each multiple of 4096-byte “page” can hold a settings context each(`SettingsConfigEntry` structures).

### Creating a Settings Context

The library provides a **context-based** API. You create a `SettingsContext` structure for each flash region you want to manage:

```c
static SettingsContext g_settingsCtx;
```

### Initialization

Before using any settings-related functions, you must initialize the context with the `settings_init` function. The parameters are:

- A pointer to the `SettingsContext` struct you declared (e.g., `&g_settingsCtx`).
- The start address offset of the settings space in the FLASH memory. The flash address start in the RP2040 and RP235x microcontrollers is always 0x10000000, so to simplify the configuration of the settings space, the user must provide the offset from the start address of the FLASH memory. The offset must be a multiple of 4096 bytes. Example: if the settings space starts at 0x10011000, the offset is 0x1000 bytes. It's also important to understand that the settings space should not overlap with the application code or any other data stored in the FLASH memory. It's a good idea to use the last pages of the FLASH memory to store the settings space to avoid any conflict with the application code.
- The size of the settings space in bytes. The size must be a multiple of 4096 bytes. The size of the settings space is the total amount of bytes that the settings library can use to store the settings. The size of the settings space must be greater than the sum of the size of all the settings that the user wants to store. Example: if the user wants to store 40 settings of 128 bytes each, the size of the settings space must be at least 40*128 = 5120 bytes = 2 pages = 2*4096 bytes = 8192 bytes. It's recommended to keep a margin for future growth of the settings space.
- The default settings entries: The user must provide an array of `SettingsConfigEntry` structures that define the default settings. of the application. Since it's not possible to add or remove any settings at runtime, the user must define all the settings that the application will use at compile time. An example of the `SettingsConfigEntry` structure is shown below:
- A 16-bit “magic” number and 16-bit “version” used to detect and validate stored settings.

- The `version number`: A 16 bit unsigned integer used to help to identify what version of the settings are stored in the FLASH memory.

**Important**: If the flash region has invalid or no settings stored, `settings_init` will copy your default entries into the context. If a valid magic number is found, it will load the existing settings from flash (overwriting defaults).

Example:

```c
// Default entries
SettingsConfigEntry entries[] = {
    {"TEST1", SETTINGS_TYPE_STRING, "TEST PARAM 1"},
    {"TEST2", SETTINGS_TYPE_BOOL,   "false"},
    {"TEST3", SETTINGS_TYPE_INT,    "60"},
    {"TEST4", SETTINGS_TYPE_STRING, "TEST PARAM 4"}
};

// The context you defined globally or statically
static SettingsContext g_settingsCtx;

// Initialize the context
int result = settings_init(
    &g_settingsCtx,
    entries,
    sizeof(entries) / sizeof(entries[0]),
    0x1E000,  // flash offset
    8192,     // total size, must be multiple of 4096
    0x1234,   // magic (16 bits)
    0x0001    // version (16 bits)
);

if (result < 0) {
    // Error handling
    printf("Error initializing settings: %d\n", result);
    // Possibly abort or handle error
}
```

### Writing Settings to Flash

The user can write the settings to the FLASH memory by calling the `settings_save` function. The function will write the settings to the FLASH memory and will return a status code. The status code can be one of the following:
- 0: The settings were saved successfully.
- Not 0: An error occurred while saving the settings. The error code is a negative number that indicates the type of error.

```c
int status = settings_save(&g_settingsCtx, true);
if (status != 0) {
    printf("Error saving settings: %d\n", status);
}
```

- The first parameter is the pointer to your context.
- The second parameter (`true` or `false`) indicates whether interrupts should be disabled during the flash write. Set to `true` to disable the interrupts and `false` to keep the interrupts enabled. 

**Tip**: We recommend saving the settings to the FLASH memory only when the user changes the settings to avoid too many writes to the flash. The settings are stored in the RAM memory and are lost when the power is removed. The settings are loaded from the FLASH memory when the application starts.

### Erasing Settings from Flash

The user can erase the settings from the FLASH memory by calling the `settings_erase` function. The function will erase the settings from the FLASH memory and will return a status code. The status code can be one of the following:
- 0: The settings were erased successfully.
- Not 0: An error occurred while erasing the settings. The error code is a negative number that indicates the type of error.

```c
int status = settings_erase(&g_settingsCtx);
if (status != 0) {
    printf("Error erasing settings: %d\n", status);
}
```

This physically erases your settings region in flash. If you want to use the settings library again after erasing, you must call `settings_init` again.

### Reading Settings

The user can read the settings from the FLASH memory by calling the `settings_find_entry` function. The function will read the settings from the FLASH memory and will return the `SettingsConfigEntry` or NULL. The function needs the key of the setting to read. The key must be an alphanumeric string of uppercase letters, numbers and underscores terminated by a null character.
```c
SettingsConfigEntry *entry = settings_find_entry(&g_settingsCtx, "TEST1");
if (entry != NULL) {
    printf("Key: %s\n", entry->key);
    printf("Type: %d\n", entry->dataType);
    printf("Value: %s\n", entry->value);
} else {
    printf("Setting not found\n");
}
```

### Changing Settings

You can change a setting’s value by calling one of the “put” functions:

- `settings_put_bool`: Changes the value of a boolean setting. The function needs the key of the setting and the new value of the setting. The value must be a string of characters terminated by a null character. The value must be "true" or "false", "TRUE" or "FALSE", "t" or "f", "T" or "F"
```c
  int status = settings_put_bool(&g_settingsCtx, "TEST2", true);
  if (status != 0) {
      printf("Error changing boolean setting: %d\n", status);
  }
```
- `settings_put_string`: Changes the value of a string setting. The function needs the key of the setting and the new value of the setting. The value must be a string of characters terminated by a null character.
```c
  int status = settings_put_string(&g_settingsCtx, "TEST1", "NEW VALUE");
  if (status != 0) {
      printf("Error changing string setting: %d\n", status);
  }
```
- `settings_put_integer`: Changes the value of an integer setting. The function needs the key of the setting and the new value of the setting. The value must be a string of characters terminated by a null character. The value must be a valid 32 bit integer.
```c
  int status = settings_put_integer(&g_settingsCtx, "TEST3", 100);
  if (status != 0) {
      printf("Error changing integer setting: %d\n", status);
  }
```

The settings library will handle the `type` of the setting as another parameter, and it's possible to change from one type to another. Once changed in RAM, you can call `settings_save` to persist them.

### Printing Settings

The user can print all the settings stored in the FLASH memory by calling the `settings_print` function. The function will print all the settings stored in the FLASH memory in the standard output. To print all settings in the context:

```c
// If you pass NULL as the buffer, it prints directly to stderr
settings_print(&g_settingsCtx, NULL);
```

You may provide a buffer to store the output if you do not want to print directly.

### Deinitialization

When you no longer need the settings library, or before re-initializing with different parameters, call:

```c
settings_deinit(&g_settingsCtx);
```

This frees any memory the library allocated for storing the settings in RAM.

## Example Project

The `/examples` folder contains a simple example project that shows how to use the settings library. The example project uses the `pico-sdk` and must be deployed in a Raspberry Pi Pico or a RP2040 or RP235x microcontroller. The example project is a command-line tool to manage the settings stored in the FLASH memory. The example project uses the UART interface to communicate with the user. The user can send commands to the application to manage the settings. The example project is a good starting point to understand how to use the settings library.

## Develop and Test

### Clang/LLVM

This project relies on LLVM/Clang for building and linting. Install LLVM on your platform:

- **MacOS**:
```bash
  brew install llvm

  echo 'export PATH="/usr/local/opt/llvm/bin:$PATH"' >> ~/.bash_profile
  echo 'export LDFLAGS="-L/usr/local/opt/llvm/lib"' >> ~/.bash_profile
  echo 'export CPPFLAGS="-I/usr/local/opt/llvm/include"' >> ~/.bash_profile
```

- **Ubuntu**:
```bash
  sudo apt-get install clang
```

- **Windows**:
  Download from the official LLVM website and install it.

## License

The source code of the project is licensed under the GNU General Public License v3.0. The full license is accessible in the [LICENSE](LICENSE) file. 

The project can also be licensed under a commercial license. Please contact the author for more information.