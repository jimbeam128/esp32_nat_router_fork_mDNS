#include "boot_features.h"

#include "nvs.h"
#include "esp_log.h"

#define BOOT_FEATURES_KEY "boot_features"
#define PARAM_NAMESPACE   "router_config"

static const char *TAG = "BOOT_FEATURES";

static uint32_t g_boot_features = DEFAULT_BOOT_FEATURES;

void boot_features_load(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS: %s, using defaults", esp_err_to_name(err));
        g_boot_features = DEFAULT_BOOT_FEATURES;
        return;
    }

    uint32_t value = DEFAULT_BOOT_FEATURES;
    err = nvs_get_u32(nvs, BOOT_FEATURES_KEY, &value);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        value = DEFAULT_BOOT_FEATURES;
        err = nvs_set_u32(nvs, BOOT_FEATURES_KEY, value);
        if (err == ESP_OK) {
            err = nvs_commit(nvs);
        }
    }

    if (err == ESP_OK) {
        g_boot_features = value;
    } else {
        ESP_LOGW(TAG, "Failed to load boot features: %s, using defaults", esp_err_to_name(err));
        g_boot_features = DEFAULT_BOOT_FEATURES;
    }

    nvs_close(nvs);
}

uint32_t boot_features_get(void)
{
    return g_boot_features;
}

void boot_features_set(uint32_t features)
{
    g_boot_features = features;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS for save: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_u32(nvs, BOOT_FEATURES_KEY, features);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save boot features: %s", esp_err_to_name(err));
    }

    nvs_close(nvs);
}

bool boot_feature_enabled(uint32_t feature)
{
    return (g_boot_features & feature) != 0;
}