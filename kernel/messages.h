/*
 * messages.h - IPC Message System with Slab Allocator
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef MESSAGES_H
#define MESSAGES_H

#include "kernel.h"

// Message Size Classes (slab allocator)
#define MSG_SLAB_16     0
#define MSG_SLAB_64     1
#define MSG_SLAB_256    2
#define MSG_SLAB_1024   3
#define MSG_SLAB_4096   4
#define MSG_SLAB_COUNT  5

#define MSG_MAX_SIZE    4096
#define MSG_QUEUE_SIZE  64

// Standard Message Types
enum msg_type {
    MSG_TYPE_DATA = 1,
    MSG_TYPE_SIGNAL = 2,
    MSG_TYPE_REQUEST = 3,
    MSG_TYPE_RESPONSE = 4,
    MSG_TYPE_POINTER = 5,   // Zero-copy pointer message
};

// Message Envelope (variable size)
struct message {
    uint32_t sender_id;
    uint32_t receiver_id;
    uint32_t type;
    uint32_t size;          // Actual data size
    uint32_t slab_class;    // Which slab allocated from
    uint32_t flags;
    uint64_t timestamp;
    uint8_t  data[];        // Flexible array member
};

// Message Queue (uses pointers to messages)
struct msg_queue {
    struct message* messages[MSG_QUEUE_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;
};

// Initialize IPC System (with slab allocator)
void msg_init(void);

// Allocate/Free message buffers
struct message* msg_alloc(size_t data_size);
void msg_free(struct message* msg);

// Send Message
int msg_send(uint32_t sender, uint32_t receiver, uint32_t type,
             const void* data, uint32_t size);

// Send zero-copy pointer message
int msg_send_ptr(uint32_t sender, uint32_t receiver, void* ptr, uint32_t size);

// Receive Message (blocking)
int msg_receive(uint32_t receiver, struct message* msg);

// Check for pending messages
bool msg_available(uint32_t receiver);

// Get pending count
uint32_t msg_count(uint32_t receiver);

// Clear queue
void msg_clear(uint32_t receiver);

#endif // MESSAGES_H
