/*
 * Project Zenin - Z-Bus IPC & Event Wire Protocol
 * Minimalist, Zero-Copy, AI-Native System Event Protocol
 * Target RAM Footprint: < 2 MB
 */

#ifndef ZENIN_ZBUS_H
#define ZENIN_ZBUS_H

#include <stdint.h>
#include <stddef.h>

#define ZBUS_MAGIC 0x5A4E494E  /* "ZNIN" in ASCII */
#define ZBUS_MAX_PAYLOAD 4096

/* Core System Subsystem IDs */
typedef enum {
    ZBUS_SUB_KERNEL      = 0x01,
    ZBUS_SUB_POWER       = 0x02,
    ZBUS_SUB_DISPLAY     = 0x03,
    ZBUS_SUB_INPUT       = 0x04,
    ZBUS_SUB_MODEM       = 0x05,
    ZBUS_SUB_AUDIO       = 0x06,
    ZBUS_SUB_AI_CORE     = 0x10,
    ZBUS_SUB_UI_SHELL    = 0x20
} zbus_subsystem_t;

/* AI-Native Intent Action Types */
typedef enum {
    ZBUS_ACT_QUERY_STATE = 0x0001,
    ZBUS_ACT_SET_STATE   = 0x0002,
    ZBUS_ACT_NOTIFY      = 0x0003,
    ZBUS_ACT_AI_INTENT   = 0x0010,  /* Natural language or structured AI intent */
    ZBUS_ACT_EMERGENCY   = 0x00FF
} zbus_action_t;

/* Standard Z-Bus Frame Header (Fixed 24-byte header) */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;         /* Magic: 0x5A4E494E */
    uint16_t version;       /* Protocol version (e.g. 1) */
    uint16_t sender_id;     /* Origin subsystem */
    uint16_t target_id;     /* Destination subsystem (or 0xFFFF for broadcast) */
    uint16_t action;        /* Action opcode (zbus_action_t) */
    uint32_t transaction_id;/* Sequence ID for correlation */
    uint32_t payload_len;   /* Length of data following header */
    uint32_t checksum;      /* CRC32 of payload */
} zbus_header_t;
#pragma pack(pop)

/* Helper macros */
#define ZBUS_IS_VALID(hdr) ((hdr)->magic == ZBUS_MAGIC)

#endif /* ZENIN_ZBUS_H */
