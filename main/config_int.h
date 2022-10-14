

#define CONFIG_NVS_NAME "config"
#define CONFIG_NVS_KEY  "config"

double config_get_number(cJSON* value);
void config_get_button_event(int button_index, const button_event_t* event, const char* event_name, cJSON* button);
void config_set_button_event(int button_index, cJSON* button, const char* event_name, button_event_t* event);