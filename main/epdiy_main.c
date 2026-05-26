/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "led_strip.h"
#include "sdkconfig.h"

#include "epdiy.h"

#include "firasans_12.h"
#include "firasans_20.h"
#include "img_beach.h"
#include "img_board.h"
#include "img_zebra.h"

#define WAVEFORM EPD_BUILTIN_WAVEFORM

// choose the default demo board depending on the architecture
#ifdef CONFIG_IDF_TARGET_ESP32
#define DEMO_BOARD epd_board_v6
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define DEMO_BOARD epd_board_v7
#endif

EpdiyHighlevelState hl;

const EpdDisplay_t ES108FC = {
    .width = 1920,
    .height = 1080,
    .bus_width = 16,
    .bus_speed = 17,
    .default_waveform = &epdiy_ED047TC1,
    .display_type = DISPLAY_TYPE_GENERIC,
};


void idf_setup() {
    epd_init(&DEMO_BOARD, &ES108FC, EPD_LUT_64K);
    
    // Set VCOM for boards that allow to set this in software (in mV).
    // This will print an error if unsupported. In this case,
    // set VCOM using the hardware potentiometer and delete this line.
    epd_set_vcom(1560);

    hl = epd_hl_init(WAVEFORM);

    // Default orientation is EPD_ROT_LANDSCAPE
    epd_set_rotation(EPD_ROT_LANDSCAPE);

    printf(
        "Dimensions after rotation, width: %d height: %d\n\n", epd_rotated_display_width(),
        epd_rotated_display_height()
    );

    heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);
    heap_caps_print_heap_info(MALLOC_CAP_SPIRAM);
}

void delay(uint32_t millis) {
    vTaskDelay(millis / portTICK_PERIOD_MS);
}

static inline void checkError(enum EpdDrawError err) {
    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE("demo", "draw error: %X", err);
    }
}

void draw_progress_bar(int x, int y, int width, int percent, uint8_t* fb) {
    const uint8_t white = 0xFF;
    const uint8_t black = 0x0;

    EpdRect border = {
        .x = x,
        .y = y,
        .width = width,
        .height = 20,
    };
    epd_fill_rect(border, white, fb);
    epd_draw_rect(border, black, fb);

    EpdRect bar = {
        .x = x + 5,
        .y = y + 5,
        .width = (width - 10) * percent / 100,
        .height = 10,
    };

    epd_fill_rect(bar, black, fb);

    checkError(epd_hl_update_area(&hl, MODE_DU, epd_ambient_temperature(), border));
}

void idf_loop() {
    // select the font based on display width
    const EpdFont* font;
    if (epd_width() < 1000) {
        font = &FiraSans_12;
    } else {
        font = &FiraSans_20;
    }

    uint8_t* fb = epd_hl_get_framebuffer(&hl);

    epd_poweron();
    epd_clear();
    int temperature = epd_ambient_temperature();
    epd_poweroff();

    printf("current temperature: %d\n", temperature);

    epd_fill_circle(30, 30, 15, 0, fb);
    int cursor_x = epd_rotated_display_width() / 2;
    int cursor_y = epd_rotated_display_height() / 2 - 100;

    EpdFontProperties font_props = epd_font_properties_default();
    font_props.flags = EPD_DRAW_ALIGN_CENTER;

    char srotation[32];
    sprintf(srotation, "Loading demo...\nRotation: %d", epd_get_rotation());

    epd_write_string(font, srotation, &cursor_x, &cursor_y, fb, &font_props);

    int bar_x = epd_rotated_display_width() / 2 - 200;
    int bar_y = epd_rotated_display_height() / 2;

    epd_poweron();
    checkError(epd_hl_update_screen(&hl, MODE_GL16, temperature));
    epd_poweroff();

    for (int i = 0; i < 6; i++) {
        epd_poweron();
        draw_progress_bar(bar_x, bar_y, 400, i * 10, fb);
        epd_poweroff();
    }

    cursor_x = epd_rotated_display_width() / 2;
    cursor_y = epd_rotated_display_height() / 2 + 100;

    epd_write_string(
        font, "Just kidding,\n this is a demo animation 😉", &cursor_x, &cursor_y, fb, &font_props
    );
    epd_poweron();
    checkError(epd_hl_update_screen(&hl, MODE_GL16, temperature));
    epd_poweroff();

    for (int i = 0; i < 6; i++) {
        epd_poweron();
        draw_progress_bar(bar_x, bar_y, 400, 50 - i * 10, fb);
        epd_poweroff();
        vTaskDelay(1);
    }

    cursor_y = epd_rotated_display_height() / 2 + 200;
    cursor_x = epd_rotated_display_width() / 2;

    EpdRect clear_area = {
        .x = 0,
        .y = epd_rotated_display_height() / 2 + 100,
        .width = epd_rotated_display_width(),
        .height = 300,
    };

    epd_fill_rect(clear_area, 0xFF, fb);

    epd_write_string(
        font, "Now let's look at some pictures.", &cursor_x, &cursor_y, fb, &font_props
    );
    epd_poweron();
    checkError(epd_hl_update_screen(&hl, MODE_GL16, temperature));
    epd_poweroff();

    delay(1000);

    epd_hl_set_all_white(&hl);

    EpdRect zebra_area = {
        .x = epd_rotated_display_width() / 2 - img_zebra_width / 2,
        .y = epd_rotated_display_height() / 2 - img_zebra_height / 2,
        .width = img_zebra_width,
        .height = img_zebra_height,
    };

    epd_draw_rotated_image(zebra_area, img_zebra_data, fb);
    epd_poweron();
    checkError(epd_hl_update_screen(&hl, MODE_GC16, temperature));
    epd_poweroff();

    delay(5000);

    EpdRect board_area = {
        .x = epd_rotated_display_width() / 2 - img_board_width / 2,
        .y = epd_rotated_display_height() / 2 - img_board_height / 2,
        .width = img_board_width,
        .height = img_board_height,
    };

    epd_draw_rotated_image(board_area, img_board_data, fb);
    cursor_x = epd_rotated_display_width() / 2;
    cursor_y = board_area.y;
    font_props.flags |= EPD_DRAW_BACKGROUND;
    epd_write_string(font, "↓ Thats the V2 board. ↓", &cursor_x, &cursor_y, fb, &font_props);

    epd_poweron();
    checkError(epd_hl_update_screen(&hl, MODE_GC16, temperature));
    epd_poweroff();

    delay(5000);
    epd_hl_set_all_white(&hl);

    EpdRect border_rect = {
        .x = 20,
        .y = 20,
        .width = epd_rotated_display_width() - 40,
        .height = epd_rotated_display_height() - 40};
    epd_draw_rect(border_rect, 0, fb);

    cursor_x = 50;
    cursor_y = 100;

    epd_write_default(
        font,
        "➸ 16 color grayscale\n"
        "➸ ~250ms - 1700ms for full frame draw 🚀\n"
        "➸ Use with 6\" or 9.7\" EPDs\n"
        "➸ High-quality font rendering ✎🙋\n"
        "➸ Partial update\n"
        "➸ Arbitrary transitions with vendor waveforms",
        &cursor_x, &cursor_y, fb
    );

    EpdRect img_beach_area = {
        .x = 0,
        .y = epd_rotated_display_height() - img_beach_height,
        .width = img_beach_width,
        .height = img_beach_height,
    };

    epd_draw_rotated_image(img_beach_area, img_beach_data, fb);

    epd_poweron();
    checkError(epd_hl_update_screen(&hl, MODE_GC16, temperature));
    epd_poweroff();

    printf("going to sleep...\n");
    epd_deinit();
    esp_deep_sleep_start();
}

static const char *TAG = "example";

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

static uint8_t s_led_state = 0;

#ifdef CONFIG_BLINK_LED_STRIP

static led_strip_handle_t led_strip;

static void blink_led(void)
{
    /* If the addressable LED is enabled */
    if (s_led_state) {
        /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
        led_strip_set_pixel(led_strip, 0, 16, 16, 16);
        /* Refresh the strip to send data */
        led_strip_refresh(led_strip);
    } else {
        /* Set all LED off to clear all pixels */
        led_strip_clear(led_strip);
    }
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1, // at least one LED on board
    };
#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
#error "unsupported LED strip backend"
#endif
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}

#elif CONFIG_BLINK_LED_GPIO

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

#else
#error "unsupported LED type"
#endif

void app_main(void)
{

    /* Configure the peripheral according to the LED type */
    configure_led();

    while (1) {
        ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
        blink_led();
        /* Toggle the LED state */
        s_led_state = !s_led_state;
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}
