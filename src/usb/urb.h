#pragma once
#include <stdint.h>

#pragma pack(push, 1)
typedef struct urb_setup
{
    union
    {
        struct
        {
            uint8_t recipient : 5;
#define RECIPIENT_DEV   0
#define RECIPIENT_IF    1
#define RECIPIENT_EP    2
#define RECIPIENT_OTHER 3
            uint8_t type : 2;
            uint8_t dir : 1;
        } bmRequestType;
        uint8_t bRequestType;
    };
    uint8_t bRequest;
#define DEV_REQ_STATUS     0
#define DEV_REQ_CLR_FEAT   1
#define DEV_REQ_SET_FEAT   3
#define DEV_REQ_SET_ADDR   5
#define DEV_REQ_GET_DESC   6
#define DEV_REQ_SET_DESC   7
#define DEV_REQ_GET_CONF   8
#define DEV_REQ_SET_CONF   9
#define DEV_REQ_GET_IF     10
#define DEV_REQ_SET_IF     11
#define DEV_REQ_SYNC_FRAME 12
    uint16_t wValue;
#define USB_DESC_TYPE_DEV     1
#define USB_DESC_TYPE_CONF    2
#define USB_DESC_TYPE_STR     3
#define USB_DESC_TYPE_IF      4
#define USB_DESC_TYPE_EP      5
#define USB_DESC_TYPE_DEV_Q   6
#define USB_DESC_TYPE_OS_CONF 7
#define USB_DESC_TYPE_IF_PWR  8

#define FEAT_EP_HLT    0
#define FEAT_REM_WK    1
#define FEAT_TEST_MODE 2
    uint16_t wIndex;
    uint16_t wLength;
} urb_setup_t;
#pragma pack(pop)

typedef struct usb_iso_packet_descriptor
{
    uint32_t offset;
    uint32_t length;
    uint32_t actual_length;
    int status;
} usb_iso_packet_descriptor_t;

typedef struct urba
{
    unsigned int pipe; // endpoint information
#define PIPE_IN        0x01
#define PIPE_OUT       0x00
#define PIPE_DIR_MASK  0x01
#define PIPE_EP_OFF    3
#define PIPE_TYPE_OFF  1
#define PIPE_TYPE_MASK 0x3
#define PIPE_TYPE_CTRL 0x0
#define PIPE_TYPE_BULK 0x1
#define PIPE_TYPE_INTR 0x2
#define PIPE_TYPE_ISO  0x3

#define PIPE_TYPE_GET(pipe) ((pipe >> PIPE_TYPE_OFF) & PIPE_TYPE_MASK);
#define PIPE_TYPE_SET(pipe) ((pipe & PIPE_TYPE_MASK) << PIPE_TYPE_OFF);
#define PIPE_DIR(pipe)      ((pipe)&PIPE_DIR_MASK)
#define PIPE_EP_GET(pipe)   (pipe >> PIPE_EP_OFF)
#define PIPE_EP_SET(pipe)   (pipe << PIPE_EP_OFF)

    unsigned int transfer_flags; // URB_ISO_ASAP, URB_SHORT_NOT_OK, etc.
#define URB_SHORT_NOT_OK        0x0001 // report short reads as errors
#define URB_ISO_ASAP            0x0002 // iso-only; use the first unexpired slot in the schedule
#define URB_NO_TRANSFER_DMA_MAP 0x0004 // urb->transfer_dma valid on submit
#define URB_ZERO_PACKET         0x0040 // Finish bulk OUT with short packet
#define URB_NO_INTERRUPT        0x0080 // HINT: no non-error interrupt needed
#define URB_FREE_BUFFER         0x0100 // Free transfer buffer with the URB
// Interal flags
#define URB_INTERNAL_PARTIAL_URB 0x0200 // This is a partial URB not fully received yet.

    // (IN) all urbs need completion routines
    void* context; // context for completion routine
    void (*complete)(struct urb*, void*); // pointer to completion routine

    // (OUT) status after each completion
    int status; // returned status

    // (IN) buffer used for data transfers
    void* transfer_buffer; // associated data buffer
    uint32_t transfer_buffer_length; // data buffer length
    int number_of_packets; // size of iso_frame_desc

    // (OUT) sometimes only part of CTRL/BULK/INTR transfer_buffer is used
    uint32_t actual_length; // actual data buffer length

    // (IN) setup stage for CTRL (pass a struct usb_ctrlrequest)
    urb_setup_t setup_packet; // setup packet (control only)

    // Only for PERIODIC transfers (ISO, INTERRUPT)
    // (IN/OUT) start_frame is set unless URB_ISO_ASAP isn't set
    int start_frame; // start frame
    int interval; // polling interval

    // ISO only: packets are only "best effort"; each can have errors
    int error_count; // number of errors

    struct usb_iso_packet_descriptor iso_frame_desc[0];

    // Sequence number of this urb request needed for unlink requests.
    uint32_t seq_num;

    // Next urb in sequence (singly linked list)
    struct urb* next;
} urb_t;