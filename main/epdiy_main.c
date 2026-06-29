/* Minute clock for the epdiy e-paper photo frame (ESP32-S3, pure ESP-IDF).
 *
 * Shows the current time as HH:MM and refreshes once per minute, aligned to the
 * minute boundary. Time comes from the system clock, which starts at
 * 1970-01-01 00:00 on boot — real wall-clock time needs NTP (WiFi) or an RTC,
 * to be added later. The original epdiy demo is kept as epdiy_demo_main.c.bak.
 */

#include <time.h>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

#include <epdiy.h>

#include "sdkconfig.h"

#include "font/firasans_12.h"
#include "font/firasans_20.h"
#include "font/firasans_96.h"

#define WAVEFORM EPD_BUILTIN_WAVEFORM

// choose the default demo board depending on the architecture
#ifdef CONFIG_IDF_TARGET_ESP32
#define TARGET_BOARD epd_board_v6
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define TARGET_BOARD epd_board_v7
#endif

static const char* TAG = "epdiy";

EpdiyHighlevelState hl;

const EpdDisplay_t ES080FC = {
    .width = 1800,
    .height = 600,
    .bus_width = 16,
    .bus_speed = 17,
    .default_waveform = &epdiy_ED097TC2,
    .display_type = DISPLAY_TYPE_GENERIC,
};
const EpdDisplay_t ES108FC = {
    .width = 1920,
    .height = 1080,
    .bus_width = 16,
    .bus_speed = 17,
    .default_waveform = &epdiy_ED047TC1,
    .display_type = DISPLAY_TYPE_GENERIC,
};
const EpdDisplay_t ES108FC2 = {
    .width = 1920,
    .height = 1080,
    .bus_width = 16,
    // 11 MHz: the seller's EC080SC2 example uses this on the same V7 16-bit board.
    // The generic demo's 17 MHz overruns this board's bus/PSRAM feed and causes
    // "line buffer underrun" / "draw error: 400" on full-frame draws.
    .bus_speed = 11,
    .default_waveform = &epdiy_ED047TC1,
    .display_type = DISPLAY_TYPE_GENERIC,
};
const EpdDisplay_t ES120 = {
    .width = 2560,
    .height = 1600,
    .bus_width = 16,
    .bus_speed = 12,
    .default_waveform = &epdiy_ED047TC1,
    .display_type = DISPLAY_TYPE_GENERIC,
};
const EpdDisplay_t ED060KD1 = {
    .width = 1448,
    .height = 1072,
    .bus_width = 8,
    .bus_speed = 20,
    .default_waveform = &epdiy_ED060SCT,
    .display_type = DISPLAY_TYPE_GENERIC,
};
const EpdDisplay_t EC080SC2 = {
    .width = 1800,
    .height = 800,
    .bus_width = 16,
    .bus_speed = 11,
    .default_waveform = &epdiy_ED047TC2,
    .display_type = DISPLAY_TYPE_GENERIC,
};

// The active panel is chosen in menuconfig (Photo Frame Configuration ->
// "E-paper display model"); defaults to ES108FC. See main/Kconfig.projbuild.
static const EpdDisplay_t* selected_display(void) {
#if defined(CONFIG_EPDIY_DISPLAY_ES080FC)
    return &ES080FC;
#elif defined(CONFIG_EPDIY_DISPLAY_ES120)
    return &ES120;
#elif defined(CONFIG_EPDIY_DISPLAY_ED060KD1)
    return &ED060KD1;
#elif defined(CONFIG_EPDIY_DISPLAY_EC080SC2)
    return &EC080SC2;
#elif defined(CONFIG_EPDIY_DISPLAY_ES108FC)
    return &ES108FC;
#else  // CONFIG_EPDIY_DISPLAY_ES108FC2 (default)
    return &ES108FC2;
#endif
}

static inline void checkError(enum EpdDrawError err) {
    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "draw error: %X", err);
    }
}

void idf_setup() {
    epd_init(&TARGET_BOARD, selected_display(), EPD_LUT_64K);

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

// Pick a font that suits the panel width
static const EpdFont* clock_font(void) {
    return epd_width() < 1000 ? &FiraSans_20 : &FiraSans_96;
}

void app_main(void) {
    idf_setup();

    // Start from a clean white screen and keep epdiy's framebuffer in sync.
    epd_poweron();
    epd_clear();
    epd_poweroff();
    epd_hl_set_all_white(&hl);

    const int w = epd_rotated_display_width();
    const int h = epd_rotated_display_height();

    // Region we repaint each minute (kept generous so the time always fits).
    EpdRect clock_area = {
        .x = w / 2 - 300,
        .y = h / 2 - 80,
        .width = 600,
        .height = 160,
    };

    while (1) {
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);

        char hhmm[8];
        snprintf(hhmm, sizeof(hhmm), "%02d:%02d", tm_now.tm_hour, tm_now.tm_min);

        uint8_t* fb = epd_hl_get_framebuffer(&hl);
        epd_fill_rect(clock_area, 0xFF, fb);  // clear the clock area to white

        EpdFontProperties props = epd_font_properties_default();
        props.flags = EPD_DRAW_ALIGN_CENTER;
        int cursor_x = w / 2;
        int cursor_y = h / 2 + 10;  // rough vertical centering for a single line
        epd_write_string(clock_font(), hhmm, &cursor_x, &cursor_y, fb, &props);

        epd_poweron();
        int temperature = epd_ambient_temperature();
        // Repaint just the clock area each minute (no full-screen flash); do a
        // flashing full refresh on the hour to clear any accumulated ghosting.
        if (tm_now.tm_min == 0) {
            checkError(epd_hl_update_screen(&hl, MODE_GC16, temperature));
        } else {
            checkError(epd_hl_update_area(&hl, MODE_GL16, temperature, clock_area));
        }
        epd_poweroff();

        ESP_LOGI(TAG, "clock %s (temp %d)", hhmm, temperature);

        // Sleep until the start of the next minute.
        now = time(NULL);
        int secs_to_next_minute = 60 - (int)(now % 60);
        vTaskDelay(pdMS_TO_TICKS(secs_to_next_minute * 1000));
    }
}
