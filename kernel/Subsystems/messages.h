/*
 * messages.h - IPC Message System with Slab Allocator
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 *
 * Implements an asynchronous inter-process communication system.
 * Uses a slab allocator for efficient message creation and destruction.
 */

#ifndef MESSAGES_H
#define MESSAGES_H

#include "kernel.h"

// =============================================================================
// Configuration
// =============================================================================

// Maximum number of messages pending in a task's queue
#define MSG_QUEUE_SIZE  64

// Maximum size of a message payload
#define MSG_MAX_SIZE    4096

// Error Codes
#define MSG_ERR_BUFFER_TOO_SMALL -2

// Slab Classes for allocation optimization
// 16, 64, 256, 1024, 4096 bytes
#define MSG_SLAB_16     0
#define MSG_SLAB_64     1
#define MSG_SLAB_256    2
#define MSG_SLAB_1024   3
#define MSG_SLAB_4096   4
#define MSG_SLAB_COUNT  5

// =============================================================================
// Message Types
// =============================================================================
enum msg_type {
    MSG_TYPE_DATA       = 1,    // Raw data packet
    MSG_TYPE_SIGNAL     = 2,    // Simple notification (size=0)
    MSG_TYPE_REQUEST    = 3,    // RPC Request
    MSG_TYPE_RESPONSE   = 4,    // RPC Response
    MSG_TYPE_POINTER    = 5,    // Shared memory handle / raw pointer
};

// =============================================================================
// Structures
// =============================================================================

/**
 * Message Envelope
 * The header + variable length payload.
 */
struct message {
    uint32_t sender_id;     // PID of Sender
    uint32_t receiver_id;   // PID of Destination
    uint32_t type;          // Message Type Enum
    uint32_t size;          // Payload Size in Bytes
    uint32_t slab_class;    // Internal: Which freelist to return to
    uint32_t flags;         // Reserved
    uint64_t timestamp;     // System tick when sent
    uint8_t  data[];        // Payload (Flexible Array Member)
};

/**
 * Per-Task Message Queue
 * Circular buffer of pointers to messages.
 */
struct msg_queue {
    // Array of pointers to allocated messages
    struct message* messages[MSG_QUEUE_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;
};

// =============================================================================
// Public API
// =============================================================================

/**
 * Initialize IPC Subsystem
 * Sets up slab allocators and clears task queues.
 */
void msg_init(void);

/**
 * Allocate a message buffer
 * Uses the appropriate slab class for the requested size.
 *
 * @param data_size Size of payload needed
 * @return Pointer to message envelope
 */
struct message* msg_alloc(size_t data_size);

/**
 * Free a message buffer
 * Returns it to the slab free list.
 */
void msg_free(struct message* msg);

/**
 * Send Message (Copy)
 * Allocates a buffer, copies data, and enqueues it to receiver.
 *
 * @param sender PID of sender
 * @param receiver PID of receiver (0 = Broadcast, handled internally)
 * @param type Message Type
 * @param data Pointer to data to copy
 * @param size Size of data
 * @return 0 on success, <0 on error (queue full, invalid PID)
 */
int msg_send(uint32_t sender, uint32_t receiver, uint32_t type,
             const void* data, uint32_t size);

/**
 * Send Message (Pointer / Zero-Copy)
 * Passes a raw pointer. Used for shared memory or large buffers.
 * CAUTION: Ensure memory ownership/lifetime is managed safely.
 *
 * @param ptr Function pointer or data pointer to send
 */
int msg_send_ptr(uint32_t sender, uint32_t receiver, void* ptr, uint32_t size);

/**
 * Receive Message
 * Blocking call. Waits until a message arrives.
 * Copies message content to out_msg buffer (caller must allocate sufficient stack/heap).
 *
 * @param receiver PID (usually current task)
 * @param out_msg Buffer to hold received message
 * @param max_size Maximum size of the output buffer
 */
int msg_receive(uint32_t receiver, struct message* out_msg, size_t max_size);

/**
 * Check for pending messages (Non-blocking poll)
 */
bool msg_available(uint32_t receiver);

/**
 * Get number of pending messages
 */
uint32_t msg_count(uint32_t receiver);

/**
 * Clear all pending messages for a task
 */
void msg_clear(uint32_t receiver);

#endif // MESSAGES_H
