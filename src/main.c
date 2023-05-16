#include <stdint.h>

#include "usbip.h"

void cb(uint8_t* ptr) { }

int main(int argc, char** argv)
{
    usbip_server_t usbip_server;
    vhci_handle_t usb_handle;

    if (vhci_init(&usb_handle))
    {
        return 1;
    }
    usb_dev_desc_t desc = {};

    usb_dev_t dev1      = usb_dev_create(&desc, LANG_ID_ENGLISH_US);

    if (vhci_register_dev(&usb_handle, &dev1))
    {
        return 1;
    }

    if (usbip_server_setup(&usbip_server, &usb_handle))
    {
        return 1;
    }

    while (1)
    {
        usbip_server_handle_once(&usbip_server);
    }
    return 0;
}