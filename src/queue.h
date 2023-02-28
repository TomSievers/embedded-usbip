#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct msg_fifo
{
    void* first;
    void* last;
    void* buffer;
    size_t buffer_len;
    void* free_blocks;
} msg_fifo_t;

int msg_fifo_init(msg_fifo_t queue, uint8_t* buf, size_t buf_len);

size_t msg_fifo_push(msg_fifo_t* queue, void* msg, size_t msg_len);

size_t msg_fifo_pop(msg_fifo_t* queue, void* out_msg, size_t out_msg_len);