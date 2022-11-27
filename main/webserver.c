#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_check.h"
#include "config.h"
#include "kvm.h"
#include "ddc.h"
#include "led.h"
#include "usb.h"
#include "webserver_int.h"
#include "webserver.h"

#define TAG "WEBSERVER"

static httpd_handle_t webserver_handle;

static bool webserver_monitor_on = true;

httpd_uri_t webserver_uri_get_config = {
    .uri = "/config",
    .method = HTTP_GET,
    .handler = webserver_get_config,
    .user_ctx = NULL
};

httpd_uri_t webserver_uri_get_api_config = {
    .uri = "/api/config",
    .method = HTTP_GET,
    .handler = webserver_get_api_config,
    .user_ctx = NULL
};

httpd_uri_t webserver_uri_post_api_config = {
    .uri = "/api/config",
    .method = HTTP_POST,
    .handler = webserver_post_api_config,
    .user_ctx = NULL
};

httpd_uri_t webserver_uri_get_kvm = {
    .uri = "/kvm",
    .method = HTTP_GET,
    .handler = webserver_get_kvm,
    .user_ctx = NULL
};

httpd_uri_t webserver_uri_get_kvm1 = {
    .uri = "/kvm/1",
    .method = HTTP_GET,
    .handler = webserver_get_kvm1,
    .user_ctx = NULL
};

httpd_uri_t webserver_uri_get_kvm2 = {
    .uri = "/kvm/2",
    .method = HTTP_GET,
    .handler = webserver_get_kvm2,
    .user_ctx = NULL
};

httpd_uri_t webserver_uri_get_monitor = {
    .uri = "/monitor",
    .method = HTTP_GET,
    .handler = webserver_get_monitor,
    .user_ctx = NULL
};

httpd_uri_t webserver_uri_post_monitor = {
    .uri = "/monitor",
    .method = HTTP_POST,
    .handler = webserver_post_monitor,
    .user_ctx = NULL
};


esp_err_t webserver_init()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;

    ESP_ERROR_CHECK(httpd_start(&webserver_handle, &config));

    ESP_ERROR_CHECK(httpd_register_uri_handler(webserver_handle, &webserver_uri_get_config));
    ESP_ERROR_CHECK(httpd_register_uri_handler(webserver_handle, &webserver_uri_get_api_config));
    ESP_ERROR_CHECK(httpd_register_uri_handler(webserver_handle, &webserver_uri_post_api_config));

    ESP_ERROR_CHECK(httpd_register_uri_handler(webserver_handle, &webserver_uri_get_kvm));
    ESP_ERROR_CHECK(httpd_register_uri_handler(webserver_handle, &webserver_uri_get_kvm1));
    ESP_ERROR_CHECK(httpd_register_uri_handler(webserver_handle, &webserver_uri_get_kvm2));

    ESP_ERROR_CHECK(httpd_register_uri_handler(webserver_handle, &webserver_uri_get_monitor));
    ESP_ERROR_CHECK(httpd_register_uri_handler(webserver_handle, &webserver_uri_post_monitor));

    return ESP_OK;
}

esp_err_t webserver_get_config(httpd_req_t* req)
{
    size_t file_size;
    FILE* fp = fopen("/spiffs/config.html", "r+");
    if (fp == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    fseek(fp, 0L, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    ESP_LOGI(TAG, "config.html size %u", file_size);
  
    char* html = malloc(file_size);
    fread(html, file_size, 1, fp);
    fclose(fp);
    fp = NULL;   

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, file_size);

    free(html);

    return ESP_OK;
}

esp_err_t webserver_get_api_config(httpd_req_t* req)
{
    int ret = ESP_OK;

    config_t* config;

    config = (config_t*)malloc(sizeof(config_t));
    if (config == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    if (config_get(config) != ESP_OK)
    {
        httpd_resp_send_500(req);
        goto cleanup;
    }

    char* json = NULL;

    ret = config_serialize(config, &json);
    if (ret != ESP_OK)
    {
        httpd_resp_send_500(req);
        goto cleanup;
    }

    ESP_GOTO_ON_ERROR(httpd_resp_set_type(req, "application/json"), cleanup, TAG, "httpd_resp_set_type failed (%s)", esp_err_to_name(err_rc_));

    ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

cleanup:
    if (config != NULL)
    {
        free(config);
    }
    if (json != NULL)
    {
        free(json);
    }

    return ret;
}

esp_err_t webserver_post_api_config(httpd_req_t* req)
{
    char* json = malloc(req->content_len + 1);
    if (json == NULL)
    {
        ESP_LOGE(TAG, "webserver_post_api_config malloc failed");
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    int ret = httpd_req_recv(req, json, req->content_len);
    if (ret != req->content_len)
    {
        ESP_LOGE(TAG, "httpd_req_recv failed (%s)", esp_err_to_name(ret));
        free(json);
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    json[req->content_len] = 0;

    printf("post: %s\n", json);

    config_t config;

    ret = config_deserialize(json, &config);
    if (ret != ESP_OK)
    {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    if (config_save2(&config) != ESP_OK)
    {
        ESP_LOGE(TAG, "config_save2 failed");
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    kvm_sync_port();

    return webserver_get_api_config(req);
}

esp_err_t webserver_get_kvm(httpd_req_t* req)
{
    char json[100];

    cJSON* root = cJSON_CreateObject();
    if (root == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    int port = kvm_get_port();

    cJSON_AddNumberToObject(root, "kvm", port);
    //cJSON_AddNumberToObject(root, "last_input", kvm_last_input);

    if (!cJSON_PrintPreallocated(root, json, sizeof(json), true))
    {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");

    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

esp_err_t webserver_get_kvm1(httpd_req_t* req)
{
    kvm_set_port(1);

    return webserver_get_kvm(req);
}

esp_err_t webserver_get_kvm2(httpd_req_t* req)
{
    kvm_set_port(2);
    
    return webserver_get_kvm(req);
}

esp_err_t webserver_get_monitor(httpd_req_t* req)
{
    char json[100];

    cJSON* root = cJSON_CreateObject();
    if (root == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    cJSON_AddBoolToObject(root, "on", webserver_monitor_on);

    if (!cJSON_PrintPreallocated(root, json, sizeof(json), true))
    {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");

    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

esp_err_t webserver_post_monitor(httpd_req_t* req)
{
    char json[100];

    if (req->content_len >= sizeof(json))
    {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    int ret = httpd_req_recv(req, json, req->content_len);
    if (ret <= 0)
    {
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    json[ret] = 0;

    cJSON* root = cJSON_Parse(json);
    if (root == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    cJSON* monitor_status = cJSON_GetObjectItem(root, "on");
    if (cJSON_IsTrue(monitor_status))
    {
        ddc_set_vcp(VCP_POWER_FEATURE, 1);
        led_enable(true);
        usb_enable2(true);
        kvm_enable(true);
        
        webserver_monitor_on = true;
    }
    else if (webserver_monitor_on)
    {
        ddc_set_vcp(VCP_POWER_FEATURE, 5);
        led_enable(false);
        usb_enable2(false);
        kvm_enable(false);

        webserver_monitor_on = false;
    }

    cJSON_Delete(root);

    return webserver_get_monitor(req);
}