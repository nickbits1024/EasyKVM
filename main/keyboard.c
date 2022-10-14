#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"
#include "usb/usb_helpers.h"
#include "driver/gpio.h"
#include "config.h"
#include "keyboard_int.h"
#include "keyboard.h"
#include "usb.h"
#include "kvm.h"
#include "actions.h"

#define TAG "KEYBOARD"

static void keyboard_client_event_handler(const usb_host_client_event_msg_t* event_msg, void* arg)
{
    keyboard_driver_t* driver = (keyboard_driver_t*)arg;
    switch (event_msg->event)
    {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGI(TAG, "Device #%d connected", event_msg->new_dev.address);
            if (driver->dev_addr == 0)
            {
                driver->dev_addr = event_msg->new_dev.address;
                xSemaphoreGive(driver->device_sem);
            }
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGI(TAG, "Device #%d disconnected", driver->dev_addr);
            driver->dev_addr = 0;
            driver->shutdown_pending = true;
            break;
        default:
            ESP_LOGE(TAG, "Unkown USB client event");
            abort();
            break;
    }
}

static void keyboard_usb_client_task(void* arg)
{
    keyboard_driver_t* driver = (keyboard_driver_t*)arg;

    for (;;)
    {
        usb_host_client_handle_events(driver->client_hdl, portMAX_DELAY);
        //ESP_LOGI(TAG, "client event");
    }
}

esp_err_t keyboard_preinit()
{


    return ESP_OK;
}

esp_err_t keyboard_init(keyboard_handle_t* kbd_handle)
{
    *kbd_handle = NULL;

     keyboard_driver_t* driver = malloc(sizeof(keyboard_driver_t));
    memset(driver, 0, sizeof(keyboard_driver_t));

    driver->device_sem = xSemaphoreCreateBinary();
    driver->ctrl_req_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(driver->ctrl_req_sem);
    driver->ctrl_resp_sem = xSemaphoreCreateBinary();
    portMUX_INITIALIZE(&driver->mux);
    driver->hold_sem = xSemaphoreCreateBinary();

    usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = keyboard_client_event_handler,
            .callback_arg = (void*)driver,
        }
    };
    ESP_ERROR_CHECK(usb_host_client_register(&client_config, &driver->client_hdl));

    xTaskCreate(keyboard_usb_client_task, "keyboard_usb_client_task", 10000, (void*)driver, 1, NULL);
    xTaskCreate(keyboard_session_task, "keyboard_session_task", 4000, (void*)driver, 1, NULL);
    xTaskCreate(keyboard_hold_task, "hold_task", 10000, (void*)driver, 1, NULL);

    *kbd_handle = driver;

    return ESP_OK;
}

static esp_err_t keyboard_shutdown(keyboard_driver_t* driver)
{
    ESP_ERROR_CHECK(usb_host_endpoint_halt(driver->dev_hdl, driver->endpoint_addr));
    ESP_ERROR_CHECK(usb_host_endpoint_flush(driver->dev_hdl, driver->endpoint_addr));

    // process above events
    usb_host_client_handle_events(driver->client_hdl, 0);

    ESP_ERROR_CHECK(usb_host_transfer_free(driver->in_transfer));
    ESP_ERROR_CHECK(usb_host_transfer_free(driver->ctrl_transfer));
    driver->in_transfer = NULL;
    driver->ctrl_transfer = NULL;

    ESP_ERROR_CHECK(usb_host_interface_release(driver->client_hdl, driver->dev_hdl, driver->interface_num));
    ESP_ERROR_CHECK(usb_host_device_close(driver->client_hdl, driver->dev_hdl));
    driver->dev_hdl = NULL;
    driver->dev_addr = 0;

    driver->shutdown_pending = false;

    return ESP_OK;
}

static void keyboard_print_hid_desc(const usb_hid_desc_t* hid_desc)
{
    printf("\t\t *** HID descriptor ***\n");
    printf("\t\tbLength %d\n", hid_desc->bLength);
    printf("\t\tbDescriptorType %d\n", hid_desc->bDescriptorType);
    printf("\t\tbcdHID  %d.%d0\n", ((hid_desc->bcdHID >> 8) & 0xF), ((hid_desc->bcdHID >> 4) & 0xF));
    printf("\t\tbCountryCode %d\n", hid_desc->bCountryCode);
    printf("\t\tbNumDescriptors %d\n", hid_desc->bNumDescriptors);
    printf("\t\tbReportType %d\n", hid_desc->bReportType);
    printf("\t\twReportLength %d\n", hid_desc->wReportLength);    
}

static void keyboard_hid_desc(const usb_standard_desc_t* usb_desc)
{
    switch (usb_desc->bDescriptorType)
    {
    case USB_W_VALUE_DT_HID:
        keyboard_print_hid_desc((usb_hid_desc_t*)usb_desc);
        break;
    default:
        printf("unknown desc %x\n", usb_desc->bDescriptorType);
        break;
    }
    
}

static void keyboard_session_task(void* arg)
{
    keyboard_driver_t* driver = (keyboard_driver_t*)arg;

    for (;;)
    {
        xSemaphoreTake(driver->device_sem, portMAX_DELAY);

        driver->shutdown_pending = false;

        //printf("open device #%d\n", driver->dev_addr);
        ESP_ERROR_CHECK(usb_host_device_open(driver->client_hdl, driver->dev_addr, &driver->dev_hdl));

        const usb_config_desc_t* config_desc;
        ESP_ERROR_CHECK(usb_host_get_active_config_descriptor(driver->dev_hdl, &config_desc));
        usb_print_config_descriptor(config_desc, keyboard_hid_desc);

        int offset = 0;
        uint16_t total_length = config_desc->wTotalLength;
        const usb_standard_desc_t* next_desc = (const usb_standard_desc_t *)config_desc;
        
        const usb_intf_desc_t* current_interface_desc = NULL;
        const usb_ep_desc_t* current_endpoint_desc = NULL;
        const usb_hid_desc_t* current_hid_desc = NULL;

        const usb_intf_desc_t* hid_interface_desc = NULL;
        const usb_ep_desc_t* hid_endpoint_desc = NULL;
        const usb_hid_desc_t* hid_desc = NULL;
        do 
        {
            switch (next_desc->bDescriptorType)
            {
                case USB_W_VALUE_DT_INTERFACE:
                    current_interface_desc = (usb_intf_desc_t*)next_desc;
                    current_hid_desc = NULL;
                    break;
                case USB_W_VALUE_DT_ENDPOINT:
                    current_endpoint_desc = (const usb_ep_desc_t*)next_desc;
                    if (current_hid_desc != NULL && current_endpoint_desc->bEndpointAddress >= 0x80)
                    {
                        hid_interface_desc = current_interface_desc;
                        hid_desc = current_hid_desc;
                        hid_endpoint_desc = current_endpoint_desc;
                        ESP_LOGI(TAG, "found hid on interface %d, endpoint 0x%x max_packet_size=%d report_size=%d", 
                            hid_interface_desc->bInterfaceNumber, hid_endpoint_desc->bEndpointAddress, 
                            hid_endpoint_desc->wMaxPacketSize, hid_desc->wReportLength);
                        break;
                    }
                    break;
                case USB_W_VALUE_DT_HID: 
                    current_hid_desc = (usb_hid_desc_t*)next_desc;
                    break;
            }

            next_desc = usb_parse_next_descriptor(next_desc, total_length, &offset);

        } 
        while (hid_desc == NULL && next_desc != NULL);     

        if (hid_interface_desc != NULL && hid_desc != NULL && hid_endpoint_desc != NULL)
        {
            driver->interface_num = hid_interface_desc->bInterfaceNumber;
            driver->endpoint_addr = hid_endpoint_desc->bEndpointAddress;

            ESP_ERROR_CHECK(usb_host_interface_claim(driver->client_hdl, driver->dev_hdl, driver->interface_num, 0));

            ESP_ERROR_CHECK(usb_host_transfer_alloc(hid_endpoint_desc->wMaxPacketSize, 0, &driver->in_transfer));

            driver->in_transfer->timeout_ms = 0;
            driver->in_transfer->device_handle = driver->dev_hdl;
            driver->in_transfer->callback = keyboard_in_callback;
            driver->in_transfer->context = driver;
            driver->in_transfer->bEndpointAddress = hid_endpoint_desc->bEndpointAddress;
            driver->in_transfer->num_bytes = hid_endpoint_desc->wMaxPacketSize;

            ESP_ERROR_CHECK(usb_host_transfer_alloc(KEYBOARD_MAX_CONTROL_SIZE, 0, &driver->ctrl_transfer));

            driver->ctrl_transfer->timeout_ms = 0;
            driver->ctrl_transfer->device_handle = driver->dev_hdl;
            driver->ctrl_transfer->callback = keyboard_ctrl_callback;
            driver->ctrl_transfer->context = driver;
            driver->ctrl_transfer->bEndpointAddress = 0;

            usb_setup_packet_t* setup = (usb_setup_packet_t*)driver->ctrl_transfer->data_buffer;        
            // USB_SETUP_PACKET_INIT_GET_DEVICE_DESC(setup);
            // usb_device_desc_t* dev_desc = (usb_device_desc_t*)(driver->ctrl_transfer->data_buffer + sizeof(usb_setup_packet_t));
            // keyboard_send_control_transfer(driver);
            // usb_print_device_descriptor(dev_desc);
            
            // USB_SETUP_PACKET_INIT_GET_CONFIG_DESC(setup, 0, sizeof(usb_config_desc_t));
            // keyboard_send_control_transfer(driver);
            // usb_config_desc_t* config_desc = (usb_config_desc_t*)(driver->ctrl_transfer->data_buffer + sizeof(usb_setup_packet_t));
            // USB_SETUP_PACKET_INIT_GET_CONFIG_DESC(setup, 0, config_desc->wTotalLength);
            // keyboard_send_control_transfer(driver);
            // usb_print_config_descriptor(config_desc, keyboard_hid_desc);

            // USB_SETUP_PACKET_INIT_SET_CONFIG(setup, config_desc->bConfigurationValue);
            // keyboard_send_control_transfer(driver);


            //printf("hid report size %d\n", hid_desc->wReportLength);
            
            setup->bmRequestType = USB_BM_REQUEST_TYPE_DIR_OUT | USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
            setup->bRequest = USB_HID_SET_IDLE;
            setup->wValue = 0;
            setup->wLength = 0;        
            setup->wIndex = 0;
            keyboard_send_control_transfer(driver);      

            // setup->bmRequestType = USB_BM_REQUEST_TYPE_DIR_OUT | USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
            // setup->bRequest = USB_HID_SET_PROTOCOL;
            // setup->wValue = USB_HID_REPORT_PROTOCOL;
            // setup->wLength = 0;        
            // setup->wIndex = hid_interface_desc->bInterfaceNumber;
            // keyboard_send_control_transfer(driver);      

            setup->bmRequestType = USB_BM_REQUEST_TYPE_DIR_IN | USB_BM_REQUEST_TYPE_TYPE_STANDARD | USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
            setup->bRequest = USB_B_REQUEST_GET_DESCRIPTOR; 
            setup->wValue = (USB_W_VALUE_DT_REPORT << 8);
            setup->wIndex = 0;
            setup->wLength = sizeof(usb_setup_packet_t) + hid_desc->wReportLength;
            keyboard_send_control_transfer(driver);      

            ESP_ERROR_CHECK(usb_host_transfer_submit(driver->in_transfer));

        }

        while (!driver->shutdown_pending)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        ESP_LOGI(TAG, "shutting down...");
        ESP_ERROR_CHECK(keyboard_shutdown(driver));

        ESP_LOGI(TAG, "session ended");

        driver->dev_hdl = NULL;


    }
}

static void keyboard_ctrl_callback(usb_transfer_t* transfer)
{
    keyboard_driver_t* driver = (keyboard_driver_t*)transfer->context;

    //printf("ctrl xfer status=%d size=%d data", transfer->status, transfer->actual_num_bytes);

    // if (transfer->actual_num_bytes > 0)
    // {
    //     for (int i = 0; i < transfer->actual_num_bytes; i++)
    //     {
    //         printf(" %02x", transfer->data_buffer[i]);
    //     }
    // }
    // printf("\n");

    xSemaphoreGive(driver->ctrl_resp_sem);
}

static void keyboard_in_callback(usb_transfer_t* transfer)
{
    keyboard_driver_t* driver = (keyboard_driver_t*)transfer->context;
    esp_err_t ret;

    //printf("in xfer status=%d size=%d\n", transfer->status, transfer->actual_num_bytes);

    if (transfer->status != USB_TRANSFER_STATUS_COMPLETED)
    {
        ESP_LOGE(TAG, "inbound USB transfer failure (error %d)", transfer->status);
        if (transfer->status != USB_TRANSFER_STATUS_CANCELED)
        {
            ret = usb_host_transfer_submit(driver->in_transfer);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Transfer resubmit failed (error %d)", ret);
                //ESP_ERROR_CHECK(s4_shutdown(driver));
                driver->shutdown_pending = true;
            }
        }
        
        return;
    }

    if (transfer->actual_num_bytes >= 8)
    {   
        int start_index = transfer->actual_num_bytes > 8 ? 1 : 0;
        bool states[256];

        portENTER_CRITICAL(&driver->mux);
        memcpy(states, driver->keyboard_states, sizeof(states));
        portEXIT_CRITICAL(&driver->mux);

        for (int i = 0; i < 256; i++)
        {
            if (states[i] != 0)
            {
                bool found = false;

                for (int j = 0; j < 6; j++)
                {
                    uint8_t key = transfer->data_buffer[start_index + j];
                    if (key != 0)
                    {
                        found |= i == key;
                    }
                }
                if (!found)
                {
                    states[i] = false;
                    ESP_LOGI(TAG, "keyup: %x", i);            
                    keyboard_onkeyup(driver, i);
                }
            }
        }

        for (int i = 0; i < 6; i++)
        {
            uint8_t key = transfer->data_buffer[start_index + i];
            if (key != 0)
            {
                if (!states[key])
                {
                    states[key] = true;
                    ESP_LOGI(TAG, "keydown: %x", key);
                    keyboard_onkeydown(driver, key);
                }
            }
        }

        portENTER_CRITICAL(&driver->mux);
        memcpy(driver->keyboard_states, states, sizeof(driver->keyboard_states));
        portEXIT_CRITICAL(&driver->mux);
    }

    usb_host_client_handle_events(driver->client_hdl, 0);
    if (!driver->shutdown_pending)
    {
        ESP_ERROR_CHECK(usb_host_transfer_submit(driver->in_transfer));
    }
}

static usb_transfer_status_t keyboard_send_control_transfer(keyboard_driver_t* driver)
{
    usb_setup_packet_t* setup = (usb_setup_packet_t*)driver->ctrl_transfer->data_buffer;

    xSemaphoreTake(driver->ctrl_req_sem, portMAX_DELAY);
    xSemaphoreTake(driver->ctrl_resp_sem, 0);
    driver->ctrl_transfer->num_bytes = sizeof(usb_setup_packet_t) + setup->wLength;
    ESP_ERROR_CHECK(usb_host_transfer_submit_control(driver->client_hdl, driver->ctrl_transfer));
    xSemaphoreTake(driver->ctrl_resp_sem, portMAX_DELAY);
    usb_transfer_status_t result = driver->ctrl_transfer->status;
    xSemaphoreGive(driver->ctrl_req_sem);

    return result;
}


static void keyboard_hold_task(void* arg)
{
    keyboard_driver_t* driver = (keyboard_driver_t*)arg;

    config_t* config = (config_t*)malloc(sizeof(config_t));
    if (config == NULL)
    {
        abort();
    }

    for (;;)
    {
        xSemaphoreTake(driver->hold_sem, portMAX_DELAY);

        ESP_ERROR_CHECK(config_get(config));

        portENTER_CRITICAL(&driver->mux);   
        TickType_t start = driver->hold_start;
        portEXIT_CRITICAL(&driver->mux);
        
        if (driver->hold_start != 0 && driver->hold_key != 0 && config->hold_delay > 0)
        {
            vTaskDelayUntil(&start, config->hold_delay / portTICK_PERIOD_MS);

            uint8_t key = 0;
            portENTER_CRITICAL(&driver->mux);   
            if (driver->keyboard_states[driver->hold_key])
            {
                key = driver->hold_key;
            }
            driver->hold_key = 0;
            driver->hold_start = 0;
            driver->keyboard_states[key] = false;
            portEXIT_CRITICAL(&driver->mux);

            if (key != 0)
            {
                keyboard_onhold(driver, key);
            }
        }
        
    }
}

static void keyboard_onkeydown(keyboard_driver_t* driver, uint8_t key)
{
    bool hold_started = false;
    portENTER_CRITICAL(&driver->mux);
    if (driver->hold_start == 0)
    {
        driver->hold_key = key;
        driver->hold_start = xTaskGetTickCount();
        hold_started = true;
    }
    portEXIT_CRITICAL(&driver->mux);
    if (hold_started)
    {
        xSemaphoreGive(driver->hold_sem);
    }

}

static void keyboard_onkeyup(keyboard_driver_t* driver, uint8_t key)
{
    portENTER_CRITICAL(&driver->mux);   
    if (driver->hold_key == key)
    {
        driver->hold_key = 0;
        driver->hold_start = 0;
    }
    portEXIT_CRITICAL(&driver->mux);   

    ESP_LOGI(TAG, "click: 0x%x", key);

    keyboard_execute_button_event(driver, key, BUTTON_CLICK_EVENT_INDEX);
}

static void keyboard_onhold(keyboard_driver_t* driver, uint8_t key)
{
    ESP_LOGI(TAG, "hold: 0x%x", key);

    keyboard_execute_button_event(driver, key, BUTTON_HOLD_EVENT_INDEX);
}

static void keyboard_execute_button_event(keyboard_driver_t* driver, uint8_t key, int event_index)
{
    config_t config;

    if (config_get(&config) != ESP_OK)
    {
        return;
    }

    int index = key - KEY_1;
    if (index < 0 || index >= NUM_BUTTONS)
    {
        return;
    }

    button_event_t* event = &config.buttons[index].events[event_index];

    switch (event->function)
    {
    case BUTTON_FUNCTION_KVM_NEXT_PORT:
        ESP_LOGI(TAG, "execute KVM next input");
        kvm_next_port();
        break;
    case BUTTON_FUNCTION_KVM_RESET:
        ESP_LOGI(TAG, "execute KVM reset");
        kvm_reset();    
        break;
    case BUTTON_FUNCTION_HA_SERVICE:
        ESP_LOGI(TAG, "execute ha service %s on %s", event->ha_service.service_name, event->ha_service.entity_id);
        ha_call_service(event->ha_service.service_name, event->ha_service.entity_id);
        break;
    }
}