
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

typedef struct imported_dev
{
    uint16_t busnum;
    uint16_t devnum;
    struct imported_dev* next;
} imported_dev_t;

typedef struct usbip_client
{
    int sock;
    uint8_t data_stream[1024];
    stream_fifo_t out_fifo;
    imported_dev_t* imported_devs;
} usbip_client_t;

#ifdef USBIP_IMPORTED_DEV_POOL_SIZE
INIT_MEM_POOL(imported_dev_pool, imported_dev_t, USBIP_IMPORTED_DEV_POOL_SIZE);
static inline void* imported_dev_alloc(size_t size) { return mem_pool_alloc(&imported_dev_pool); }
static inline void imported_dev_free(void* dev) { mem_pool_free(&imported_dev_pool, dev); }
#else
static inline void* imported_dev_alloc(size_t size) { return malloc(size); }
static inline void imported_dev_free(void* dev) { free(dev); }
#endif

#ifdef USBIP_CLIENT_POOL_SIZE
INIT_MEM_POOL(client_pool, usbip_client_t, USBIP_CLIENT_POOL_SIZE);
INIT_MEM_POOL(client_node_pool, node_t, USBIP_CLIENT_POOL_SIZE);
static inline void* client_alloc() { return mem_pool_alloc(&client_pool); }
static inline void client_free(void* client) { mem_pool_free(&client_pool, client); }
static inline void* client_node_alloc(size_t size) { return mem_pool_alloc(&client_node_pool); }
static inline void client_node_free(void* node) { mem_pool_free(&client_node_pool, node); }
#else
static inline void* client_alloc() { return malloc(sizeof(usbip_client_t)); }
static inline void client_free(void* client) { free(client); }
static inline void* client_node_alloc(size_t size) { return malloc(size); }
static inline void client_node_free(void* node) { free(node); }
#endif

INIT_LINKED_LIST(client_list, client_node_alloc, client_node_free);

void sock_stop(int sock)
{
    shutdown(sock, O_RDWR);
    close(sock);
}

void client_stop(usbip_server_t* handle, usbip_client_t* client, size_t i)
{
    sock_stop(client->sock);
    linked_list_rem(&client_list, i);
    client_free(client);
}

static uint8_t tmp_buf[4096];

int add_client(usbip_server_t* handle, int sock)
{
    usbip_client_t* client = client_alloc();

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

    if (linked_list_push(&client_list, client) == -1)
    {
        client_free(client);
        return -1;
    }

    return 0;
}

int usb_dev_to_buf(stream_fifo_t* fifo, vusb_dev_t* dev)
{
    if (stream_fifo_push(fifo, dev->dev->path, 256) == 0)
    {
        return -1;
    }

    if (stream_fifo_push(fifo, dev->dev->busid, 32) == 0)
    {
        return -1;
    }

    uint8_t temp[32] = { 0 };

    uint8_t* buf = temp;

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

    if (stream_fifo_push(fifo, temp, buf - temp) == 0)
    {
        return -1;
    }

    return 0;
}

int usb_dev_if_to_buf(stream_fifo_t* fifo, vusb_dev_t* dev)
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
                uint8_t temp[] = { cur_if->desc.bInterfaceClass, cur_if->desc.bInterfaceSubClass,
                    cur_if->desc.bInterfaceProtocol, 0 };

                if (stream_fifo_push(fifo, temp, sizeof(temp)) == 0)
                {
                    return -1;
                }

                cur_if = cur_if->next;
            }
            cur = cur->next;
        }
    }

    return 0;
}

void fill_devlist(vusb_dev_t* dev, void* ctx)
{
    stream_fifo_t* fifo = (*(void**)ctx);
    int err = usb_dev_to_buf(fifo, dev);

    if (err == -1)
    {
        return;
    }

    err = usb_dev_if_to_buf(fifo, dev);

    if (err == -1)
    {
        return;
    }
}

int usbip_resp_devlist(usbip_server_t* handle, usbip_client_t* client, size_t i)
{
    hdr_rep_devlist_t reply = { .hdr = { .op_code = TO_NETWORK_ENDIAN_U16(REP_DEVLIST),
                                    .version = TO_NETWORK_ENDIAN_U16(USBIP_VERSION),
                                    .status = TO_NETWORK_ENDIAN_U32(USBIP_STATUS_OK) },
        .dev_count = TO_NETWORK_ENDIAN_U32(handle->vhci_handle->devices.size) };

    if (stream_fifo_push(&client->out_fifo, &reply, sizeof(hdr_rep_devlist_t)) == 0)
    {
        client_stop(handle, client, i);
        return -1;
    }

    if (handle->vhci_handle->devices.size > 0)
    {
        vhci_iter_devices(handle->vhci_handle, fill_devlist, &client->out_fifo);
    }

    return 0;
}

int usbip_handle_import(usbip_server_t* handle, usbip_client_t* client, size_t i)
{
    char busid[32] = { 0 };

    int bytes = recv(client->sock, busid, 32, 0);

    hdr_common_t hdr = { 0 };

    hdr.version = TO_NETWORK_ENDIAN_U16(USBIP_VERSION);
    hdr.op_code = TO_NETWORK_ENDIAN_U16(REP_IMPORT);
    hdr.status = TO_NETWORK_ENDIAN_U32(USBIP_STATUS_ERROR);

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
            hdr.status = TO_NETWORK_ENDIAN_U32(USBIP_STATUS_OK);

            if (stream_fifo_push(&client->out_fifo, &hdr, sizeof(hdr)) == 0)
            {
                client_stop(handle, client, i);
                return -1;
            }

            int err = usb_dev_to_buf(&client->out_fifo, dev);

            if (err == -1)
            {
                client_stop(handle, client, i);
                return -1;
            }
        }
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
    memset(handle, 0, sizeof(usbip_server_t));

    handle->vhci_handle = usb_handle;

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

void write_cmd_response_header(usbip_client_t* client, hdr_cmd_t cmd)
{
    uint8_t buf[sizeof(hdr_cmd_t)] = { 0 };
    uint8_t* buf_ptr = buf;

    memcpy(buf_ptr, &cmd, sizeof(hdr_cmd_t));

    stream_fifo_push(&client->out_fifo, buf, sizeof(hdr_cmd_t));
}

void urb_complete_cb(struct urb* urb, void* context)
{
    uint8_t buf[48] = { 0 };
    uint8_t* buf_ptr = buf;
    usbip_client_t* client = context;

    hdr_cmd_t hdr = {
        .command = TO_NETWORK_ENDIAN_U32(USBIP_RET_SUBMIT),
        .seq_num = TO_NETWORK_ENDIAN_U32(urb->seq_num),
        .busnum = 0,
        .devnum = 0,
        .direction = 0,
        .endpoint = 0,
    };

    cmd_t* cmd = (cmd_t*)(&hdr.padding);
    cmd->status = TO_NETWORK_ENDIAN_U32(urb->status);
    cmd->start_frame = TO_NETWORK_ENDIAN_U32(urb->start_frame);
    cmd->number_of_packets = TO_NETWORK_ENDIAN_U32(urb->number_of_packets);
    cmd->error_count = TO_NETWORK_ENDIAN_U32(urb->error_count);
    cmd->length = TO_NETWORK_ENDIAN_U32(urb->actual_length);

    write_cmd_response_header(client, hdr);

    if (PIPE_DIR(urb->pipe) == PIPE_IN)
    {
        stream_fifo_push(&client->out_fifo, urb->transfer_buffer, urb->actual_length);
    }
}

vusb_dev_t* get_client_dev(usbip_server_t* handle, usbip_client_t* client, hdr_cmd_t hdr)
{
    imported_dev_t* imported = client->imported_devs;

    do
    {
        if (imported != NULL && imported->busnum == hdr.busnum && imported->devnum == hdr.devnum)
        {
            break;
        }

        imported = (imported != NULL) ? imported->next : NULL;
    } while (imported != NULL);

    cmd_t* cmd = (cmd_t*)(&hdr.padding);

    // Device not imported
    if (imported == NULL)
    {
        hdr.command = TO_NETWORK_ENDIAN_U32(
            (hdr.command == USBIP_CMD_SUBMIT) ? USBIP_RET_SUBMIT : USBIP_RET_UNLINK);
        hdr.seq_num = TO_NETWORK_ENDIAN_U32(hdr.seq_num);
        cmd->status = TO_NETWORK_ENDIAN_U32(-EINVAL);
        write_cmd_response_header(client, hdr);
        return NULL;
    }

    return vhci_get_device(handle->vhci_handle, hdr.busnum, hdr.devnum);
}

int handle_urb_submit(usbip_server_t* handle, usbip_client_t* client, size_t i, hdr_cmd_t hdr)
{
    vusb_dev_t* dev = get_client_dev(handle, client, hdr);

    cmd_t* cmd = (cmd_t*)(&hdr.padding);

    // Device not found
    if (dev == NULL)
    {
        hdr.command = TO_NETWORK_ENDIAN_U32(USBIP_RET_SUBMIT);
        hdr.seq_num = TO_NETWORK_ENDIAN_U32(hdr.seq_num);
        cmd->status = TO_NETWORK_ENDIAN_U32(-ENODEV);
        write_cmd_response_header(client, hdr);
        return 0;
    }

    // Parse the intial URB submit header
    cmd->txfer_flags = FROM_NETWORK_ENDIAN_U32(cmd->txfer_flags);
    cmd->length = FROM_NETWORK_ENDIAN_U32(cmd->length);
    cmd->start_frame = FROM_NETWORK_ENDIAN_U32(cmd->start_frame);
    cmd->number_of_packets = FROM_NETWORK_ENDIAN_U32(cmd->number_of_packets);
    cmd->interval = FROM_NETWORK_ENDIAN_U32(cmd->interval);

    urb_setup_t* setup = (urb_setup_t*)(cmd->setup);

    setup->wValue = FROM_NETWORK_ENDIAN_U16(setup->wValue);

    urb_t urb = {
        .transfer_flags = cmd->txfer_flags,
        .transfer_buffer_length = cmd->length,
        .start_frame = cmd->start_frame,
        .number_of_packets = cmd->number_of_packets,
        .interval = cmd->interval,
        .setup_packet = *setup,
        .seq_num = hdr.seq_num,
        .transfer_buffer = NULL,
        .pipe = PIPE_DIR(hdr.direction) | PIPE_EP_SET(hdr.endpoint),
        .complete = urb_complete_cb,
        .context = client,
    };

    int err = vhci_urb_init(handle->vhci_handle, dev, &urb);

    // URB init failed
    if (err == -1)
    {
        hdr.command = TO_NETWORK_ENDIAN_U32(USBIP_RET_SUBMIT);
        hdr.seq_num = TO_NETWORK_ENDIAN_U32(hdr.seq_num);
        cmd->status = TO_NETWORK_ENDIAN_U32(-errno);
        write_cmd_response_header(client, hdr);
        return 0;
    }

    if (PIPE_DIR(urb.pipe) == PIPE_OUT)
    {
        ssize_t bytes = recv(client->sock, urb.transfer_buffer, urb.transfer_buffer_length, 0);

        if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            return -1;
        }
        else if (bytes == urb.transfer_buffer_length)
        {
            urb.actual_length = bytes;
            err = vhci_submit_urb(handle->vhci_handle, urb);

            // URB submit failed
            if (err == -1)
            {
                hdr.command = TO_NETWORK_ENDIAN_U32(USBIP_RET_SUBMIT);
                hdr.seq_num = TO_NETWORK_ENDIAN_U32(hdr.seq_num);
                cmd->status = TO_NETWORK_ENDIAN_U32(-errno);
                write_cmd_response_header(client, hdr);
                return 0;
            }
        }
        else
        {
            // Not all data received
        }
    }

    return 0;
}

int handle_urb_unlink(usbip_server_t* handle, usbip_client_t* client, size_t i, hdr_cmd_t hdr)
{
    vusb_dev_t* dev = get_client_dev(handle, client, hdr);

    cmd_t* cmd = (cmd_t*)(&hdr.padding);

    // Device not found
    if (dev == NULL)
    {
        hdr.command = TO_NETWORK_ENDIAN_U32(USBIP_RET_UNLINK);
        hdr.seq_num = TO_NETWORK_ENDIAN_U32(hdr.seq_num);
        cmd->status = TO_NETWORK_ENDIAN_U32(-ENODEV);
        write_cmd_response_header(client, hdr);
        return 0;
    }

    int err = vhci_unlink_urb(handle->vhci_handle, dev, hdr.seq_num);

    // URB unlink failed
    if (err == -1)
    {
        hdr.command = TO_NETWORK_ENDIAN_U32(USBIP_RET_UNLINK);
        hdr.seq_num = TO_NETWORK_ENDIAN_U32(hdr.seq_num);
        cmd->status = TO_NETWORK_ENDIAN_U32(-errno);
        write_cmd_response_header(client, hdr);
        return 0;
    }

    hdr.command = TO_NETWORK_ENDIAN_U32(USBIP_RET_UNLINK);
    hdr.seq_num = TO_NETWORK_ENDIAN_U32(hdr.seq_num);
    cmd->status = TO_NETWORK_ENDIAN_U32(-ECONNRESET);
    write_cmd_response_header(client, hdr);

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
    }

    uint32_t hdr[2] = { 0 };
    const uint32_t intial_hdr_size = 8;

    ssize_t bytes = recv(client->sock, &hdr, intial_hdr_size, 0);

    if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        client_stop(handle, client, i);
        return -1;
    }
    else if (bytes == sizeof(hdr))
    {
        hdr[0] = FROM_NETWORK_ENDIAN_U32(hdr[0]);

        if (hdr[0] > USBIP_CMD_UNLINK)
        {
            uint16_t version = hdr[0] >> 16;
            uint16_t op_code = hdr[0] & 0xFFFF;
            uint32_t status = hdr[1];

            if (version == USBIP_VERSION)
            {
                switch (op_code)
                {
                case REQ_DEVLIST:
                    return usbip_resp_devlist(handle, client, i);
                case REQ_IMPORT:
                    return usbip_handle_import(handle, client, i);
                default:
                    break;
                }
            }
        }
        else
        {
            hdr_cmd_t cmd;

            cmd.command = FROM_NETWORK_ENDIAN_U32(hdr[0]);
            cmd.seq_num = FROM_NETWORK_ENDIAN_U32(hdr[1]);

            ssize_t bytes = recv(client->sock, ((uint8_t*)(&cmd)) + intial_hdr_size,
                sizeof(hdr_cmd_t) - intial_hdr_size, 0);

            if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                client_stop(handle, client, i);
                return -1;
            }
            else if (bytes == sizeof(hdr_cmd_t) - intial_hdr_size)
            {

                cmd.busnum = FROM_NETWORK_ENDIAN_U16(cmd.busnum);
                cmd.devnum = FROM_NETWORK_ENDIAN_U16(cmd.devnum);
                cmd.direction = FROM_NETWORK_ENDIAN_U16(cmd.direction);
                cmd.endpoint = FROM_NETWORK_ENDIAN_U16(cmd.endpoint);

                switch (cmd.command)
                {
                case USBIP_CMD_SUBMIT:
                    return handle_urb_submit(handle, client, i, cmd);
                    break;
                case USBIP_CMD_UNLINK:
                    return handle_urb_unlink(handle, client, i, cmd);
                    break;
                default:
                    break;
                }
            }
        }
    }

    return 0;
}

int usbip_server_handle_once(usbip_server_t* handle)
{
    usbip_accept_new_client(handle);

    linked_list_iter(&client_list, usbip_client_handle, handle);

    return 0;
}