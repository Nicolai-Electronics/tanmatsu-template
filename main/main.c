#include <stdio.h>
#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/led.h"
#include "bsp/power.h"
#include "custom_certificates.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "esp_log.h"
#include "hal/lcd_types.h"
#include "nvs_flash.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "portmacro.h"
#include "wifi_connection.h"
#include "wifi_remote.h"

// Constants
static char const TAG[] = "main";

// Global variables
static size_t                       display_h_res        = 0;
static size_t                       display_v_res        = 0;
static lcd_color_rgb_pixel_format_t display_color_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
static lcd_rgb_data_endian_t        display_data_endian  = LCD_RGB_DATA_ENDIAN_LITTLE;
static pax_buf_t                    fb                   = {0};
static QueueHandle_t                input_event_queue    = NULL;

#if defined(CONFIG_BSP_TARGET_KAMI)
// Temporary addition for supporting epaper devices (irrelevant for Tanmatsu)
static pax_col_t palette[] = {0xffffffff, 0xff000000, 0xffff0000};  // white, black, red
#endif

void blit(void) {
    bsp_display_blit(0, 0, display_h_res, display_v_res, pax_buf_get_pixels(&fb));
}

void app_main(void) {
    // Start the GPIO interrupt service
    gpio_install_isr_service(0);

    // Initialize the Non Volatile Storage service
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        res = nvs_flash_init();
    }
    ESP_ERROR_CHECK(res);

    // Initialize the Board Support Package
    ESP_ERROR_CHECK(bsp_device_initialize());

    uint8_t led_data[] = {
        0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF,
    };
    bsp_led_write(led_data, sizeof(led_data));

    // Get display parameters and rotation
    res = bsp_display_get_parameters(&display_h_res, &display_v_res, &display_color_format, &display_data_endian);
    ESP_ERROR_CHECK(res);  // Check that the display parameters have been initialized
    bsp_display_rotation_t display_rotation = bsp_display_get_default_rotation();

    // Convert ESP-IDF color format into PAX buffer type
    pax_buf_type_t format = PAX_BUF_24_888RGB;
    switch (display_color_format) {
        case LCD_COLOR_PIXEL_FORMAT_RGB565:
            format = PAX_BUF_16_565RGB;
            break;
        case LCD_COLOR_PIXEL_FORMAT_RGB888:
            format = PAX_BUF_24_888RGB;
            break;
        default:
            break;
    }

    // Convert BSP display rotation format into PAX orientation type
    pax_orientation_t orientation = PAX_O_UPRIGHT;
    switch (display_rotation) {
        case BSP_DISPLAY_ROTATION_90:
            orientation = PAX_O_ROT_CCW;
            break;
        case BSP_DISPLAY_ROTATION_180:
            orientation = PAX_O_ROT_HALF;
            break;
        case BSP_DISPLAY_ROTATION_270:
            orientation = PAX_O_ROT_CW;
            break;
        case BSP_DISPLAY_ROTATION_0:
        default:
            orientation = PAX_O_UPRIGHT;
            break;
    }

        // Initialize graphics stack
#if defined(CONFIG_BSP_TARGET_KAMI)
    // Temporary addition for supporting epaper devices (irrelevant for Tanmatsu)
    format = PAX_BUF_2_PAL;
#endif
    pax_buf_init(&fb, NULL, display_h_res, display_v_res, format);
    pax_buf_reversed(&fb, display_data_endian == LCD_RGB_DATA_ENDIAN_BIG);
#if defined(CONFIG_BSP_TARGET_KAMI)
    // Temporary addition for supporting epaper devices (irrelevant for Tanmatsu)
    fb.palette      = palette;
    fb.palette_size = sizeof(palette) / sizeof(pax_col_t);
#endif
    pax_buf_set_orientation(&fb, orientation);

#if defined(CONFIG_BSP_TARGET_KAMI)
#define BLACK 0
#define WHITE 1
#define RED   2
#else
#define BLACK 0xFF000000
#define WHITE 0xFFFFFFFF
#define RED   0xFFFF0000
#endif

    // Get input event queue from BSP
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    // Start WiFi stack (if your app does not require WiFi or BLE you can remove this section)
    pax_background(&fb, WHITE);
    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 0, "Connecting to radio...");
    blit();

    if (wifi_remote_initialize() == ESP_OK) {

        pax_background(&fb, WHITE);
        pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 0, "Starting WiFi stack...");
        blit();
        wifi_connection_init_stack();  // Start the Espressif WiFi stack

        pax_background(&fb, WHITE);
        pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 0, "Connecting to WiFi network...");
        blit();

        if (wifi_connect_try_all() == ESP_OK) {
            pax_background(&fb, WHITE);
            pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 0, "Succesfully connected to WiFi network");
            blit();
        } else {
            pax_background(&fb, RED);
            pax_draw_text(&fb, WHITE, pax_font_sky_mono, 16, 0, 0, "Failed to connect to WiFi network");
            blit();
        }
    } else {
        bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
        ESP_LOGE(TAG, "WiFi radio not responding, WiFi not available");
        pax_background(&fb, RED);
        pax_draw_text(&fb, WHITE, pax_font_sky_mono, 16, 0, 0, "WiFi unavailable");
        blit();
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    // Main section of the app

    // This example shows how to read from the BSP event queue to read input events

    // If you want to run something at an interval in this same main thread you can replace portMAX_DELAY with an amount
    // of ticks to wait, for example pdMS_TO_TICKS(1000)

    pax_background(&fb, WHITE);
    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 0, "Welcome! Press any key to trigger an event.");
    blit();

    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            bsp_led_write(led_data, sizeof(led_data));
            switch (event.type) {
                case INPUT_EVENT_TYPE_KEYBOARD: {
                    if (event.args_keyboard.ascii != '\b' ||
                        event.args_keyboard.ascii != '\t') {  // Ignore backspace & tab keyboard events
                        ESP_LOGI(TAG, "Keyboard event %c (%02x) %s", event.args_keyboard.ascii,
                                 (uint8_t)event.args_keyboard.ascii, event.args_keyboard.utf8);
                        pax_simple_rect(&fb, WHITE, 0, 0, pax_buf_get_width(&fb), 72);
                        pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 0, "Keyboard event");
                        char text[64];
                        snprintf(text, sizeof(text), "ASCII:     %c (0x%02x)", event.args_keyboard.ascii,
                                 (uint8_t)event.args_keyboard.ascii);
                        pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 18, text);
                        snprintf(text, sizeof(text), "UTF-8:     %s", event.args_keyboard.utf8);
                        pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 36, text);
                        snprintf(text, sizeof(text), "Modifiers: 0x%0" PRIX32, event.args_keyboard.modifiers);
                        pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 54, text);
                        blit();
                    }
                    break;
                }
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    ESP_LOGI(TAG, "Navigation event %0" PRIX32 ": %s", (uint32_t)event.args_navigation.key,
                             event.args_navigation.state ? "pressed" : "released");

                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F1) {
                        bsp_device_restart_to_launcher();
                    }
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F2) {
                        bsp_input_set_backlight_brightness(0);
                    }
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F3) {
                        bsp_input_set_backlight_brightness(100);
                    }

                    pax_simple_rect(&fb, WHITE, 0, 100, pax_buf_get_width(&fb), 72);
                    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 100 + 0, "Navigation event");
                    char text[64];
                    snprintf(text, sizeof(text), "Key:       0x%0" PRIX32, (uint32_t)event.args_navigation.key);
                    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 100 + 18, text);
                    snprintf(text, sizeof(text), "State:     %s", event.args_navigation.state ? "pressed" : "released");
                    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 100 + 36, text);
                    snprintf(text, sizeof(text), "Modifiers: 0x%0" PRIX32, event.args_navigation.modifiers);
                    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 100 + 54, text);
                    blit();
                    break;
                }
                case INPUT_EVENT_TYPE_ACTION: {
                    ESP_LOGI(TAG, "Action event 0x%0" PRIX32 ": %s", (uint32_t)event.args_action.type,
                             event.args_action.state ? "yes" : "no");
                    pax_simple_rect(&fb, WHITE, 0, 200 + 0, pax_buf_get_width(&fb), 72);
                    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 200 + 0, "Action event");
                    char text[64];
                    snprintf(text, sizeof(text), "Type:      0x%0" PRIX32, (uint32_t)event.args_action.type);
                    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 200 + 36, text);
                    snprintf(text, sizeof(text), "State:     %s", event.args_action.state ? "yes" : "no");
                    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 200 + 54, text);
                    blit();
                    break;
                }
                case INPUT_EVENT_TYPE_SCANCODE: {
                    ESP_LOGI(TAG, "Scancode event 0x%0" PRIX32, (uint32_t)event.args_scancode.scancode);
                    pax_simple_rect(&fb, WHITE, 0, 300 + 0, pax_buf_get_width(&fb), 72);
                    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 300 + 0, "Scancode event");
                    char text[64];
                    snprintf(text, sizeof(text), "Scancode:  0x%0" PRIX32, (uint32_t)event.args_scancode.scancode);
                    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 300 + 36, text);
                    blit();
                    break;
                }
                default:
                    break;
            }
        }
    }
}
