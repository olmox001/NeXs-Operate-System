/*
 * messages.h - Inter-Process Communication (IPC) Definitions
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

#ifndef MESSAGES_H
#define MESSAGES_H

#include "kernel.h"

#define MSG_MAX_SIZE 256
#define MSG_QUEUE_SIZE 64

// Standard Message Types
enum msg_type {
    MSG_TYPE_DATA = 1,      // Raw Data Payload
    MSG_TYPE_SIGNAL = 2,    // Control Signal
    MSG_TYPE_REQUEST = 3,   // Service Request
    MSG_TYPE_RESPONSE = 4,  // Service Response
};

// Message Envelope
struct message {
    uint32_t sender_id;         // Source Task ID
    uint32_t receiver_id;       // Destination Task ID (0 for Broadcast)
    uint32_t type;              // Message protocol type
    uint32_t size;              // Payload size in bytes
    uint8_t data[MSG_MAX_SIZE]; // Payload Buffer
    uint64_t timestamp;         // System Tick Timestamp
};

// Circular Message Queue
struct msg_queue {
    struct message messages[MSG_QUEUE_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;
};

// Initialize IPC System
void msg_init(void);

// Send Message (0 = Success, -1 = Error)
int msg_send(uint32_t sender, uint32_t receiver, uint32_t type, 
             const void* data, uint32_t size);

// Receive Message (Blocking)
int msg_receive(uint32_t receiver, struct message* msg);

// Check if Queue has messages
bool msg_available(uint32_t receiver);

// Get number of pending messages
uint32_t msg_count(uint32_t receiver);

// Discard all pending messages
void msg_clear(uint32_t receiver);

#endif // MESSAGES_H
