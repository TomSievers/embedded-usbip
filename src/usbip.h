#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "linked_list.h"
#include "usb/vhci.h"

typedef struct usbip_server
{
    vhci_handle_t* vhci_handle;
    int listen_sock;
    linked_list_t client_list;
    linked_list_t dev_list;
} usbip_server_t;

int usbip_server_setup(usbip_server_t* handle, vhci_handle_t* usb_handle);

int usbip_add_dev(usbip_server_t* handle, usb_dev_t* dev);

int usbip_server_handle_once(usbip_server_t* handle);