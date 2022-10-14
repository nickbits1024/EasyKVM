#ifndef LED_H
#define LED_H

esp_err_t led_init();
esp_err_t led_enable(bool enable);
esp_err_t led_set_rgb_color(uint8_t r, uint8_t g, uint8_t b);

#endif