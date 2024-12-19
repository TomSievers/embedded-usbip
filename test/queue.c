#include "queue.h"
#include "test.h"
#include <string.h>

test(test_msg_fifo_init)
{
    msg_fifo_t queue;
    uint8_t buf[20];

    assert_int_eq(msg_fifo_init(&queue, buf, 20), 0);
    assert_ptr_eq(queue.start, buf);
    assert_ptr_eq(queue.head, buf);
    assert_ptr_eq(queue.first, NULL);
    assert_int_eq(queue.buffer_len, 20);

    return 1;
}

test(test_msg_fifo_push)
{
    msg_fifo_t queue;
    uint8_t buf[64] = { 0 };

    assert_int_eq(msg_fifo_init(&queue, buf, 64), 0);

    uint8_t msg[] = { 0x01, 0x02, 0x03, 0x04 };

    assert_int_eq(msg_fifo_push(&queue, msg, 4), 4);
    assert_ptr_eq(queue.head, buf + 4 + sizeof(void*) + sizeof(size_t));
    assert_ptr_eq(queue.first, buf);

    return 1;
}

test(test_msg_fifo_pop)
{
    msg_fifo_t queue;
    uint8_t buf[64];

    assert_int_eq(msg_fifo_init(&queue, buf, 64), 0);

    uint8_t msg[] = { 0x01, 0x02, 0x03, 0x04 };

    assert_int_eq(msg_fifo_push(&queue, msg, 4), 4);

    uint8_t out_msg[4];

    assert_int_eq(msg_fifo_pop(&queue, out_msg, 4), 4);
    assert_int_eq(memcmp(out_msg, msg, 4), 0);

    return 1;
}

test(test_msg_fifo_pop_empty)
{
    msg_fifo_t queue;
    uint8_t buf[64];

    assert_int_eq(msg_fifo_init(&queue, buf, 64), 0);

    uint8_t out_msg[4];

    assert_int_eq(msg_fifo_pop(&queue, out_msg, 4), 0);

    return 1;
}

test(test_msg_fifo_push_full)
{
    msg_fifo_t queue;
    uint8_t buf[64];

    assert_int_eq(msg_fifo_init(&queue, buf, 64), 0);

    uint8_t msg[64] = { 0x01 };

    size_t size = 64 - 1 - sizeof(size_t) - sizeof(void*);

    assert_int_eq(msg_fifo_push(&queue, msg, size), size);
    assert_int_eq(msg_fifo_push(&queue, msg, 1), 0);

    return 1;
}

test(test_msg_fifo_too_large_initial)
{
    msg_fifo_t queue;
    uint8_t buf[64];

    assert_int_eq(msg_fifo_init(&queue, buf, 64), 0);

    uint8_t msg[64] = { 0x01 };

    assert_int_eq(msg_fifo_push(&queue, msg, 64), 0);

    return 1;
}

test(test_msg_fifo_pop_too_small)
{
    msg_fifo_t queue;
    uint8_t buf[64];

    assert_int_eq(msg_fifo_init(&queue, buf, 64), 0);

    uint8_t msg[64] = { 0x01 };

    size_t size = 64 - 1 - sizeof(size_t) - sizeof(void*);

    assert_int_eq(msg_fifo_push(&queue, msg, size), size);

    uint8_t out_msg[32];

    assert_int_eq(msg_fifo_pop(&queue, out_msg, 32), 0);

    uint8_t out_msg2[64];

    assert_int_eq(msg_fifo_pop(&queue, out_msg2, 64), size);
    assert_int_eq(memcmp(out_msg2, msg, size), 0);

    return 1;
}

test(test_msg_fifo_wraparound_split)
{
    msg_fifo_t queue;
    uint8_t buf[64];

    assert_int_eq(msg_fifo_init(&queue, buf, 64), 0);

    uint8_t msg[64] = { 0x01 };

    assert_int_eq(msg_fifo_push(&queue, msg, 16), 16);
    assert_int_eq(msg_fifo_pop(&queue, msg, 16), 16);

    for (size_t i = 0; i < 32; ++i)
    {
        msg[i] = i;
    }

    assert_int_eq(msg_fifo_push(&queue, msg, 32), 32);

    uint8_t out_msg[32];

    assert_int_eq(msg_fifo_pop(&queue, out_msg, 32), 32);

    assert_int_eq(memcmp(out_msg, msg, 32), 0);

    return 1;
}

test(test_msg_fifo_wraparound_split_edge)
{
    msg_fifo_t queue;
    uint8_t buf[64];

    assert_int_eq(msg_fifo_init(&queue, buf, 64), 0);

    uint8_t msg[64] = { 0x01 };

    assert_int_eq(msg_fifo_push(&queue, msg, 32), 32);
    assert_int_eq(msg_fifo_pop(&queue, msg, 32), 32);

    for (size_t i = 0; i < 32; ++i)
    {
        msg[i] = i;
    }

    assert_int_eq(msg_fifo_push(&queue, msg, 32), 32);

    uint8_t out_msg[32];

    assert_int_eq(msg_fifo_pop(&queue, out_msg, 32), 32);

    assert_int_eq(memcmp(out_msg, msg, 32), 0);

    return 1;
}

test(test_msg_fifo_wraparound_non_split)
{
    msg_fifo_t queue;
    uint8_t buf[64];

    assert_int_eq(msg_fifo_init(&queue, buf, 64), 0);

    uint8_t msg[64] = { 0x01 };

    assert_int_eq(msg_fifo_push(&queue, msg, 33), 33);
    assert_int_eq(msg_fifo_pop(&queue, msg, 33), 33);

    for (size_t i = 0; i < 32; ++i)
    {
        msg[i] = i;
    }

    assert_int_eq(msg_fifo_push(&queue, msg, 32), 32);

    uint8_t out_msg[32];

    assert_int_eq(msg_fifo_pop(&queue, out_msg, 32), 32);

    assert_int_eq(memcmp(out_msg, msg, 32), 0);

    return 1;
}

test(test_stream_fifo_init)
{
    stream_fifo_t queue;
    uint8_t buf[64];

    assert_int_eq(stream_fifo_init(&queue, buf, 64), 0);
    assert_ptr_eq(queue.start, buf);
    assert_ptr_eq(queue.head, buf);
    assert_ptr_eq(queue.tail, buf);
    assert_int_eq(queue.buffer_len, 64);

    return 1;
}

test(test_stream_fifo_push)
{
    stream_fifo_t queue;
    uint8_t buf[64];

    assert_int_eq(stream_fifo_init(&queue, buf, 64), 0);

    uint8_t msg[64] = { 0x01 };

    assert_int_eq(stream_fifo_push(&queue, msg, 32), 32);
    assert_ptr_eq(queue.tail, buf + 32);
    assert_int_eq(stream_fifo_length(&queue), 32);

    return 1;
}

test(test_stream_fifo_pop)
{
    stream_fifo_t queue;
    uint8_t buf[64];

    assert_int_eq(stream_fifo_init(&queue, buf, 64), 0);

    uint8_t msg[64] = { 0x01 };

    for (size_t i = 0; i < 32; ++i)
    {
        msg[i] = i;
    }

    assert_int_eq(stream_fifo_push(&queue, msg, 32), 32);
    assert_int_eq(stream_fifo_length(&queue), 32);

    uint8_t out_msg[32];

    assert_int_eq(stream_fifo_pop(&queue, out_msg, 32), 32);
    assert_int_eq(memcmp(out_msg, msg, 32), 0);
    assert_int_eq(stream_fifo_length(&queue), 0);

    return 1;
}

test(test_stream_fifo_pop_empty)
{
    stream_fifo_t queue;
    uint8_t buf[64];

    assert_int_eq(stream_fifo_init(&queue, buf, 64), 0);

    uint8_t out_msg[32];

    assert_int_eq(stream_fifo_pop(&queue, out_msg, 32), 0);

    return 1;
}

test(test_stream_fifo_pop_wraparound)
{
    stream_fifo_t queue;
    uint8_t buf[64];

    assert_int_eq(stream_fifo_init(&queue, buf, 64), 0);

    uint8_t msg[64] = { 0x01 };

    for (size_t i = 0; i < 64; ++i)
    {
        msg[i] = i;
    }

    assert_int_eq(stream_fifo_push(&queue, msg, 48), 48);
    assert_int_eq(stream_fifo_length(&queue), 48);

    uint8_t out_msg[48];

    assert_int_eq(stream_fifo_pop(&queue, out_msg, 48), 48);
    assert_int_eq(memcmp(out_msg, msg, 48), 0);
    assert_int_eq(stream_fifo_length(&queue), 0);

    for (size_t i = 0; i < 32; ++i)
    {
        msg[i] = i + 32;
    }

    assert_int_eq(stream_fifo_push(&queue, msg, 32), 32);
    assert_int_eq(stream_fifo_length(&queue), 32);

    uint8_t out_msg2[32];

    assert_int_eq(stream_fifo_pop(&queue, out_msg2, 32), 32);
    assert_int_eq(memcmp(out_msg2, msg, 32), 0);
    assert_int_eq(stream_fifo_length(&queue), 0);

    return 1;
}

int main(void)
{
    run_test(test_msg_fifo_init);
    run_test(test_msg_fifo_push);
    run_test(test_msg_fifo_pop);
    run_test(test_msg_fifo_pop_empty);
    run_test(test_msg_fifo_push_full);
    run_test(test_msg_fifo_too_large_initial);
    run_test(test_msg_fifo_pop_too_small);
    run_test(test_msg_fifo_wraparound_split);
    run_test(test_msg_fifo_wraparound_split_edge);
    run_test(test_msg_fifo_wraparound_non_split);
    run_test(test_stream_fifo_init);
    run_test(test_stream_fifo_push);
    run_test(test_stream_fifo_pop);
    run_test(test_stream_fifo_pop_empty);
    run_test(test_stream_fifo_pop_wraparound);

    printf("Tests finished\n");

    return EXIT_SUCCESS;
}