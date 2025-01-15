#pragma once

#include "dev.h"
#include "linked_list.h"
#include "urb.h"
#include <stdint.h>

typedef struct vusb_dev
{
    usb_dev_t* dev;
    urb_t urb_list;
} vusb_dev_t;

typedef struct vhci_handle
{
    linked_list_t devices;
    uint32_t last_busnum;
    uint32_t last_devnum;
} vhci_handle_t;

typedef void (*vhci_iter_cb)(vusb_dev_t* dev, void* ctx);

/**
 * @brief Initialize Virtual Host controller.
 * @param handle, The handle to be initialized.
 * @return int, -1 on error and errno set, otherwise 0
 */
int vhci_init(vhci_handle_t* handle);

/**
 * @brief Register a device to the Host controller.
 * @param handle, The handle to which the device will be registerd.
 * @param dev, The device to register.
 * @return int, -1 on error and errno set, otherwise 0
 */
int vhci_register_dev(vhci_handle_t* handle, usb_dev_t* dev);

/**
 * @brief Remove a device from the Host controller.
 * @param handle, The handle from which the device should be removed.
 * @param dev, Pointer to the device that should be removed.
 */
int vhci_remove_device(vhci_handle_t* handle, usb_dev_t* dev);

/**
 * @brief Iterate through devices in Host controller.
 * @param handle, The Host controller to iterate.
 * @param cb, The function to call for every device.
 * @param ctx, A context to be passed into the function, may be NULL
 */
void vhci_iter_devices(vhci_handle_t* handle, vhci_iter_cb cb, void* ctx);

/**
 * @brief Find a device with a given busid.
 * @param handle, The Host controller to search in.
 * @param busid, The bus id of the device, const char[32].
 * @return vusb_dev_t*, Found device or NULL if not found.
 */
vusb_dev_t* vhci_find_device(vhci_handle_t* handle, const char* busid);

/**
 * @brief Get a device with the given busnum and devnum
 * @param handle, The Host controller to search in.
 * @param busnum, The bus number of the device.
 * @param devnum, The device number of the device.
 * @return vusb_dev_t*, Found device or NULL if not found.
 */
vusb_dev_t* vhci_get_device(vhci_handle_t* handle, uint32_t busnum, uint32_t devnum);

/**
 * @brief Handle any neccesary actions for this Host Controller once.
 * @param handle, The Host controller to handle actions for.
 */
void vhci_run_once(vhci_handle_t* handle);

/**
 * @brief Submit a URB to the Host controller.
 * @param handle, The Host controller to submit the URB to.
 * @param urb, The URB to submit.
 * @return int, -1 on error and errno set, otherwise 0
 */
int vhci_submit_urb(vhci_handle_t* handle, urb_t urb);

/**
 * @brief Unlink a URB from the Host controller.
 * @param handle, The Host controller to unlink the URB from.
 * @param seq_num, The sequence number of the URB to unlink.
 * @return int, -1 on error and errno set, otherwise 0
 */
int vhci_unlink_urb(vhci_handle_t* handle, vusb_dev_t* dev, uint32_t seq_num);

/**
 * @brief Initialize a URB for a device, allocates memory for the URB if necessary.
 * @param handle, The Host controller to which the usb device is connected.
 * @param dev, The device to initialize the URB for.
 * @param urb, The URB to initialize.
 * @return int, -1 on error and errno set, otherwise 0
 */
int vhci_urb_init(vhci_handle_t* handle, vusb_dev_t* dev, urb_t* urb);
