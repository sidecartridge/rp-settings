/**
 * @file settings.h
 * @author Diego Parrilla
 * @date August 2024
 * @copyright 2024 - GOODDATA LABS SL
 *
 * @brief Header file for the settings manager, which manages configuration
 * entries stored in flash memory for the RP2040-based system.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hardware/flash.h>
#include <hardware/resets.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>

/**
 * @brief Debug macro for printing formatted debug messages.
 *
 * Prints a debug message to stderr if _DEBUG is defined and non-zero.
 *
 * @param fmt The format string (like printf).
 * @param ... Variadic arguments for the format string.
 */
#if defined(_DEBUG) && (_DEBUG != 0)

#include <string.h>

#ifndef DPRINTF
#define DPRINTF(fmt, ...)                                                      \
  do {                                                                         \
    const char *file =                                                         \
        strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__;        \
    fprintf(stderr, "%s:%d:%s(): " fmt "", file, __LINE__, __func__,           \
            ##__VA_ARGS__);                                                    \
  } while (0)
#endif

#ifndef DPRINTFRAW
#define DPRINTFRAW(fmt, ...)                                                   \
  do {                                                                         \
    fprintf(stderr, fmt, ##__VA_ARGS__);                                       \
  } while (0)
#endif

#else

#ifndef DPRINTF
#define DPRINTF(fmt, ...)
#endif

#ifndef DPRINTFRAW
#define DPRINTFRAW(fmt, ...)
#endif

#endif // _DEBUG

/**
 * @brief Maximum length constants for the settings keys and values.
 *
 * SETTINGS_MAX_KEY_LENGTH defines the maximum length of a configuration key.
 * SETTINGS_MAX_VALUE_LENGTH defines the maximum length of a string value.
 * SETTINGS_TYPE_SIZE is the fixed size of the data type field in the
 * configuration entry. Do not change
 *
 * It's possible to change the maximum length of the key and value before
 * starting to use the settings manager. As a rule of thumb, try that the sum of
 * all the three values is a divisor of 4096 (flash page size). By default, the
 * sum of the three values is 128.
 */
#define SETTINGS_MAX_KEY_LENGTH 30
#define SETTINGS_MAX_VALUE_LENGTH 96
#define SETTINGS_TYPE_SIZE2 2

/**
 * @brief Key for the magic and version settings.
 *
 * This key is used to store the magic number and version of the settings
 * structure in the flash memory. The magic number is used to validate the
 * settings structure, and the version is used to check if the settings
 * structure is compatible with the current program.
 *
 * It is always the first entry in the configuration.
 */
#define SETTINGS_MAGICVERSION_KEY "MAGICVERSION"

#define SETTINGS_FLASH_PAGE_SIZE 4096
#define SETTINGS_DEFAULT_FLASH_SIZE 4096

#define SETTINGS_BASE_10 10
#define SETTINGS_SHIFT_LEFT_16_BITS 16

/**
 * @brief Enumeration of possible data types for configuration entries.
 */
typedef enum {
  SETTINGS_TYPE_INT = 0,    ///< Integer type setting
  SETTINGS_TYPE_STRING = 1, ///< String type setting
  SETTINGS_TYPE_BOOL = 2    ///< Boolean type setting
} SettingsDataType;

/**
 * @brief Structure representing a single configuration entry.
 *
 * Each entry contains a key, data type, and value (as a string, integer, or
 * boolean). The sizeof this structure should be a divisor of 4096 (flash page
 * size).
 */
typedef struct {
  char key[SETTINGS_MAX_KEY_LENGTH];     ///< The configuration key (name)
  SettingsDataType dataType;             ///< The data type of the setting
  char value[SETTINGS_MAX_VALUE_LENGTH]; ///< The value of the setting (stored
                                         ///< as a string)
} SettingsConfigEntry;

/**
 * @brief Structure representing the overall configuration data.
 *
 * Contains a magic number for validation, a pointer to an array of
 * configuration entries, and the number of entries in the configuration.
 *
 * The magic number can be obtained at the flash settings storage address +
 * SETTINGS_MAX_KEY_LENGTH + sizeof(SettingsDataType) if it was saved correctly
 * in the flash memory previously.
 */
typedef struct {
  uint32_t magic; ///< Magic number for verifying settings validity
  SettingsConfigEntry *entries; ///< Array of configuration entries
  size_t count;                 ///< Number of configuration entries
} ConfigData;

/**
 * @brief Initialize the settings configuration.
 *
 * Loads default entries, checks flash for valid existing settings, and
 * initializes the settings manager with the specified parameters.
 *
 * This function must be called before any other settings function. It is
 * mandatory to create a default configuration with the desired default settings
 * values before calling this function. Only the key-values defined in the
 * default configuration will be managed by the settings manager.
 *
 * If some of the parameters given are invalid, the program will assert.
 *
 * @param defaultEntries Pointer to the array of default configuration entries.
 * @param defaultNumEntries Number of default configuration entries.
 * @param flash_offset Offset in flash memory where settings are stored.
 * @param flash_size Size of the flash memory region allocated for settings.
 * Must be a multiple of 4096.
 * @param magic Magic number for settings validation. Must be a 16-bit value.
 * @param version Version of the settings structure. Must be a 16-bit value.
 * @return int 0 on success, non-zero on failure.
 */
int settings_init(const SettingsConfigEntry *defaultEntries,
                  const uint16_t defaultNumEntries, const uint32_t flashOffset,
                  const uint32_t flashSize, const uint16_t magic,
                  const uint16_t version);

/**
 * @brief Save the current configuration settings to flash.
 *
 * Saves all configuration entries as a batch. This should be used only in
 * configuration mode. Try to avoid calling this function very often, as it
 * will wear out the flash memory.
 *
 * @return int 0 on success, non-zero on failure.
 */
int settings_save();

/**
 * @brief Reset the configuration to default values.
 *
 * Erases the current configuration and wipes the flash memory region where
 * settings are stored. To use again the settings manager, it is necessary to
 * call settings_init() again.
 *
 * @return int 0 on success, non-zero on failure.
 */
int settings_erase();

/**
 * @brief Print the current configuration in a tabular format.
 */
void settings_print();

/**
 * @brief Find a configuration entry by its key.
 *
 * If they key is not found, it returns NULL.
 * If the key is found, it returns a pointer to the SettingsConfigEntry
 * structure. To access the value, use the value field.
 *
 * Example:
 * SettingsConfigEntry* entry = settings_find_entry("desired_key");
 * if (entry != NULL) {
 *     // Access the entry's data using entry->value, entry->dataType, etc.
 * } else {
 *     // Entry with the desired key was not found.
 * }
 *
 * @param key The key of the configuration entry to find.
 * @return SettingsConfigEntry* Pointer to the found configuration entry, or
 * NULL if not found.
 */
SettingsConfigEntry *settings_find_entry(const char *key);

/**
 * @brief Update a boolean configuration entry.
 *
 * Update the value of a boolean configuration entry by its key.
 *
 * @param key The key of the entry.
 * @param value The boolean value to set.
 * @return int 0 on success, non-zero on failure.
 */
int settings_put_bool(const char key[SETTINGS_MAX_KEY_LENGTH], bool value);

/**
 * @brief Update a string configuration entry.
 *
 * Update the value of a string configuration entry by its key.
 *
 * @param key The key of the entry.
 * @param value The string value to set.
 * @return int 0 on success, non-zero on failure.
 */
int settings_put_string(const char key[SETTINGS_MAX_KEY_LENGTH],
                        const char *value);

/**
 * @brief Udate an integer configuration entry.
 *
 * Update the value of an integer configuration entry by its key.
 *
 * @param key The key of the entry.
 * @param value The integer value to set.
 * @return int 0 on success, non-zero on failure.
 */
int settings_put_integer(const char key[SETTINGS_MAX_KEY_LENGTH], int value);

#endif // SETTINGS_H
