#include <stdint.h>

#include "usbip.h"
#include <unistd.h>

void cb(uint8_t* ptr) { }

int main(int argc, char** argv)
{
    usbip_server_t usbip_server;
    vhci_handle_t usb_handle;

    // Initialize Virtual Host Controller
    if (vhci_init(&usb_handle))
    {
        return 1;
    }

    // Create USB Device Descriptor
    usb_dev_desc_t dev = {
        .bcdDevice = 0x0200,
        .bcdUSB = 0x0200,
        .bDeviceClass = 0xEF,
        .bDeviceSubClass = 0x02,
        .bDeviceProtocol = 0x01,
        .idVendor = 0x0000,
        .idProduct = 0x0000,
        .iManufacturer = 0,
        .iSerialNumber = 0,
        .iProduct = 0,
        .bMaxPacketSize0 = 64,
    };

    // Create USB Device
    usb_dev_t dev1 = usb_dev_create(&dev, LANG_ID_ENGLISH_US);

    usb_conf_t conf = { .desc = {
                            .bConfigurationValue = 0,
                            .iConfiguration = 0,
                            .bMaxPower = 255,
                            .one = 1,
                            .rmt_wake = 0,
                            .self_power = 1,
                            .reserved = 0,
                        } };

    usb_dev_add_config(&dev1, &conf);

    usb_if_group_t if_grp = {};

    usb_if_t acm_if = { .desc = {
                            .bInterfaceClass = 0x02, // CDC
                            .bInterfaceSubClass = 0x02, // ACM
                            .bInterfaceProtocol = 0x01, // AT-commands V.250
                            .bAlternateSetting = 0,
                            .bInterfaceNumber = 0,
                        } };

    usb_ep_t ep1 = { .desc = {
                         .ep_nb = 1,
                         .dir = USB_EP_IN,
                         .txfer_type = USB_EP_BULK,
                         .sync_type = 0,
                         .usage_type = 0,
                         .packet_size = 128,
                         .bInterval = 1,
                     } };

    usb_if_t dcd_if = { .desc = {
                            .bInterfaceClass = 0x0A, // CDC-Data
                            .bInterfaceSubClass = 0x00, // unused
                            .bInterfaceProtocol = 0x00, // No protocol
                            .bAlternateSetting = 0,
                            .bInterfaceNumber = 1,
                        } };

    usb_ep_t ep2_in = { .desc = {
                            .ep_nb = 2,
                            .dir = USB_EP_IN,
                            .txfer_type = USB_EP_BULK,
                            .sync_type = 0,
                            .usage_type = 0,
                            .packet_size = (1 << 10) - 1,
                            .bInterval = 1,
                        } };

    usb_ep_t ep2_out = { .desc = {
                             .ep_nb = 2,
                             .dir = USB_EP_OUT,
                             .txfer_type = USB_EP_BULK,
                             .sync_type = 0,
                             .usage_type = 0,
                             .packet_size = (1 << 10) - 1,
                             .bInterval = 1,
                         } };

    usb_if_add_ep(&acm_if, &ep1);

    usb_if_add_ep(&dcd_if, &ep2_in);

    usb_if_add_ep(&dcd_if, &ep2_out);

    usb_if_grp_add(&if_grp, &acm_if);

    usb_if_grp_add(&if_grp, &dcd_if);

    usb_dev_add_if_grp(&dev1, 0, &if_grp);

    dev1.cur_config = 0;

    // Register the device with Virtual Host Controller
    if (vhci_register_dev(&usb_handle, &dev1))
    {
        return 1;
    }

    // Start USBIP server
    if (usbip_server_setup(&usbip_server, &usb_handle))
    {
        return 1;
    }

    while (1)
    {
        usbip_server_handle_once(&usbip_server);
        usleep(10000);
    }
    return 0;
}
