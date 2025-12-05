
// messages.h - Inter-Process Communication Message System
#ifndef MESSAGES_H
#define MESSAGES_H

#include "kernel.h"

#define MSG_MAX_SIZE 256
#define MSG_QUEUE_SIZE 64

// Message types
enum msg_type {
    MSG_TYPE_DATA = 1,      // Data message
    MSG_TYPE_SIGNAL = 2,    // Signal message
    MSG_TYPE_REQUEST = 3,   // Service request
    MSG_TYPE_RESPONSE = 4,  // Service response
};

// Message structure
struct message {
    uint32_t sender_id;         // Sender task ID
    uint32_t receiver_id;       // Receiver task ID (0 = broadcast)
    uint32_t type;              // Message type
    uint32_t size;              // Data size
    uint8_t data[MSG_MAX_SIZE]; // Message data
    uint64_t timestamp;         // Timestamp (ticks)
};

// Message queue (per task)
struct msg_queue {
    struct message messages[MSG_QUEUE_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;
};

// Initialize message system
void msg_init(void);

// Send message (returns 0 on success, -1 on failure)
int msg_send(uint32_t sender, uint32_t receiver, uint32_t type, 
             const void* data, uint32_t size);

// Receive message (blocking if no messages)
int msg_receive(uint32_t receiver, struct message* msg);

// Check if messages are available
bool msg_available(uint32_t receiver);

// Get message count in queue
uint32_t msg_count(uint32_t receiver);

// Clear message queue
void msg_clear(uint32_t receiver);

#endif // MESSAGES_H
