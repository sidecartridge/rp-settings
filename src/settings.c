/**
 * @file settings.c
 */

#include "settings.h"

/*
 * -----------
 * STATIC HELPER FUNCTIONS
 * -----------
 */

/**
 * @brief Verify the format of a given key (uppercase, numbers, or '_').
 */
static int checkKeyFormat(const char key[SETTINGS_MAX_KEY_LENGTH]) {
  // Check if the key is empty
  if (strlen(key) == 0) {
    DPRINTF("Error: Key is empty.\n");
    return -1;  // Invalid key format
  }

  // Loop through each character in the key
  for (size_t i = 0; i < strlen(key); i++) {
    char chr = key[i];

    // Check if the character is not an uppercase letter, digit or underscore
    if (!isupper(chr) && !isdigit(chr) && chr != '_') {
      DPRINTF(
          "Error: Invalid character '%c' in key '%s'. "
          "Only uppercase letters, numbers, and '_' are allowed.\n",
          chr, key);
      return -1;  // Invalid key format
    }
  }

  return 0;  // Valid key format
}

/**
 * @brief Verify that the type is one of the known types.
 */
static int checkTypeFormat(SettingsDataType type) {
  if (type != SETTINGS_TYPE_INT && type != SETTINGS_TYPE_STRING &&
      type != SETTINGS_TYPE_BOOL) {
    DPRINTF("Error: Invalid type format.\n");
    return -1;
  }
  return 0;
}

/**
 * @brief Load the default entries into memory as the initial config.
 *
 * Populates the context's configData with the entries that pass validation.
 */
static void settingsLoadDefaultEntries(SettingsContext *ctx,
                                       const SettingsConfigEntry *entries,
                                       uint16_t numEntries) {
  // Start with zero count
  ctx->configData.count = 0;

  for (uint16_t i = 0; i < numEntries; i++) {
    if (entries[i].key[0] == '\0' || strlen(entries[i].key) == 0) {
      // If we hit an empty key, we consider it the end of the default list
      break;
    }

    if (checkTypeFormat(entries[i].dataType) != 0) {
      DPRINTF("WARNING: Invalid type format for key %s.\n", entries[i].key);
    } else if (checkKeyFormat(entries[i].key) != 0) {
      DPRINTF("WARNING: Invalid key format for key %s.\n", entries[i].key);
    } else {
      // Warn if the key is longer than the limit (should be truncated anyway)
      if (strlen(entries[i].key) > (SETTINGS_MAX_KEY_LENGTH - 1)) {
        DPRINTF(
            "WARNING: SETTINGS_MAX_KEY_LENGTH is %d but key %s "
            "is %zu characters long.\n",
            SETTINGS_MAX_KEY_LENGTH, entries[i].key, strlen(entries[i].key));
      }

      // Copy the entry
      SettingsConfigEntry tmpEntry = entries[i];
      ctx->configData.entries[ctx->configData.count] = tmpEntry;
      ctx->configData.count++;
    }
  }

  if (ctx->configData.count != numEntries) {
    DPRINTF(
        "WARNING: Mismatch between the number of default entries (%d) "
        "and the number of entries loaded (%zu).\n",
        numEntries, ctx->configData.count);
  } else {
    DPRINTF("Loaded %zu default entries.\n", ctx->configData.count);
  }
}

/**
 * @brief Load all entries from FLASH if valid, otherwise use default entries.
 */
static int settingsLoadAllEntries(SettingsContext *ctx,
                                  const SettingsConfigEntry *entries,
                                  uint16_t numEntries, uint16_t maxEntries) {
  uint8_t *currentAddress = (uint8_t *)(ctx->flashSettingsOffset + XIP_BASE);

  // First, load default entries
  settingsLoadDefaultEntries(ctx, entries, numEntries);

  // The magic value is stored as a string in the first "entry",
  // i.e. at offset = first entry's value field. By design, your code
  // placed it in the first entry's value, which is:
  //    offset ( 0 ) => start of first entry
  //    offset (30) => key
  //    offset (34) => dataType
  //    offset (35) => value
  // But let's keep it consistent with your original approach:
  // We'll read the magic as if it is in the memory after we've read it from
  // flash.

  // The code below copies the entire set of entries from flash in a loop,
  // but first we check the magic.
  // We'll read from flash to see what was stored.

  // Actually read the magic from the stored data
  // The original code:
  //   magicAddress = currentAddress + SETTINGS_MAX_KEY_LENGTH +
  //   sizeof(SettingsDataType)

  const uint8_t *magicAddress =
      (const uint8_t *)(currentAddress + SETTINGS_MAX_KEY_LENGTH +
                        sizeof(SettingsDataType));

  char magicChar[SETTINGS_MAX_VALUE_LENGTH] = {0};
  for (size_t i = 0; i < SETTINGS_MAX_VALUE_LENGTH; i++) {
    if (magicAddress[i] == '\0') {
      break;
    }
    magicChar[i] = magicAddress[i];
  }
  uint32_t storedMagic = (uint32_t)strtoul(magicChar, NULL, SETTINGS_BASE_10);

  if (storedMagic != ctx->configData.magic) {
    // No matching magic => no valid config found in FLASH. Use default values.
    DPRINTF("%lu != %lu. No config found in FLASH. Using default values.\n",
            storedMagic, ctx->configData.magic);
    return -1;
  }

  DPRINTF("Magic value found in FLASH: %lu. Loading existing values.\n",
          storedMagic);

  // Now read each entry in a loop
  // We'll simply read as many entries as we can, up to numEntries
  uint16_t count = 0;
  while (count < numEntries) {
    SettingsConfigEntry entry = {0};
    memcpy(&entry, currentAddress, sizeof(SettingsConfigEntry));
    currentAddress += sizeof(SettingsConfigEntry);

    if (entry.key[0] == '\0') {
      // This indicates we've reached the end
      break;
    }

    if (checkKeyFormat(entry.key) != 0) {
      DPRINTF(
          "Invalid key format for key at address %p. "
          "Likely end of entries in FLASH.\n",
          (void *)currentAddress);
      break;
    }

    if (checkTypeFormat(entry.dataType) != 0) {
      DPRINTF(
          "Invalid type format for key %s stored. "
          "Likely end of entries in FLASH.\n",
          entry.key);
      break;
    }

    // Overwrite the matching default entry in ctx->configData
    // if it exists:
    for (size_t i = 0; i < ctx->configData.count; i++) {
      if (strncmp(ctx->configData.entries[i].key, entry.key,
                  SETTINGS_MAX_KEY_LENGTH) == 0) {
        ctx->configData.entries[i] = entry;
        break;  // Found the match, updated, done
      }
    }
    count++;
  }

  return 0;
}

/*
 * -----------
 * PUBLIC API IMPLEMENTATION
 * -----------
 */

int settings_init(SettingsContext *ctx,
                  const SettingsConfigEntry *defaultEntries,
                  uint16_t defaultNumEntries, uint32_t flashOffset,
                  uint32_t flashSize, uint16_t magic, uint16_t version) {
  // 1) Validate/Assign flash parameters
  assert(flashSize % SETTINGS_FLASH_PAGE_SIZE == 0);
  ctx->flashSettingsSize = flashSize;
  assert(flashOffset % SETTINGS_FLASH_PAGE_SIZE == 0);
  ctx->flashSettingsOffset = flashOffset;

  DPRINTF("Flash settings size: %lu\n", (unsigned long)ctx->flashSettingsSize);
  DPRINTF("Flash settings offset: 0x%lx\n",
          (unsigned long)ctx->flashSettingsOffset);

  // 2) Allocate memory for all possible entries
  size_t maxEntries = ctx->flashSettingsSize / sizeof(SettingsConfigEntry);
  DPRINTF("Max entries count: %zu\n", maxEntries);

  assert(defaultNumEntries <= maxEntries);
  DPRINTF("Default entries count: %d\n", defaultNumEntries);

  // 3) Prepare the configData structure
  ctx->configData.entries =
      (SettingsConfigEntry *)malloc(ctx->flashSettingsSize);
  if (!ctx->configData.entries) {
    DPRINTF("Error: Unable to allocate memory for config entries.\n");
    return -1;
  }
  ctx->configData.count = 0;

  // 4) Build the 32-bit magic from (magic << 16) | version
  ctx->configData.magic =
      ((uint32_t)magic << SETTINGS_SHIFT_LEFT_16_BITS) | version;
  DPRINTF("Combined magic: 0x%08lx\n", (unsigned long)ctx->configData.magic);

  // 5) Create an augmented default array with the MAGICVERSION entry at the
  // front.
  SettingsConfigEntry *defaultEntriesWithMagic = (SettingsConfigEntry *)malloc(
      (defaultNumEntries + 1) * sizeof(SettingsConfigEntry));
  if (!defaultEntriesWithMagic) {
    DPRINTF(
        "Error: Unable to allocate memory for default entries with magic.\n");
    return -1;
  }

  // Fill the special "MAGICVERSION" entry
  char magicValue[SETTINGS_MAX_VALUE_LENGTH];
  snprintf(magicValue, sizeof(magicValue), "%lu",
           (unsigned long)ctx->configData.magic);
  SettingsConfigEntry magicEntry = {
      SETTINGS_MAGICVERSION_KEY, SETTINGS_TYPE_INT, {0}};
  strncpy(magicEntry.value, magicValue, SETTINGS_MAX_VALUE_LENGTH - 1);
  magicEntry.value[SETTINGS_MAX_VALUE_LENGTH - 1] = '\0';

  // Put the magic entry first, then copy the user’s defaults
  defaultEntriesWithMagic[0] = magicEntry;
  memcpy(defaultEntriesWithMagic + 1, defaultEntries,
         defaultNumEntries * sizeof(SettingsConfigEntry));

  // 6) Load from flash (or default) into ctx->configData
  int error = settingsLoadAllEntries(ctx, defaultEntriesWithMagic,
                                     (uint16_t)(defaultNumEntries + 1),
                                     (uint16_t)maxEntries);

  free(defaultEntriesWithMagic);

  // Return the number of entries loaded, or error
  return (error == 0 ? (int)ctx->configData.count : error);
}

int settings_deinit(SettingsContext *ctx) {
  if (!ctx) return -1;

  // Reset the entire structure
  if (ctx->configData.entries) {
    free(ctx->configData.entries);
    ctx->configData.entries = NULL;
  }
  ctx->configData.count = 0;
  ctx->flashSettingsSize = SETTINGS_DEFAULT_FLASH_SIZE;
  ctx->flashSettingsOffset = 0;

  return 0;
}

int settings_save(SettingsContext *ctx, bool disable_interrupts) {
  if (!ctx) return -1;

  // Check if we don't exceed the reserved space
  size_t totalUsed = ctx->configData.count * sizeof(SettingsConfigEntry);
  if (totalUsed > ctx->flashSettingsSize) {
    DPRINTF("Error: config size %zu exceeds reserved space %u.\n", totalUsed,
            ctx->flashSettingsSize);
    return -1;
  }

  DPRINTF("Writing %zu entries to FLASH (size=%zu bytes).\n",
          ctx->configData.count, totalUsed);

  uint32_t ints = 0;
  if (disable_interrupts) {
    ints = save_and_disable_interrupts();
  }

  flash_range_erase(ctx->flashSettingsOffset, ctx->flashSettingsSize);
  flash_range_program(ctx->flashSettingsOffset,
                      (uint8_t *)ctx->configData.entries,
                      ctx->flashSettingsSize);

  if (disable_interrupts) {
    restore_interrupts(ints);
  }

  return 0;
}

int settings_erase(SettingsContext *ctx) {
  if (!ctx) return -1;

  // Erase the flash region
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(ctx->flashSettingsOffset, ctx->flashSettingsSize);
  restore_interrupts(ints);

  // Free and reset
  if (ctx->configData.entries) {
    free(ctx->configData.entries);
    ctx->configData.entries = NULL;
  }
  ctx->configData.count = 0;

  return 0;
}

SettingsConfigEntry *settings_find_entry(
    SettingsContext *ctx, const char key[SETTINGS_MAX_KEY_LENGTH]) {
  if (!ctx) return NULL;
  if (checkKeyFormat(key) != 0) {
    DPRINTF("Invalid key format for key %s.\n", key);
    return NULL;
  }

  for (size_t i = 0; i < ctx->configData.count; i++) {
    if (strncmp(ctx->configData.entries[i].key, key, SETTINGS_MAX_KEY_LENGTH) ==
        0) {
      return &ctx->configData.entries[i];
    }
  }
  DPRINTF("Key %s not found.\n", key);
  return NULL;
}

/**
 * @brief Internal helper to update an entry if it exists.
 */
static int settingsUpdateEntry(SettingsContext *ctx,
                               const char key[SETTINGS_MAX_KEY_LENGTH],
                               SettingsDataType dataType,
                               const char value[SETTINGS_MAX_VALUE_LENGTH]) {
  if (checkKeyFormat(key) != 0) {
    DPRINTF("Invalid key format: %s\n", key);
    return -1;
  }
  if (checkTypeFormat(dataType) != 0) {
    DPRINTF("Invalid data type for key: %s\n", key);
    return -1;
  }

  for (size_t i = 0; i < ctx->configData.count; i++) {
    if (strncmp(ctx->configData.entries[i].key, key, SETTINGS_MAX_KEY_LENGTH) ==
        0) {
      // Key found, update
      ctx->configData.entries[i].dataType = dataType;
      strncpy(ctx->configData.entries[i].value, value,
              SETTINGS_MAX_VALUE_LENGTH - 1);
      ctx->configData.entries[i].value[SETTINGS_MAX_VALUE_LENGTH - 1] = '\0';
      return 0;
    }
  }
  DPRINTF("Key %s not found (cannot update).\n", key);
  return -1;
}

int settings_put_bool(SettingsContext *ctx,
                      const char key[SETTINGS_MAX_KEY_LENGTH], bool value) {
  return settingsUpdateEntry(ctx, key, SETTINGS_TYPE_BOOL,
                             value ? "true" : "false");
}

int settings_put_string(SettingsContext *ctx,
                        const char key[SETTINGS_MAX_KEY_LENGTH],
                        const char *value) {
  if (!value) {
    DPRINTF("Error: NULL value for string key %s\n", key);
    return -1;
  }
  char buffer[SETTINGS_MAX_VALUE_LENGTH];
  strncpy(buffer, value, SETTINGS_MAX_VALUE_LENGTH - 1);
  buffer[SETTINGS_MAX_VALUE_LENGTH - 1] = '\0';
  return settingsUpdateEntry(ctx, key, SETTINGS_TYPE_STRING, buffer);
}

int settings_put_integer(SettingsContext *ctx,
                         const char key[SETTINGS_MAX_KEY_LENGTH], int value) {
  char buffer[SETTINGS_MAX_VALUE_LENGTH];
  snprintf(buffer, sizeof(buffer), "%d", value);
  buffer[SETTINGS_MAX_VALUE_LENGTH - 1] = '\0';
  return settingsUpdateEntry(ctx, key, SETTINGS_TYPE_INT, buffer);
}

/**
 * @brief Print the current configuration in a tabular format.
 */
void settings_print(SettingsContext *ctx, char *buffer) {
  if (!ctx) return;

  // We'll use a fixed-size buffer if none is provided
  size_t remaining = 2048;
  char *outputBuffer = NULL;

  if (buffer == NULL) {
    // Allocate a buffer if none is provided
    outputBuffer = (char *)malloc(remaining);
    if (!outputBuffer) {
      DPRINTF("Error: Unable to allocate memory for output buffer.\n");
      return;
    }
  } else {
    // Use the provided buffer
    outputBuffer = buffer;
  }

  char *ptr = outputBuffer;
  size_t len = 0;

  // Loop through each entry
  for (size_t i = 0; i < ctx->configData.count && remaining > 0; i++) {
    const char *typeStr = "UNK";
    switch (ctx->configData.entries[i].dataType) {
      case SETTINGS_TYPE_INT:
        typeStr = "INT";
        break;
      case SETTINGS_TYPE_STRING:
        typeStr = "STR";
        break;
      case SETTINGS_TYPE_BOOL:
        typeStr = "BOOL";
        break;
      default:
        typeStr = "UNK";
        break;
    }

    // Print in the format: "KEY (TYPE): Value\n"
    len = snprintf(ptr, remaining, "%s (%s): %s\n",
                   ctx->configData.entries[i].key, typeStr,
                   ctx->configData.entries[i].value);

    ptr += len;
    remaining = (len < remaining) ? (remaining - len) : 0;
  }

  // Ensure null-termination
  if (remaining > 0) {
    *ptr = '\0';
  }

  // If no external buffer was provided, print and free
  if (buffer == NULL) {
    DPRINTFRAW("%s", outputBuffer);
    free(outputBuffer);
  }
}
