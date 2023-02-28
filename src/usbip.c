#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>

#include <netinet/tcp.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "conv.h"
#include "usbip.h"
#include "usbip_types.h"
#include <stdio.h>

// #define USBIP_STATIC_CLIENT_POOL_SIZE 5
#if !defined(USBIP_STATIC_CLIENT_POOL_SIZE) || !defined(USBIP_DEVICE_POOL_SIZE)
#include <memory.h>
#endif

#define DEV_SIZE sizeof(usb_dev_t) - sizeof(dev_urb_cb_t) - sizeof(usb_if_t*)

#ifdef USBIP_CLIENT_POOL_SIZE
static mem_pool_t client_mem_pool;
static mem_pool_t client_node_mem_pool;
static usbip_client_t client_pool[USBIP_STATIC_CLIENT_POOL_SIZE];
static node_t client_node_pool[USBIP_STATIC_CLIENT_POOL_SIZE];

void* _client_mem_alloc(size_t n)
{
    return mem_pool_alloc(client_mem_pool);
}

void* _client_mem_free(void* obj)
{
    return mem_pool_free(client_mem_pool, obj);
}

void* _client_node_mem_alloc(size_t n)
{
    return mem_pool_alloc(client_node_mem_pool);
}

void* _client_node_mem_free(void* obj)
{
    return mem_pool_free(client_node_mem_pool, obj);
}

alloc_fn client_alloc      = _client_mem_alloc;
alloc_fn client_node_alloc = _client_node_mem_alloc;
free_fn client_free        = _client_mem_free;
free_fn client_node_free   = _client_node_mem_free;
#else
alloc_fn client_alloc      = malloc;
alloc_fn client_node_alloc = malloc;
free_fn client_free        = free;
free_fn client_node_free   = free;
#endif

#ifdef USBIP_DEVICE_POOL_SIZE
static mem_pool_t dev_node_mem_pool;
static node_t dev_node_pool[USBIP_STATIC_CLIENT_POOL_SIZE];

void* _dev_mem_alloc(size_t n)
{
    return mem_pool_alloc(dev_node_mem_pool);
}

void* _dev_mem_free(void* obj)
{
    return mem_pool_free(dev_node_mem_pool, obj);
}

alloc_fn dev_node_alloc = _dev_mem_alloc;
free_fn dev_node_free   = _dev_mem_free;
#else
alloc_fn dev_node_alloc    = malloc;
free_fn dev_node_free      = free;
#endif

typedef struct usbip_client
{
    int sock;
    usb_dev_t* imported_dev;
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

    client->sock         = sock;
    client->imported_dev = NULL;

    if (linked_list_push(&handle->client_list, client) == -1)
    {
        client_free(client);
        return -1;
    }

    return 0;
}

int dev_list_cpy(void* data, size_t i, void* ctx)
{
    const size_t dev_len = sizeof(usb_dev_t) - sizeof(dev_urb_cb_t) - sizeof(usb_if_t*);
    usb_dev_t* dev       = data;
    size_t* out_idx      = ctx;

    uint8_t* out         = tmp_buf + *out_idx;

    out                  = memcpy(out, dev, sizeof(struct dev_info));
    *out_idx             += dev_len;

    for (size_t if_idx = 0; if_idx < dev->info.interface_cnt; ++if_idx)
    {
        out      = memcpy(out, &dev->interfaces[if_idx], sizeof(usb_if_t));
        *out_idx += sizeof(usb_if_t);
    }

    return 0;
}

int usbip_resp_devlist(usbip_server_t* handle, usbip_client_t* client, size_t i)
{
    size_t idx = sizeof(hdr_rep_devlist_t);
    linked_list_iter(&handle->dev_list, dev_list_cpy, &idx);

    hdr_rep_devlist_t* hdr = (hdr_rep_devlist_t*)tmp_buf;

    hdr->hdr.op_code       = TO_NETWORK_ENDIAN_U16(REP_DEVLIST);
    hdr->hdr.version       = TO_NETWORK_ENDIAN_U16(USBIP_VERSION);
    hdr->hdr.status        = USBIP_STATUS_OK;
    hdr->dev_count         = TO_NETWORK_ENDIAN_U32(handle->dev_list.size);

    int remaining          = idx;

    while (remaining != 0)
    {
        int bytes = send(client->sock, tmp_buf + (idx - remaining), remaining, 0);

        if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            client_stop(handle, client, i);
            return 1;
        }

        remaining -= bytes;
    }

    return 0;
}

int dev_idx = -1;

int find_dev(void* data, size_t i, void* ctx)
{
    usb_dev_t* dev = data;
    char* busid    = ctx;

    if (strncmp(busid, dev->info.busid, 32) == 0)
    {
        dev_idx = i;
    }

    return 0;
}

int usbip_handle_import(usbip_server_t* handle, usbip_client_t* client, size_t i)
{
    dev_idx = -1;
    char busid[32];

    int bytes = recv(client->sock, busid, sizeof(busid), 0);

    if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        return 1;
    }

    linked_list_iter(&handle->dev_list, find_dev, busid);

    hdr_common_t* hdr = (hdr_common_t*)tmp_buf;

    hdr->version      = TO_NETWORK_ENDIAN_U16(USBIP_VERSION);
    hdr->op_code      = TO_NETWORK_ENDIAN_U16(REP_IMPORT);

    if (dev_idx >= 0)
    {
        usb_dev_t* dev = linked_list_get(&handle->dev_list, dev_idx);

        hdr->status    = USBIP_STATUS_OK;
        memcpy(tmp_buf + sizeof(hdr_common_t), dev, sizeof(usb_dev_t) - sizeof(dev_urb_cb_t) - sizeof(usb_if_t*));

        size_t idx    = sizeof(hdr_common_t) + DEV_SIZE;

        int remaining = idx;

        while (remaining != 0)
        {
            int bytes = send(client->sock, tmp_buf + (idx - remaining), remaining, 0);

            if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                client_stop(handle, client, i);
                return 1;
            }

            remaining -= bytes;
        }
    }
    else
    {
        hdr->status = TO_NETWORK_ENDIAN_U32(USBIP_STATUS_ERROR);
    }

    return 0;
}

int usbip_handle_submit(usbip_server_t* handle, usbip_client_t* client, size_t i, hdr_cmd_t hdr)
{
    cmd_t cmd;

    int bytes = recv(client->sock, &cmd, sizeof(struct cmd_base), 0);

    if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        return 1;
    }
    else if (bytes == sizeof(struct cmd_base))
    {
    }

    return 0;
}

int usbip_accept_new_client(usbip_server_t* handle)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(struct sockaddr_in);

    // Check for a new connection
    int client_sock      = accept(handle->listen_sock, (struct sockaddr*)&client_addr, &client_len);

    // Handle a new connection
    if (client_sock != -1)
    {
        int flags = fcntl(client_sock, F_GETFL);

        if (flags < 0)
        {
            sock_stop(client_sock);
            return -1;
        }

        // Set non blocking on new client.
        if (fcntl(client_sock, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            sock_stop(client_sock);
            return -1;
        }

        int no_delay           = 1;
        int keepalive_interval = 10;
        int keepalive_cnt      = 10;

        // Set no delay on client.
        if (setsockopt(client_sock, SOL_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay)))
        {
            sock_stop(client_sock);
            return -1;
        }

        // Setup keepalive packets for client.
        if (setsockopt(client_sock, SOL_SOCKET, TCP_KEEPINTVL, &keepalive_interval, sizeof(keepalive_interval)))
        {
            sock_stop(client_sock);
            return -1;
        }

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
    else if (client_sock == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        // Log error
        sock_stop(handle->listen_sock);
        return -1;
    }

    return 0;
}

int usbip_server_setup(usbip_server_t* handle)
{

#ifdef USBIP_DEVICE_POOL_SIZE
    init_mem_pool(sizeof(node_t), dev_node_pool, USBIP_DEVICE_POOL_SIZE, dev_node_mem_pool);
#endif

#ifdef USBIP_CLIENT_POOL_SIZE
    init_mem_pool(sizeof(usbip_client_t), client_pool, USBIP_CLIENT_POOL_SIZE, client_mem_pool);
    init_mem_pool(sizeof(node_t), client_node_pool, USBIP_CLIENT_POOL_SIZE, client_node_mem_pool);
#endif

    if (linked_list_init(client_node_alloc, client_node_free, &handle->client_list))
    {
        return -1;
    }

    if (linked_list_init(dev_node_alloc, dev_node_free, &handle->dev_list))
    {
        return -1;
    }

    handle->listen_sock = socket(AF_INET, SOCK_STREAM | O_NONBLOCK, 0);

    if (handle->listen_sock == -1)
    {
        return -1;
    }

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(struct sockaddr_in));

    addr.sin_port   = TO_NETWORK_ENDIAN_U16(3240);
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &(addr.sin_addr));

    if (bind(handle->listen_sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1)
    {
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

int usbip_client_handle(void* data, size_t i, void* ctx)
{
    usbip_server_t* handle = ctx;
    usbip_client_t* client = data;

    if (client->imported_dev == NULL)
    {
        hdr_common_t hdr;

        ssize_t bytes = recv(client->sock, &hdr, sizeof(hdr_common_t), 0);

        int errno_val = errno;

        if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            client_stop(handle, client, i);
            return 1;
        }
        else if (bytes == sizeof(hdr_common_t))
        {
            hdr.version = FROM_NETWORK_ENDIAN_U16(hdr.version);
            hdr.op_code = FROM_NETWORK_ENDIAN_U16(hdr.op_code);
            hdr.status  = FROM_NETWORK_ENDIAN_U32(hdr.status);

            if (hdr.version == USBIP_VERSION)
            {
                switch (hdr.op_code)
                {
                case REQ_DEVLIST:
                    usbip_resp_devlist(handle, client, i);
                    break;
                case REQ_IMPORT:
                    usbip_handle_import(handle, client, i);
                    break;
                default:
                    break;
                }
            }
        }
        else if (bytes == 0)
        {
            client_stop(handle, client, i);
            return 1;
        }
    }
    else
    {
        hdr_cmd_t cmd;

        ssize_t bytes = recv(client->sock, &cmd, sizeof(hdr_cmd_t), 0);
        if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            client_stop(handle, client, i);
            return 1;
        }
        else if (bytes == sizeof(hdr_cmd_t))
        {
            cmd.command   = FROM_NETWORK_ENDIAN_U16(cmd.command);
            cmd.seq_num   = FROM_NETWORK_ENDIAN_U16(cmd.seq_num);
            cmd.busnum    = FROM_NETWORK_ENDIAN_U16(cmd.busnum);
            cmd.devnum    = FROM_NETWORK_ENDIAN_U16(cmd.devnum);
            cmd.direction = FROM_NETWORK_ENDIAN_U16(cmd.direction);
            cmd.endpoint  = FROM_NETWORK_ENDIAN_U16(cmd.endpoint);

            switch (cmd.command)
            {
            case CMD_SUBMIT:
                break;
            case CMD_UNLINK:
                break;
            default:
                break;
            }
        }
        else if (bytes == 0)
        {
            client_stop(handle, client, i);
            return 1;
        }
    }

    return 0;
}

int usbip_server_handle_once(usbip_server_t* handle)
{
    usbip_accept_new_client(handle);

    linked_list_iter(&handle->client_list, usbip_client_handle, handle);

    return 0;
}

int usbip_add_dev(usbip_server_t* handle, usb_dev_t* dev)
{
    dev->info.bus        = TO_NETWORK_ENDIAN_U32(dev->info.bus);
    dev->info.devnum     = TO_NETWORK_ENDIAN_U32(dev->info.devnum);
    dev->info.busnum     = TO_NETWORK_ENDIAN_U32(dev->info.busnum);
    dev->info.device_bcd = TO_NETWORK_ENDIAN_U16(dev->info.device_bcd);
    dev->info.product_id = TO_NETWORK_ENDIAN_U16(dev->info.product_id);
    dev->info.speed      = TO_NETWORK_ENDIAN_U32(dev->info.speed);
    dev->info.vendor_id  = TO_NETWORK_ENDIAN_U16(dev->info.vendor_id);

    return linked_list_push(&handle->dev_list, dev);
}