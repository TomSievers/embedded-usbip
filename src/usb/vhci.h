#pragma once

#include "dev.h"
#include "linked_list.h"
#include "urb.h"
#include <stdint.h>

typedef struct vusb_dev
{
    usb_dev_t* dev;
    int client;
    urb_t urb_list;
} vusb_dev_t;

typedef struct vhci_handle
{
    linked_list_t devices;
} vhci_handle_t;

typedef void (*vhci_iter_cb)(vusb_dev_t*, void*);

int vhci_init(vhci_handle_t* handle);

int vhci_register_dev(vhci_handle_t* handle, usb_dev_t* dev);

int vhci_remove_device(vhci_handle_t* handle, usb_dev_t* dev);

int vhci_iter_devices(vhci_handle_t* handle, vhci_iter_cb cb, void* ctx);

vusb_dev_t* vhci_find_device(vhci_handle_t* handle, const char* busid);

int vhci_run_once(vhci_handle_t* handle);
