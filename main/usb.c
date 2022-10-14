#include "usb/usb_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "usb_int.h"
#include "usb.h"

#define TAG "USB"

static void usb_host_lib_task(void* arg);

static void usb_host_lib_task(void* arg)
{
    for (;;)
    {
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, NULL));
    }
}

esp_err_t usb_preinit()
{
    gpio_config_t io_conf;

    io_conf.pin_bit_mask = USB_HUB_RESET_GPIO_SEL;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    return ESP_OK;
}

esp_err_t usb_init()
{
    // don't reset too soon
    //vTaskDelay(1000 / portTICK_PERIOD_MS);
    //usb_reset();

    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    xTaskCreate(usb_host_lib_task, "usb_host_lib_task", 4096, NULL, 1, NULL);

    return ESP_OK;
}

// void usb_reset()
// {
//     gpio_set_level(USB_HUB_RESET_GPIO_NUM, 1);
//     ets_delay_us(1000);
//     gpio_set_level(USB_HUB_RESET_GPIO_NUM, 0);
//     ets_delay_us(1000);
//     gpio_set_level(USB_HUB_RESET_GPIO_NUM, 1);
// }