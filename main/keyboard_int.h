//#define KEYBOARD_MAX_DATA_SIZE      64
#define KEYBOARD_MAX_CONTROL_SIZE   1000
//#define KEYBOARD_INTERFACE_NUM      0
//#define KEYBOARD_ENDPOINT_ADDRESS   0x81

#define KEYBOARD_MAX_KEYS_DOWN       5

typedef struct
{
    usb_host_client_handle_t client_hdl;
    uint8_t interface_num;
    uint8_t dev_addr;
    uint8_t endpoint_addr;
    usb_device_handle_t dev_hdl;

    SemaphoreHandle_t device_sem;
    SemaphoreHandle_t ctrl_req_sem;
    SemaphoreHandle_t ctrl_resp_sem;
    TickType_t hold_start;
    uint8_t hold_key;
    portMUX_TYPE mux;
    SemaphoreHandle_t hold_sem;
    usb_transfer_t* in_transfer;
    usb_transfer_t* ctrl_transfer;
    //uint8_t in_buffer[KEYBOARD_MAX_PACKET_SIZE];
    //uint8_t in_buffer_size;
    
    bool keyboard_states[256];
    //SemaphoneHandle_t keyboard_sem;

    volatile bool shutdown_pending;
}
keyboard_driver_t;

static void keyboard_hold_task(void* arg);
static void keyboard_session_task(void* arg);
static void keyboard_in_callback(usb_transfer_t* transfer);
static void keyboard_ctrl_callback(usb_transfer_t* transfer);
static esp_err_t keyboard_shutdown(keyboard_driver_t* driver);
static usb_transfer_status_t keyboard_send_control_transfer(keyboard_driver_t* driver);
static void keyboard_onkeydown(keyboard_driver_t* driver, uint8_t key);
static void keyboard_onkeyup(keyboard_driver_t* driver, uint8_t key);
static void keyboard_onhold(keyboard_driver_t* driver, uint8_t key);
static void keyboard_execute_button_event(keyboard_driver_t* driver, uint8_t key, int event_index);