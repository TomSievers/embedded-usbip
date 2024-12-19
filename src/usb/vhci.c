#include "vhci.h"
#include "conv.h"
#include "sock.h"
#include "usbip_types.h"
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wchar.h>

#define URB_RET_HDR_SIZE sizeof(hdr_cmd_t) + sizeof(struct ret_base)

typedef __ssize_t ssize_t;

#ifdef DEV_POOL_SIZE
static mem_pool_t dev_mem_pool;
static mem_pool_t dev_node_mem_pool;
static vusb_dev_t dev_pool[DEV_POOL_SIZE];
static node_t dev_node_pool[DEV_POOL_SIZE];

void* _dev_mem_alloc(size_t n) { return mem_pool_alloc(dev_mem_pool); }

void* _dev_mem_free(void* obj) { return mem_pool_free(dev_mem_pool, obj); }

void* _dev_node_mem_alloc(size_t n) { return mem_pool_alloc(dev_node_mem_pool); }

void* _dev_node_mem_free(void* obj) { return mem_pool_free(dev_node_mem_pool, obj); }

alloc_fn dev_alloc = _dev_mem_alloc;
alloc_fn dev_node_alloc = _dev_node_mem_alloc;
free_fn dev_free = _dev_mem_free;
free_fn dev_node_free = _dev_node_mem_free;
#else
alloc_fn dev_alloc = malloc;
alloc_fn dev_node_alloc = malloc;
free_fn dev_free = free;
free_fn dev_node_free = free;
#endif

#ifdef URB_POOL_SIZE
static mem_pool_t urb_mem_pool;
static urb_t urb_pool[URB_POOL_SIZE];

void* _urb_mem_alloc(size_t n) { return mem_pool_alloc(urb_mem_pool); }

void* _urb_mem_free(void* obj) { return mem_pool_free(urb_mem_pool, obj); }

alloc_fn urb_alloc = _urb_mem_alloc;
free_fn urb_free = _urb_mem_free;
#else
alloc_fn urb_alloc = malloc;
free_fn urb_free = free;
#endif

static void stop_sock(int* sock)
{
    shutdown(*sock, O_RDWR);
    close(*sock);
    *sock = NO_SOCK;
}

int vhci_init(vhci_handle_t* handle)
{
#ifdef DEV_POOL_SIZE
    init_mem_pool(sizeof(vusb_dev_t), dev_pool, DEV_POOL_SIZE, dev_mem_pool);
    init_mem_pool(sizeof(node_t), dev_node_pool, DEV_POOL_SIZE, dev_node_mem_pool);
#endif

#ifdef URB_POOL_SIZE
    init_mem_pool(sizeof(urb_t), urb_mem_pool, URB_POOL_SIZE, urb_mem_pool);
#endif

    memset(handle, 0, sizeof(vhci_handle_t));

    if (linked_list_init(dev_node_alloc, dev_node_free, &handle->devices))
    {
        return -1;
    }

    return 0;
}

void vhci_urb_complete(vusb_dev_t* dev, urb_t* urb, void* ctx)
{
    uint16_t seq_num = *((uint16_t*)ctx);
    uint8_t buf[512];
    hdr_cmd_t* hdr = (hdr_cmd_t*)buf;
    struct ret_base* ret = (struct ret_base*)(buf + sizeof(hdr_cmd_t));
    uint8_t* data = (buf + URB_RET_HDR_SIZE);
    memset(hdr, 0, sizeof(hdr_cmd_t));
    hdr->command = USBIP_RET_SUBMIT;
    hdr->seq_num = seq_num;

    ret->actual_length = urb->actual_length;
    ret->error_count = urb->error_count;
    ret->number_of_packets = urb->number_of_packets;
    ret->start_frame = urb->start_frame;
    ret->status = urb->status;

    for (size_t i = 0; i < urb->actual_length; ++i)
    {
        data[i] = ((uint8_t*)urb->transfer_buffer)[i];
    }

    send(dev->client, buf, URB_RET_HDR_SIZE + urb->actual_length, 0);
}

void handle_get_desc(vusb_dev_t* dev, urb_t* urb)
{
    uint8_t desc_type = urb->setup_packet.wValue >> 8;
    uint8_t desc_index = urb->setup_packet.wValue;
    urb->status = -EINVAL;
    if (urb->transfer_buffer != NULL && urb->transfer_buffer_length > 2
        && desc_type <= USB_DESC_TYPE_IF_PWR)
    {
        switch (desc_type)
        {
        case USB_DESC_TYPE_DEV:
        {
            void* dest = urb->transfer_buffer;

            usb_desc_hdr_t* hdr = dest;
            hdr->bLength = sizeof(usb_dev_desc_t) + sizeof(usb_desc_hdr_t);
            hdr->bDescriptorType = USB_DESC_TYPE_CONF;

            usb_dev_desc_t* desc = &dev->dev->desc;

            memcpy(dest + sizeof(usb_desc_hdr_t), desc, sizeof(usb_dev_desc_t));
            urb->actual_length = hdr->bLength;
            break;
        }
        case USB_DESC_TYPE_CONF:
        {
            ssize_t remaining = urb->transfer_buffer_length;
            usb_conf_t* conf = usb_dev_get_config(dev->dev, desc_index);
            void* dest = urb->transfer_buffer;
            usb_desc_hdr_t* hdr = dest;
            hdr->bDescriptorType = USB_DESC_TYPE_CONF;
            hdr->bLength = sizeof(usb_conf_desc_t) + sizeof(usb_desc_hdr_t);
            if (conf != NULL)
            {
                if ((remaining -= hdr->bLength) < 0)
                {
                    urb->status = -ENOMEM;
                    return;
                }

                dest = memcpy(dest + sizeof(usb_desc_hdr_t), &conf->desc, sizeof(usb_conf_desc_t))
                    + sizeof(usb_conf_desc_t);
                usb_if_group_t* cur_if_grp = conf->interfaces;
                while (cur_if_grp != NULL)
                {
                    usb_if_t* cur_if = cur_if_grp->interfaces;
                    while (cur_if_grp != NULL)
                    {
                        hdr = dest;
                        hdr->bDescriptorType = USB_DESC_TYPE_IF;
                        hdr->bLength = sizeof(usb_if_desc_t) + sizeof(usb_desc_hdr_t);

                        if ((remaining -= hdr->bLength) < 0)
                        {
                            urb->status = -ENOMEM;
                            return;
                        }

                        dest = memcpy(dest + sizeof(usb_desc_hdr_t), &cur_if->desc,
                                   sizeof(usb_if_desc_t))
                            + sizeof(usb_if_desc_t);

                        usb_ep_t* ep = cur_if->endpoints;

                        while (ep != NULL)
                        {
                            hdr = dest;
                            hdr->bDescriptorType = USB_DESC_TYPE_EP;
                            hdr->bLength = sizeof(usb_ep_desc_t) + sizeof(usb_desc_hdr_t);

                            if ((remaining -= hdr->bLength) < 0)
                            {
                                urb->status = -ENOMEM;
                                return;
                            }

                            dest = memcpy(dest + sizeof(usb_desc_hdr_t), &ep->desc,
                                       sizeof(usb_ep_desc_t))
                                + sizeof(usb_ep_desc_t);

                            ep = ep->next;
                        }

                        cur_if = cur_if->next;
                    }
                    cur_if_grp = cur_if_grp->next;
                }

                urb->actual_length = urb->transfer_buffer_length - remaining;
            }

            break;
        }
        case USB_DESC_TYPE_STR:
        {
            void* dest = urb->transfer_buffer;
            // Lang ID request
            if (desc_index == 0)
            {
                if (urb->transfer_buffer_length < sizeof(usb_desc_hdr_t) + 2)
                {
                    urb->status = -ENOMEM;
                    return;
                }

                usb_desc_hdr_t* hdr = dest;
                hdr->bDescriptorType = USB_DESC_TYPE_STR;
                hdr->bLength = sizeof(usb_desc_hdr_t) + 2;

                *((uint16_t*)dest + sizeof(usb_desc_hdr_t)) = dev->dev->lang_id;

                urb->actual_length = hdr->bLength;
            }
            else
            {
                usb_string_desc_t* str = usb_dev_get_str(dev->dev, desc_index);

                if (str == NULL)
                {
                    urb->status = -EINVAL;
                    return;
                }

                size_t len = wcsnlen(str->string, 255 / sizeof(wchar_t));
                size_t byte_len = len * sizeof(wchar_t);

                if (urb->transfer_buffer_length < sizeof(usb_desc_hdr_t) + byte_len)
                {
                    urb->status = -ENOMEM;
                    return;
                }

                usb_desc_hdr_t* hdr = dest;
                hdr->bDescriptorType = USB_DESC_TYPE_STR;
                hdr->bLength = sizeof(usb_desc_hdr_t) + byte_len;
                memcpy(urb->transfer_buffer + sizeof(usb_desc_hdr_t), str, byte_len);
                urb->actual_length = hdr->bLength;
            }
            break;
        }
        default:
            urb->status = ENOTSUP;
            break;
        }
    }
}

void handle_urb(vusb_dev_t* dev, urb_t* urb)
{
    uint8_t direction = PIPE_DIR(urb->pipe);
    uint8_t ep = PIPE_EP(urb->pipe);
    uint8_t type = PIPE_TYPE(urb->pipe);
    if (ep == 0 && type == PIPE_TYPE_CTRL)
    {
        urb->actual_length = 0;
        switch (urb->setup_packet.bRequest)
        {
        case DEV_REQ_STATUS:
        {
            uint16_t idx = urb->setup_packet.wIndex;
            uint8_t type = urb->setup_packet.bmRequestType.recipient;

            urb->actual_length = 2;
            ((uint16_t*)urb->transfer_buffer)[0] = 0;
            break;
        }
        case DEV_REQ_CLR_FEAT:
        {
            uint16_t idx = urb->setup_packet.wIndex;
            uint8_t type = urb->setup_packet.bmRequestType.recipient;
            uint16_t feature = urb->setup_packet.wValue;

            // TODO: IMPL
            break;
        }
        case DEV_REQ_SET_FEAT:
        {
            uint8_t idx = urb->setup_packet.wIndex & 0xFF;
            uint8_t test = urb->setup_packet.wIndex >> 8;
            uint8_t type = urb->setup_packet.bmRequestType.recipient;
            uint16_t feature = urb->setup_packet.wValue;

            // TODO: IMPL
            break;
        }
        case DEV_REQ_GET_DESC:
        {
            handle_get_desc(dev, urb);
            break;
        }
        case DEV_REQ_GET_CONF:
        {
            if (urb->setup_packet.wLength == 1 && urb->transfer_buffer_length == 1
                && direction == PIPE_IN)
            {
                ((uint8_t*)urb->transfer_buffer)[0] = dev->dev->cur_config;
                urb->status = 0;
                urb->actual_length = 1;
            }
            else
            {
                urb->status = EINVAL;
            }

            break;
        }
        case DEV_REQ_SET_CONF:
        {
            uint8_t config = urb->setup_packet.wValue & 0xFF;
            usb_conf_t* conf = usb_dev_get_config(dev->dev, config);
            if (conf != NULL)
            {
                dev->dev->cur_config = conf->desc.bConfigurationValue;
                urb->actual_length = 0;
            }
            else
            {
                urb->status = EINVAL;
            }
            break;
        }
        case DEV_REQ_GET_IF:
        {
            urb->status = EINVAL;
            uint16_t idx = urb->setup_packet.wIndex;

            usb_if_group_t* cur_if = usb_dev_get_if_grp(dev->dev, dev->dev->cur_config, idx);

            if (cur_if != NULL)
            {
                ((uint8_t*)urb->transfer_buffer)[0] = cur_if->cur_alt_set;

                urb->actual_length = 1;
            }

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
            // urb->dev->ep[ep].in(urb->transfer_buffer, urb->transfer_buffer_length);
        }
        else
        {
            /*urb->actual_length
                = urb->dev->ep[ep].out(urb->transfer_buffer, urb->transfer_buffer_length);*/
        }
    }
}

int vhci_register_dev(vhci_handle_t* handle, usb_dev_t* dev)
{
    if (handle == NULL || dev == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    dev->busnum = 1;
    dev->devnum = 1;
    dev->bus = 1;

    vusb_dev_t* vdev = dev_alloc(sizeof(vusb_dev_t));

    if (vdev == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    vdev->dev = dev;

    uint32_t devnum = 0;

    if (handle->devices.size > 0)
    {
        vusb_dev_t* last_vdev = linked_list_get(&handle->devices, handle->devices.size - 1);

        if (last_vdev == NULL)
        {
            dev_free(vdev);
            errno = ECANCELED;
            return -1;
        }

        devnum = last_vdev->dev->devnum;
    }

    if (linked_list_push(&handle->devices, vdev) == -1)
    {
        dev_free(vdev);
        return -1;
    }

    dev->devnum = devnum + 1;

    snprintf(dev->path, 256, "/dev/bus/usb/%03d/%03d", dev->busnum, dev->devnum);
    snprintf(dev->busid, 32, "%u-%u", dev->busnum, dev->devnum);

    return 0;
}

typedef struct vhci_iter_ctx
{
    void* user_ctx;
    vhci_iter_cb user_cb;
} vhci_iter_ctx_t;

int vhci_iter_internal(void* data, size_t idx, void* ctx)
{
    vhci_iter_ctx_t* iter_ctx = ctx;

    iter_ctx->user_cb(data, iter_ctx->user_ctx);
    return 0;
}

void vhci_iter_devices(vhci_handle_t* handle, vhci_iter_cb cb, void* ctx)
{
    vhci_iter_ctx_t iter_ctx = {
        .user_ctx = ctx,
        .user_cb = cb,
    };

    linked_list_iter(&handle->devices, vhci_iter_internal, &iter_ctx);
}

typedef struct vhci_find_dev_ctx
{
    const char* busid;
    vusb_dev_t* dev;
} vhci_find_dev_ctx_t;

int vhci_find_dev_iter(void* data, size_t idx, void* ctx)
{
    vhci_find_dev_ctx_t* find_ctx = ctx;
    vusb_dev_t* dev = data;

    if (strncmp(find_ctx->busid, dev->dev->busid, 32) == 0)
    {
        find_ctx->dev = dev;
    }

    return 0;
}

vusb_dev_t* vhci_find_device(vhci_handle_t* handle, const char* busid)
{
    vhci_find_dev_ctx_t ctx = {
        .busid = busid,
        .dev = NULL,
    };
    linked_list_iter(&handle->devices, vhci_find_dev_iter, &ctx);

    return ctx.dev;
}

int vhci_handle_dev(void* data, size_t idx, void* ctx)
{
    vhci_handle_t* handle = ctx;
    vusb_dev_t* dev = data;

    if (dev->client != NO_SOCK)
    {
    }

    return 0;
}

int vhci_submit_urb(vhci_handle_t* handle, urb_t urb)
{
    errno = ENOTSUP;
    return -1;
}

int vhci_unlink_urb(vhci_handle_t* handle, uint32_t seq_num)
{
    errno = ENOTSUP;
    return -1;
}

void vhci_run_once(vhci_handle_t* handle)
{
    linked_list_iter(&handle->devices, vhci_find_dev_iter, handle);
}