#pragma once

#include <sdkconfig.h>

#ifdef CONFIG_IDF_TARGET_ESP32C6
#define CHIP_CONFIG_SETUP_CODE_PAIRER_DISCOVERY_TIMEOUT_SECS 90
#endif

#ifndef CONFIG_ESP_MATTER_ENABLE_MATTER_SERVER

#ifdef CONFIG_ESP_MATTER_COMMISSIONER_ENABLE
// Enable or disable whether this device advertises as a commissioner.
#define CHIP_DEVICE_CONFIG_ENABLE_COMMISSIONER_DISCOVERY 1
#endif // CONFIG_ESP_MATTER_COMMISSIONER_ENABLE

// Number of devices a controller can be simultaneously connected to
#define CHIP_CONFIG_CONTROLLER_MAX_ACTIVE_DEVICES 8

#endif // CONFIG_ESP_MATTER_ENABLE_MATTER_SERVER
