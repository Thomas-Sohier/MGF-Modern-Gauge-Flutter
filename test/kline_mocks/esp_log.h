#pragma once

static inline void mock_kline_esp_log(const char *tag, const char *format,
                                      ...) {
    (void)tag;
    (void)format;
}

#define ESP_LOGE(tag, ...) mock_kline_esp_log((tag), __VA_ARGS__)
#define ESP_LOGW(tag, ...) mock_kline_esp_log((tag), __VA_ARGS__)
#define ESP_LOGI(tag, ...) mock_kline_esp_log((tag), __VA_ARGS__)
#define ESP_LOGD(tag, ...) mock_kline_esp_log((tag), __VA_ARGS__)
