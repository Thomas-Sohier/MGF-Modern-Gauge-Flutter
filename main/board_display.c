#include "board_display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_log.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_io_additions.h" // SPI 3 fils (init ST7701) via expander
#include "esp_lcd_st7701.h"
#include "esp_io_expander_tca9554.h"
#include "esp_lcd_touch_cst816s.h"      // CST820 compatible pilote CST816S
#include "esp_lvgl_port.h"

static const char *TAG = "board_disp";

// ─────────────────────────────────────────────────────────────────────────────
// Brochage — Waveshare ESP32-S3-Touch-LCD-2.1 (source : wiki Waveshare).
// ─────────────────────────────────────────────────────────────────────────────

// Bus I²C partagé : tactile CST820 + expander TCA9554 (+ IMU/RTC non utilisés).
#define I2C_PORT            I2C_NUM_0
#define PIN_I2C_SDA         15
#define PIN_I2C_SCL         7
#define I2C_HZ              400000

// Expander TCA9554 (adresse 0x20 = A2A1A0 = 000).
#define EXIO_LCD_RST        IO_EXPANDER_PIN_NUM_0 // EXIO1 sur la sérigraphie
#define EXIO_TP_RST         IO_EXPANDER_PIN_NUM_1 // EXIO2
#define EXIO_LCD_CS         IO_EXPANDER_PIN_NUM_2 // EXIO3
// NB : la sérigraphie note EXIO1..8 ; l'API TCA9554 indexe IO0..7 -> décalage 1.

// SPI 3 fils d'initialisation du ST7701 (CS porté par l'expander).
#define PIN_LCD_SDA         1
#define PIN_LCD_SCL         2

// Bus RGB (RGB565 : 5 bits B, 6 bits G, 5 bits R).
#define PIN_PCLK            41
#define PIN_DE              40
#define PIN_VSYNC           39
#define PIN_HSYNC           38
#define PIN_BL              6

// Ordre attendu par esp_lcd_rgb : B0..B4, G0..G5, R0..R4.
#define RGB_DATA_GPIOS      { \
    5, 45, 48, 47, 21,        /* B0..B4 */ \
    14, 13, 12, 11, 10, 9,    /* G0..G5 */ \
    46, 3, 8, 18, 17          /* R0..R4 */ }

// Tactile CST820 (I²C, reset via expander).
#define PIN_TP_INT          16

#define LCD_H_RES           480
#define LCD_V_RES           480

static esp_io_expander_handle_t s_expander = NULL;

// ─────────────────────────────────────────────────────────────────────────────

static esp_err_t i2c_bus_init(void) {
    const i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(I2C_PORT, &conf), TAG, "i2c cfg");
    return i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
}

static esp_err_t expander_init(void) {
    ESP_RETURN_ON_ERROR(
        esp_io_expander_new_i2c_tca9554(I2C_PORT,
                                        ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000,
                                        &s_expander),
        TAG, "tca9554");

    // LCD_RST, TP_RST, LCD_CS en sortie.
    esp_io_expander_set_dir(s_expander,
                            EXIO_LCD_RST | EXIO_TP_RST | EXIO_LCD_CS,
                            IO_EXPANDER_OUTPUT);
    // CS haut (inactif) ; resets relâchés puis pulsés.
    esp_io_expander_set_level(s_expander, EXIO_LCD_CS, 1);
    esp_io_expander_set_level(s_expander, EXIO_LCD_RST | EXIO_TP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    esp_io_expander_set_level(s_expander, EXIO_LCD_RST | EXIO_TP_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}

static esp_err_t panel_init(esp_lcd_panel_io_handle_t *out_io,
                            esp_lcd_panel_handle_t *out_panel) {
    // 1. IO SPI 3 fils pour la séquence d'init du ST7701 (CS sur l'expander).
    const spi_line_config_t line = {
        .cs_io_type = IO_TYPE_EXPANDER,
        .cs_gpio_num = EXIO_LCD_CS,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = PIN_LCD_SCL,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = PIN_LCD_SDA,
        .io_expander = s_expander,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_cfg = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line, 0);
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_3wire_spi(&io_cfg, out_io), TAG, "io 3wire");

    // 2. Panneau RGB 480×480, framebuffers en PSRAM + bounce buffer.
    const esp_lcd_rgb_panel_config_t rgb_cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .psram_trans_align = 64,
        .data_width = 16, // RGB565
        .bits_per_pixel = 16,
        .de_gpio_num = PIN_DE,
        .pclk_gpio_num = PIN_PCLK,
        .vsync_gpio_num = PIN_VSYNC,
        .hsync_gpio_num = PIN_HSYNC,
        .disp_gpio_num = -1,
        .data_gpio_nums = RGB_DATA_GPIOS,
        .timings = {
            .pclk_hz = 16 * 1000 * 1000,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            // ⚠ Porches/pulses à VALIDER contre la démo Waveshare si l'image
            // « roule » ou est décalée.
            .hsync_pulse_width = 8,
            .hsync_back_porch = 10,
            .hsync_front_porch = 20,
            .vsync_pulse_width = 8,
            .vsync_back_porch = 10,
            .vsync_front_porch = 20,
            .flags.pclk_active_neg = true,
        },
        .num_fbs = 2,                          // double buffer
        .bounce_buffer_size_px = LCD_H_RES * 10,
        .flags = {
            .fb_in_psram = true,
        },
    };

    // Le ST7701 encapsule la config RGB. init_cmds NULL -> séquence par défaut du
    // composant. ⚠ Si l'écran reste noir/brouillé, fournir ici la séquence
    // d'init spécifique du panneau Waveshare (tableau st7701_lcd_init_cmd_t).
    const st7701_vendor_config_t vendor_cfg = {
        .rgb_config = &rgb_cfg,
        .flags = {
            .mirror_by_cmd = 0,
            .auto_del_panel_io = 0,
        },
    };
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1, // reset géré via l'expander (EXIO_LCD_RST)
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = (void *)&vendor_cfg,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7701(*out_io, &panel_cfg, out_panel),
                        TAG, "st7701");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*out_panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*out_panel), TAG, "init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(*out_panel, true), TAG, "on");
    return ESP_OK;
}

static lv_display_t *lvgl_bringup(esp_lcd_panel_io_handle_t io,
                                  esp_lcd_panel_handle_t panel) {
    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
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
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .bb_mode = true,        // bounce buffer
            .avoid_tearing = true,
        },
    };
    return lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);
}

static void touch_bringup(lv_display_t *disp) {
    esp_lcd_panel_io_handle_t tp_io = NULL;
    const esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_PORT,
                                             &tp_io_cfg, &tp_io));

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = -1, // TP_RST via l'expander (déjà relâché)
        .int_gpio_num = PIN_TP_INT,
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };
    esp_lcd_touch_handle_t tp = NULL;
    if (esp_lcd_touch_new_i2c_cst816s(tp_io, &tp_cfg, &tp) != ESP_OK) {
        ESP_LOGW(TAG, "tactile CST820 non initialisé (pilote CST816S)");
        return;
    }
    lvgl_port_add_touch(&(lvgl_port_touch_cfg_t){.disp = disp, .handle = tp});
}

// ─────────────────────────────────────────────────────────────────────────────

lv_display_t *board_display_start(void) {
    // Rétroéclairage éteint le temps de l'init.
    gpio_config_t bl = {
        .pin_bit_mask = 1ULL << PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bl);
    gpio_set_level(PIN_BL, 0);

    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(expander_init());

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(panel_init(&io, &panel));

    lv_display_t *disp = lvgl_bringup(io, panel);
    touch_bringup(disp);
    return disp;
}

void board_display_backlight_on(void)  { gpio_set_level(PIN_BL, 1); }
void board_display_backlight_off(void) { gpio_set_level(PIN_BL, 0); }

bool board_display_lock(uint32_t timeout_ms) { return lvgl_port_lock(timeout_ms); }
void board_display_unlock(void)             { lvgl_port_unlock(); }
