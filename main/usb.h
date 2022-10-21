#ifndef USB_H
#define USB_H

// typedef struct {
//     uint8_t   request_type;
//     uint8_t   request;
//     uint16_t  value;
//     uint16_t  index;
//     uint16_t  length;
// } 
// __attribute__((packed)) USB_SETUP_PACKET;

// #define USB_REQUEST_DIRECTION_IN 0x80
// #define USB_REQUEST_DIRECTION_OUT 0x00

// #define USB_REQUEST_TYPE_STANDARD   (0x00 << 5)
// #define USB_REQUEST_TYPE_CLASS      (0x01 << 5)
// #define USB_REQUEST_TYPE_VENDOR     (0x02 << 5)

// #define USB_RECIPIENT_DEVICE        0x00
// #define USB_RECIPIENT_INTERFACE     0x01
// #define USB_RECIPIENT_ENDPOINT      0x02
// #define USB_RECIPIENT_OTHER         0x03

// #define USB_GET_DESCRIPTOR              6
// #define USB_GET_CONFIGURATION           8
// #define USB_GET_INTERFACE               10

// #define USB_DESC_TYPE_DEVICE             1
// #define USB_DESC_TYPE_CONFIGURATION      2
// #define USB_DESC_TYPE_STRING             3
// #define USB_DESC_TYPE_INTERFACE          4
// #define USB_DESC_TYPE_ENDPOINT           5

typedef struct
{
  uint8_t  bLength;         /**< Numeric expression that is the total size of the HID descriptor */
  uint8_t  bDescriptorType; /**< Constant name specifying type of HID descriptor. */

  uint16_t bcdHID;          /**< Numeric expression identifying the HID Class Specification release */
  uint8_t  bCountryCode;    /**< Numeric expression identifying country code of the localized hardware.  */
  uint8_t  bNumDescriptors; /**< Numeric expression specifying the number of class descriptors */

  uint8_t  bReportType;     /**< Type of HID class report. */
  uint16_t wReportLength;   /**< the total size of the Report descriptor. */
} __attribute__((packed)) usb_hid_desc_t;

#define USB_W_VALUE_DT_HID      0x21
#define USB_W_VALUE_DT_REPORT   0x22

#define USB_HID_GET_REPORT      0x02
#define USB_HID_INPUT_REPORT    0x01
#define USB_HID_SET_IDLE        0x0a
#define USB_HID_SET_PROTOCOL    0x0b

#define USB_HID_BOOT_PROTOCOL   0
#define USB_HID_REPORT_PROTOCOL 1

esp_err_t usb_preinit();
esp_err_t usb_init();
void usb_enable2(bool enable);

#endif