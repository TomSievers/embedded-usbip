#include "queue.h"
#include "debug.h"
#include "errno.h"
#include <string.h>
#include <sys/socket.h>

#pragma pack(push, 1)
typedef struct fifo_item
{
    size_t len;
    struct fifo_item* next;
    uint8_t data;
} fifo_item_t;

#define FIFO_HDR_LEN sizeof(fifo_item_t) - 1

#pragma pack(pop)

int msg_fifo_init(msg_fifo_t* queue, uint8_t* buf, size_t buf_len)
{
    if (buf == NULL)
    {
        DEBUG_PRINT("msg_fifo_init: buf is NULL\n");
        errno = EINVAL;
        return -1;
    }

    if (buf_len < sizeof(fifo_item_t))
    {
        DEBUG_PRINT("msg_fifo_init: buf_len is too small for single fifo item\n");
        errno = ERANGE;
        return -1;
    }

    queue->start = buf;
    queue->buffer_len = buf_len;
    queue->first = NULL;
    queue->head = buf;

    return 0;
}

static inline fifo_item_t* fifo_add_new_item(msg_fifo_t* queue, size_t msg_len)
{
    fifo_item_t* new_item = queue->head;
    new_item->next = NULL;
    new_item->len = msg_len;

    return new_item;
}

size_t msg_fifo_push(msg_fifo_t* queue, void* msg, size_t msg_len)
{
    if (queue->buffer_len < msg_len + FIFO_HDR_LEN)
    {
        DEBUG_PRINT("msg_fifo_push: message too large\n");
        return 0;
    }

    void* end = queue->start + queue->buffer_len;
    fifo_item_t* new_item = NULL;
    // Check for wraparound of fifo
    if (queue->head + msg_len + FIFO_HDR_LEN >= end)
    {
        size_t first_block_len = end - queue->head;
        // Check if the fifo header fits into the remaining bytes at the end.
        if (first_block_len < FIFO_HDR_LEN)
        {
            // FIFO header does not fit insert at start.
            queue->head = queue->start;

            if (queue->first != NULL
                && queue->start + (msg_len + FIFO_HDR_LEN) - first_block_len >= queue->first)
            {
                // We would overwrite the first message, do not insert.
                DEBUG_PRINT("msg_fifo_push: fifo full\n");
                return 0;
            }

            new_item = fifo_add_new_item(queue, msg_len);

            memcpy(&new_item->data, msg, msg_len);
            queue->head = queue->start + msg_len;
        }
        else
        {
            // Header does fit insert all the possible data.
            if (queue->first != NULL
                && queue->start + (msg_len + FIFO_HDR_LEN) - first_block_len >= queue->first)
            {
                // We would overwrite the first message, do not insert.
                DEBUG_PRINT("msg_fifo_push: fifo full\n");
                return 0;
            }
            new_item = fifo_add_new_item(queue, msg_len);

            first_block_len -= FIFO_HDR_LEN;

            // Write until end of fifo.
            memcpy(&new_item->data, msg, first_block_len);
            // Write from start the remaining data.
            memcpy(queue->start, msg + first_block_len, msg_len - first_block_len);
            queue->head = queue->start + (msg_len - first_block_len);
        }
    }
    else
    {
        // Check if we would overwrite the first message.
        if (queue->first != NULL && queue->head + msg_len + FIFO_HDR_LEN >= queue->first)
        {
            DEBUG_PRINT("msg_fifo_push: fifo full\n");
            return 0;
        }

        new_item = fifo_add_new_item(queue, msg_len);

        memcpy(&new_item->data, msg, msg_len);

        queue->head += msg_len + FIFO_HDR_LEN;
    }

    // Check if the queue is empty.
    if (queue->first == NULL)
    {
        queue->first = new_item;
        queue->last = new_item;
    }
    else
    {
        ((fifo_item_t*)(queue->last))->next = new_item;
        queue->last = new_item;
    }

    return msg_len;
}

size_t msg_fifo_pop(msg_fifo_t* queue, void* out_msg, size_t out_msg_len)
{
    // Make sure we have an item in the queue.
    if (queue->first != NULL)
    {
        fifo_item_t* item = queue->first;

        // Check if the message fit in the output buffer.
        if (item->len <= out_msg_len)
        {
            void* end = queue->start + queue->buffer_len;

            void* item_end = ((uint8_t*)item) + item->len + FIFO_HDR_LEN;

            queue->first = item->next;

            // Check if the data wraps around.
            if (item_end > end)
            {
                // This message is split.
                size_t first_block_len = end - (void*)(((uint8_t*)item) + FIFO_HDR_LEN);

                memcpy(out_msg, &item->data, first_block_len);
                memcpy(out_msg + first_block_len, queue->start, item->len - first_block_len);
            }
            else
            {
                memcpy(out_msg, &item->data, item->len);
            }

            return item->len;
        }
    }
    return 0;
}

int stream_fifo_init(stream_fifo_t* queue, uint8_t* buf, size_t buf_len)
{
    if (buf == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (buf_len < sizeof(fifo_item_t))
    {
        errno = ERANGE;
        return -1;
    }

    queue->start = buf;
    queue->head = buf;
    queue->tail = buf;
    queue->buffer_len = buf_len;

    return 0;
}

ssize_t stream_fifo_push(stream_fifo_t* queue, void* msg, size_t msg_len)
{
    int remaining = queue->tail - queue->head;
    if (queue->head == queue->tail)
    {
        remaining = queue->buffer_len;
    }

    // Check if the queue has wrapped around.
    if (remaining < 0)
    {
        remaining = queue->buffer_len + -remaining;
    }

    // Check if we have enough space in the buffer.
    if (remaining < msg_len)
    {
        return -ENOBUFS;
    }

    // Check if the message wraps around.
    if (queue->tail + msg_len > queue->start + queue->buffer_len)
    {
        // Copy the first part of the message.
        int first_len = queue->start + queue->buffer_len - queue->tail;
        memcpy(queue->tail, msg, first_len);
        queue->tail = queue->start;

        // Copy the second part of the message.
        memcpy(queue->tail, msg + first_len, msg_len - first_len);
        queue->tail += msg_len - first_len;
    }
    else
    {
        memcpy(queue->tail, msg, msg_len);
        queue->tail += msg_len;
    }

    return msg_len;
}

size_t stream_fifo_length(stream_fifo_t* queue)
{
    int length = queue->tail - queue->head;
    if (length < 0)
    {
        length = queue->buffer_len + length;
    }

    return length;
}

size_t stream_fifo_pop(stream_fifo_t* queue, void* out_msg, size_t out_msg_len)
{
    int avail = stream_fifo_length(queue);

    if (avail < out_msg_len)
    {
        out_msg_len = avail;
    }

    void* end = queue->start + queue->buffer_len;

    // Check if the queue has wrapped around.
    if (queue->head + out_msg_len > end)
    {
        // Copy the first part of the message.
        int first_len = end - queue->head;
        memcpy(out_msg, queue->head, first_len);
        queue->head = queue->start;

        // Copy the second part of the message.
        memcpy(out_msg + first_len, queue->head, out_msg_len - first_len);
        queue->head += out_msg_len - first_len;
    }
    else
    {
        memcpy(out_msg, queue->head, out_msg_len);
        queue->head += out_msg_len;
    }

    return out_msg_len;
}

int stream_fifo_send_sock(stream_fifo_t* queue, int sock)
{
    // Check if the queue has wrapped around.
    if (queue->tail < queue->head)
    {
        void* end = queue->start + queue->buffer_len;
        int length = end - queue->head;

        int bytes = send(sock, queue->head, length, 0);

        // Check if we sent any bytes.
        if (bytes > 0)
        {
            void* new_head = (uint8_t*)queue->head + bytes;

            // Check if we have reached the end of the buffer.
            if (new_head >= end)
            {
                bytes -= queue->head - end;
                queue->head = queue->start + bytes;
            }
            else
            {
                queue->head = new_head;
            }
        }

        return bytes;
    }
    else if (queue->head < queue->tail)
    {
        size_t length = queue->tail - queue->head;
        int bytes = send(sock, queue->head, length, 0);

        // Check if we sent any bytes.
        if (bytes > 0)
        {
            queue->head = (uint8_t*)queue->head + bytes;
        }

        return bytes;
    }

    return 0;
}