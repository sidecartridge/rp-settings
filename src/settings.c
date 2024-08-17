#include "settings.h"

// Static variables for storing the configuration data and flash memory settings
// Global structure for holding settings
static ConfigData configData;
// Size settings flash memory
static uint32_t flashSettingsSize = SETTINGS_DEFAULT_FLASH_SIZE;
// Offset in settings flash memory
static uint32_t flashSettingsOffset = 0;

// We should verify the key format always
static int checkKeyFormat(const char key[SETTINGS_MAX_KEY_LENGTH]) {
  // Check if the key is empty
  if (strlen(key) == 0) {
    DPRINTF("Error: Key is empty.\n");
    return -1;  // Invalid key format
  }

  // Loop through each character in the key
  for (size_t i = 0; i < strlen(key); i++) {
    char chr = key[i];

    // Check if the character is not an uppercase letter, a number, or an
    // underscore
    if (!isupper(chr) && !isdigit(chr) && chr != '_') {
      DPRINTF(
          "Error: Invalid character '%c' in key. Only uppercase letters, "
          "numbers, and '_' are allowed.\n",
          chr);
      return -1;  // Invalid key format
    }
  }

  return 0;  // Valid key format
}

// Check if the type is valid
static int checkTypeFormat(SettingsDataType type) {
  if (type != SETTINGS_TYPE_INT && type != SETTINGS_TYPE_STRING &&
      type != SETTINGS_TYPE_BOOL) {
    DPRINTF("Error: Invalid type format.\n");
    return -1;  // Invalid type format
  }
  return 0;  // Valid type format
}

// Load the default entries into memory as the initial configuration
static void settingsLoadDefaultEntries(const SettingsConfigEntry *entries,
                                       uint16_t numEntries) {
  for (uint16_t i = 0; i < numEntries; i++) {
    if (entries[i].key[0] == '\0' || strlen(entries[i].key) == 0) {
      break;  // Exit the loop if we encounter a key length of 0 (end of
              // entries)
    }
    if (checkTypeFormat(entries[i].dataType) != 0) {
      DPRINTF("WARNING: Invalid type format for key %s.\n", entries[i].key);
    } else if (checkKeyFormat(entries[i].key) != 0) {
      DPRINTF("WARNING: Invalid key format for key %s.\n", entries[i].key);
    } else {
      if (strlen(entries[i].key) > (SETTINGS_MAX_KEY_LENGTH - 1)) {
        DPRINTF(
            "WARNING: SETTINGS_MAX_KEY_LENGTH is %d but key %s is %d "
            "characters long.\n",
            SETTINGS_MAX_KEY_LENGTH, entries[i].key, strlen(entries[i].key));
      }
      SettingsConfigEntry tmpEntry = entries[i];
      configData.entries[i] = tmpEntry;
      configData.count++;
    }
  }

  if (configData.count != numEntries) {
    DPRINTF(
        "WARNING: Mismatch between the number of default entries (%d) and "
        "the number of entries loaded (%d).\n",
        numEntries, configData.count);
  } else {
    DPRINTF("Loaded %d default entries.\n", configData.count);
  }
}

// Load all entries from the FLASH memory, if any. Otherwise, use the default
// entries.
static void settingsLoadAllEntries(const SettingsConfigEntry *entries,
                                   uint16_t numEntries, uint16_t maxEntries) {
  uint8_t *currentAddress = (uint8_t *)(flashSettingsOffset + XIP_BASE);

  // First, load default entries
  settingsLoadDefaultEntries(entries, numEntries);

  // Read the magic value from the FLASH memory
  // it must be always the first value in the memory setting
  const uint8_t *magicAddress =
      (const uint8_t *)(currentAddress + SETTINGS_MAX_KEY_LENGTH +
                        sizeof(SettingsDataType));
  // Now it's safe to read the magic value until \0 o SETTINGS_MAX_VALUE_LENGTH
  char magicChar[SETTINGS_MAX_VALUE_LENGTH] = {0};
  for (size_t i = 0; i < SETTINGS_MAX_VALUE_LENGTH; i++) {
    if (magicAddress[i] == '\0') {
      break;
    }
    magicChar[i] = magicAddress[i];
  }
  uint32_t magic = (uint32_t)strtoul(magicChar, NULL, SETTINGS_BASE_10);

  if (magic != configData.magic) {
    // No config found in FLASH. Use default values
    DPRINTF("%lu!=%lu. No config found in FLASH. Using default values.\n",
            magic, configData.magic);
    return;
  }
  DPRINTF("Magic value found in FLASH: %lu. Loading the existing values.\n",
          magic);

  uint16_t count = 0;
  while (count < numEntries) {
    SettingsConfigEntry entry = {0};
    memcpy(&entry, currentAddress, sizeof(SettingsConfigEntry));

    currentAddress += sizeof(SettingsConfigEntry);

    // Check for the end of the config entries
    if (entry.key[0] == '\0') {
      break;  // Exit the loop if we encounter a key length of 0 (end of
              // entries)
    }

    if (checkKeyFormat(entry.key) != 0) {
      DPRINTF(
          "Invalid key format for key at address %p. Likely end of entries "
          "in FLASH.\n",
          currentAddress);
      break;
    }

    if (checkTypeFormat(entry.dataType) != 0) {
      DPRINTF(
          "Invalid type format for key %s stored. Likely end of entries in "
          "FLASH.\n",
          entry.key);
      break;
    }

    // Check if this key already exists in our loaded default entries
    char keyStr[SETTINGS_MAX_KEY_LENGTH + 1] = {0};
    strncpy(keyStr, entry.key, SETTINGS_MAX_KEY_LENGTH);
    SettingsConfigEntry *existingEntry = settings_find_entry(keyStr);
    if (existingEntry) {
      *existingEntry = entry;
    }
    // No else part here since we know every memory entry has a default
    count++;
  }
}

int settings_init(const SettingsConfigEntry *defaultEntries,
                  const uint16_t defaultNumEntries, const uint32_t flashOffset,
                  const uint32_t flashSize, const uint16_t magic,
                  const uint16_t version) {
  // Check if the flash_settings_size is multiple of SETTINGS_FLASH_PAGE_SIZE
  assert(flashSize % SETTINGS_FLASH_PAGE_SIZE == 0);
  flashSettingsSize = flashSize;
  DPRINTF("Flash settings size: %lu\n", flashSettingsSize);

  // Check if the flash_settings_offset is multiple of SETTINGS_FLASH_PAGE_SIZE
  assert(flashOffset % SETTINGS_FLASH_PAGE_SIZE == 0);
  flashSettingsOffset = flashOffset;
  DPRINTF("Flash settings offset: %lx\n", flashSettingsOffset);

  // Count the number of elements in the defaultEntries array
  size_t maxEntries = flashSettingsSize / sizeof(SettingsConfigEntry);
  DPRINTF("Max entries count: %d\n", maxEntries);

  // Check if the number of default entries exceeds the maximum number of
  // entries
  assert(defaultNumEntries <= maxEntries);
  DPRINTF("Default entries count: %d\n", defaultNumEntries);

  // Initialize the number of entries to the default entries count
  uint32_t entriesMemorySize = flashSettingsSize;
  configData.entries = (SettingsConfigEntry *)malloc(entriesMemorySize);
  DPRINTF("Reserved memory %lu for %d entries.\n", entriesMemorySize,
          maxEntries);

  // Create the large magic value by combining the magic and version
  configData.magic = (magic << SETTINGS_SHIFT_LEFT_16_BITS) | version;

  // Append at the beginning of the defaultEntries array the magic value
  char magicValue[SETTINGS_MAX_VALUE_LENGTH];
  snprintf(magicValue, sizeof(magicValue), "%lu", configData.magic);
  DPRINTF("Magic value string: %s\n", magicValue);
  SettingsConfigEntry magicEntry = {
      SETTINGS_MAGICVERSION_KEY, SETTINGS_TYPE_INT, {0}};
  strncpy(magicEntry.value, magicValue, SETTINGS_MAX_VALUE_LENGTH - 1);
  magicEntry.value[SETTINGS_MAX_VALUE_LENGTH - 1] =
      '\0';  // Ensure null-termination

  // Clone the default entries array in a newarray with the magic entry at the
  // beginning
  SettingsConfigEntry *defaultEntriesWithMagic = (SettingsConfigEntry *)malloc(
      (defaultNumEntries + 1) * sizeof(SettingsConfigEntry));
  defaultEntriesWithMagic[0] = magicEntry;
  memcpy(defaultEntriesWithMagic + 1, defaultEntries,
         defaultNumEntries * sizeof(SettingsConfigEntry));

  // Load the configuration from FLASH
  settingsLoadAllEntries(defaultEntriesWithMagic, defaultNumEntries + 1,
                         maxEntries);

  // Return the number of entries loaded into memory
  return configData.count;
}

// SettingsConfigEntry* entry = settings_find_entry("desired_key");
// if (entry != NULL) {
//     // Access the entry's data using entry->value, entry->dataType, etc.
// } else {
//     // Entry with the desired key was not found.
// }
SettingsConfigEntry *settings_find_entry(
    const char key[SETTINGS_MAX_KEY_LENGTH]) {
  // Check if the key is format valid
  if (checkKeyFormat(key) != 0) {
    DPRINTF("Invalid key format for key %s.\n", key);
    return NULL;
  }
  for (size_t i = 0; i < configData.count; i++) {
    if (strncmp(configData.entries[i].key, key, SETTINGS_MAX_KEY_LENGTH) == 0) {
      return &configData.entries[i];
    }
  }
  DPRINTF("Key %s not found.\n", key);
  return NULL;
}

static int settingsUpdateEntry(const char key[SETTINGS_MAX_KEY_LENGTH],
                               SettingsDataType dataType,
                               char value[SETTINGS_MAX_VALUE_LENGTH]) {
  // Check if the key is format valid
  if (checkKeyFormat(key) != 0) {
    DPRINTF("Invalid key format for key %s.\n", key);
    return -1;
  }
  // Check if the key already exists
  for (size_t i = 0; i < configData.count; i++) {
    if (strncmp(configData.entries[i].key, key, SETTINGS_MAX_KEY_LENGTH) == 0) {
      // Key already exists. Update its value and dataType
      configData.entries[i].dataType = dataType;
      strncpy(configData.entries[i].value, value,
              SETTINGS_MAX_VALUE_LENGTH - 1);
      configData.entries[i].value[SETTINGS_MAX_VALUE_LENGTH - 1] =
          '\0';  // Ensure null-termination
      return 0;  // Successfully updated existing entry
    }
  }
  DPRINTF("Key %s not found.\n", key);
  return -1;  // Key not found. Cannot update non-existing entry
}

int settings_put_bool(const char key[SETTINGS_MAX_KEY_LENGTH], bool value) {
  return settingsUpdateEntry(key, SETTINGS_TYPE_BOOL, value ? "true" : "false");
}

int settings_put_string(const char key[SETTINGS_MAX_KEY_LENGTH],
                        const char *value) {
  char configValue[SETTINGS_MAX_VALUE_LENGTH];
  strncpy(configValue, value, SETTINGS_MAX_VALUE_LENGTH - 1);
  configValue[SETTINGS_MAX_VALUE_LENGTH - 1] = '\0';  // Ensure null termination
  return settingsUpdateEntry(key, SETTINGS_TYPE_STRING, configValue);
}

int settings_put_integer(const char key[SETTINGS_MAX_KEY_LENGTH], int value) {
  char configValue[SETTINGS_MAX_VALUE_LENGTH];
  snprintf(configValue, sizeof(configValue), "%d", value);
  // Set \0 at the end of the configValue string
  configValue[SETTINGS_MAX_VALUE_LENGTH - 1] = '\0';
  return settingsUpdateEntry(key, SETTINGS_TYPE_INT, configValue);
}

int settings_save() {
  uint8_t *address = (uint8_t *)(flashSettingsOffset + XIP_BASE);

  // Ensure we don't exceed the reserved space
  if (configData.count * sizeof(SettingsConfigEntry) > flashSettingsSize) {
    return -1;  // Error: Config size exceeds reserved space
  }
  DPRINTF("Writing %d entries to FLASH.\n", configData.count);
  DPRINTF("Size of entries: %lu\n",
          configData.count * sizeof(SettingsConfigEntry));

  uint32_t ints = save_and_disable_interrupts();

  // Erase the content before writing the configuration
  // overwriting it's not enough
  flash_range_erase(flashSettingsOffset,
                    flashSettingsSize);  // 4 Kbytes multiple

  // Transfer config to FLASH
  flash_range_program(flashSettingsOffset, (uint8_t *)configData.entries,
                      flashSettingsSize);

  restore_interrupts(ints);

  return 0;  // Successful write
}

int settings_erase() {
  uint32_t ints = save_and_disable_interrupts();

  // Erase the content before writing the configuration
  // overwriting it's not enough
  flash_range_erase(flashSettingsOffset, flashSettingsSize);  // 4 Kbytes

  restore_interrupts(ints);

  free(configData.entries);
  memset(&configData, 0, sizeof(ConfigData));

  return 0;  // Successful write
}

void settings_print() {
  char dashes[SETTINGS_MAX_VALUE_LENGTH + 2] = {0};
  for (size_t i = 0; i < SETTINGS_MAX_VALUE_LENGTH + 2; i++) {
    dashes[i] = '-';
  }
  DPRINTF("+---+%.*s+%.*s+----------+\n", SETTINGS_MAX_KEY_LENGTH + 2, dashes,
          SETTINGS_MAX_VALUE_LENGTH + 2, dashes);
  DPRINTF("|IDX| %-*s | %-*s |   Type   |\n", SETTINGS_MAX_KEY_LENGTH, "Key",
          SETTINGS_MAX_VALUE_LENGTH, "Value");
  DPRINTF("+---+%.*s+%.*s+----------+\n", SETTINGS_MAX_KEY_LENGTH + 2, dashes,
          SETTINGS_MAX_VALUE_LENGTH + 2, dashes);

  for (size_t i = 0; i < configData.count; i++) {
    char valueStr[SETTINGS_MAX_VALUE_LENGTH];  // Buffer to format the value

    switch (configData.entries[i].dataType) {
      case SETTINGS_TYPE_INT:
      case SETTINGS_TYPE_STRING:
      case SETTINGS_TYPE_BOOL:
        snprintf(valueStr, sizeof(valueStr), "%s", configData.entries[i].value);
        break;
      default:
        snprintf(valueStr, sizeof(valueStr), "Unknown");
        break;
    }

    char *typeStr;
    switch (configData.entries[i].dataType) {
      case SETTINGS_TYPE_INT:
        typeStr = "INT";
        break;
      case SETTINGS_TYPE_STRING:
        typeStr = "STRING";
        break;
      case SETTINGS_TYPE_BOOL:
        typeStr = "BOOL";
        break;
      default:
        typeStr = "UNKNOWN";
        break;
    }
    char keyStr[SETTINGS_MAX_KEY_LENGTH + 1] = {0};
    strncpy(keyStr, configData.entries[i].key, SETTINGS_MAX_KEY_LENGTH);
    // DPRINTF("|%-3d| %-20s | %-30s | %-8s |\n", i, keyStr, valueStr, typeStr);
    DPRINTF("|%-3d| %-*s | %-*s | %-8s |\n", i, SETTINGS_MAX_KEY_LENGTH, keyStr,
            SETTINGS_MAX_VALUE_LENGTH, valueStr, typeStr);
  }

  DPRINTF("+---+%.*s+%.*s+----------+\n", SETTINGS_MAX_KEY_LENGTH + 2, dashes,
          SETTINGS_MAX_VALUE_LENGTH + 2, dashes);
}
