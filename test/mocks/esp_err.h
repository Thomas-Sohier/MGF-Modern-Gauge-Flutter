#pragma once

#include <stdint.h>

typedef int32_t esp_err_t;

#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 0x101
#define ESP_ERR_INVALID_STATE 0x102
#define ESP_ERR_NO_MEM 0x103
#define ESP_ERR_TIMEOUT 0x104
#define ESP_ERR_NVS_NOT_FOUND 0x201
#define ESP_ERR_NVS_TYPE_MISMATCH 0x202
#define ESP_ERR_NVS_INVALID_LENGTH 0x203
#define ESP_ERR_MOCK_IO 0x301

const char *esp_err_to_name(esp_err_t error);
