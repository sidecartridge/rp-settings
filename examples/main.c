/**
 * File: main.c
 * Author: Diego Parrilla Santamaría
 * Date: July 2023
 * Copyright: 2023 - GOODDATA LABS SL
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

// Command lookup structure, now with argument support
typedef struct {
  const char *command;
  void (*handler)(const char *arg);
} Command;

// Command handlers
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

// Command handlers
void cmdHelp(const char *arg) {
  DPRINTF("Available commands:\n");
  DPRINTF("  help    - Show available commands\n");
  DPRINTF("  print   - Show settings\n");
  DPRINTF("  save    - Save settings\n");
  DPRINTF("  erase   - Erase settings\n");
  DPRINTF("  get     - Get a setting (requires a key)\n");
  DPRINTF("  put_int - Set an integer setting (requires a key and value)\n");
  DPRINTF("  put_bool- Set a boolean setting (requires a key and value)\n");
  DPRINTF("  put_string - Set a string setting (requires a key and value)\n");
}

void cmdPrint(const char *arg) { settings_print(); }

void cmdSave(const char *arg) { settings_save(); }

void cmdErase(const char *arg) {
  settings_erase();
  DPRINTF("Settings erased.\n");
}

void cmdGet(const char *arg) {
  if (arg && strlen(arg) > 0) {
    SettingsConfigEntry *entry = settings_find_entry(&arg[0]);
    if (entry != NULL) {
      DPRINTF("Key: %s, Value: %s\n", entry->key, entry->value);
    }
  } else {
    DPRINTF("No key provided for 'get' command.\n");
  }
}

void cmdPutInt(const char *arg) {
  char key[SETTINGS_MAX_KEY_LENGTH] = {0};
  int value = 0;
  if (sscanf(arg, "%s %d", key, &value) == 2) {
    int err = settings_put_integer(key, value);
    if (err == 0) {
      DPRINTF("Key: %s, Value: %d\n", key, value);
    }
  } else {
    DPRINTF("Invalid arguments for 'put_int' command.\n");
  }
}

void cmdPutBool(const char *arg) {
  char key[SETTINGS_MAX_KEY_LENGTH] = {0};

  char valueStr[VALUE_STR_SIZE] = {
      0};  // Small buffer to store the value string (e.g., "true", "false")
  bool value;

  // Scan the key and the value string
  if (sscanf(arg, "%s %7s", key, valueStr) == 2) {
    // Convert the value_str to lowercase for easier comparison
    for (int i = 0; valueStr[i]; i++) {
      valueStr[i] = tolower(valueStr[i]);
    }

    // Check if the value is true or false
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
    int err = settings_put_bool(key, value);
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

  // Scan the first word (key)
  if (sscanf(arg, "%s", key) == 1) {
    // Find the position after the key in the argument string
    const char *value = strchr(arg, ' ');
    if (value) {
      // Move past the space to the start of the value
      value++;

      // Check if the value is non-empty
      if (strlen(value) > 0) {
        int err = settings_put_string(key, value);
        if (err == 0) {
          DPRINTF("Key: %s, Value: %s\n", key, value);
        }
      } else {
        int err = settings_put_string(key, value);
        if (err == 0) {
          DPRINTF("Key: %s, Value: <EMPTY>\n", key);
        }
      }
    } else {
      int err = settings_put_string(key, value);
      if (err == 0) {
        DPRINTF("Key: %s, Value: <EMPTY>\n", key);
      }
    }
  } else {
    DPRINTF("Invalid arguments for 'put_string' command.\n");
  }
}

void cmdUnknown(const char *arg) {
  DPRINTF("Unknown command. Type 'help' for a list of commands.\n");
}

// Function to find and execute a command, with optional argument
void processCommand(const char *input) {
  // Split the input into command and argument
  char command[COMMAND_SIZE] = {0};
  char arg[COMMAND_SIZE] = {0};
  sscanf(input, "%63s %63[^\n]", command, arg);  // Split at the first space

  for (size_t i = 0; i < numCommands; i++) {
    if (strcmp(command, commands[i].command) == 0) {
      commands[i].handler(arg);  // Pass the argument to the handler
      return;
    }
  }
  cmdUnknown(NULL);  // If no command matches
}
int main() {
  stdio_init_all();  // Initialize standard I/O for USB or UART
  setvbuf(stdout, NULL, _IONBF,
          1);  // specify that the stream should be unbuffered

  char inputBuffer[INPUT_BUFFER_SIZE];  // Buffer to store user input
  size_t inputPos = 0;                  // Current position in the buffer

  DPRINTF("RP - Settings CLI Tool\n");
  DPRINTF("Type 'help' for a list of commands.\n");

  SettingsConfigEntry entries[] = {
      {"TEST1", SETTINGS_TYPE_STRING, "TEST PARAM 1"},
      {"TEST2", SETTINGS_TYPE_BOOL, "false"},
      {"TEST3", SETTINGS_TYPE_INT, "60"},
      {"TEST4", SETTINGS_TYPE_STRING, "TEST PARAM 4"}};

  settings_init(entries, sizeof(entries) / sizeof(entries[0]), SETTINGS_ADDRESS,
                BUFFER_SIZE, MAGIC_NUMBER, VERSION_NUMBER);

  DPRINTF("> ");  // Print the prompt
  while (true) {
    int character = getchar_timeout_us(
        TIMEOUT_DURATION_US);  // Read a character with timeout
    if (character != PICO_ERROR_TIMEOUT) {
      if (character == '\r' || character == '\n') {
        // When Enter is pressed, process the command
        if (inputPos > 0) {
          printf("\n");                  // Print a newline
          inputBuffer[inputPos] = '\0';  // Null-terminate the string
          processCommand(inputBuffer);   // Process the input command
          inputPos = 0;                  // Reset buffer position
          DPRINTF("> ");                 // Print the prompt
        }
      } else if (inputPos < INPUT_BUFFER_SIZE - 1) {
        putchar(character);  // Echo the character back to the terminal
        inputBuffer[inputPos++] = character;  // Store character in buffer
      }
    }

    // Optionally add other code to run in the main loop here
  }

  return 0;
}
