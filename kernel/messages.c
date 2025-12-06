/*
 * messages.c - IPC Message System with Slab Allocator
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "messages.h"
#include "libc.h"
#include "buddy.h"
#include "handlers.h"

// Slab size table
static const size_t slab_sizes[MSG_SLAB_COUNT] = {16, 64, 256, 1024, 4096};

// Slab free lists
struct slab_block {
    struct slab_block* next;
};
static struct slab_block* slab_free[MSG_SLAB_COUNT];
static uint32_t slab_alloc_count[MSG_SLAB_COUNT];

// Task queues
static struct msg_queue* task_queues[MAX_TASKS];

// Select appropriate slab class for size
static int size_to_slab(size_t size) {
    for (int i = 0; i < MSG_SLAB_COUNT; i++) {
        if (size <= slab_sizes[i]) return i;
    }
    return -1; // Too large
}

void msg_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        task_queues[i] = NULL;
    }
    for (int i = 0; i < MSG_SLAB_COUNT; i++) {
        slab_free[i] = NULL;
        slab_alloc_count[i] = 0;
    }
}

struct message* msg_alloc(size_t data_size) {
    int slab = size_to_slab(data_size);
    if (slab < 0) return NULL;
    
    size_t total = sizeof(struct message) + slab_sizes[slab];
    struct message* msg;
    
    // Check slab free list first
    if (slab_free[slab]) {
        msg = (struct message*)slab_free[slab];
        slab_free[slab] = slab_free[slab]->next;
    } else {
        // Allocate from buddy
        msg = (struct message*)buddy_alloc(total);
        if (!msg) return NULL;
        slab_alloc_count[slab]++;
    }
    
    memset(msg, 0, total);
    msg->slab_class = slab;
    msg->size = data_size;
    return msg;
}

void msg_free(struct message* msg) {
    if (!msg) return;
    
    // Return to slab free list
    struct slab_block* blk = (struct slab_block*)msg;
    blk->next = slab_free[msg->slab_class];
    slab_free[msg->slab_class] = blk;
}

static struct msg_queue* get_queue(uint32_t task_id) {
    if (task_id >= MAX_TASKS) return NULL;
    
    if (!task_queues[task_id]) {
        task_queues[task_id] = (struct msg_queue*)buddy_alloc(sizeof(struct msg_queue));
        if (task_queues[task_id]) {
            memset(task_queues[task_id], 0, sizeof(struct msg_queue));
        }
    }
    return task_queues[task_id];
}

int msg_send(uint32_t sender, uint32_t receiver, uint32_t type,
             const void* data, uint32_t size) {
    if (size > MSG_MAX_SIZE) return -1;
    
    // Handle broadcast
    if (receiver == 0) {
        int success = 0;
        for (uint32_t i = 1; i < MAX_TASKS; i++) {
            if (i != sender && task_queues[i]) {
                if (msg_send(sender, i, type, data, size) == 0) success++;
            }
        }
        return success > 0 ? 0 : -1;
    }
    
    struct msg_queue* queue = get_queue(receiver);
    if (!queue) return -1;
    if (queue->count >= MSG_QUEUE_SIZE) return -1;
    
    struct message* msg = msg_alloc(size);
    if (!msg) return -1;
    
    msg->sender_id = sender;
    msg->receiver_id = receiver;
    msg->type = type;
    msg->timestamp = get_timer_ticks();
    
    if (data && size > 0) {
        memcpy(msg->data, data, size);
    }
    
    queue->messages[queue->write_pos] = msg;
    queue->write_pos = (queue->write_pos + 1) % MSG_QUEUE_SIZE;
    queue->count++;
    
    return 0;
}

int msg_send_ptr(uint32_t sender, uint32_t receiver, void* ptr, uint32_t size) {
    struct msg_queue* queue = get_queue(receiver);
    if (!queue) return -1;
    if (queue->count >= MSG_QUEUE_SIZE) return -1;
    
    struct message* msg = msg_alloc(sizeof(void*));
    if (!msg) return -1;
    
    msg->sender_id = sender;
    msg->receiver_id = receiver;
    msg->type = MSG_TYPE_POINTER;
    msg->size = size;
    msg->timestamp = get_timer_ticks();
    *(void**)msg->data = ptr;
    
    queue->messages[queue->write_pos] = msg;
    queue->write_pos = (queue->write_pos + 1) % MSG_QUEUE_SIZE;
    queue->count++;
    
    return 0;
}

int msg_receive(uint32_t receiver, struct message* out_msg) {
    if (!out_msg) return -1;
    
    struct msg_queue* queue = get_queue(receiver);
    if (!queue) return -1;
    
    while (queue->count == 0) {
        hlt();
    }
    
    struct message* msg = queue->messages[queue->read_pos];
    memcpy(out_msg, msg, sizeof(struct message) + msg->size);
    
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
    
    // Free all queued messages
    while (queue->count > 0) {
        msg_free(queue->messages[queue->read_pos]);
        queue->read_pos = (queue->read_pos + 1) % MSG_QUEUE_SIZE;
        queue->count--;
    }
}