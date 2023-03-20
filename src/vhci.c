#include "vhci.h"
#include "usbip_types.h"
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <wchar.h>

#define URB_RET_HDR_SIZE sizeof(hdr_cmd_t) + sizeof(struct ret_base)

typedef __ssize_t ssize_t;

void vhci_urb_complete(urb_t* urb, void* ctx)
{
    uint16_t seq_num = *((uint16_t*)ctx);
    uint8_t buf[512];
    hdr_cmd_t* hdr       = (hdr_cmd_t*)buf;
    struct ret_base* ret = (struct ret_base*)(buf + sizeof(hdr_cmd_t));
    uint8_t* data        = (buf + URB_RET_HDR_SIZE);
    memset(hdr, 0, sizeof(hdr_cmd_t));
    hdr->command           = USBIP_RET_SUBMIT;
    hdr->seq_num           = seq_num;

    ret->actual_length     = urb->actual_length;
    ret->error_count       = urb->error_count;
    ret->number_of_packets = urb->number_of_packets;
    ret->start_frame       = urb->start_frame;
    ret->status            = urb->status;

    for (size_t i = 0; i < urb->actual_length; ++i)
    {
        data[i] = ((uint8_t*)urb->transfer_buffer)[i];
    }

    send(urb->dev->remote, buf, URB_RET_HDR_SIZE + urb->actual_length, 0);
}

void handle_urb(urb_t* urb)
{
    uint8_t direction = PIPE_DIR(urb->pipe);
    uint8_t ep        = PIPE_EP(urb->pipe);
    uint8_t type      = PIPE_TYPE(urb->pipe);
    if (ep == 0 && type == PIPE_TYPE_CTRL)
    {
        switch (urb->setup_packet.bRequest)
        {
        case DEV_REQ_STATUS:
        {
            uint16_t idx                         = urb->setup_packet.wIndex;
            uint8_t type                         = urb->setup_packet.bmRequestType.recipient;

            urb->actual_length                   = 2;
            ((uint16_t*)urb->transfer_buffer)[0] = 0;
            break;
        }
        case DEV_REQ_CLR_FEAT:
        {
            uint16_t idx     = urb->setup_packet.wIndex;
            uint8_t type     = urb->setup_packet.bmRequestType.recipient;
            uint16_t feature = urb->setup_packet.wValue;

            // TODO: IMPL
            break;
        }
        case DEV_REQ_SET_FEAT:
        {
            uint8_t idx      = urb->setup_packet.wIndex & 0xFF;
            uint8_t test     = urb->setup_packet.wIndex >> 8;
            uint8_t type     = urb->setup_packet.bmRequestType.recipient;
            uint16_t feature = urb->setup_packet.wValue;

            // TODO: IMPL
            break;
        }
        case DEV_REQ_GET_DESC:
        {
            uint8_t desc_type  = urb->setup_packet.wValue >> 8;
            uint8_t desc_index = urb->setup_packet.wValue;
            urb->status        = EINVAL;
            if (urb->transfer_buffer != NULL && urb->transfer_buffer_length > 2
                && desc_type <= USB_DESC_TYPE_IF_PWR)
            {
                switch (desc_type)
                {
                case USB_DESC_TYPE_DEV:
                {
                    void* dest               = urb->transfer_buffer;
                    dev_desc_t* dev          = &urb->dev->desc;
                    dev->hdr.bDescriptorType = USB_DESC_TYPE_CONF;
                    dev->hdr.bLength         = sizeof(dev_desc_t);

                    memcpy(dest, dev, dev->hdr.bLength);
                    urb->actual_length = dev->hdr.bLength;
                    break;
                }
                case USB_DESC_TYPE_CONF:
                {
                    urb->actual_length = 0;
                    ssize_t remaining  = urb->transfer_buffer_length;
                    if (desc_index < urb->dev->desc.bNumConfigurations)
                    {
                        urb->status                    = 0;
                        void* dest                     = urb->transfer_buffer;
                        conf_desc_t* conf              = &urb->dev->configurations[desc_index];
                        conf->base.hdr.bDescriptorType = USB_DESC_TYPE_CONF;
                        conf->base.hdr.bLength         = sizeof(struct conf_desc_base);

                        if ((remaining -= conf->base.hdr.bLength) > 0)
                        {
                            dest = memcpy(dest, conf, conf->base.hdr.bLength)
                                + conf->base.hdr.bLength;

                            for (size_t i = 0; i < conf->if_desc_count; ++i)
                            {
                                if_desc_t* if_desc                = &conf->if_descriptors[i];
                                if_desc->base.hdr.bDescriptorType = USB_DESC_TYPE_IF;
                                if_desc->base.hdr.bLength         = sizeof(struct if_desc_base);

                                if ((remaining -= if_desc->base.hdr.bLength) < 0)
                                {
                                    urb->status = ENOMEM;
                                    break;
                                }

                                dest = memcpy(dest, if_desc, if_desc->base.hdr.bLength)
                                    + if_desc->base.hdr.bLength;
                                for (size_t j = 0; j < if_desc->ep_desc_count; ++j)
                                {
                                    ep_desc_t* ep           = &if_desc->ep_descriptors[j];
                                    ep->hdr.bDescriptorType = USB_DESC_TYPE_EP;
                                    ep->hdr.bLength         = sizeof(ep_desc_t);

                                    if ((remaining -= ep->hdr.bLength) < 0)
                                    {
                                        urb->status = ENOMEM;
                                        break;
                                    }

                                    dest = memcpy(dest, ep, ep->hdr.bLength) + ep->hdr.bLength;
                                }
                            }

                            if (urb->status == 0)
                            {
                                urb->actual_length = urb->transfer_buffer_length - remaining;
                            }
                        }
                        else
                        {
                            urb->status = ENOMEM;
                        }
                    }
                    break;
                }
                case USB_DESC_TYPE_STR:
                {
                    if (desc_index == 0)
                    {
                        usb_desc_t* hdr                      = urb->transfer_buffer;
                        hdr->bDescriptorType                 = USB_DESC_TYPE_STR;
                        hdr->bLength                         = sizeof(usb_desc_t) + 2;
                        ((uint16_t*)urb->transfer_buffer)[1] = urb->dev->lang_id;
                    }
                    else if (desc_index - 1 < urb->dev->string_desc_count)
                    {
                        wchar_t* str         = urb->dev->strings[desc_index - 1];
                        size_t len           = wcsnlen(str, 255 / sizeof(wchar_t));
                        size_t byte_len      = len * sizeof(wchar_t);
                        usb_desc_t* hdr      = urb->transfer_buffer;
                        hdr->bDescriptorType = USB_DESC_TYPE_STR;
                        hdr->bLength         = sizeof(usb_desc_t) + byte_len;

                        if (urb->transfer_buffer_length < byte_len)
                        {
                            urb->status = ENOMEM;
                        }
                        else
                        {
                            memcpy(urb->transfer_buffer + sizeof(usb_desc_t), str, byte_len);
                            urb->actual_length = hdr->bLength;
                        }
                    }
                    break;
                }
                default:
                    urb->status = ENOTSUP;
                    break;
                }
            }
            break;
        }
        case DEV_REQ_GET_CONF:
        {
            if (urb->setup_packet.wLength == 1 && urb->transfer_buffer_length == 1
                && direction == PIPE_IN)
            {
                ((uint8_t*)urb->transfer_buffer)[0] = urb->dev->cur_configuration;
                urb->status                         = 0;
                urb->actual_length                  = 1;
            }
            else
            {
                urb->status        = EINVAL;
                urb->actual_length = 0;
            }

            urb->complete(urb, urb->context);

            break;
        }
        case DEV_REQ_SET_CONF:
        {
            uint8_t config = urb->setup_packet.wValue & 0xFF;
            if (config > 0 && config - 1 < urb->dev->desc.bNumConfigurations)
            {
                urb->dev->cur_configuration = config - 1;
                urb->actual_length          = 0;
            }
            else
            {
                urb->status = EINVAL;
            }
            break;
        }
        case DEV_REQ_GET_IF:
        {
            uint16_t idx    = urb->setup_packet.wIndex;
            size_t cur_conf = urb->dev->cur_configuration;

            ((uint8_t*)urb->transfer_buffer)[0]
                = urb->dev->configurations[cur_conf].if_alt_set[idx];

            urb->actual_length = 1;

            break;
        }
        default:
            urb->status = ENOTSUP;
            break;
        }
        urb->complete(urb, urb->context);
    }
    else
    {
        if (direction == PIPE_IN)
        {
            urb->dev->ep[ep].in(urb->transfer_buffer, urb->transfer_buffer_length);
        }
        else
        {
            urb->actual_length
                = urb->dev->ep[ep].out(urb->transfer_buffer, urb->transfer_buffer_length);
        }
    }
}