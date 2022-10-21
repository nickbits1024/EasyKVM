#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"
#include "usb/usb_helpers.h"
#include "driver/gpio.h"
#include "kvm_int.h"
#include "kvm.h"
#include "ddc.h"
#include "usb.h"
#include "led.h"
#include "config.h"

#define TAG "KVM"

static int8_t kvm_port;
static nvs_handle_t kvm_nvs_handle;
static kvm_state_t kvm_state;

void kvm_sync_port()
{
    uint16_t input;

    ESP_ERROR_CHECK(led_set_rgb_color(255, 0, 0));

    kvm_enable(false);

    config_t* config = (config_t*)malloc(sizeof(config_t));
    if (config == NULL)
    {
        return;
    }

    ESP_ERROR_CHECK(config_get(config));

    uint8_t r = 0, g = 0, b = 0;

    switch (kvm_port)
    {
    case 0:
        ESP_ERROR_CHECK(gpio_set_level(KVM_PORT_GPIO_NUM, 0));
        input = config->pc1_vcp;
        r = config->pc2_led_r;
        g = config->pc2_led_g;
        b = config->pc2_led_b;
        break;
    case 1:
        ESP_ERROR_CHECK(gpio_set_level(KVM_PORT_GPIO_NUM, 1));
        input = config->pc2_vcp;
        r = config->pc1_led_r;
        g = config->pc1_led_g;
        b = config->pc1_led_b;
        break;
    default:
        abort();
        break;
    }

    free(config);
    
    if (input != 0)
    {
        ddc_set_vcp(VCP_INPUT_SOURCE_FEATURE, input);
    }

    if (kvm_state.last_input != input)
    {
        kvm_state.last_input = input;
        kvm_save_state();
    }
    kvm_enable(true);

    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(led_set_rgb_color(r, g, b));
}

esp_err_t kvm_preinit()
{
    gpio_config_t io_conf;

    io_conf.pin_bit_mask = KVM_PORT_GPIO_SEL | KVM_ENABLE_GPIO_SEL;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    //ESP_ERROR_CHECK(gpio_set_level(KVM_PORT_GPIO_NUM, 1));

    return ESP_OK;
}

void kvm_reset()
{
    usb_enable2(false);
    kvm_enable(false);
    //usb_reset();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    usb_enable2(true);
    kvm_enable(true);
    kvm_sync_port();
}

esp_err_t kvm_load_state()
{
    size_t size = sizeof(kvm_state_t);
    esp_err_t ret = nvs_get_blob(kvm_nvs_handle, KVM_STATE_KEY, &kvm_state, &size);
    if (ret == ESP_OK && size == sizeof(kvm_state))
    {
        ESP_LOGI(TAG, "saved last_input = %d", kvm_state.last_input);

        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t kvm_save_state()
{
    ESP_ERROR_CHECK(nvs_set_blob(kvm_nvs_handle, KVM_STATE_KEY, &kvm_state, sizeof(kvm_state_t)));
    ESP_ERROR_CHECK(nvs_commit(kvm_nvs_handle));

    return ESP_OK;
}

esp_err_t kvm_init()
{
    ESP_RETURN_ON_ERROR(nvs_open(KVM_NVS_NAME, NVS_READWRITE, &kvm_nvs_handle),
        TAG, "nvs_open failed (%s)", esp_err_to_name(err_rc_));

    if (kvm_load_state() != ESP_OK)
    {
        ESP_RETURN_ON_ERROR(ddc_get_vcp(VCP_INPUT_SOURCE_FEATURE, &kvm_state.last_input),
            TAG, "ddc_get_vcp failed (%s)", esp_err_to_name(err_rc_));
    }

    ESP_LOGI(TAG, "input = %d", kvm_state.last_input);

    config_t* config = (config_t*)malloc(sizeof(config_t));
    if (config == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(config_get(config));

    if (kvm_state.last_input == config->pc2_vcp)
    {
        kvm_port = 1;
    }
    else if (kvm_state.last_input == config->pc1_vcp)
    {
        kvm_port = 0;
    }

    free(config);

    kvm_sync_port();

    usb_enable2(true);

    return ESP_OK;
}

void kvm_next_port()
{
    kvm_port++;
    kvm_port %= KVM_NUM_PORTS;

    ESP_LOGI(TAG, "set port=%d", kvm_port);

    kvm_sync_port();
}

void kvm_set_port(int port)
{
    kvm_port = port == 2 ? 0 : 1;
    kvm_sync_port();
}

int kvm_get_port()
{
    return kvm_port == 0 ? 2 : 1;
}

void kvm_enable(bool enable)
{
    ESP_ERROR_CHECK(gpio_set_level(KVM_ENABLE_GPIO_NUM, enable ? 1 : 0));
}