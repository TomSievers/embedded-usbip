#include "queue.h"
#include "errno.h"
#include <string.h>

#pragma pack(push, 1)
typedef struct fifo_item
{
    size_t len;
    struct fifo_item* next;
    uint8_t data;
} fifo_item_t;

#define FIFO_HDR_LEN sizeof(fifo_item_t) - 1

#pragma pack(pop)

int msg_fifo_init(msg_fifo_t queue, uint8_t* buf, size_t buf_len)
{
    if (buf == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    if (buf_len < sizeof(fifo_item_t))
    {
        errno = ERANGE;
        return -1;
    }

    queue.start      = buf;
    queue.buffer_len = buf_len;
    queue.first      = NULL;
    queue.last       = NULL;
    queue.head       = buf;

    return 0;
}

inline fifo_item_t* fifo_add_new_item(msg_fifo_t* queue, size_t msg_len)
{
    fifo_item_t* new_item = queue->head;
    new_item->next        = NULL;
    new_item->len         = msg_len;

    if (queue->last == NULL)
    {
        queue->first = new_item;
        queue->last  = new_item;
    }
    else
    {
        ((fifo_item_t*)queue->last)->next = new_item;
        queue->last                       = new_item;
    }

    return new_item;
}

size_t msg_fifo_push(msg_fifo_t* queue, void* msg, size_t msg_len)
{
    void* end = queue->start + queue->buffer_len;
    if (queue->head + msg_len + FIFO_HDR_LEN >= end)
    {
        size_t first_block_len = end - queue->head;
        if (first_block_len < FIFO_HDR_LEN)
        {
            queue->head = queue->start;

            if (queue->head + msg_len + FIFO_HDR_LEN >= queue->start)
            {
                // We would overwrite the first message, do not insert.
                return 0;
            }

            fifo_item_t* new_item = fifo_add_new_item(queue, msg_len);

            memcpy(&new_item->data, msg, msg_len);
            queue->head = queue->start + msg_len;
        }
        else
        {
            if (queue->start + (msg_len + FIFO_HDR_LEN) - first_block_len >= queue->start)
            {
                // We would overwrite the first message, do not insert.
                return 0;
            }
            fifo_item_t* new_item = fifo_add_new_item(queue, msg_len);

            memcpy(&new_item->data, msg, first_block_len);
            memcpy(queue->start, msg + first_block_len, msg_len - first_block_len);
            queue->head = queue->start + (msg_len - first_block_len);
        }
    }
    else
    {
        if (queue->head + msg_len + FIFO_HDR_LEN >= queue->first)
        {
            // We would overwrite the first message, do not insert.
            return 0;
        }

        fifo_item_t* new_item = fifo_add_new_item(queue, msg_len);

        memcpy(&new_item->data, msg, msg_len);

        queue->head += msg_len + FIFO_HDR_LEN;
    }
    return msg_len;
}

size_t msg_fifo_pop(msg_fifo_t* queue, void* out_msg, size_t out_msg_len)
{
    if (queue->first != NULL)
    {
        fifo_item_t* first = queue->first;

        if (first->len <= out_msg_len)
        {
            void* end      = queue->start + queue->buffer_len;

            void* item_end = first + first->len + FIFO_HDR_LEN;

            queue->first   = first->next;

            if (item_end > end)
            {
                // This message is split.
                size_t first_block_len = end - (void*)(first + FIFO_HDR_LEN);

                memcpy(&first->data, out_msg, first_block_len);
                memcpy(queue->start, out_msg + first_block_len, first->len - first_block_len);
            }
            else
            {
                memcpy(&first->data, out_msg, first->len);
            }

            return first->len;
        }
    }
    return 0;
}