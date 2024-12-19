
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "conv.h"
#include "queue.h"
#include "usb/urb.h"
#include "usbip.h"
#include "usbip_types.h"

#if !defined(USBIP_STATIC_CLIENT_POOL_SIZE) || !defined(USBIP_DEVICE_POOL_SIZE)
#include <memory.h>
#endif

#define DEV_INFO_SIZE sizeof(struct dev_info)

#ifdef USBIP_CLIENT_POOL_SIZE
static mem_pool_t client_mem_pool;
static mem_pool_t client_node_mem_pool;
static usbip_client_t client_pool[USBIP_STATIC_CLIENT_POOL_SIZE];
static node_t client_node_pool[USBIP_STATIC_CLIENT_POOL_SIZE];

void* _client_mem_alloc(size_t n) { return mem_pool_alloc(client_mem_pool); }

void* _client_mem_free(void* obj) { return mem_pool_free(client_mem_pool, obj); }

void* _client_node_mem_alloc(size_t n) { return mem_pool_alloc(client_node_mem_pool); }

void* _client_node_mem_free(void* obj) { return mem_pool_free(client_node_mem_pool, obj); }

alloc_fn client_alloc = _client_mem_alloc;
alloc_fn client_node_alloc = _client_node_mem_alloc;
free_fn client_free = _client_mem_free;
free_fn client_node_free = _client_node_mem_free;
#else
alloc_fn client_alloc = malloc;
alloc_fn client_node_alloc = malloc;
free_fn client_free = free;
free_fn client_node_free = free;
#endif

typedef struct usbip_client
{
    int sock;
    uint8_t data_stream[1024];
    stream_fifo_t out_fifo;
    uint32_t state;
#define CLIENT_PENDING_IMPORT     0x01
#define CLIENT_DEV_IMPORTED       0x02
#define CLIENT_PENDING_URB_SUBMIT 0x04
#define CLIENT_PENDING_URB_UNLINK 0x08
    uint32_t pending_urb_size;
} usbip_client_t;

void sock_stop(int sock)
{
    shutdown(sock, O_RDWR);
    close(sock);
}

void client_stop(usbip_server_t* handle, usbip_client_t* client, size_t i)
{
    sock_stop(client->sock);
    linked_list_rem(&handle->client_list, i);
    client_free(client);
}

static uint8_t tmp_buf[4096];

int add_client(usbip_server_t* handle, int sock)
{
    usbip_client_t* client = client_alloc(sizeof(usbip_client_t));

    if (client == NULL)
    {
        return -1;
    }

    client->sock = sock;

    if (stream_fifo_init(&client->out_fifo, client->data_stream, 1024) == -1)
    {
        client_free(client);
        return -1;
    }

    if (linked_list_push(&handle->client_list, client) == -1)
    {
        client_free(client);
        return -1;
    }

    return 0;
}

/*int client_try_send(usbip_server_t* handle, usbip_client_t* client, size_t i)
{
    size_t remaining = len;
    while (remaining != 0)
    {
        int bytes = send(client->sock, tmp_buf + (len - remaining), remaining, 0);

        if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            client_stop(handle, client, i);
            return 1;
        }

        remaining -= bytes;
    }
}*/

uint8_t* usb_dev_to_buf(vusb_dev_t* dev, uint8_t* buf)
{
    buf = memcpy(buf, dev->dev->path, 256) + 256;
    buf = memcpy(buf, dev->dev->busid, 32) + 32;
    buf = WRITE_BUF_NETWORK_ENDIAN_U32(buf, dev->dev->busnum) + sizeof(uint32_t);
    buf = WRITE_BUF_NETWORK_ENDIAN_U32(buf, dev->dev->devnum) + sizeof(uint32_t);
    buf = WRITE_BUF_NETWORK_ENDIAN_U32(buf, dev->dev->speed) + sizeof(uint32_t);
    buf = WRITE_BUF_NETWORK_ENDIAN_U16(buf, dev->dev->desc.idVendor) + sizeof(uint16_t);
    buf = WRITE_BUF_NETWORK_ENDIAN_U16(buf, dev->dev->desc.idProduct) + sizeof(uint16_t);
    buf = WRITE_BUF_NETWORK_ENDIAN_U16(buf, dev->dev->desc.bcdDevice) + sizeof(uint16_t);
    *buf = dev->dev->desc.bDeviceClass;
    buf += 1;
    *buf = dev->dev->desc.bDeviceSubClass;
    buf += 1;
    *buf = dev->dev->desc.bDeviceProtocol;
    buf += 1;
    *buf = dev->dev->cur_config;
    buf += 1;
    *buf = dev->dev->desc.bNumConfigurations;
    buf += 1;

    usb_conf_t* conf = usb_dev_get_config(dev->dev, dev->dev->cur_config);

    if (conf == NULL)
    {
        *buf = 0;
    }
    else
    {
        *buf = conf->desc.bNumInterfaces;
    }

    buf += 1;

    return buf;
}

uint8_t* usb_dev_if_to_buf(vusb_dev_t* dev, uint8_t* buf)
{

    usb_conf_t* conf = usb_dev_get_config(dev->dev, dev->dev->cur_config);

    if (conf != NULL)
    {
        usb_if_group_t* cur = conf->interfaces;

        while (cur != NULL)
        {
            usb_if_t* cur_if = cur->interfaces;
            while (cur_if != NULL)
            {
                *buf = cur_if->desc.bInterfaceClass;
                buf += 1;
                *buf = cur_if->desc.bInterfaceSubClass;
                buf += 1;
                *buf = cur_if->desc.bInterfaceProtocol;
                buf += 1;
                *buf = 0;
                buf += 1;

                cur_if = cur_if->next;
            }
            cur = cur->next;
        }
    }

    return buf;
}

void fill_devlist(vusb_dev_t* dev, void* ctx)
{
    uint8_t* buf = (*(void**)ctx);
    buf = usb_dev_to_buf(dev, buf);
    buf = usb_dev_if_to_buf(dev, buf);

    (*(void**)ctx) = buf;
}

int usbip_resp_devlist(usbip_server_t* handle, usbip_client_t* client, size_t i)
{
    hdr_rep_devlist_t* hdr = (hdr_rep_devlist_t*)tmp_buf;

    hdr->hdr.op_code = TO_NETWORK_ENDIAN_U16(REP_DEVLIST);
    hdr->hdr.version = TO_NETWORK_ENDIAN_U16(USBIP_VERSION);
    hdr->hdr.status = USBIP_STATUS_OK;
    hdr->dev_count = TO_NETWORK_ENDIAN_U32(handle->vhci_handle->devices.size);

    uint8_t* dev_buf = tmp_buf + sizeof(hdr_rep_devlist_t);

    if (handle->vhci_handle->devices.size > 0)
    {
        vhci_iter_devices(handle->vhci_handle, fill_devlist, &dev_buf);
    }

    size_t length = dev_buf - tmp_buf;

    if (stream_fifo_push(&client->out_fifo, tmp_buf, length) == 0)
    {
        client_stop(handle, client, i);
        return -1;
    }

    return 0;
}

int usbip_handle_import(usbip_server_t* handle, usbip_client_t* client, size_t i)
{
    char busid[32] = { 0 };

    int bytes = recv(client->sock, busid, 32, 0);

    hdr_common_t* hdr = (hdr_common_t*)tmp_buf;

    hdr->version = TO_NETWORK_ENDIAN_U16(USBIP_VERSION);
    hdr->op_code = TO_NETWORK_ENDIAN_U16(REP_IMPORT);
    hdr->status = TO_NETWORK_ENDIAN_U32(USBIP_STATUS_ERROR);

    int length = 8;

    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
    {
        // No data yet
        return 0;
    }
    else if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        client_stop(handle, client, i);
        return -1;
    }
    else if (bytes > 0)
    {
        vusb_dev_t* dev = vhci_find_device(handle->vhci_handle, busid);

        if (dev != NULL)
        {
            hdr->status = TO_NETWORK_ENDIAN_U32(USBIP_STATUS_OK);

            uint8_t* res = usb_dev_to_buf(dev, tmp_buf + sizeof(hdr_common_t));

            length = res - tmp_buf;
        }
        else
        {
            hdr->status = TO_NETWORK_ENDIAN_U32(USBIP_STATUS_ERROR);
        }
    }

    if (stream_fifo_push(&client->out_fifo, tmp_buf, length) == 0)
    {
        client_stop(handle, client, i);
        return -1;
    }

    return 0;
}

int usbip_accept_new_client(usbip_server_t* handle)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(struct sockaddr_in);

    // Check for a new connection
    int client_sock = accept(handle->listen_sock, (struct sockaddr*)&client_addr, &client_len);

    // Handle a new connection
    if (client_sock != -1)
    {
        // Get flags for new client.
        int flags = fcntl(client_sock, F_GETFL);

        // Unable to get flags
        if (flags < 0)
        {
            sock_stop(client_sock);
            return -1;
        }

        // Set non blocking on new client.
        if (fcntl(client_sock, F_SETFL, flags | O_NONBLOCK | O_CLOEXEC) < 0)
        {
            sock_stop(client_sock);
            return -1;
        }

        int no_delay = 1;
        int keepalive_interval = 10;
        int keepalive_cnt = 10;

        // Set no delay on client.
        if (setsockopt(client_sock, SOL_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay)))
        {
            sock_stop(client_sock);
            return -1;
        }

        // Setup keepalive packets for client.
        if (setsockopt(client_sock, SOL_SOCKET, TCP_KEEPINTVL, &keepalive_interval,
                sizeof(keepalive_interval)))
        {
            sock_stop(client_sock);
            return -1;
        }

        // Set keepalive count
        if (setsockopt(client_sock, SOL_SOCKET, TCP_KEEPCNT, &keepalive_cnt, sizeof(keepalive_cnt)))
        {
            sock_stop(client_sock);
            return -1;
        }

        // Add client to list of current clients (may fail if no more clients can be accepted).
        if (add_client(handle, client_sock))
        {
            sock_stop(client_sock);
            return -1;
        }
    }
    // Acceptor socket failed in some way, stop this socket.
    else if (client_sock == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        sock_stop(handle->listen_sock);
        return -1;
    }

    return 0;
}

int usbip_server_setup(usbip_server_t* handle, vhci_handle_t* usb_handle)
{

#ifdef USBIP_CLIENT_POOL_SIZE
    init_mem_pool(sizeof(usbip_client_t), client_pool, USBIP_CLIENT_POOL_SIZE, client_mem_pool);
    init_mem_pool(sizeof(node_t), client_node_pool, USBIP_CLIENT_POOL_SIZE, client_node_mem_pool);
#endif

    memset(handle, 0, sizeof(usbip_server_t));

    handle->vhci_handle = usb_handle;

    if (linked_list_init(client_node_alloc, client_node_free, &handle->client_list))
    {
        return -1;
    }

    handle->listen_sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

    if (handle->listen_sock == -1)
    {
        return -1;
    }

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(struct sockaddr_in));

    addr.sin_port = TO_NETWORK_ENDIAN_U16(3240);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = 0;

    if (bind(handle->listen_sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1)
    {
        printf("bind failed %s", strerror(errno));
        sock_stop(handle->listen_sock);
        return -1;
    }

    int flags = fcntl(handle->listen_sock, F_GETFL);

    if (flags < 0)
    {
        sock_stop(handle->listen_sock);
        return -1;
    }

    if (fcntl(handle->listen_sock, F_SETFL, flags | O_NONBLOCK))
    {
        sock_stop(handle->listen_sock);
        return -1;
    }

    if (listen(handle->listen_sock, 1))
    {
        sock_stop(handle->listen_sock);
        return -1;
    }

    return 0;
}

int handle_urb_submit(usbip_server_t* handle, usbip_client_t* client, size_t i, hdr_cmd_t cmd)
{
    uint8_t buf[28] = { 0 };

    int bytes = recv(client->sock, buf, 28, 0);

    if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        client_stop(handle, client, i);
        return -1;
    }
    else if (bytes == 28)
    {
        uint32_t txfer_flags = FROM_NETWORK_ENDIAN_U32(*(uint32_t*)(buf));
        uint32_t txfer_buf_len = FROM_NETWORK_ENDIAN_U32(*(uint32_t*)(buf + 4));
        uint32_t iso_start_frame = FROM_NETWORK_ENDIAN_U32(*(uint32_t*)(buf + 8));
        uint32_t iso_pkt_cnt = FROM_NETWORK_ENDIAN_U32(*(uint32_t*)(buf + 12));
        uint32_t interval = FROM_NETWORK_ENDIAN_U32(*(uint32_t*)(buf + 16));
        uint64_t usb_setup = FROM_NETWORK_ENDIAN_U64(*(uint64_t*)(buf + 20));

        uint8_t* txfer_buffer = malloc(txfer_buf_len);

        if (txfer_buffer == NULL)
        {
            return -1;
        }

        bytes = recv(client->sock, txfer_buffer, txfer_buf_len, 0);

        if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            free(txfer_buffer);
            client_stop(handle, client, i);
            return -1;
        }
        else if (bytes == txfer_buf_len)
        {
            urb_t urb = {
                .transfer_flags = txfer_flags,
                .transfer_buffer_length = txfer_buf_len,
                .start_frame = iso_start_frame,
                .number_of_packets = iso_pkt_cnt,
                .interval = interval,
                .setup_packet = *((urb_setup_t*)&usb_setup),
                .seq_num = cmd.seq_num,
                .transfer_buffer = txfer_buffer,
            };

            if (vhci_submit_urb(handle->vhci_handle, urb) == -1)
            {
                free(txfer_buffer);
                return -1;
            }
        }
        else
        {
            free(txfer_buffer);
            return 0;
        }

        client->state |= CLIENT_PENDING_URB_SUBMIT;
    }

    return 0;
}

int usbip_client_handle(void* data, size_t i, void* ctx)
{
    usbip_server_t* handle = ctx;
    usbip_client_t* client = data;

    if (stream_fifo_length(&client->out_fifo) > 0)
    {
        int bytes = stream_fifo_send_sock(&client->out_fifo, client->sock);

        // Send failed
        if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            client_stop(handle, client, i);
            return -1;
        }
        // No data left, handle further operations
        else if (stream_fifo_length(&client->out_fifo) == 0)
        {
            if (client->state & CLIENT_PENDING_IMPORT)
            {
                client->state &= ~CLIENT_PENDING_IMPORT;
                client->state |= CLIENT_DEV_IMPORTED;
                return 0;
            }
            else
            {
                client_stop(handle, client, i);
                return -1;
            }
        }
    }

    if (client->state == 0)
    {
        hdr_common_t hdr;

        ssize_t bytes = recv(client->sock, &hdr, sizeof(hdr_common_t), 0);

        int errno_val = errno;

        if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            client_stop(handle, client, i);
            return -1;
        }
        else if (bytes == sizeof(hdr_common_t))
        {
            hdr.version = FROM_NETWORK_ENDIAN_U16(hdr.version);
            hdr.op_code = FROM_NETWORK_ENDIAN_U16(hdr.op_code);
            hdr.status = FROM_NETWORK_ENDIAN_U32(hdr.status);

            if (hdr.version == USBIP_VERSION)
            {
                switch (hdr.op_code)
                {
                case REQ_DEVLIST:
                    return usbip_resp_devlist(handle, client, i);
                case REQ_IMPORT:
                    int res = usbip_handle_import(handle, client, i);
                    if (res == 0)
                    {
                        client->state |= CLIENT_PENDING_IMPORT;
                    }
                    return res;
                default:
                    break;
                }
            }
        }
        // No data from peer, close socket
        else if (bytes == 0)
        {
            client_stop(handle, client, i);
            return -1;
        }
    }
    else if (client->state & CLIENT_DEV_IMPORTED)
    {
        hdr_cmd_t cmd;

        ssize_t bytes = recv(client->sock, &cmd, sizeof(hdr_cmd_t), 0);

        // Socket error, close socket
        if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            client_stop(handle, client, i);
            return -1;
        }
        // New command from client
        else if (bytes == sizeof(hdr_cmd_t))
        {
            cmd.command = FROM_NETWORK_ENDIAN_U16(cmd.command);
            cmd.seq_num = FROM_NETWORK_ENDIAN_U16(cmd.seq_num);
            cmd.busnum = FROM_NETWORK_ENDIAN_U16(cmd.busnum);
            cmd.devnum = FROM_NETWORK_ENDIAN_U16(cmd.devnum);
            cmd.direction = FROM_NETWORK_ENDIAN_U16(cmd.direction);
            cmd.endpoint = FROM_NETWORK_ENDIAN_U16(cmd.endpoint);
        }
        else if (bytes == 0)
        {
            client_stop(handle, client, i);
            return -1;
        }

        switch (cmd.command)
        {
        case USBIP_CMD_SUBMIT:
            return handle_urb_submit(handle, client, i, cmd);
            break;
        case USBIP_CMD_UNLINK:
            vhci_unlink_urb(handle->vhci_handle, cmd.seq_num);
            break;
            // Unknown command, close socket
        default:
            client_stop(handle, client, i);
            return -1;
        }
    }
    else if (client->state & CLIENT_PENDING_IMPORT)
    {
        return usbip_handle_import(handle, client, i);
    }

    return 0;
}

int usbip_server_handle_once(usbip_server_t* handle)
{
    usbip_accept_new_client(handle);

    linked_list_iter(&handle->client_list, usbip_client_handle, handle);

    return 0;
}