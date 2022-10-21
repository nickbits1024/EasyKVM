#include "usb/usb_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "usb_int.h"
#include "usb.h"
#include "led.h"

#define TAG "USB"

static void usb_host_lib_task(void* arg);

static void usb_host_lib_task(void* arg)
{
    for (;;)
    {
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, NULL));
    }
}

static void usb_oc_check_task(void* arg)
{
    bool over_current = false;
    bool was_over_current = false;
    uint8_t r, g, b;

    for (;;)
    {       
        over_current = gpio_get_level(USB_OC_GPIO_NUM) == 0;
        if (over_current)
        {
            ESP_LOGE(TAG, "overcurrent condition detected");
            was_over_current = true;
            usb_enable2(false);
            led_get_rgb_color(&r, &g, &b);
            led_set_rgb_color(255, 0, 0);
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
        else if (was_over_current)
        {
            ESP_LOGI(TAG, "overcurrent condition no longer detected");
            was_over_current = false;
            usb_enable2(true);
            led_set_rgb_color(r, g, b);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

esp_err_t usb_preinit()
{
    gpio_config_t io_conf;

    io_conf.pin_bit_mask = USB_OC_GPIO_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    io_conf.pin_bit_mask = USB_ENABLE_GPIO_SEL;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    return ESP_OK;
}

esp_err_t usb_init()
{
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    xTaskCreate(usb_host_lib_task, "usb_host_lib_task", 4096, NULL, 1, NULL);
    xTaskCreate(usb_oc_check_task, "usb_oc_check_task", 4096, NULL, 1, NULL);

    return ESP_OK;
}

void usb_enable2(bool enable)
{
    ESP_ERROR_CHECK(gpio_set_level(USB_ENABLE_GPIO_NUM, enable ? 1 : 0));
}