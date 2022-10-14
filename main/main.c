#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_intr_alloc.h"
#include "usb/usb_host.h"
#include "usb.h"
#include "keyboard.h"
#include "kvm.h"
#include "ddc.h"
#include "ble.h"
#include "wifi.h"
#include "led.h"
#include "webserver.h"
#include "config.h"

void app_main(void)
{
    keyboard_handle_t kbd_handle;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    esp_vfs_spiffs_conf_t spiffs = 
    {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
 
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffs));

    ESP_ERROR_CHECK(usb_preinit());
    ESP_ERROR_CHECK(keyboard_preinit());
    ESP_ERROR_CHECK(kvm_preinit());

    ESP_ERROR_CHECK(led_init());
    ESP_ERROR_CHECK(config_init());
    ESP_ERROR_CHECK(ddc_init());
    ESP_ERROR_CHECK(usb_init());
    ESP_ERROR_CHECK(keyboard_init(&kbd_handle));
    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(webserver_init());
    ESP_ERROR_CHECK(ble_init());
    ESP_ERROR_CHECK(kvm_init());
}
