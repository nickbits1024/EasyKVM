#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "led_int.h"
#include "led.h"

#define TAG "LED"

bool led_enabled = true;
uint8_t last_r;
uint8_t last_g;
uint8_t last_b;
led_strip_t* led_strip;

esp_err_t led_init()
{
    gpio_config_t io_conf;

    io_conf.pin_bit_mask = LED_GPIO_SEL;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    ESP_ERROR_CHECK(gpio_config(&io_conf));


    led_strip = led_strip_init(LED_CHANNEL, LED_GPIO_NUM, 1);
    ets_delay_us(5000);

    ESP_ERROR_CHECK(led_strip->set_pixel(led_strip, 0, 0, 0, 0));
    ESP_ERROR_CHECK(led_strip->refresh(led_strip, 100));

    return ESP_OK;
}

esp_err_t led_enable(bool enable)
{
    led_enabled = enable;

    if (enable)
    {
        ESP_ERROR_CHECK(led_strip->set_pixel(led_strip, 0, last_r, last_g, last_b));
        ESP_ERROR_CHECK(led_strip->refresh(led_strip, 100));
    }
    else
    {
        ESP_ERROR_CHECK(led_strip->clear(led_strip, 100));
    }

    return ESP_OK;
}

esp_err_t led_set_rgb_color(uint8_t r, uint8_t g, uint8_t b)
{
    ESP_LOGI(TAG, "led set (%d, %d, %d) en=%d", r, g, b, led_enabled);
    if (led_enabled)
    {
        ESP_ERROR_CHECK(led_strip->set_pixel(led_strip, 0, r, g, b));
        ESP_ERROR_CHECK(led_strip->refresh(led_strip, 100));
    }

    last_r = r;
    last_g = g;
    last_b = b;

    return ESP_OK;
}