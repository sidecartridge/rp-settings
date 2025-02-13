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
#include <hardware/flash.h>
#include <hardware/resets.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Debug macro for printing formatted debug messages.
 *
 * Prints a debug message to stderr if _DEBUG is defined and non-zero.
 *
 * @param fmt The format string (like printf).
 * @param ... Variadic arguments for the format string.
 */
#if defined(_DEBUG) && (_DEBUG != 0)

#ifndef DPRINTF
#define DPRINTF(fmt, ...)                                               \
  do {                                                                  \
    const char *file =                                                  \
        strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__; \
    fprintf(stderr, "%s:%d:%s(): " fmt "", file, __LINE__, __func__,    \
            ##__VA_ARGS__);                                             \
  } while (0)
#endif

#ifndef DPRINTFRAW
#define DPRINTFRAW(fmt, ...)             \
  do {                                   \
    fprintf(stderr, fmt, ##__VA_ARGS__); \
  } while (0)
#endif

#else

#ifndef DPRINTF
#define DPRINTF(fmt, ...)
#endif

#ifndef DPRINTFRAW
#define DPRINTFRAW(fmt, ...)
#endif

#endif  // _DEBUG

/**
 * @brief Maximum length constants for the settings keys and values.
 */
#define SETTINGS_MAX_KEY_LENGTH 30
#define SETTINGS_MAX_VALUE_LENGTH 96

/**
 * @brief Key for the magic and version settings.
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
  SETTINGS_TYPE_INT = 0,     ///< Integer type setting
  SETTINGS_TYPE_STRING = 1,  ///< String type setting
  SETTINGS_TYPE_BOOL = 2     ///< Boolean type setting
} SettingsDataType;

/**
 * @brief Structure representing a single configuration entry.
 */
typedef struct {
  char key[SETTINGS_MAX_KEY_LENGTH];      ///< The configuration key (name)
  SettingsDataType dataType;              ///< The data type of the setting
  char value[SETTINGS_MAX_VALUE_LENGTH];  ///< The value of the setting (string)
} SettingsConfigEntry;

/**
 * @brief Structure representing the overall configuration data.
 */
typedef struct {
  uint32_t magic;                ///< Magic number for verifying settings
  SettingsConfigEntry *entries;  ///< Array of configuration entries
  size_t count;                  ///< Number of configuration entries
} ConfigData;

/**
 * @brief The "context" structure holding all state for one "instance"
 *        of the settings manager (e.g. for one block in flash).
 */
typedef struct {
  ConfigData configData;
  uint32_t flashSettingsSize;
  uint32_t flashSettingsOffset;
} SettingsContext;

/**
 * @brief Initialize the settings configuration (for one context).
 *
 * Loads default entries, checks flash for valid existing settings, and
 * initializes the settings manager with the specified parameters. Must be
 * called before other settings functions on this context.
 *
 * @param ctx            Pointer to the SettingsContext to initialize.
 * @param defaultEntries Pointer to the array of default configuration entries.
 * @param defaultNumEntries Number of default configuration entries.
 * @param flashOffset    Offset in flash memory where settings are stored.
 * @param flashSize      Size of the flash memory region allocated for settings
 *                       (must be multiple of 4096).
 * @param magic          16-bit magic number for settings validation.
 * @param version        16-bit version of the settings structure.
 * @return int           Returns count of loaded entries on success,
 *                       or negative value on failure.
 */
int settings_init(SettingsContext *ctx,
                  const SettingsConfigEntry *defaultEntries,
                  uint16_t defaultNumEntries, uint32_t flashOffset,
                  uint32_t flashSize, uint16_t magic, uint16_t version);

/**
 * @brief Deinitializes the settings module (for one context).
 *
 * Cleans up resources or state associated with the context.
 *
 * @param ctx Pointer to the SettingsContext to deinitialize.
 * @return int 0 on success, non-zero on failure.
 */
int settings_deinit(SettingsContext *ctx);

/**
 * @brief Save the current configuration settings to flash (for one context).
 *
 * @param ctx               Pointer to the SettingsContext.
 * @param disable_interrupts If true, interrupts will be disabled while writing.
 * @return int             0 on success, non-zero on failure.
 */
int settings_save(SettingsContext *ctx, bool disable_interrupts);

/**
 * @brief Reset the configuration to default values (for one context).
 *
 * Erases the current configuration and wipes the flash memory region where
 * settings are stored. The context is effectively invalidated; to use again,
 * call settings_init() once more.
 *
 * @param ctx Pointer to the SettingsContext.
 * @return int 0 on success, non-zero on failure.
 */
int settings_erase(SettingsContext *ctx);

/**
 * @brief Print the current configuration in a tabular format.
 *
 * @param ctx    Pointer to the SettingsContext.
 * @param buffer Optional buffer for storing output. If NULL, a buffer is
 *               allocated internally and printed to stderr.
 */
void settings_print(SettingsContext *ctx, char *buffer);

/**
 * @brief Find a configuration entry by its key.
 *
 * @param ctx Pointer to the SettingsContext.
 * @param key The key of the configuration entry to find.
 * @return Pointer to the found entry, or NULL if not found or invalid key.
 */
SettingsConfigEntry *settings_find_entry(
    SettingsContext *ctx, const char key[SETTINGS_MAX_KEY_LENGTH]);

/**
 * @brief Update a boolean configuration entry.
 *
 * @param ctx   Pointer to the SettingsContext.
 * @param key   The key of the entry.
 * @param value The boolean value to set.
 * @return int 0 on success, non-zero on failure (invalid key, or key not
 * found).
 */
int settings_put_bool(SettingsContext *ctx,
                      const char key[SETTINGS_MAX_KEY_LENGTH], bool value);

/**
 * @brief Update a string configuration entry.
 *
 * @param ctx   Pointer to the SettingsContext.
 * @param key   The key of the entry.
 * @param value The string value to set.
 * @return int 0 on success, non-zero on failure (invalid key, or key not
 * found).
 */
int settings_put_string(SettingsContext *ctx,
                        const char key[SETTINGS_MAX_KEY_LENGTH],
                        const char *value);

/**
 * @brief Update an integer configuration entry.
 *
 * @param ctx   Pointer to the SettingsContext.
 * @param key   The key of the entry.
 * @param value The integer value to set.
 * @return int 0 on success, non-zero on failure (invalid key, or key not
 * found).
 */
int settings_put_integer(SettingsContext *ctx,
                         const char key[SETTINGS_MAX_KEY_LENGTH], int value);

#endif  // SETTINGS_H
