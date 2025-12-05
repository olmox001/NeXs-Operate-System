/*
 * messages.c - Inter-Process Communication (IPC) Message System
 *
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, NeXs Operate System
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "messages.h"
#include "libc.h"
#include "buddy.h"

// Dynamic Array of Task Message Queues
static struct msg_queue* task_queues[MAX_TASKS];
static uint64_t system_ticks = 0;

/**
 * Initialize Message System
 */
void msg_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        task_queues[i] = NULL;
    }
    system_ticks = 0;
    
    // System Integrity Check
    ASSERT(system_ticks == 0);
}

/**
 * Retrieve or allocate a message queue for a task
 */
static struct msg_queue* get_queue(uint32_t task_id) {
    if (task_id >= MAX_TASKS) {
        PANIC("Invalid Task ID in Msg System");
        return NULL;
    }
    
    if (!task_queues[task_id]) {
        // Lazy Allocation
        task_queues[task_id] = (struct msg_queue*)buddy_alloc(sizeof(struct msg_queue));
        if (task_queues[task_id]) {
            memset(task_queues[task_id], 0, sizeof(struct msg_queue));
        }
    }
    
    return task_queues[task_id];
}

/**
 * Send a Message
 */
int msg_send(uint32_t sender, uint32_t receiver, uint32_t type,
             const void* data, uint32_t size) {
    
    // Validate Message Size
    if (size > MSG_MAX_SIZE) {
        return -1; // overflow
    }
    
    // Validate Receiver ID
    if (receiver >= MAX_TASKS && receiver != 0) {
        return -1; // Invalid target
    }
    
    system_ticks++;
    
    // Handle Broadcast (Receiver ID 0)
    if (receiver == 0) {
        int success = 0;
        for (uint32_t i = 1; i < MAX_TASKS; i++) { // Skip Task 0 (Kernel) usually, but logic allows it
            if (i != sender && task_queues[i]) {
                if (msg_send(sender, i, type, data, size) == 0) {
                    success++;
                }
            }
        }
        return success > 0 ? 0 : -1;
    }
    
    struct msg_queue* queue = get_queue(receiver);
    if (!queue) {
        return -1; // Allocation failure
    }
    
    // Check for Queue Capacity
    if (queue->count >= MSG_QUEUE_SIZE) {
        return -1; // Queue Full (Drop Message)
    }
    
    // Enqueue Message
    struct message* msg = &queue->messages[queue->write_pos];
    msg->sender_id = sender;
    msg->receiver_id = receiver;
    msg->type = type;
    msg->size = size;
    msg->timestamp = system_ticks;
    
    if (data && size > 0) {
        memcpy(msg->data, data, size);
    }
    
    queue->write_pos = (queue->write_pos + 1) % MSG_QUEUE_SIZE;
    queue->count++;
    
    return 0;
}

/**
 * Receive a Message (Blocking)
 */
int msg_receive(uint32_t receiver, struct message* msg) {
    if (!msg) {
        return -1;
    }
    
    struct msg_queue* queue = get_queue(receiver);
    if (!queue) {
        return -1;
    }
    
    // Wait for message (Spin/Sleep Loop)
    while (queue->count == 0) {
        hlt(); // Wait for interrupt (e.g., timer or I/O)
    }
    
    // Dequeue Message
    memcpy(msg, &queue->messages[queue->read_pos], sizeof(struct message));
    
    queue->read_pos = (queue->read_pos + 1) % MSG_QUEUE_SIZE;
    queue->count--;
    
    return 0;
}

/**
 * Check for Pending Messages (Non-Blocking)
 */
bool msg_available(uint32_t receiver) {
    if (receiver >= MAX_TASKS) {
        return false;
    }
    
    struct msg_queue* queue = task_queues[receiver];
    if (!queue) {
        return false;
    }
    
    return queue->count > 0;
}

/**
 * Get Pending Message Count
 */
uint32_t msg_count(uint32_t receiver) {
    if (receiver >= MAX_TASKS) {
        return 0;
    }
    
    struct msg_queue* queue = task_queues[receiver];
    if (!queue) {
        return 0;
    }
    
    return queue->count;
}

/**
 * Flush Message Queue
 */
void msg_clear(uint32_t receiver) {
    if (receiver >= MAX_TASKS) {
        return;
    }
    
    struct msg_queue* queue = task_queues[receiver];
    if (!queue) {
        return;
    }
    
    queue->read_pos = 0;
    queue->write_pos = 0;
    queue->count = 0;
}