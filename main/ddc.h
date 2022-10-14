#ifndef DDC_H
#define DDC_H

#define VCP_INPUT_SOURCE_FEATURE        0x60
#define VCP_POWER_FEATURE               0xd6

esp_err_t ddc_init();
esp_err_t ddc_set_vcp(uint8_t feature, uint16_t value);
esp_err_t ddc_get_vcp(uint8_t feature, uint16_t* value);

#endif