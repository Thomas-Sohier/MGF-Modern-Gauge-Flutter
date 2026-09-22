#include "infrastructure/board_display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#include "esp_io_expander_tca95xx_16bit.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_st7701.h"
#include "esp_lcd_touch_cst816s.h" // CST820-compatible driver
#include "esp_lvgl_port.h"

#include "infrastructure/lilygo_st7701_init.h"

static const char *TAG = "board_disp";

// LILYGO T-RGB 2.1 Full Circle (H597) pinout.  The LCD data GPIO order is
// deliberately kept as the upstream D0..D15 order, not reconstructed from
// the colour names in the vendor comments (which disagree between revisions).
#define LCD_H_RES             480
#define LCD_V_RES             480
#define PIN_LCD_BL            GPIO_NUM_46
#define PIN_LCD_HSYNC         GPIO_NUM_47
#define PIN_LCD_VSYNC         GPIO_NUM_41
#define PIN_LCD_DE            GPIO_NUM_45
#define PIN_LCD_PCLK          GPIO_NUM_42
#define PIN_I2C_SDA           GPIO_NUM_8
#define PIN_I2C_SCL           GPIO_NUM_48
#define PIN_TOUCH_IRQ         GPIO_NUM_1
#define I2C_PORT              I2C_NUM_0
#define I2C_HZ                400000
#define XL9535_ADDRESS        0x20

// These are XL9535/XL9555 port numbers.  IO0 is not used by the H597.
#define EXIO_TOUCH_RST       IO_EXPANDER_PIN_NUM_1
#define EXIO_PWR_EN          IO_EXPANDER_PIN_NUM_2
#define EXIO_LCD_CS          IO_EXPANDER_PIN_NUM_3
#define EXIO_LCD_SDA         IO_EXPANDER_PIN_NUM_4
#define EXIO_LCD_SCL         IO_EXPANDER_PIN_NUM_5
#define EXIO_LCD_RST         IO_EXPANDER_PIN_NUM_6

#ifndef MGF_LILYGO_T_RGB_V2
#define MGF_LILYGO_T_RGB_V2 0
#endif

typedef struct {
    const char *name;
    uint32_t pclk_hz;
    uint32_t hsync_pulse_width;
    uint32_t hsync_back_porch;
    uint32_t hsync_front_porch;
    uint32_t vsync_pulse_width;
    uint32_t vsync_back_porch;
    uint32_t vsync_front_porch;
    const int *data_gpio_nums;
    const st7701_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
} lilygo_display_profile_t;

static const int s_rgb_data_original[] = {
    7, 6, 5, 3, 2,
    14, 13, 12, 11, 10, 9,
    21, 18, 17, 16, 15,
};

static const int s_rgb_data_v2[] = {
    43, 7, 6, 5, 3,
    14, 13, 12, 11, 10, 9,
    44, 21, 18, 17, 16,
};

#if MGF_LILYGO_T_RGB_V2
static const lilygo_display_profile_t s_profile = {
    .name = "V2",
    .pclk_hz = 10 * 1000 * 1000,
    .hsync_pulse_width = 2,
    .hsync_back_porch = 34,
    .hsync_front_porch = 20,
    .vsync_pulse_width = 2,
    .vsync_back_porch = 20,
    .vsync_front_porch = 50,
    .data_gpio_nums = s_rgb_data_v2,
    .init_cmds = lilygo_st7701_2_1_inches_rev2,
    .init_cmds_size = LILYGO_ST7701_2_1_INCHES_REV2_COUNT,
};
#else
static const lilygo_display_profile_t s_profile = {
    .name = "original",
    .pclk_hz = 8 * 1000 * 1000,
    .hsync_pulse_width = 1,
    .hsync_back_porch = 30,
    .hsync_front_porch = 50,
    .vsync_pulse_width = 1,
    .vsync_back_porch = 30,
    .vsync_front_porch = 20,
    .data_gpio_nums = s_rgb_data_original,
    .init_cmds = lilygo_st7701_2_1_inches,
    .init_cmds_size = LILYGO_ST7701_2_1_INCHES_COUNT,
};
#endif

static i2c_master_bus_handle_t s_i2c_bus;
static esp_io_expander_handle_t s_expander;
static uint8_t s_backlight_level;
static portMUX_TYPE s_backlight_lock = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t i2c_bus_init(void) {
    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_PORT,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&config, &s_i2c_bus);
}

static esp_err_t expander_init(void) {
    ESP_RETURN_ON_ERROR(
        esp_io_expander_new_i2c_tca95xx_16bit(s_i2c_bus, XL9535_ADDRESS, &s_expander),
        TAG, "XL9535-compatible expander at 0x%02x", XL9535_ADDRESS);

    const uint32_t output_pins = EXIO_TOUCH_RST | EXIO_PWR_EN | EXIO_LCD_CS |
                                 EXIO_LCD_SDA | EXIO_LCD_SCL | EXIO_LCD_RST;
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(s_expander, output_pins,
                                                IO_EXPANDER_OUTPUT),
                        TAG, "configure XL9535 outputs");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, EXIO_PWR_EN, 1),
                        TAG, "enable panel power");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, EXIO_LCD_CS, 1),
                        TAG, "idle LCD CS");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(
                            s_expander, EXIO_LCD_SDA | EXIO_LCD_SCL, 0),
                        TAG, "idle LCD serial lines");

    // The vendor sequence requires PWR_EN before the LCD reset pulse.  Touch
    // reset is released with the LCD, then the CST820 gets its own settle time.
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(
                            s_expander, EXIO_LCD_RST | EXIO_TOUCH_RST, 0),
                        TAG, "assert panel and touch reset");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, EXIO_LCD_RST, 1),
                        TAG, "release LCD reset");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, EXIO_TOUCH_RST, 1),
                        TAG, "release touch reset");
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

static esp_err_t panel_init(esp_lcd_panel_io_handle_t *out_io,
                            esp_lcd_panel_handle_t *out_panel) {
    const spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_EXPANDER,
        .cs_expander_pin = EXIO_LCD_CS,
        .scl_io_type = IO_TYPE_EXPANDER,
        .scl_expander_pin = EXIO_LCD_SCL,
        .sda_io_type = IO_TYPE_EXPANDER,
        .sda_expander_pin = EXIO_LCD_SDA,
        .io_expander = s_expander,
    };
    const esp_lcd_panel_io_3wire_spi_config_t io_config =
        ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_3wire_spi(&io_config, out_io),
                        TAG, "create 9-bit ST7701 serial IO");

    esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .psram_trans_align = 64,
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 2,
        .bounce_buffer_size_px = LCD_H_RES * 10,
        .hsync_gpio_num = PIN_LCD_HSYNC,
        .vsync_gpio_num = PIN_LCD_VSYNC,
        .de_gpio_num = PIN_LCD_DE,
        .pclk_gpio_num = PIN_LCD_PCLK,
        .disp_gpio_num = -1,
        .timings = {
            .pclk_hz = s_profile.pclk_hz,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = s_profile.hsync_pulse_width,
            .hsync_back_porch = s_profile.hsync_back_porch,
            .hsync_front_porch = s_profile.hsync_front_porch,
            .vsync_pulse_width = s_profile.vsync_pulse_width,
            .vsync_back_porch = s_profile.vsync_back_porch,
            .vsync_front_porch = s_profile.vsync_front_porch,
            .flags.pclk_active_neg = true,
        },
        .flags.fb_in_psram = true,
    };
    for (size_t i = 0; i < 16; ++i) {
        rgb_config.data_gpio_nums[i] = s_profile.data_gpio_nums[i];
    }

    const st7701_vendor_config_t vendor_config = {
        .init_cmds = s_profile.init_cmds,
        .init_cmds_size = s_profile.init_cmds_size,
        .rgb_config = &rgb_config,
        .flags = {
            .mirror_by_cmd = 0,
            .auto_del_panel_io = 0,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1, // reset is on XL9535 IO6
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = (void *)&vendor_config,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7701(*out_io, &panel_config,
                                                 out_panel),
                        TAG, "create ST7701S RGB panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*out_panel), TAG,
                        "initialize ST7701S profile %s", s_profile.name);
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(*out_panel, true), TAG,
                        "enable RGB panel");
    return ESP_OK;
}

static lv_display_t *lvgl_bringup(esp_lcd_panel_io_handle_t io,
                                  esp_lcd_panel_handle_t panel) {
    const lvgl_port_cfg_t port_config = ESP_LVGL_PORT_INIT_CONFIG();
    esp_err_t err = lvgl_port_init(&port_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LVGL port init failed: %s", esp_err_to_name(err));
        return NULL;
    }

    const lvgl_port_display_cfg_t display_config = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = LCD_H_RES * LCD_V_RES,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_spiram = true,
        },
    };
    const lvgl_port_display_rgb_cfg_t rgb_config = {
        .flags = {
            .bb_mode = true,
            .avoid_tearing = true,
        },
    };
    return lvgl_port_add_disp_rgb(&display_config, &rgb_config);
}

static void touch_bringup(lv_display_t *display) {
    esp_lcd_panel_io_handle_t touch_io = NULL;
    const esp_lcd_panel_io_i2c_config_t touch_io_config = {
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS,
        .scl_speed_hz = I2C_HZ,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 0,
        .flags = {
            .disable_control_phase = 1,
        },
    };
    esp_err_t err = esp_lcd_new_panel_io_i2c(s_i2c_bus, &touch_io_config,
                                             &touch_io);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "CST820 I2C IO unavailable: %s", esp_err_to_name(err));
        return;
    }

    const esp_lcd_touch_config_t touch_config = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = -1, // CST820 reset is XL9535 IO1
        .int_gpio_num = PIN_TOUCH_IRQ,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    esp_lcd_touch_handle_t touch = NULL;
    err = esp_lcd_touch_new_i2c_cst816s(touch_io, &touch_config, &touch);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "CST820 unavailable through CST816S driver: %s",
                 esp_err_to_name(err));
        return;
    }
    if (lvgl_port_add_touch(&(lvgl_port_touch_cfg_t){
            .disp = display,
            .handle = touch,
        }) == NULL) {
        ESP_LOGW(TAG, "failed to attach CST820 to LVGL");
    }
}

lv_display_t *board_display_start(void) {
    const gpio_config_t backlight_config = {
        .pin_bit_mask = 1ULL << PIN_LCD_BL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&backlight_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "configure AW9364 backlight: %s", esp_err_to_name(err));
        return NULL;
    }
    gpio_set_level(PIN_LCD_BL, 0);
    s_backlight_level = 0;

    err = i2c_bus_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "create I2C bus: %s", esp_err_to_name(err));
        return NULL;
    }
    err = expander_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "initialize XL9535-compatible expander: %s",
                 esp_err_to_name(err));
        return NULL;
    }

    esp_lcd_panel_io_handle_t panel_io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    err = panel_init(&panel_io, &panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "initialize LILYGO T-RGB panel: %s",
                 esp_err_to_name(err));
        return NULL;
    }

    lv_display_t *display = lvgl_bringup(panel_io, panel);
    if (display == NULL) {
        ESP_LOGE(TAG, "failed to create LVGL RGB display");
        return NULL;
    }
    touch_bringup(display);
    ESP_LOGI(TAG, "LILYGO T-RGB H597 profile=%s %ux%u @ %lu Hz",
             s_profile.name, LCD_H_RES, LCD_V_RES,
             (unsigned long)s_profile.pclk_hz);
    return display;
}

void board_display_backlight_set_brightness(uint8_t level) {
    if (level > 16) {
        level = 16;
    }

    portENTER_CRITICAL(&s_backlight_lock);
    if (level == s_backlight_level) {
        portEXIT_CRITICAL(&s_backlight_lock);
        return;
    }
    if (level == 0) {
        gpio_set_level(PIN_LCD_BL, 0);
        esp_rom_delay_us(3000);
        s_backlight_level = 0;
        portEXIT_CRITICAL(&s_backlight_lock);
        return;
    }
    if (s_backlight_level == 0) {
        gpio_set_level(PIN_LCD_BL, 1);
        s_backlight_level = 16;
        esp_rom_delay_us(30);
    }

    const uint8_t from = (uint8_t)(16 - s_backlight_level);
    const uint8_t to = (uint8_t)(16 - level);
    const uint8_t pulses = (uint8_t)((16 + to - from) % 16);
    for (uint8_t i = 0; i < pulses; ++i) {
        gpio_set_level(PIN_LCD_BL, 0);
        gpio_set_level(PIN_LCD_BL, 1);
    }
    s_backlight_level = level;
    portEXIT_CRITICAL(&s_backlight_lock);
}

void board_display_backlight_on(void) {
    board_display_backlight_set_brightness(16);
}

void board_display_backlight_off(void) {
    board_display_backlight_set_brightness(0);
}

bool board_display_lock(uint32_t timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void board_display_unlock(void) {
    lvgl_port_unlock();
}
