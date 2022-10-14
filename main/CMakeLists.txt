idf_component_register(SRCS "led.c" "actions.c" "config.c" "wifi.c" "ble.c" "ddc.c" "kvm.c" "main.c" "usb.c" "keyboard.c" "webserver.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_http_server json nvs_flash usb bt spiffs esp_http_client led_strip)