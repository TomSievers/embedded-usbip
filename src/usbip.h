#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "linked_list.h"

typedef void (*dev_urb_cb_t)(uint8_t* data);

#pragma pack(push, 1)
typedef struct usb_if
{
    uint8_t class;
    uint8_t sub_class;
    uint8_t protocol;
    uint8_t reserved;
} usb_if_t;

typedef struct usb_dev
{
    struct dev_info
    {
        char path[256];
        char busid[32];
        uint32_t bus;
        uint32_t busnum;
        uint32_t devnum;
        uint32_t speed;
        uint16_t vendor_id;
        uint16_t product_id;
        uint16_t device_bcd;
        uint8_t class;
        uint8_t sub_class;
        uint8_t protocol;
        uint8_t configuration_value;
        uint8_t configuration_cnt;
        uint8_t interface_cnt;
    } info;
    usb_if_t* interfaces;
    dev_urb_cb_t callback;
} usb_dev_t;
#pragma pack(pop)

typedef struct usbip_server
{
    int listen_sock;
    linked_list_t client_list;
    linked_list_t dev_list;
} usbip_server_t;

int usbip_server_setup(usbip_server_t* handle);

int usbip_add_dev(usbip_server_t* handle, usb_dev_t* dev);

int usbip_server_handle_once(usbip_server_t* handle);