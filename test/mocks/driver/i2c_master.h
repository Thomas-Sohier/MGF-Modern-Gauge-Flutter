#pragma once

// Host-only opaque handles needed by shared_i2c.h. RTC tests inject the bus
// callbacks directly and do not exercise the ESP-IDF I2C master adapter.
typedef struct i2c_master_bus_s *i2c_master_bus_handle_t;
typedef struct i2c_master_dev_s *i2c_master_dev_handle_t;
