#pragma once

#include <stdint.h>

#pragma pack(push, 1)
typedef struct hdr_common
{
#define USBIP_VERSION 0x0111
    uint16_t version;
#define OP_REQUEST  0x8000
#define OP_REPLY    0x0000
#define OP_IMPORT   0x0003
#define OP_DEVLIST  0x0005
#define REQ_DEVLIST (OP_REQUEST | OP_DEVLIST)
#define REP_DEVLIST (OP_REPLY | OP_DEVLIST)
#define REQ_IMPORT  (OP_REQUEST | OP_IMPORT)
#define REP_IMPORT  (OP_REPLY | OP_IMPORT)
    uint16_t op_code;
#define USBIP_STATUS_OK    0
#define USBIP_STATUS_ERROR 1
    uint32_t status;
} hdr_common_t;

typedef struct hdr_rep_devlist
{
    hdr_common_t hdr;
    uint32_t dev_count;
} hdr_rep_devlist_t;

typedef struct hdr_cmd
{
#define USBIP_CMD_SUBMIT 0x01
#define USBIP_RET_SUBMIT 0x03
#define USBIP_CMD_UNLINK 0x02
#define USBIP_RET_UNLINK 0x04
    uint32_t command;
    uint32_t seq_num;
    uint16_t busnum;
    uint16_t devnum;
#define USBIP_DIR_OUT 0
#define USBIP_DIR_IN  1
    uint32_t direction;
    uint32_t endpoint;
} hdr_cmd_t;

typedef struct iso_packet
{
    uint32_t offset;
    uint32_t length;
    uint32_t actual_length;
    uint32_t status;
} iso_packet_t;

typedef struct cmd
{
    struct cmd_base
    {
        uint32_t txfer_flags;
        uint32_t txfer_buf_len;
        uint32_t iso_start_frame;
        uint32_t iso_pkt_cnt;
        uint32_t interval;
        uint64_t usb_setup;
    } base;
    uint8_t* txfer_buffer;
    iso_packet_t* iso_packets;
} cmd_t;

typedef struct ret
{
    struct ret_base
    {
        uint32_t status;
        uint32_t actual_length;
        uint32_t start_frame;
        uint32_t number_of_packets;
        uint32_t error_count;
        uint64_t padding;
    } base;
    uint8_t* txfer_buffer;
    iso_packet_t* iso_packets;
} ret_t;

#pragma pack(pop)