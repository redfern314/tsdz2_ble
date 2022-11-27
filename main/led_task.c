#include "led_task.h"
#include "led_strip.h"
#include "nimble/nimble_port_freertos.h"

// Don't strictly need a mutex for these as reads and writes should be atomic, but
// adding one for good practice.
static uint8_t g_red = 0;
static uint8_t g_green = 0;
static uint8_t g_blue = 0;
static uint32_t g_delay = 50;
static SemaphoreHandle_t g_mutex;

void runLedTask(void* pvParameters) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = 8,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);

    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == NULL) {
        return;
    }

    color_t state = RED;
    const int max_intens = 20;
    int cur_intens = max_intens;
    int next_intens = 0;
    uint8_t static_red;
    uint8_t static_green;
    uint8_t static_blue;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint32_t delay;

    while(true) {
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        static_red = g_red;
        static_green = g_green;
        static_blue = g_blue;
        delay = g_delay;
        xSemaphoreGive(g_mutex);

        if (delay == 0) {
            led_strip_set_pixel(led_strip, 0, static_red, static_green, static_blue);
        } else {
            if (next_intens < max_intens) {
                next_intens++;
                cur_intens--;
            } else {
                cur_intens = next_intens;
                next_intens = 0;
                state++;
                if (state == NO_COLOR) {
                    state = RED;
                }
            }
            switch (state) {
                case RED:
                    red = cur_intens;
                    green = next_intens;
                    blue = 0;
                    break;
                case GREEN:
                    red = 0;
                    green = cur_intens;
                    blue = next_intens;
                    break;
                case BLUE:
                    red = next_intens;
                    green = 0;
                    blue = cur_intens;
                    break;
                default:
                    red = 0;
                    green = 0;
                    blue = 0;
                    break;
            }
            led_strip_set_pixel(led_strip, 0, red, green, blue);
        }
        /* Refresh the strip to send data */
        led_strip_refresh(led_strip);
        vTaskDelay((delay == 0 ? 100 : delay) / portTICK_PERIOD_MS);
    }
}

void setColor(color_t color, uint8_t val) {
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    switch(color) {
        case RED:
            g_red = val;
            break;
        case GREEN:
            g_green = val;
            break;
        case BLUE:
            g_blue = val;
            break;
        default:
            break;
    }
    xSemaphoreGive(g_mutex);
}

uint8_t getColor(color_t color) {
    uint8_t val = 0;
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    switch(color) {
        case RED:
            val = g_red;
            break;
        case GREEN:
            val = g_green;
            break;
        case BLUE:
            val = g_blue;
            break;
        default:
            break;
    }
    xSemaphoreGive(g_mutex);
    return val;
}

uint32_t getDelay() {
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    uint8_t val = g_delay;
    xSemaphoreGive(g_mutex);
    return val;
}

void setDelay(uint32_t ms) {
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    g_delay = ms;
    xSemaphoreGive(g_mutex);
}
