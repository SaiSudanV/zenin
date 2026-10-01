/*
 * Project Zenin - Zero-Copy Z-Bus Event Mesh Implementation
 * Dynamically allocated in Kernel Heap to preserve BSS.
 */

#include "../include/zbus.h"
#include "../include/kheap.h"

typedef struct {
    zbus_message_t queue[ZBUS_QUEUE_CAPACITY];
    volatile uint32_t head;
    volatile uint32_t tail;
    uint32_t total_processed;
} zbus_ring_t;

static zbus_ring_t *zbus_state = NULL;

void zbus_init(void) {
    zbus_state = (zbus_ring_t *)kmalloc(sizeof(zbus_ring_t));
    if (zbus_state) {
        zbus_state->head = 0;
        zbus_state->tail = 0;
        zbus_state->total_processed = 0;
    }
}

bool zbus_publish(uint32_t sender_id, uint32_t receiver_id, zbus_msg_type_t type, const void *data, uint32_t len) {
    if (!zbus_state) return false;

    uint32_t next_head = (zbus_state->head + 1) % ZBUS_QUEUE_CAPACITY;
    if (next_head == zbus_state->tail) {
        return false; /* Ring buffer full */
    }

    zbus_message_t *slot = &zbus_state->queue[zbus_state->head];
    slot->sender_id = sender_id;
    slot->receiver_id = receiver_id;
    slot->type = type;
    slot->payload_len = (len >= ZBUS_PAYLOAD_MAX) ? (ZBUS_PAYLOAD_MAX - 1) : len;

    if (data && len > 0) {
        const uint8_t *src = (const uint8_t *)data;
        for (uint32_t i = 0; i < slot->payload_len; i++) {
            slot->payload[i] = src[i];
        }
    }
    slot->payload[slot->payload_len] = '\0';

    zbus_state->head = next_head;
    return true;
}

bool zbus_poll(zbus_message_t *out_msg) {
    if (!zbus_state || !out_msg || (zbus_state->tail == zbus_state->head)) {
        return false; /* Queue empty */
    }

    zbus_message_t *slot = &zbus_state->queue[zbus_state->tail];
    out_msg->sender_id = slot->sender_id;
    out_msg->receiver_id = slot->receiver_id;
    out_msg->type = slot->type;
    out_msg->payload_len = slot->payload_len;

    for (uint32_t i = 0; i <= slot->payload_len; i++) {
        out_msg->payload[i] = slot->payload[i];
    }

    zbus_state->tail = (zbus_state->tail + 1) % ZBUS_QUEUE_CAPACITY;
    zbus_state->total_processed++;
    return true;
}

uint32_t zbus_get_processed_count(void) {
    return zbus_state ? zbus_state->total_processed : 0;
}
