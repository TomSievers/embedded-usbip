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
    usb_dev_desc_t desc = { .bcdDevice = 0x0100,
        .bcdUSB = 0x0200,
        .bDeviceClass = 0xFF,
        .idVendor = 0x0000,
        .bDeviceProtocol = 0xFF,
        .bDeviceSubClass = 0xFF,
        .bMaxPacketSize0 = 64,
        .iManufacturer = 0,
        .iProduct = 0,
        .iSerialNumber = 0 };

    usb_dev_t dev1 = usb_dev_create(&desc, LANG_ID_ENGLISH_US);

    usb_conf_t conf = { .desc = { .bConfigurationValue = 0,
                            .iConfiguration = 0,
                            .bMaxPower = 0,
                            .one = 1,
                            .rmt_wake = 0,
                            .self_power = 0,
                            .reserved = 0 } };

    usb_dev_add_config(&dev1, &conf);

    usb_if_group_t if_grp = {};

    usb_if_t if0 = { .desc = {
                         .bInterfaceNumber = 0,
                         .bAlternateSetting = 0,
                         .bInterfaceClass = 0xFF,
                         .bInterfaceSubClass = 0xFF,
                         .bInterfaceProtocol = 0xFF,
                         .iInterface = 0,
                     } };

    usb_ep_t ep0_in = { .desc = { .ep_nb = 0,
                            .dir = 1,
                            .txfer_type = 0,
                            .usage_type = 0,
                            .sync_type = 0,
                            .packet_size = 64 } };

    usb_ep_t ep0_out = { .desc = { .ep_nb = 0,
                             .dir = 0,
                             .txfer_type = 0,
                             .usage_type = 0,
                             .sync_type = 0,
                             .packet_size = 64 } };

    usb_if_add_ep(&if0, &ep0_in);

    usb_if_add_ep(&if0, &ep0_out);

    usb_if_grp_add(&if_grp, &if0);

    usb_dev_add_if_grp(&dev1, 0, &if_grp);

    dev1.cur_config = 0;

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