#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "config.h"
#include "config_int.h"

#define TAG "CONFIG"

nvs_handle_t config_nvs_handle;
config_t config_config;
static portMUX_TYPE config_mux = portMUX_INITIALIZER_UNLOCKED;

esp_err_t config_init()
{
    ESP_RETURN_ON_ERROR(nvs_open(CONFIG_NVS_NAME, NVS_READWRITE, &config_nvs_handle),
        TAG, "nvs_open failed (%s)", esp_err_to_name(err_rc_));

    size_t config_size = 0;

    esp_err_t ret = nvs_get_blob(config_nvs_handle, CONFIG_NVS_KEY, NULL, &config_size);
    if (ret == ESP_OK && config_size > 0)
    {
        char* json = malloc(config_size + 1);
        if (json == NULL)
        {
            return ESP_ERR_NO_MEM;
        }

        size_t read_size = config_size;
        ret = nvs_get_blob(config_nvs_handle, CONFIG_NVS_KEY, json, &read_size);
        if (ret == ESP_OK && config_size == read_size)
        {
            json[config_size] = 0;

            ESP_LOGI(TAG, "loaded config: %s", json);
            ret = config_deserialize(json, &config_config);
            if (ret == ESP_OK)
            {
                ESP_LOGI(TAG, "saved ha url = %s", config_config.ha_url);
                ESP_LOGI(TAG, "saved ha auth = %s", config_config.ha_auth);
            }
        }
    }

    return ESP_OK;
}

esp_err_t config_save2(const config_t* config)
{
    portENTER_CRITICAL(&config_mux);
    memcpy(&config_config, config, sizeof(config_t));
    portEXIT_CRITICAL(&config_mux);

    char* json;

    ESP_ERROR_CHECK(config_serialize(config, &json));

    ESP_ERROR_CHECK(nvs_set_blob(config_nvs_handle, CONFIG_NVS_KEY, json, strlen(json)));
    ESP_ERROR_CHECK(nvs_commit(config_nvs_handle));

    free(json);

    return ESP_OK;
}

esp_err_t config_get(config_t* config)
{
    portENTER_CRITICAL(&config_mux);
    memcpy(config, &config_config, sizeof(config_t));
    portEXIT_CRITICAL(&config_mux);

    return ESP_OK;
}

esp_err_t config_serialize(const config_t* config, char** json)
{
    cJSON* doc = cJSON_CreateObject();
    if (doc == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(doc, "hold_delay", config->hold_delay);
    cJSON_AddNumberToObject(doc, "pc1_vcp", config->pc1_vcp);
    cJSON_AddNumberToObject(doc, "pc1_led_r", config->pc1_led_r);
    cJSON_AddNumberToObject(doc, "pc1_led_g", config->pc1_led_g);
    cJSON_AddNumberToObject(doc, "pc1_led_b", config->pc1_led_b);
    cJSON_AddNumberToObject(doc, "pc2_vcp", config->pc2_vcp);
    cJSON_AddNumberToObject(doc, "pc2_led_r", config->pc2_led_r);
    cJSON_AddNumberToObject(doc, "pc2_led_g", config->pc2_led_g);
    cJSON_AddNumberToObject(doc, "pc2_led_b", config->pc2_led_b);
    cJSON_AddStringToObject(doc, "ha_url", config->ha_url);
    cJSON_AddStringToObject(doc, "ha_auth", config->ha_auth);

    cJSON* buttons = cJSON_AddArrayToObject(doc, "buttons");
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
        cJSON* button = cJSON_CreateObject();
        cJSON_AddItemToArray(buttons, button);

        config_get_button_event(i, &config->buttons[i].events[BUTTON_CLICK_EVENT_INDEX], "click", button);
        config_get_button_event(i, &config->buttons[i].events[BUTTON_HOLD_EVENT_INDEX], "hold", button);
    }

    *json = cJSON_PrintUnformatted(doc);
    cJSON_Delete(doc);
    if (*json == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t config_deserialize(const char* json, config_t* config)
{
    cJSON* doc = cJSON_Parse(json);
    if (doc == NULL)
    {
        ESP_LOGE(TAG, "failed to parse config: %s", json);
        return ESP_FAIL;
    }

    cJSON* hold_delay = cJSON_GetObjectItemCaseSensitive(doc, "hold_delay");
    if (hold_delay != NULL)
    {
        config->hold_delay = config_get_number(hold_delay);
    }

    cJSON* pc1_vcp = cJSON_GetObjectItemCaseSensitive(doc, "pc1_vcp");
    if (pc1_vcp != NULL)
    {
        config->pc1_vcp = config_get_number(pc1_vcp);
    }

    cJSON* pc1_led_r = cJSON_GetObjectItemCaseSensitive(doc, "pc1_led_r");
    if (pc1_led_r != NULL)
    {
        config->pc1_led_r = config_get_number(pc1_led_r);
    }
    cJSON* pc1_led_g = cJSON_GetObjectItemCaseSensitive(doc, "pc1_led_g");
    if (pc1_led_g != NULL)
    {
        config->pc1_led_g = config_get_number(pc1_led_g);
    }
    cJSON* pc1_led_b = cJSON_GetObjectItemCaseSensitive(doc, "pc1_led_b");
    if (pc1_led_b != NULL)
    {
        config->pc1_led_b = config_get_number(pc1_led_b);
    }

    cJSON* pc2_vcp = cJSON_GetObjectItemCaseSensitive(doc, "pc2_vcp");
    if (pc2_vcp != NULL)
    {
        config->pc2_vcp = config_get_number(pc2_vcp);
    }
    cJSON* pc2_led_r = cJSON_GetObjectItemCaseSensitive(doc, "pc2_led_r");
    if (pc2_led_r != NULL)
    {
        config->pc1_led_r = config_get_number(pc1_led_r);
    }
    cJSON* pc2_led_g = cJSON_GetObjectItemCaseSensitive(doc, "pc2_led_g");
    if (pc2_led_g != NULL)
    {
        config->pc2_led_g = config_get_number(pc2_led_g);
    }
    cJSON* pc2_led_b = cJSON_GetObjectItemCaseSensitive(doc, "pc2_led_b");
    if (pc2_led_b != NULL)
    {
        config->pc2_led_b = config_get_number(pc2_led_b);
    }

    cJSON* ha_url = cJSON_GetObjectItem(doc, "ha_url");
    if (ha_url != NULL)
    {
        char* s = cJSON_GetStringValue(ha_url);
        if (s != NULL)
        {
            strncpy(config->ha_url, s, sizeof(config->ha_url) - 1);
            config->ha_url[sizeof(config->ha_url) - 1] = 0;
        }
    }
    cJSON* ha_auth = cJSON_GetObjectItem(doc, "ha_auth");
    if (ha_auth != NULL)
    {
        char* s = cJSON_GetStringValue(ha_auth);
        if (s != NULL)
        {
            strncpy(config->ha_auth, s, sizeof(config->ha_auth) - 1);
            config->ha_url[sizeof(config->ha_auth) - 1] = 0;
        }
    }
    cJSON* buttons = cJSON_GetObjectItem(doc, "buttons");
    for (int i = 0; i < cJSON_GetArraySize(buttons) && i < NUM_BUTTONS; i++)
    {
        cJSON* button = cJSON_GetArrayItem(buttons, i);
        if (button == NULL)
        {
            continue;
        }

        config_set_button_event(i, button, "click", &config->buttons[i].events[BUTTON_CLICK_EVENT_INDEX]);
        config_set_button_event(i, button, "hold", &config->buttons[i].events[BUTTON_HOLD_EVENT_INDEX]);
    }

    cJSON_Delete(doc);

    return ESP_OK;
}

void config_get_button_event(int button_index, const button_event_t* event, const char* event_name, cJSON* button)
{
    cJSON* button_event = cJSON_CreateObject();
    cJSON_AddItemToObject(button, event_name, button_event);

    cJSON_AddNumberToObject(button_event, "function", event->function);
    cJSON* options = cJSON_CreateObject();
    cJSON_AddItemToObject(button_event, "options", options);
    switch (event->function)
    {
        case BUTTON_FUNCTION_HA_SERVICE:
            cJSON_AddStringToObject(options, "ha_service_name", event->ha_service.service_name);
            cJSON_AddStringToObject(options, "ha_entity_id", event->ha_service.entity_id);
            break;
    }
}

void config_set_button_event(int button_index, cJSON* button, const char* event_name, button_event_t* event)
{
    event->function = BUTTON_FUNCTION_NONE;

    cJSON* button_event = cJSON_GetObjectItem(button, event_name);
    if (button_event != NULL)
    {
        cJSON* function = cJSON_GetObjectItem(button_event, "function");
        if (function != NULL)
        {
            event->function = config_get_number(function);
        }
    }

    ESP_LOGI(TAG, "button #%d %s function: %d", button_index + 1, event_name, event->function);

    cJSON* options = cJSON_GetObjectItem(button_event, "options");

    switch (event->function)
    {
        case BUTTON_FUNCTION_HA_SERVICE:
        {
            if (options != NULL)
            {
                cJSON* ha_service_name = cJSON_GetObjectItem(options, "ha_service_name");
                if (ha_service_name != NULL)
                {
                    char* s = cJSON_GetStringValue(ha_service_name);
                    if (s != NULL)
                    {
                        strncpy(event->ha_service.service_name, s, sizeof(event->ha_service.service_name) - 1);
                        event->ha_service.service_name[sizeof(event->ha_service.service_name) - 1] = 0;
                    }
                }
                cJSON* ha_entity_id = cJSON_GetObjectItem(options, "ha_entity_id");
                if (ha_entity_id != NULL)
                {
                    char* s = cJSON_GetStringValue(ha_entity_id);
                    if (s != NULL)
                    {
                        strncpy(event->ha_service.entity_id, s, sizeof(event->ha_service.entity_id) - 1);
                        event->ha_service.entity_id[sizeof(event->ha_service.entity_id) - 1] = 0;
                    }
                }
            }
            ESP_LOGI(TAG, "button #%d service_name: %s entity_id: %s", button_index + 1, event->ha_service.service_name, event->ha_service.entity_id);
            break;
        }
    }

}
double config_get_number(cJSON* value)
{
    if (value != NULL)
    {
        if (cJSON_IsNumber(value))
        {
            return cJSON_GetNumberValue(value);
        }
        if (cJSON_IsString(value))
        {
            return atoi(cJSON_GetStringValue(value));
        }
    }
    return NAN;
}