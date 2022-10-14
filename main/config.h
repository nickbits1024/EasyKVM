#ifndef CONFIG_H_
#define CONFIG_H

#define NUM_BUTTONS                     5

#define BUTTON_FUNCTION_NONE            0
#define BUTTON_FUNCTION_KVM_NEXT_PORT   1
#define BUTTON_FUNCTION_HA_SERVICE      2
#define BUTTON_FUNCTION_KVM_RESET       3

#define NUM_BUTTON_EVENTS               2
#define BUTTON_CLICK_EVENT_INDEX        0
#define BUTTON_HOLD_EVENT_INDEX         1

#define HA_SERVICE_NAME_SIZE            64
#define HA_ENTITY_ID_SIZE               64

typedef struct
{
    uint8_t function;
    union
    {
        struct 
        {
            char service_name[HA_SERVICE_NAME_SIZE];
            char entity_id[HA_ENTITY_ID_SIZE];          
        } ha_service;
    };

} button_event_t;

typedef struct 
{
    button_event_t events[NUM_BUTTON_EVENTS];
}
config_button_t;

typedef struct
{
    int hold_delay;
    char ha_url[1024];
    char ha_auth[256];
    uint8_t pc1_vcp;
    uint8_t pc2_vcp;
    uint8_t pc1_led_r;
    uint8_t pc1_led_g;
    uint8_t pc1_led_b;
    uint8_t pc2_led_r;
    uint8_t pc2_led_g;
    uint8_t pc2_led_b;

    config_button_t buttons[NUM_BUTTONS];
}
config_t;

esp_err_t config_init();
esp_err_t config_save2(const config_t* config);
esp_err_t config_get(config_t* config);

esp_err_t config_serialize(const config_t* config, char** json);
esp_err_t config_deserialize(const char* json, config_t* config);

#endif