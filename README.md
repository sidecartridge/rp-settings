# RP2040 and RP235x Microcontrollers settings tool

## Introduction

This tool is used to create and manage a settings section in the FLASH memory of the RP2040 and RP235x microcontrollers. The settings are stored in a specific section of the FLASH memory and can be read by the application at runtime. 

## Installation

Copy the files `CMakeLists.txt`,  `settings.h` and `settings.c` inside the `/src` folder of your project. For better organization, you can create a folder called `settings` and put the files inside it and reference them in the `CMakeLists.txt` of your own project:

```
+-- /src
|   +-- /settings
|   |   +-- CMakeLists.txt
|   |   +-- settings.h
|   |   +-- settings.c
|   +-- CMakeLists.txt
|   +-- main.c
```

Add the following lines to the `CMakeLists.txt` file of your project:

```cmake
# Add subdirectories
add_subdirectory(settings)

target_sources(yourproject PRIVATE  ${CMAKE_SOURCE_DIR}/settings/settings.c)
```

And modify the `target_link_libraries` to include the `settings` library. You must also include the `pico_stdlib` and the `hardware_flash` libraries in the `pico-sdk`.

Example:

```cmake
target_link_libraries(yourproject PRIVATE
        pico_stdlib              # for core functionality
        hardware_flash # Specific hardware flash library
        settings                # for settings library
        )
```

## Usage

### Overview
The settings are stored in a specific section of the FLASH memory. The settings space can be a multiple of 4096 bytes -the size of a page in the FLASH memory-. Each page can store 32 settings of 128 bytes each. Each setting is divided in three parts:
- the key (30 bytes). An alphanumeric string of uppercase letters, numbers and underscores terminated by a null character.
- the value (96 bytes). A string of characters terminated by a null character.
- the type (2 bytes). A 16 bit unsigned integer that indicates the type of the value. The following types are supported:
  - Integer
  - Boolean 
  - String

It's possible to have more than 4096 bytes of settings space. In this case, the settings are stored in multiple pages, being transparent to the user

### Initialization
The settings library must be initialized ALWAYS before using it. The initialization process needs the following parameters:

- The start address offset of the settings space in the FLASH memory. The flash address start in the RP2040 and RP235x microcontrollers is always 0x10000000, so to simplify the configuration of the settings space, the user must provide the offset from the start address of the FLASH memory. The offset must be a multiple of 4096 bytes. Example: if the settings space starts at 0x10011000, the offset is 0x1000 bytes. It's also important to understand that the settings space should not overlap with the application code or any other data stored in the FLASH memory. It's a good idea to use the last pages of the FLASH memory to store the settings space to avoid any conflict with the application code.
- The size of the settings space in bytes. The size must be a multiple of 4096 bytes. The size of the settings space is the total amount of bytes that the settings library can use to store the settings. The size of the settings space must be greater than the sum of the size of all the settings that the user wants to store. Example: if the user wants to store 40 settings of 128 bytes each, the size of the settings space must be at least 40*128 = 5120 bytes = 2 pages = 2*4096 bytes = 8192 bytes. It's recommended to keep a margin for future growth of the settings space.
- The default settings entries: The user must provide an array of `SettingsConfigEntry` structures that define the default settings. of the application. Since it's not possible to add or remove any settings at runtime, the user must define all the settings that the application will use at compile time. An example of the `SettingsConfigEntry` structure is shown below:

```c
  SettingsConfigEntry entries[] = {
      {"TEST1", SETTINGS_TYPE_STRING, "TEST PARAM 1"},
      {"TEST2", SETTINGS_TYPE_BOOL, "false"},
      {"TEST3", SETTINGS_TYPE_INT, "60"},
      {"TEST4", SETTINGS_TYPE_STRING, "TEST PARAM 4"}};
```

It's possible to add more settings in the future by modifying the `SettingsConfigEntry` array and recompiling the application. The settings library will take care of the rest.
- The number of entries in the `SettingsConfigEntry` array.
- The `magic numer`: A 16 bit unsigned integer used to help to identify what version of the settings are stored in the FLASH memory.
- The `version number`: A 16 bit unsigned integer used to help to identify what version of the settings are stored in the FLASH memory.

To initialize the settings library, the user must call the `settings_init` function with the parameters described above. If the initialization process fails, the application will abort. The function will check the different parameters for errors and will write the errors in the standard output.

```c
  SettingsConfigEntry entries[] = {
      {"TEST1", SETTINGS_TYPE_STRING, "TEST PARAM 1"},
      {"TEST2", SETTINGS_TYPE_BOOL, "false"},
      {"TEST3", SETTINGS_TYPE_INT, "60"},
      {"TEST4", SETTINGS_TYPE_STRING, "TEST PARAM 4"}};

  settings_init(entries, sizeof(entries) / sizeof(entries[0]), 0x1E000, 8192, 0x1234, 0x0001);
```

### Writing settings to the FLASH memory

The user can write the settings to the FLASH memory by calling the `settings_save` function. The function will write the settings to the FLASH memory and will return a status code. The status code can be one of the following:
- 0: The settings were saved successfully.
- Not 0: An error occurred while saving the settings. The error code is a negative number that indicates the type of error.

```c
  int status = settings_save();
  if (status != 0) {
    printf("Error saving settings: %d\n", status);
  }
```

We recommend saving the settings to the FLASH memory only when the user changes the settings to avoid too many writes to the flash. The settings are stored in the RAM memory and are lost when the power is removed. The settings are loaded from the FLASH memory when the application starts.

### Erase settings from the FLASH memory

The user can erase the settings from the FLASH memory by calling the `settings_erase` function. The function will erase the settings from the FLASH memory and will return a status code. The status code can be one of the following:
- 0: The settings were erased successfully.
- Not 0: An error occurred while erasing the settings. The error code is a negative number that indicates the type of error.

```c
  int status = settings_erase();
  if (status != 0) {
    printf("Error erasing settings: %d\n", status);
  }
```

After erasing the settings from the FLASH memory, the settings library will need to be initialized again before using it with the `settings_init` function.

### Reading settings from the FLASH memory

The user can read the settings from the FLASH memory by calling the `settings_find_entry` function. The function will read the settings from the FLASH memory and will return the `SettingsConfigEntry` or NULL. The function needs the key of the setting to read. The key must be an alphanumeric string of uppercase letters, numbers and underscores terminated by a null character.

```c
    SettingsConfigEntry *entry = settings_find_entry("TEST1");
    if (entry != NULL) {
        printf("Key: %s\n", entry->key);
        printf("Type: %d\n", entry->type);
        printf("Value: %s\n", entry->value);
    } else {
        printf("Setting not found\n");
    }
```

### Changing settings

The user can change the settings by three different functions depending on the type of the setting:

- `settings_put_bool`: Changes the value of a boolean setting. The function needs the key of the setting and the new value of the setting. The value must be a string of characters terminated by a null character. The value must be "true" or "false", "TRUE" or "FALSE", "t" or "f", "T" or "F"
```c
    int status = settings_put_bool("TEST2", "true");
    if (status != 0) {
        printf("Error changing setting: %d\n", status);
    }
```

- `settings_put_string`: Changes the value of a string setting. The function needs the key of the setting and the new value of the setting. The value must be a string of characters terminated by a null character.
```c
    int status = settings_put_string("TEST1", "NEW VALUE");
    if (status != 0) {
        printf("Error changing setting: %d\n", status);
    }
```

- `settings_put_integer`: Changes the value of an integer setting. The function needs the key of the setting and the new value of the setting. The value must be a string of characters terminated by a null character. The value must be a valid 32 bit integer.
```c
    int status = settings_put_integer("TEST3", "100");
    if (status != 0) {
        printf("Error changing setting: %d\n", status);
    }
```

The settings library will handle the `type` of the setting as another parameter, and it's possible to change from one type to another. 

### Print settings

The user can print all the settings stored in the FLASH memory by calling the `settings_print` function. The function will print all the settings stored in the FLASH memory in the standard output.

```c
    settings_print();
```

## Example project

The `/examples` folder contains a simple example project that shows how to use the settings library. The example project uses the `pico-sdk` and must be deployed in a Raspberry Pi Pico or a RP2040 or RP235x microcontroller. The example project is a command-line tool to manage the settings stored in the FLASH memory. The example project uses the UART interface to communicate with the user. The user can send commands to the application to manage the settings. The example project is a good starting point to understand how to use the settings library.

## Develop and test

### CLANG

The project relies on the LLVM libraries to compile and check the code. 

### Install LLVM project:

#### MacOS
```bash
brew install llvm

echo 'export PATH="/usr/local/opt/llvm/bin:$PATH"' >> ~/.bash_profile
echo 'export LDFLAGS="-L/usr/local/opt/llvm/lib"' >> ~/.bash_profile
echo 'export CPPFLAGS="-I/usr/local/opt/llvm/include"' >> ~/.bash_profile

```

#### Ubuntu
```bash
sudo apt-get install clang
```

#### Windows
Download the LLVM project from the official website and install it.

## Licenses

The source code of the project is licensed under the GNU General Public License v3.0. The full license is accessible in the [LICENSE](LICENSE) file. 

The project can also be licensed under a commercial license. Please contact the author for more information.