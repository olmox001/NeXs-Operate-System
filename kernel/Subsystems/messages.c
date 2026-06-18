/*
 * messages.c - IPC Message System with Slab Allocator Implementation
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "messages.h"
#include "libx.h"
#include "buddy.h"
#include "handlers.h" // For context (scheduler aware?)
#include "timer.h"    // For timestamps

// =============================================================================
// Slab Allocator State
// =============================================================================

// Fixed payload sizes for slabs
static const size_t slab_sizes[MSG_SLAB_COUNT] = {
    16,     // SLAB_16
    64,     // SLAB_64
    256,    // SLAB_256
    1024,   // SLAB_1024
    4096    // SLAB_4096
};

// Generic list node for free blocks
struct slab_block {
    struct slab_block* next;
};

// Heads of free lists for each class
static struct slab_block* slab_free[MSG_SLAB_COUNT];

// Statistics
static uint32_t slab_alloc_count[MSG_SLAB_COUNT];

// =============================================================================
// Global Queue Registry
// =============================================================================
// Indexed by PID. TODO: Dynamic array or Hash Map for scalability.
static struct msg_queue* task_queues[MAX_TASKS];

// =============================================================================
// Internal Helpers
// =============================================================================

/**
 * Determine best slab class for a given size.
 * Returns index 0-4, or -1 if too large.
 */
static int size_to_slab(size_t size) {
    for (int i = 0; i < MSG_SLAB_COUNT; i++) {
        if (size <= slab_sizes[i]) return i;
    }
    return -1; // Exceeds MSG_MAX_SIZE
}

/**
 * Get (or create) queue for a task ID
 */
static struct msg_queue* get_queue(uint32_t task_id) {
    if (task_id >= MAX_TASKS) return NULL;

    // Lazy Allocation
    if (!task_queues[task_id]) {
        task_queues[task_id] = (struct msg_queue*)buddy_alloc(sizeof(struct msg_queue));
        if (task_queues[task_id]) {
            memset(task_queues[task_id], 0, sizeof(struct msg_queue));
        }
    }
    return task_queues[task_id];
}

// =============================================================================
// API Implementation
// =============================================================================

void msg_init(void) {
    // Clear Queues
    for (int i = 0; i < MAX_TASKS; i++) {
        task_queues[i] = NULL;
    }
    // Clear Slabs
    for (int i = 0; i < MSG_SLAB_COUNT; i++) {
        slab_free[i] = NULL;
        slab_alloc_count[i] = 0;
    }
}

/**
 * Allocate Message from Slab
 */
struct message* msg_alloc(size_t data_size) {
    int slab = size_to_slab(data_size);
    if (slab < 0) return NULL;

    size_t total_size = sizeof(struct message) + slab_sizes[slab];
    struct message* msg;

    // 1. Check Free List
    if (slab_free[slab]) {
        msg = (struct message*)slab_free[slab];
        slab_free[slab] = slab_free[slab]->next;
    } else {
        // 2. Allocate New Block from Heap
        msg = (struct message*)buddy_alloc(total_size);
        if (!msg) return NULL;
        slab_alloc_count[slab]++;
    }

    memset(msg, 0, total_size);
    msg->slab_class = slab;
    msg->size = data_size;

    return msg;
}

/**
 * Return Message to Slab
 */
void msg_free(struct message* msg) {
    if (!msg) return;

    // Cast to list node
    struct slab_block* blk = (struct slab_block*)msg;

    // Push to head of free list
    blk->next = slab_free[msg->slab_class];
    slab_free[msg->slab_class] = blk;
}

/**
 * Send Message (Copy)
 */
int msg_send(uint32_t sender, uint32_t receiver, uint32_t type,
             const void* data, uint32_t size) {

    if (size > MSG_MAX_SIZE) return -1;

    // Broadcast Logic (Sender -> All Others)
    if (receiver == 0) {
        int success = 0;
        for (uint32_t i = 1; i < MAX_TASKS; i++) {
            if (i != sender && task_queues[i]) {
                if (msg_send(sender, i, type, data, size) == 0) success++;
            }
        }
        return success > 0 ? 0 : -1;
    }

    // Single Recipient
    struct msg_queue* queue = get_queue(receiver);
    if (!queue) return -1;

    // Check Full
    if (queue->count >= MSG_QUEUE_SIZE) return -1;

    // Allocate
    struct message* msg = msg_alloc(size);
    if (!msg) return -1;

    // Copy Data
    msg->sender_id = sender;
    msg->receiver_id = receiver;
    msg->type = type;
    msg->timestamp = timer_get_ticks();

    if (data && size > 0) {
        memcpy(msg->data, data, size);
    }

    // Enqueue
    queue->messages[queue->write_pos] = msg;
    queue->write_pos = (queue->write_pos + 1) % MSG_QUEUE_SIZE;
    queue->count++;

    // Wake up receiver if sleeping? (Task state handling logic would go here)
    // if (receiver_task->state == TASK_WAITING_MSG) scheduler_wake(receiver_task);

    return 0;
}

/**
 * Send Message (Pointer)
 */
int msg_send_ptr(uint32_t sender, uint32_t receiver, void* ptr, uint32_t size) {
    struct msg_queue* queue = get_queue(receiver);
    if (!queue) return -1;
    if (queue->count >= MSG_QUEUE_SIZE) return -1;

    // Allocate for ptr size only (8 bytes)
    struct message* msg = msg_alloc(sizeof(void*));
    if (!msg) return -1;

    msg->sender_id = sender;
    msg->receiver_id = receiver;
    msg->type = MSG_TYPE_POINTER;
    msg->size = size; // Metadata: size of the object pointed to
    msg->timestamp = timer_get_ticks();

    // Store pointer in data payload
    *(void**)msg->data = ptr;

    queue->messages[queue->write_pos] = msg;
    queue->write_pos = (queue->write_pos + 1) % MSG_QUEUE_SIZE;
    queue->count++;

    return 0;
}

/**
 * Receive Message (Blocking)
 */
int msg_receive(uint32_t receiver, struct message* out_msg, size_t max_size) {
    if (!out_msg) return -1;

    struct msg_queue* queue = get_queue(receiver);
    if (!queue) return -1;

    // Wait Loop
    // TODO: Use scheduler sleep/wake blocks instead of spinloop hlt()
    while (queue->count == 0) {
        asm volatile("hlt");
    }

    // Dequeue
    struct message* msg = queue->messages[queue->read_pos];

    // Security Check
    if (sizeof(struct message) + msg->size > max_size) {
        // Do not remove message from queue, allowing caller to retry with larger buffer
        return MSG_ERR_BUFFER_TOO_SMALL;
    }

    // Copy to user provided buffer envelope
    memcpy(out_msg, msg, sizeof(struct message) + msg->size);

    // Free internal buffer
    msg_free(msg);

    queue->read_pos = (queue->read_pos + 1) % MSG_QUEUE_SIZE;
    queue->count--;

    return 0;
}

bool msg_available(uint32_t receiver) {
    if (receiver >= MAX_TASKS) return false;
    struct msg_queue* queue = task_queues[receiver];
    return queue && queue->count > 0;
}

uint32_t msg_count(uint32_t receiver) {
    if (receiver >= MAX_TASKS) return 0;
    struct msg_queue* queue = task_queues[receiver];
    return queue ? queue->count : 0;
}

void msg_clear(uint32_t receiver) {
    if (receiver >= MAX_TASKS) return;
    struct msg_queue* queue = task_queues[receiver];
    if (!queue) return;

    while (queue->count > 0) {
        msg_free(queue->messages[queue->read_pos]);
        queue->read_pos = (queue->read_pos + 1) % MSG_QUEUE_SIZE;
        queue->count--;
    }
}
