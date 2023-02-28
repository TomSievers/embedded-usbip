#include "queue.h"
#include "errno.h"
#include <string.h>

#pragma pack(push, 1)
typedef struct fifo_item
{
    size_t len;
    size_t block_len;
    struct fifo_item* next;
    uint8_t data;
} fifo_item_t;

#define FIFO_HDR_LEN sizeof(fifo_item_t) - 1

typedef struct free_block
{
    struct free_block* next;
    size_t len;
} free_block_t;
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

    queue.buffer              = buf;
    queue.buffer_len          = buf_len;
    queue.first               = NULL;
    queue.free_blocks         = buf;

    free_block_t* free_blocks = queue.free_blocks;

    free_blocks->len          = buf_len;
    free_blocks->next         = NULL;

    return 0;
}

inline void combine_consecutive_blocks(free_block_t* block)
{
    while (block->next == block + block->len)
    {
        block->len  += block->next->len;
        block->next = block->next->next;
    }
}

size_t msg_fifo_push(msg_fifo_t* queue, void* msg, size_t msg_len)
{
    free_block_t* cur_block       = queue->free_blocks;
    free_block_t* prev_block      = NULL;

    free_block_t* best_fit        = NULL;
    free_block_t* best_fit_before = NULL;
    size_t last_fit               = SIZE_MAX;
    size_t complete_msg_len       = msg_len + FIFO_HDR_LEN;

    while (cur_block != NULL)
    {
        combine_consecutive_blocks(cur_block);
        if (cur_block->len >= complete_msg_len)
        {
            size_t cur_fit = cur_block->len - complete_msg_len;
            if (cur_fit < last_fit)
            {
                last_fit        = cur_fit;
                best_fit        = cur_block;
                best_fit_before = prev_block;
            }
        }

        prev_block = cur_block;
        cur_block  = cur_block->next;
    }

    if (best_fit != NULL)
    {
        fifo_item_t* last = queue->last;
        fifo_item_t* item = (fifo_item_t*)best_fit;

        if (last_fit < sizeof(free_block_t))
        {
            item->block_len = complete_msg_len + last_fit;
        }
        else
        {
            free_block_t* new_block = (void*)item + complete_msg_len;
            new_block->len          = last_fit;
            new_block->next         = best_fit->next;
            if (best_fit_before != NULL)
            {
                best_fit_before->next = new_block;
            }
            else
            {
                queue->free_blocks = new_block;
            }

            item->block_len = complete_msg_len;
        }

        item->next = NULL;
        last->next = item;
        item->len  = msg_len;

        memcpy(&item->data, msg, item->len);

        return 0;
    }

    return 0;
}

size_t msg_fifo_pop(msg_fifo_t* queue, void* out_msg, size_t out_msg_len)
{
    fifo_item_t* item = queue->first;

    if (item->len > out_msg_len)
    {
        return 0;
    }

    queue->first = item->next;

    memcpy(out_msg, &item->data, item->len);

    size_t ret               = item->len;

    uintptr_t item_ptr       = (uintptr_t)item;
    free_block_t* cur_block  = queue->free_blocks;
    free_block_t* prev_block = queue->free_blocks;

    while (item_ptr > (uintptr_t)cur_block)
    {
        prev_block = cur_block;
        if (cur_block->next == NULL)
        {
            break;
        }
        cur_block = cur_block->next;
    }

    free_block_t* new_block = (free_block_t*)item;
    new_block->len          = item->block_len;
    new_block->next         = prev_block->next;
    prev_block->next        = new_block;

    return ret;
}