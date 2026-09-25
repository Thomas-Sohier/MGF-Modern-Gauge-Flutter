#pragma once

// Fonction variadique : accepte ESP_LOGW(TAG, "msg") sans argument, comme
// ESP-IDF, tout en évaluant (et donc typant) les arguments.
static inline void mock_esp_log(const char *tag, const char *format, ...) {
    (void)tag;
    (void)format;
}

#define ESP_LOGE(tag, ...) mock_esp_log((tag), __VA_ARGS__)
#define ESP_LOGW(tag, ...) mock_esp_log((tag), __VA_ARGS__)
#define ESP_LOGI(tag, ...) mock_esp_log((tag), __VA_ARGS__)
