#include <stdint.h>

#include "usbip.h"
#include "vhci.h"

void cb(uint8_t* ptr)
{
}

int main(int argc, char** argv)
{
    usbip_server_t handle;

    if (usbip_server_setup(&handle))
    {
        return 1;
    }

    usb_if_t ifs = {
        .class     = 0xFF,
        .sub_class = 0,
        .protocol  = 0
    };

    usb_dev_t dev = {
        .info = {
            .path                = "/dev/a",
            .busid               = "1-1",
            .bus                 = 1,
            .busnum              = 1,
            .devnum              = 1,
            .speed               = 1,
            .vendor_id           = 0xFFFF,
            .product_id          = 0xFFFF,
            .device_bcd          = 0x200,
            .class               = 0xFF,
            .sub_class           = 0xFF,
            .protocol            = 0x00,
            .configuration_value = 0,
            .configuration_cnt   = 1,
            .interface_cnt       = 1,
        },
        .interfaces = &ifs,
        .callback   = cb
    };

    if (usbip_add_dev(&handle, &dev))
    {
        return -1;
    }

    while (1)
    {
        usbip_server_handle_once(&handle);
    }
    return 0;
}