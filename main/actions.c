#include <string.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "actions.h"
#include "config.h"

#define TAG "ACTIONS"

esp_err_t ha_call_service(const char* service_name, const char* entity_id)
{
    int ret = ESP_OK;
    esp_http_client_handle_t http_client_handle = NULL;
    char* params = NULL;

    config_t* config = (config_t*)malloc(sizeof(config_t));
    if (config == NULL)
    {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    ESP_GOTO_ON_ERROR(config_get(config), cleanup, TAG, "config_get failed (%s)", esp_err_to_name(err_rc_));

    if (strlen(config->ha_url) == 0 || strlen(config->ha_auth) == 0)
    {
        ESP_LOGE(TAG, "ha_call_service: ha_url or ha_auth is empty");
        ret = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }

    char service_name_copy[HA_SERVICE_NAME_SIZE];
    strncpy(service_name_copy, service_name, HA_SERVICE_NAME_SIZE);

    char* split = strstr(service_name_copy, ".");
    if (split == NULL)
    {
        ret = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }

    *split = 0;

    char* domain = service_name_copy;
    char* service = split + 1;

    char url[sizeof(config->ha_url)];
    strlcpy(url, config->ha_url, sizeof(url));
    if (url[strlen(url) - 1] != '/')
    {
        strlcat(url, "/", sizeof(url));
    }

    snprintf(url + strlen(url), sizeof(url) - strlen(url), "api/services/%s/%s", domain, service);

    cJSON* doc = cJSON_CreateObject();
    cJSON_AddStringToObject(doc, "entity_id", entity_id);
    params = cJSON_Print(doc);
    cJSON_Delete(doc);

    ESP_LOGI(TAG, "ha_call_service: url=%s", url);
    ESP_LOGI(TAG, "    params=%s", params);

    esp_http_client_config_t http_config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach
    };

    char bearer[300];
    snprintf(bearer, sizeof(bearer), "Bearer %s", config->ha_auth);

    http_client_handle = esp_http_client_init(&http_config);
    if (http_client_handle == NULL)
    {
        ret = ESP_FAIL;
        goto cleanup;
    }
    int params_size = strlen(params);

    ESP_GOTO_ON_ERROR(esp_http_client_set_header(http_client_handle, "Content-Type", "text/json"), 
        cleanup, TAG, "esp_http_client_set_header failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(esp_http_client_set_header(http_client_handle, "Authorization", bearer), 
        cleanup, TAG, "esp_http_client_set_header failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(esp_http_client_open(http_client_handle, params_size), cleanup, TAG, "esp_http_client_open failed (%s)", esp_err_to_name(err_rc_));

    int status_code;
    int content_length;

    if (esp_http_client_write(http_client_handle, params, params_size) != params_size)
    {
        ESP_LOGE(TAG, "esp_http_client_write failed");
        ret = ESP_FAIL;
    }
    else if ((content_length = esp_http_client_fetch_headers(http_client_handle)) == ESP_FAIL)
    {
        ESP_LOGE(TAG, "esp_http_client_fetch_headers failed (%d)", content_length);
        ret = ESP_FAIL;
    }
    else if ((status_code = esp_http_client_get_status_code(http_client_handle)) != 200)
    {
        ESP_LOGE(TAG, "HTTP request failed: %d", status_code);
        ret = ESP_FAIL;
    }

    ESP_GOTO_ON_ERROR(esp_http_client_close(http_client_handle), cleanup, TAG, "esp_http_client_close failed (%s)", esp_err_to_name(err_rc_));

cleanup:
    if (config != NULL)
    {
        free(config);
    }
    if (params != NULL)
    {
        free(params);
    }
    if (http_client_handle != NULL)
    {
        esp_http_client_cleanup(http_client_handle);
    }

    return ret;
}