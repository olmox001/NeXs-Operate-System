// messages.c - Implementation
#include "messages.h"
#include "libc.h"
#include "buddy.h"

// Message queues for each task (dynamic allocation)
static struct msg_queue* task_queues[MAX_TASKS];
static uint64_t system_ticks = 0;

// Initialize message system
void msg_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        task_queues[i] = NULL;
    }
    system_ticks = 0;
    // Self-test
    ASSERT(system_ticks == 0);
}

// Get or create task queue
static struct msg_queue* get_queue(uint32_t task_id) {
    if (task_id >= MAX_TASKS) {
        PANIC("Invalid Task ID in Msg System");
        return NULL;
    }
    
    if (!task_queues[task_id]) {
        task_queues[task_id] = (struct msg_queue*)buddy_alloc(sizeof(struct msg_queue));
        if (task_queues[task_id]) {
            memset(task_queues[task_id], 0, sizeof(struct msg_queue));
        }
    }
    
    return task_queues[task_id];
}

// Send message
int msg_send(uint32_t sender, uint32_t receiver, uint32_t type,
             const void* data, uint32_t size) {
    
    if (size > MSG_MAX_SIZE) {
        return -1; // Message too large
    }
    
    if (receiver >= MAX_TASKS && receiver != 0) {
        return -1; // Invalid receiver
    }
    
    system_ticks++;
    
    // Handle broadcast (receiver = 0)
    if (receiver == 0) {
        int success = 0;
        for (uint32_t i = 0; i < MAX_TASKS; i++) {
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
        return -1; // Failed to allocate queue
    }
    
    // Check if queue is full
    if (queue->count >= MSG_QUEUE_SIZE) {
        return -1; // Queue full
    }
    
    // Add message to queue
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

// Receive message
int msg_receive(uint32_t receiver, struct message* msg) {
    if (!msg) {
        return -1;
    }
    
    struct msg_queue* queue = get_queue(receiver);
    if (!queue) {
        return -1;
    }
    
    // Wait for message (blocking)
    while (queue->count == 0) {
        hlt(); // Sleep until interrupt
    }
    
    // Get message from queue
    memcpy(msg, &queue->messages[queue->read_pos], sizeof(struct message));
    
    queue->read_pos = (queue->read_pos + 1) % MSG_QUEUE_SIZE;
    queue->count--;
    
    return 0;
}

// Check if messages are available
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

// Get message count
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

// Clear message queue
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