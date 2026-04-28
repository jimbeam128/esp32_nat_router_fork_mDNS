#pragma once

#include <stdint.h>
#include <stdbool.h>

#define FEATURE_LED_STRIP       (1U << 0)
#define FEATURE_PCAP            (1U << 1)
#define FEATURE_REMOTE_CONSOLE  (1U << 2)
#define FEATURE_SYSLOG          (1U << 3)
#define FEATURE_OLED            (1U << 4)
#define FEATURE_CONSOLE         (1U << 5)
#define FEATURE_MQTT_HA         (1U << 6)

#define DEFAULT_BOOT_FEATURES   0

void boot_features_load(void);
uint32_t boot_features_get(void);
void boot_features_set(uint32_t features);
bool boot_feature_enabled(uint32_t feature);