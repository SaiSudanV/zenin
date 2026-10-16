/*
 * Project Zenin - Zero-Copy Z-Bus Event Mesh Implementation
 * Dynamically allocated in Kernel Heap to preserve BSS.
 */

#include "../include/zbus.h"

typedef struct {
    zbus_message_t queue[ZBUS_QUEUE_CAPACITY];
    uint32_t head; /* Monotonic producer counter; single producer only. */
    uint32_t tail; /* Monotonic consumer counter; single consumer only. */
    uint32_t total_processed;
} zbus_ring_t;

static zbus_ring_t zbus_storage;
static zbus_ring_t *zbus_state;

void zbus_init(void) {
    zbus_state = &zbus_storage;
    __atomic_store_n(&zbus_state->head, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&zbus_state->tail, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&zbus_state->total_processed, 0, __ATOMIC_RELAXED);
}

static bool zbus_reserve_slot(uint32_t *head_out, uint32_t *index_out) {
    if (!zbus_state) return false;
    uint32_t head = __atomic_load_n(&zbus_state->head, __ATOMIC_RELAXED);
    uint32_t tail = __atomic_load_n(&zbus_state->tail, __ATOMIC_ACQUIRE);
    if ((uint32_t)(head - tail) >= ZBUS_QUEUE_CAPACITY) return false;
    *head_out = head;
    *index_out = head % ZBUS_QUEUE_CAPACITY;
    return true;
}

bool zbus_publish(uint32_t sender_id, uint32_t receiver_id, zbus_msg_type_t type, const void *data, uint32_t len) {
    uint32_t head, index;
    if (!zbus_reserve_slot(&head, &index)) return false;
    zbus_message_t *slot = &zbus_state->queue[index];
    slot->sender_id = sender_id;
    slot->receiver_id = receiver_id;
    slot->type = type;
    slot->payload_len = (len >= ZBUS_PAYLOAD_MAX) ? (ZBUS_PAYLOAD_MAX - 1) : len;

    for (uint32_t i = 0; i < ZBUS_PAYLOAD_MAX; i++) slot->payload[i] = 0;
    slot->payload_ptr = 0;
    if (data && len > 0) {
        const uint8_t *src = (const uint8_t *)data;
        for (uint32_t i = 0; i < slot->payload_len; i++) {
            slot->payload[i] = src[i];
        }
    }
    slot->payload[slot->payload_len] = '\0';

    __atomic_store_n(&zbus_state->head, head + 1, __ATOMIC_RELEASE);
    return true;
}

bool zbus_publish_ptr(uint32_t sender_id, uint32_t receiver_id, void *raw_ptr) {
    uint32_t head, index;
    if (!zbus_reserve_slot(&head, &index)) return false;
    zbus_message_t *slot = &zbus_state->queue[index];
    slot->sender_id = sender_id;
    slot->receiver_id = receiver_id;
    slot->type = ZBUS_MSG_PTR_HANDOFF;
    slot->payload_len = 0;
    for (uint32_t i = 0; i < ZBUS_PAYLOAD_MAX; i++) slot->payload[i] = 0;
    slot->payload_ptr = (uintptr_t)raw_ptr;
    __atomic_store_n(&zbus_state->head, head + 1, __ATOMIC_RELEASE);
    return true;
}

bool zbus_poll(zbus_message_t *out_msg) {
    if (!zbus_state || !out_msg) return false;
    uint32_t tail = __atomic_load_n(&zbus_state->tail, __ATOMIC_RELAXED);
    uint32_t head = __atomic_load_n(&zbus_state->head, __ATOMIC_ACQUIRE);
    if (tail == head) return false;

    zbus_message_t *slot = &zbus_state->queue[tail % ZBUS_QUEUE_CAPACITY];
    *out_msg = *slot;
    __atomic_store_n(&zbus_state->tail, tail + 1, __ATOMIC_RELEASE);
    uint32_t processed = __atomic_load_n(&zbus_state->total_processed, __ATOMIC_RELAXED);
    __atomic_store_n(&zbus_state->total_processed, processed + 1, __ATOMIC_RELAXED);
    return true;
}

uint32_t zbus_get_processed_count(void) {
    return zbus_state ? __atomic_load_n(&zbus_state->total_processed, __ATOMIC_RELAXED) : 0;
}
