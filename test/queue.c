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
}

test(test_msg_fifo_push)
{
    msg_fifo_t queue;
    uint8_t buf[20];

    assert_int_eq(msg_fifo_init(&queue, buf, 20), 0);

    uint8_t msg[] = { 0x01, 0x02, 0x03, 0x04 };

    assert_int_eq(msg_fifo_push(&queue, msg, 4), 4);
    assert_ptr_eq(queue.head, buf + 4);
    assert_ptr_eq(queue.first, buf);
}

test(test_msg_fifo_pop)
{
    msg_fifo_t queue;
    uint8_t buf[20];

    assert_int_eq(msg_fifo_init(&queue, buf, 20), 0);

    uint8_t msg[] = { 0x01, 0x02, 0x03, 0x04 };

    assert_int_eq(msg_fifo_push(&queue, msg, 4), 4);

    uint8_t out_msg[4];

    assert_int_eq(msg_fifo_pop(&queue, out_msg, 4), 4);
    assert_int_eq(memcmp(out_msg, msg, 4), 0);
}

test(test_msg_fifo_pop_empty)
{
    msg_fifo_t queue;
    uint8_t buf[20];

    assert_int_eq(msg_fifo_init(&queue, buf, 20), 0);

    uint8_t out_msg[4];

    assert_int_eq(msg_fifo_pop(&queue, out_msg, 4), 0);
}

test(test_msg_fifo_push_full)
{
    msg_fifo_t queue;
    uint8_t buf[20];

    assert_int_eq(msg_fifo_init(&queue, buf, 20), 0);

    uint8_t msg[20] = { 0x01 };

    assert_int_eq(msg_fifo_push(&queue, msg, 20), 20);
    assert_int_eq(msg_fifo_push(&queue, msg, 1), 0);
}

int main(void)
{
    run_test(test_msg_fifo_init);
    run_test(test_msg_fifo_push);
    run_test(test_msg_fifo_pop);
    run_test(test_msg_fifo_pop_empty);
    run_test(test_msg_fifo_push_full);

    printf("Tests finished\n");

    return EXIT_SUCCESS;
}