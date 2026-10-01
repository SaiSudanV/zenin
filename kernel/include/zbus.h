/*
 * Project Zenin - Zero-Copy Z-Bus Event Mesh
 * High-speed, lockless ring buffer IPC.
 * Connects hardware interrupts, display compositor, and Edge AI with 0.0% idle CPU overhead.
 */

#ifndef ZENIN_ZBUS_CORE_H
#define ZENIN_ZBUS_CORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ZBUS_QUEUE_CAPACITY 16
#define ZBUS_PAYLOAD_MAX 64

typedef enum {
    ZBUS_MSG_INTERRUPT  = 0x01,
    ZBUS_MSG_POWER_EVT  = 0x02,
    ZBUS_MSG_TOUCH_EVT  = 0x03,
    ZBUS_MSG_AI_INTENT  = 0x10,
    ZBUS_MSG_SYS_HALT   = 0xFF
} zbus_msg_type_t;

typedef struct {
    uint32_t sender_id;
    uint32_t receiver_id;
    zbus_msg_type_t type;
    uint32_t payload_len;
    uint8_t payload[ZBUS_PAYLOAD_MAX];
} zbus_message_t;

/* Initialize the Z-Bus lockless ring buffer */
void zbus_init(void);

/* Publish a message to Z-Bus without memory copies */
bool zbus_publish(uint32_t sender_id, uint32_t receiver_id, zbus_msg_type_t type, const void *data, uint32_t len);

/* Retrieve next message from Z-Bus */
bool zbus_poll(zbus_message_t *out_msg);

/* Total messages processed counter */
uint32_t zbus_get_processed_count(void);

#endif /* ZENIN_ZBUS_CORE_H */
