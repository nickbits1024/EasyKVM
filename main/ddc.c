#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"
#include "usb/usb_helpers.h"
#include "driver/i2c.h"
#include "driver/periph_ctrl.h"
#include "ddc_int.h"
#include "ddc.h"

#define TAG "DDC"

esp_err_t ddc_init()
{
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = DDC_SDA_GPIO_NUM;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = DDC_SCL_GPIO_NUM;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    //conf.slave.addr_10bit_en = 0;
    //conf.slave.slave_addr = DDC_I2C_ADDRESS;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    ESP_RETURN_ON_ERROR(i2c_param_config(DDC_I2C_PORT, &conf), TAG, "I2C config failed (%s)", esp_err_to_name(err_rc_));
    ESP_RETURN_ON_ERROR(i2c_driver_install(DDC_I2C_PORT, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0), 
        TAG, "I2C driver install failed (%s)", esp_err_to_name(err_rc_));

    return ESP_OK;
}

esp_err_t ddc_reset()
{
    ESP_RETURN_ON_ERROR(i2c_reset_tx_fifo(DDC_I2C_PORT), TAG, "i2c_reset_tx_fifo failed (%s)", esp_err_to_name(err_rc_));
	ESP_RETURN_ON_ERROR(i2c_reset_rx_fifo(DDC_I2C_PORT), TAG, "i2c_reset_tx_fifo i2c_reset_rx_fifo (%s)", esp_err_to_name(err_rc_));
	periph_module_disable(PERIPH_I2C0_MODULE);
    vTaskDelay(100 / portTICK_PERIOD_MS);
	periph_module_enable(PERIPH_I2C0_MODULE);
	ESP_RETURN_ON_ERROR(i2c_driver_delete(DDC_I2C_PORT), TAG, "i2c_driver_delete failed (%s)", esp_err_to_name(err_rc_));

    vTaskDelay(100 / portTICK_PERIOD_MS);
	
    return ddc_init();
}

esp_err_t ddc_set_vcp_internal(uint8_t op, uint16_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t buffer[8];

    buffer[0] = (DDC_I2C_ADDRESS << 1) | I2C_MASTER_WRITE;
    buffer[1] = 0x51;
    buffer[2] = 0x84;
    buffer[3] = 0x03;
    buffer[4] = op;
    buffer[5] = value >> 8;
    buffer[6] = value & 0xff;
    uint8_t checksum = buffer[0];
    for (int i = 1; i < sizeof(buffer) - 1; i++)
    {
        checksum ^= buffer[i];
    }
    buffer[7] = checksum;

    //int sent = i2c_slave_write_buffer(DDC_I2C_PORT, buffer, sizeof(buffer), 1000 / portTICK_PERIOD_MS);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ESP_GOTO_ON_ERROR(i2c_master_start(cmd), 
        cleanup, TAG, "set_vcp i2c_master_start failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(i2c_master_write_byte(cmd, buffer[0], true), 
        cleanup, TAG, "set_vcp i2c_master_write_byte failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(i2c_master_write(cmd, buffer + 1, sizeof(buffer) - 1, true), 
        cleanup, TAG, "set_vcp i2c_master_write failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(i2c_master_stop(cmd), 
        cleanup, TAG, "set_vcp i2c_master_stop failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(i2c_master_cmd_begin(DDC_I2C_PORT, cmd, DDC_I2C_TIMEOUT), 
        cleanup, TAG, "set_vcp i2c_master_cmd_begin failed (%s)", esp_err_to_name(err_rc_));
    i2c_cmd_link_delete(cmd);
    cmd = NULL;

    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "set vcp 0x%x = 0x%x", op, value);

cleanup:
    if (cmd != NULL)
    {
        i2c_cmd_link_delete(cmd);
        cmd = NULL;
    }

    return ret;
}

esp_err_t ddc_set_vcp(uint8_t op, uint16_t value)
{
    esp_err_t result = ESP_OK;
    for (int i = 1; i <= 10; i++)
    {
        result = ddc_set_vcp_internal(op, value);
        if (result == ESP_OK)
        {
            return ESP_OK;
        }
        ESP_LOGE(TAG, "ddc_set_vcp error %s, retry #%d", esp_err_to_name(result), i);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        ESP_ERROR_CHECK(ddc_reset());
    }
    return result;

}

esp_err_t ddc_get_vcp_internal(uint8_t op, uint16_t* value)
{
    esp_err_t ret = ESP_OK;

    if (value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    *value = 0;

    uint8_t request_buffer[6];

    request_buffer[0] = (DDC_I2C_ADDRESS << 1) | I2C_MASTER_WRITE;
    request_buffer[1] = 0x51;
    request_buffer[2] = 0x82;
    request_buffer[3] = 0x01;
    request_buffer[4] = op;
    uint8_t checksum = request_buffer[0];
    for (int i = 1; i < sizeof(request_buffer) - 1; i++)
    {
        checksum ^= request_buffer[i];
    }
    request_buffer[5] = checksum;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ESP_GOTO_ON_ERROR(i2c_master_start(cmd),
        cleanup, TAG, "get_vcp write i2c_master_start failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(i2c_master_write_byte(cmd, request_buffer[0], true),
        cleanup, TAG, "get_vcp write i2c_master_write_byte failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(i2c_master_write(cmd, request_buffer + 1, sizeof(request_buffer) - 1, true),
        cleanup, TAG, "get_vcp write i2c_master_write failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(i2c_master_stop(cmd), cleanup, TAG, "get_vcp write i2c_master_stop failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(i2c_master_cmd_begin(DDC_I2C_PORT, cmd, DDC_I2C_TIMEOUT), 
        cleanup, TAG, "get_vcp write i2c_master_cmd_begin failed (%s)", esp_err_to_name(err_rc_));
    i2c_cmd_link_delete(cmd);
    cmd = NULL;

    ets_delay_us(40000);

    uint8_t response_buffer[12];

    memset(response_buffer, 0, sizeof(response_buffer));
    response_buffer[0] = (DDC_I2C_ADDRESS << 1) | I2C_MASTER_READ;

    cmd = i2c_cmd_link_create();
    ESP_GOTO_ON_ERROR(i2c_master_start(cmd), 
        cleanup, TAG, "get_vcp read i2c_master_start failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(i2c_master_write_byte(cmd, response_buffer[0], true), 
        cleanup, TAG, "get_vcp read i2c_master_write_byte failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(i2c_master_read(cmd, response_buffer + 1, sizeof(response_buffer) - 1, I2C_MASTER_LAST_NACK), 
        cleanup, TAG, "get_vcp read i2c_master_read failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(i2c_master_stop(cmd), cleanup, TAG, "get_vcp read i2c_master_stop failed (%s)", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(i2c_master_cmd_begin(DDC_I2C_PORT, cmd, DDC_I2C_TIMEOUT), 
        cleanup, TAG, "get_vcp read i2c_master_cmd_begin failed (%s)", esp_err_to_name(err_rc_));
    i2c_cmd_link_delete(cmd);
    cmd = NULL;

    vTaskDelay(100 / portTICK_PERIOD_MS);

    printf("response:");
    for (int i = 0; i < sizeof(response_buffer); i++)
    {
        printf(" %02x", response_buffer[i]);
    }
    printf("\n");

    *value = response_buffer[9] << 8 | response_buffer[10];

cleanup:
    if (cmd != NULL)
    {
        i2c_cmd_link_delete(cmd);
        cmd = NULL;
    }
    return ret;
}

esp_err_t ddc_get_vcp(uint8_t op, uint16_t* value)
{
    esp_err_t result = ESP_FAIL;
    for (int i = 1; i <= 10; i++)
    {
        result = ddc_get_vcp_internal(op, value);
        if (result == ESP_OK)
        {
            return ESP_OK;
        }
        ESP_LOGE(TAG, "ddc_get_vcp error %s, retry #%d", esp_err_to_name(result), i);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        ESP_ERROR_CHECK(ddc_reset());
    }
    return result;
}
