#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "infrastructure/rtc_ds3231.h"

#define REG_SECONDS 0x00u
#define REG_STATUS  0x0Fu
#define STATUS_OSF  0x80u

typedef struct {
    uint8_t registers[0x20];
    bool fail_read;
    bool fail_write;
} fake_ds3231_t;

static esp_err_t fake_write(void *context, uint8_t address, const uint8_t *data,
                            size_t data_size, uint32_t timeout_ms) {
    (void)address;
    (void)timeout_ms;
    fake_ds3231_t *fake = context;
    if (fake == NULL || data == NULL || data_size < 2 || fake->fail_write ||
        data[0] >= sizeof(fake->registers) ||
        data_size - 1 > sizeof(fake->registers) - data[0]) {
        return ESP_ERR_MOCK_IO;
    }
    memcpy(&fake->registers[data[0]], &data[1], data_size - 1);
    return ESP_OK;
}

static esp_err_t fake_write_read(void *context, uint8_t address,
                                 const uint8_t *write_data, size_t write_size,
                                 uint8_t *read_data, size_t read_size,
                                 uint32_t timeout_ms) {
    (void)address;
    (void)timeout_ms;
    fake_ds3231_t *fake = context;
    if (fake == NULL || write_data == NULL || write_size != 1 ||
        read_data == NULL || read_size == 0 || fake->fail_read ||
        write_data[0] >= sizeof(fake->registers) ||
        read_size > sizeof(fake->registers) - write_data[0]) {
        return ESP_ERR_MOCK_IO;
    }
    memcpy(read_data, &fake->registers[write_data[0]], read_size);
    return ESP_OK;
}

static shared_i2c_bus_t fake_bus(fake_ds3231_t *fake) {
    return (shared_i2c_bus_t){
        .context = fake,
        .write = fake_write,
        .write_read = fake_write_read,
        .sda_gpio = DS3231_I2C_SDA_GPIO,
        .scl_gpio = DS3231_I2C_SCL_GPIO,
    };
}

static void load_valid_time(fake_ds3231_t *fake) {
    memset(fake, 0, sizeof(*fake));
    fake->registers[0] = 0x58; // 23:59:58
    fake->registers[1] = 0x59;
    fake->registers[2] = 0x23;
    fake->registers[3] = 5; // Thursday
    fake->registers[4] = 0x29;
    fake->registers[5] = 0x02;
    fake->registers[6] = 0x24;
}

static rtc_t *create_rtc(fake_ds3231_t *fake) {
    const ds3231_config_t config = {
        .bus = fake_bus(fake),
        .address = 0,
        .timeout_ms = 25,
    };
    return ds3231_create(&config);
}

static void test_oscillator_stop_and_sync(void) {
    fake_ds3231_t fake;
    load_valid_time(&fake);
    fake.registers[REG_STATUS] = STATUS_OSF | 0x04u;

    rtc_t *rtc = create_rtc(&fake);
    assert(rtc != NULL);
    assert(rtc_probe(rtc) == RTC_ERR_OSCILLATOR_STOPPED);

    const rtc_datetime_t synchronized = {
        .year = 2024,
        .month = 2,
        .day = 29,
        .weekday = 5,
        .hour = 12,
        .minute = 34,
        .second = 56,
    };
    assert(rtc_set(rtc, &synchronized) == RTC_OK);
    assert((fake.registers[REG_STATUS] & STATUS_OSF) == 0);

    rtc_datetime_t read_back = {0};
    assert(rtc_read(rtc, &read_back) == RTC_OK);
    assert(memcmp(&read_back, &synchronized, sizeof(read_back)) == 0);
    rtc_destroy(rtc);
}

static void test_invalid_registers_and_bus_errors(void) {
    fake_ds3231_t fake;
    load_valid_time(&fake);
    rtc_t *rtc = create_rtc(&fake);
    assert(rtc != NULL);

    fake.registers[REG_SECONDS] = 0x80;
    assert(rtc_read(rtc, &(rtc_datetime_t){0}) == RTC_ERR_INVALID_TIME);

    load_valid_time(&fake);
    fake.fail_read = true;
    assert(rtc_probe(rtc) == RTC_ERR_IO);

    rtc_destroy(rtc);
}

static void test_resource_guard(void) {
    fake_ds3231_t fake;
    load_valid_time(&fake);
    shared_i2c_bus_t bus = fake_bus(&fake);
    bus.scl_gpio = 7;
    assert(ds3231_create(&(ds3231_config_t){.bus = bus}) == NULL);
}

int main(void) {
    test_oscillator_stop_and_sync();
    test_invalid_registers_and_bus_errors();
    test_resource_guard();
    puts("DS3231 RTC tests: OK");
    return 0;
}
