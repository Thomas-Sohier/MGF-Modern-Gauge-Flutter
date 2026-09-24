#pragma once

#define ESP_LOGE(tag, format, ...)                                             \
    ((void)(tag), (void)(format), (void)(__VA_ARGS__))
#define ESP_LOGW(tag, format, ...)                                             \
    ((void)(tag), (void)(format), (void)(__VA_ARGS__))
