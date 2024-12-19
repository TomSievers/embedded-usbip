#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct msg_fifo
{
    void* first;
    void* last;
    void* head;
    void* start;
    size_t buffer_len;
} msg_fifo_t;

/**
 * @brief Initialize the message FIFO.
 * @param queue The message FIFO to initialize.
 * @param buf The buffer to use for the FIFO.
 * @param buf_len The length of the buffer.
 * @return 0 on success, -1 on failure.
 */
int msg_fifo_init(msg_fifo_t* queue, uint8_t* buf, size_t buf_len);

/**
 * @brief Push a message onto the FIFO.
 * @param queue The message FIFO to push onto.
 * @param msg The message to push.
 * @param msg_len The length of the message.
 * @return The number of bytes pushed onto the FIFO or 0 if the FIFO is full.
 */
size_t msg_fifo_push(msg_fifo_t* queue, void* msg, size_t msg_len);

/**
 * @brief Pop a message from the FIFO.
 * @param queue The message FIFO to pop from.
 * @param out_msg The buffer to store the popped message.
 * @param out_msg_len The length of the buffer.
 * @return The number of bytes popped, or 0 if no message was available.
 */
size_t msg_fifo_pop(msg_fifo_t* queue, void* out_msg, size_t out_msg_len);

typedef struct stream_fifo
{
    void* start;
    void* head;
    void* tail;
    size_t buffer_len;
} stream_fifo_t;

/**
 * @brief Initialize a stream FIFO.
 * @param queue The stream FIFO to initialize.
 * @param buf The buffer to use for the FIFO.
 * @param buf_len The length of the buffer.
 */
int stream_fifo_init(stream_fifo_t* queue, uint8_t* buf, size_t buf_len);

/**
 * @brief Push a message to the FIFO.
 * @param queue The stream FIFO to push to.
 * @param msg The message to push.
 * @param msg_len The length of the message.
 * @return 0 on success, -ENOBUFS on failure.
 */
ssize_t stream_fifo_push(stream_fifo_t* queue, void* msg, size_t msg_len);

/**
 * @brief Pop a message from the FIFO.
 * @param queue The stream FIFO to pop from.
 * @param out_msg The buffer to store the popped message.
 * @param out_msg_len The length of the buffer.
 * @return The number of bytes popped from the FIFO.
 */
size_t stream_fifo_pop(stream_fifo_t* queue, void* out_msg, size_t out_msg_len);

/**
 * @brief Get the length of the FIFO.
 * @param queue The stream FIFO to get the length of.
 * @return The number of bytes in the FIFO.
 */
size_t stream_fifo_length(stream_fifo_t* queue);

/**
 * @brief Send a message over a socket.
 * @param queue The stream FIFO to send from.
 * @param sock The socket to send over.
 * @return The number of bytes sent over the socket.
 */
int stream_fifo_send_sock(stream_fifo_t* queue, int sock);