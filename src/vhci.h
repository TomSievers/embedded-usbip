#pragma once

#include "linked_list.h"
#include "queue.h"
#include <stdint.h>

typedef enum lang_id
{
    NONE            = 0,
    LANG_ENGLISH_US = 0x0409
} lang_id_t;

#pragma pack(push, 1)
typedef struct usb_desc
{
    uint8_t bLength;
    uint8_t bDescriptorType;
} usb_desc_t;

typedef struct string_desc
{
    usb_desc_t hdr;
    uint8_t str_len;
    wchar_t* str;
} string_desc_t;

typedef struct ep_desc
{
    usb_desc_t hdr;
    union
    {
        uint8_t bEndpointAddress;
        struct
        {
            uint8_t ep_nb : 4;
            uint8_t reserved : 3;
            uint8_t dir : 1;
        };
    };
    union
    {
        uint8_t bmAttributes;
        struct
        {
            uint8_t txfer_type : 2;
            uint8_t sync_type : 2;
            uint8_t usage_type : 2;
        };
    };
    union
    {
        uint16_t wMaxPacketSize;
        struct
        {
            uint16_t packet_size : 11;
            uint16_t opertunities : 2;
        };
    };

    uint8_t bInterval;
} ep_desc_t;

typedef struct if_desc
{
    struct if_desc_base
    {
        usb_desc_t hdr;
        uint8_t bInterfaceNumber;
        uint8_t bAlternateSetting;
        uint8_t bNumEndpoints;
        uint8_t bInterfaceClass;
        uint8_t bInterfaceSubClass;
        uint8_t bInterfaceProtocol;
        uint8_t iInterface;
    } base;
    ep_desc_t* ep_descriptors;
    uint8_t ep_desc_count;
} if_desc_t;

typedef struct conf_desc
{
    struct conf_desc_base
    {
        usb_desc_t hdr;
        uint8_t bNumInterfaces;
        uint8_t bConfigurationValue;
        uint8_t iConfiguration;
        union
        {
            uint8_t bmAttributes;
            struct
            {
                uint8_t reserved : 5;
                uint8_t rmt_wake : 1;
                uint8_t self_power : 1;
                uint8_t one : 1;
            };
        };
        uint8_t bMaxPower;
    } base;
    if_desc_t* if_descriptors;
    uint8_t if_desc_count;
    uint8_t cur_if_idx;
    uint8_t* if_alt_set;
} conf_desc_t;

typedef struct dev_desc
{
    usb_desc_t hdr;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} dev_desc_t;

#pragma pack(pop)

typedef struct vendpoint
{
    void (*in)(void* data, size_t len);
    size_t (*out)(void* data, size_t len);
} vendpoint_t;

typedef struct vusb_dev
{
    vendpoint_t ep[15];
    uint8_t cur_configuration;
    dev_desc_t desc;
    conf_desc_t* configurations;
    uint8_t string_desc_count;
    wchar_t** strings;
    lang_id_t lang_id;
    int remote;
} vusb_dev_t;

typedef struct vhci_handle
{
    msg_fifo_t urb_fifo;
    linked_list_t dev_list;
} vhci_handle_t;

typedef struct usb_ctrlreq
{
    union
    {
        struct
        {
            uint8_t recipient : 5;
#define RECIPIENT_DEV   0
#define RECIPIENT_IF    1
#define RECIPIENT_EP    2
#define RECIPIENT_OTHER 3
            uint8_t type : 2;
            uint8_t dir : 1;
        } bmRequestType;
        uint8_t bRequestType;
    };
    uint8_t bRequest;
#define DEV_REQ_STATUS     0
#define DEV_REQ_CLR_FEAT   1
#define DEV_REQ_SET_FEAT   3
#define DEV_REQ_SET_ADDR   5
#define DEV_REQ_GET_DESC   6
#define DEV_REQ_SET_DESC   7
#define DEV_REQ_GET_CONF   8
#define DEV_REQ_SET_CONF   9
#define DEV_REQ_GET_IF     10
#define DEV_REQ_SET_IF     11
#define DEV_REQ_SYNC_FRAME 12
    uint16_t wValue;
#define USB_DESC_TYPE_DEV     1
#define USB_DESC_TYPE_CONF    2
#define USB_DESC_TYPE_STR     3
#define USB_DESC_TYPE_IF      4
#define USB_DESC_TYPE_EP      5
#define USB_DESC_TYPE_DEV_Q   6
#define USB_DESC_TYPE_OS_CONF 7
#define USB_DESC_TYPE_IF_PWR  8

#define FEAT_EP_HLT    0
#define FEAT_REM_WK    1
#define FEAT_TEST_MODE 2
    uint16_t wIndex;
    uint16_t wLength;
} usb_ctrlreq_t;

typedef struct urb urb_t;

typedef void (*urb_completion_t)(urb_t*, void*);

typedef struct urb
{
    vusb_dev_t* dev; // pointer to associated USB device
#define PIPE_IN        0x01
#define PIPE_OUT       0x00
#define PIPE_DIR_MASK  0x01
#define PIPE_EP_OFF    3
#define PIPE_TYPE_OFF  1
#define PIPE_TYPE_MASK 0x3
#define PIPE_TYPE_CTRL 0x0
#define PIPE_TYPE_BULK 0x1
#define PIPE_TYPE_INTR 0x2
#define PIPE_TYPE_ISO  0x3

#define PIPE_TYPE(pipe) (pipe >> PIPE_TYPE_OFF) & PIPE_TYPE_MASK;
#define PIPE_DIR(pipe)  (pipe) & PIPE_DIR_MASK;
#define PIPE_EP(pipe)   pipe >> PIPE_EP_OFF;
    unsigned int pipe; // endpoint information

    unsigned int transfer_flags; // URB_ISO_ASAP, URB_SHORT_NOT_OK, etc.

    // (IN) all urbs need completion routines
    void* context; // context for completion routine
    urb_completion_t complete; // pointer to completion routine

    // (OUT) status after each completion
    int status; // returned status

    // (IN) buffer used for data transfers
    void* transfer_buffer; // associated data buffer
    uint32_t transfer_buffer_length; // data buffer length
    int number_of_packets; // size of iso_frame_desc

    // (OUT) sometimes only part of CTRL/BULK/INTR transfer_buffer is used
    uint32_t actual_length; // actual data buffer length

    // (IN) setup stage for CTRL (pass a struct usb_ctrlrequest)
    usb_ctrlreq_t setup_packet; // setup packet (control only)

    // Only for PERIODIC transfers (ISO, INTERRUPT)
    // (IN/OUT) start_frame is set unless URB_ISO_ASAP isn't set
    int start_frame; // start frame
    int interval; // polling interval

    // ISO only: packets are only "best effort"; each can have errors
    int error_count; // number of errors
    // struct usb_iso_packet_descriptor iso_frame_desc[0];
} urb_t;

int vhci_add_dev(vhci_handle_t* handle, vusb_dev_t* dev);

int vhci_submit_urb(urb_t* urb);