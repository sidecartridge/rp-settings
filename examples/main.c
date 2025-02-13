/**
 * File: main.c
 * Author: Diego Parrilla Santamaría
 * Date: July 2023
 * Copyright: 2023-25 - GOODDATA LABS SL
 * Description: C file that launches the ROM emulator or configurator
 */

#include <ctype.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>

#include "settings.h"

// Maximum buffer size for command input
enum { INPUT_BUFFER_SIZE = 128 };

enum { TIMEOUT_DURATION_US = 1000, VALUE_STR_SIZE = 8, COMMAND_SIZE = 64 };

enum {
  SETTINGS_ADDRESS = 0x1FF000,
  BUFFER_SIZE = 4096,
  MAGIC_NUMBER = 0x1234,
  VERSION_NUMBER = 0x0001
};

// Create a global (or static) context for our settings
static SettingsContext g_settingsCtx;

// Command lookup structure, now with argument support
typedef struct {
  const char *command;
  void (*handler)(const char *arg);
} Command;

// Forward declarations for command handlers
void cmdHelp(const char *arg);
void cmdPrint(const char *arg);
void cmdSave(const char *arg);
void cmdErase(const char *arg);
void cmdGet(const char *arg);
void cmdPutInt(const char *arg);
void cmdPutBool(const char *arg);
void cmdPutString(const char *arg);
void cmdUnknown(const char *arg);

// Command table
const Command commands[] = {
    {"help", cmdHelp},        {"print", cmdPrint},
    {"save", cmdSave},        {"erase", cmdErase},
    {"get", cmdGet},          {"put_int", cmdPutInt},
    {"put_bool", cmdPutBool}, {"put_string", cmdPutString}};

// Number of commands in the table
const size_t numCommands = sizeof(commands) / sizeof(commands[0]);

// --------------------------------------------------------------------
// Command Handlers
// --------------------------------------------------------------------

void cmdHelp(const char *arg) {
  DPRINTF("Available commands:\n");
  DPRINTF("  help         - Show available commands\n");
  DPRINTF("  print        - Show settings\n");
  DPRINTF("  save         - Save settings\n");
  DPRINTF("  erase        - Erase settings\n");
  DPRINTF("  get          - Get a setting (requires a key)\n");
  DPRINTF(
      "  put_int      - Set an integer setting (requires a key and value)\n");
  DPRINTF(
      "  put_bool     - Set a boolean setting (requires a key and value)\n");
  DPRINTF("  put_string   - Set a string setting (requires a key and value)\n");
}

void cmdPrint(const char *arg) {
  // Print all settings using our global context, passing NULL as buffer
  settings_print(&g_settingsCtx, NULL);
}

void cmdSave(const char *arg) {
  // Save settings to flash (disable interrupts during write)
  settings_save(&g_settingsCtx, true);
}

void cmdErase(const char *arg) {
  settings_erase(&g_settingsCtx);
  DPRINTF("Settings erased.\n");
}

void cmdGet(const char *arg) {
  if (arg && strlen(arg) > 0) {
    // Look up the given key in the global context
    SettingsConfigEntry *entry = settings_find_entry(&g_settingsCtx, arg);
    if (entry != NULL) {
      DPRINTF("Key: %s, Value: %s\n", entry->key, entry->value);
    } else {
      DPRINTF("Key '%s' not found.\n", arg);
    }
  } else {
    DPRINTF("No key provided for 'get' command.\n");
  }
}

void cmdPutInt(const char *arg) {
  char key[SETTINGS_MAX_KEY_LENGTH] = {0};
  int value = 0;

  if (sscanf(arg, "%s %d", key, &value) == 2) {
    int err = settings_put_integer(&g_settingsCtx, key, value);
    if (err == 0) {
      DPRINTF("Key: %s, Value: %d\n", key, value);
    }
  } else {
    DPRINTF(
        "Invalid arguments for 'put_int' command. Usage: put_int <key> "
        "<value>\n");
  }
}

void cmdPutBool(const char *arg) {
  char key[SETTINGS_MAX_KEY_LENGTH] = {0};
  char valueStr[VALUE_STR_SIZE] = {0};  // e.g., "true", "false"
  bool value;

  if (sscanf(arg, "%s %7s", key, valueStr) == 2) {
    // Convert the valueStr to lowercase
    for (int i = 0; valueStr[i]; i++) {
      valueStr[i] = (char)tolower((unsigned char)valueStr[i]);
    }

    // Interpret the boolean value
    if (strcmp(valueStr, "true") == 0 || strcmp(valueStr, "t") == 0 ||
        strcmp(valueStr, "1") == 0) {
      value = true;
    } else if (strcmp(valueStr, "false") == 0 || strcmp(valueStr, "f") == 0 ||
               strcmp(valueStr, "0") == 0) {
      value = false;
    } else {
      DPRINTF(
          "Invalid boolean value. Use 'true', 'false', 't', 'f', '1', or "
          "'0'.\n");
      return;
    }

    // Store the boolean value
    int err = settings_put_bool(&g_settingsCtx, key, value);
    if (err == 0) {
      DPRINTF("Key: %s, Value: %s\n", key, value ? "true" : "false");
    }
  } else {
    DPRINTF(
        "Invalid arguments for 'put_bool' command. Usage: put_bool <key> "
        "<true/false>\n");
  }
}

void cmdPutString(const char *arg) {
  char key[SETTINGS_MAX_KEY_LENGTH] = {0};

  // Scan the first word (the key)
  if (sscanf(arg, "%s", key) == 1) {
    // The rest of 'arg' after the first space is the value
    const char *value = strchr(arg, ' ');
    if (value) {
      // Move past the space
      value++;
    }

    // If value is NULL or empty string, treat as an empty string
    if (!value || strlen(value) == 0) {
      int err = settings_put_string(&g_settingsCtx, key, "");
      if (err == 0) {
        DPRINTF("Key: %s, Value: <EMPTY>\n", key);
      }
    } else {
      int err = settings_put_string(&g_settingsCtx, key, value);
      if (err == 0) {
        DPRINTF("Key: %s, Value: %s\n", key, value);
      }
    }
  } else {
    DPRINTF(
        "Invalid arguments for 'put_string' command. Usage: put_string <key> "
        "<value>\n");
  }
}

void cmdUnknown(const char *arg) {
  DPRINTF("Unknown command. Type 'help' for a list of commands.\n");
}

// --------------------------------------------------------------------
// Function to find and execute a command, with optional argument
// --------------------------------------------------------------------
void processCommand(const char *input) {
  // Split the input into command and argument
  char command[COMMAND_SIZE] = {0};
  char arg[COMMAND_SIZE] = {0};
  sscanf(input, "%63s %63[^\n]", command, arg);  // Splits at first space

  for (size_t i = 0; i < numCommands; i++) {
    if (strcmp(command, commands[i].command) == 0) {
      commands[i].handler(arg);  // Pass the argument to the handler
      return;
    }
  }
  cmdUnknown(NULL);  // If no command matches
}

// --------------------------------------------------------------------
// main()
// --------------------------------------------------------------------
int main() {
  stdio_init_all();                  // Initialize standard I/O for USB or UART
  setvbuf(stdout, NULL, _IONBF, 1);  // Make stdout unbuffered

  // Buffer to store user input
  char inputBuffer[INPUT_BUFFER_SIZE];
  size_t inputPos = 0;  // Current position in the buffer

  DPRINTF("RP - Settings CLI Tool\n");
  DPRINTF("Type 'help' for a list of commands.\n");

  // Define your default entries
  SettingsConfigEntry entries[] = {
      {"TEST1", SETTINGS_TYPE_STRING, "TEST PARAM 1"},
      {"TEST2", SETTINGS_TYPE_BOOL, "false"},
      {"TEST3", SETTINGS_TYPE_INT, "60"},
      {"TEST4", SETTINGS_TYPE_STRING, "TEST PARAM 4"}};
  size_t numDefaultEntries = sizeof(entries) / sizeof(entries[0]);

  // Initialize the global context with these defaults
  settings_init(&g_settingsCtx,  // Context pointer
                entries,         // Default entries
                (uint16_t)numDefaultEntries,
                SETTINGS_ADDRESS,  // Flash offset
                BUFFER_SIZE,       // Flash size
                MAGIC_NUMBER,      // Magic (16 bits)
                VERSION_NUMBER     // Version (16 bits)
  );

  DPRINTF("> ");  // Print the prompt

  while (true) {
    int character = getchar_timeout_us(TIMEOUT_DURATION_US);
    if (character != PICO_ERROR_TIMEOUT) {
      if (character == '\r' || character == '\n') {
        // When Enter is pressed, process the command
        if (inputPos > 0) {
          printf("\n");
          inputBuffer[inputPos] = '\0';  // Null-terminate
          processCommand(inputBuffer);   // Process command
          inputPos = 0;                  // Reset buffer
          DPRINTF("> ");                 // New prompt
        }
      } else if (inputPos < INPUT_BUFFER_SIZE - 1) {
        putchar(character);  // Echo back
        inputBuffer[inputPos++] = (char)character;
      }
    }
    // Optionally put other non-blocking tasks here...
  }

  // (Optional) If you ever need to de-initialize:
  // settings_deinit(&g_settingsCtx);

  return 0;
}
