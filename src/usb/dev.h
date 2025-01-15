#pragma once

#include "urb.h"
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

typedef __ssize_t ssize_t;

#pragma pack(push, 1)

typedef struct usb_desc_hdr
{
    uint8_t bLength;
    uint8_t bDescriptorType;
} usb_desc_hdr_t;

typedef struct usb_ep_desc
{
    union
    {
        uint8_t bEndpointAddress;
        struct
        {
            uint8_t ep_nb : 4;
            uint8_t reserved : 3;
            uint8_t dir : 1;
#define USB_EP_IN  1
#define USB_EP_OUT 0
        };
    };
    union
    {
        uint8_t bmAttributes;
        struct
        {
            uint8_t txfer_type : 2;
#define USB_EP_CTRL 0
#define USB_EP_BULK 1
#define USB_EP_INT  2
#define USB_EP_ISO  3
            uint8_t sync_type : 2;
#define USB_EP_ISO_NO_SYNC 0
#define USB_EP_ISO_ASYNC   1
#define USB_EP_ISO_ADAP    2
#define USB_EP_ISO_SYNC    3
            uint8_t usage_type : 2;
#define USB_EP_ISO_USE_DATA   0
#define USB_EP_ISO_USE_FB     1
#define USB_EP_ISO_USE_IMP_FB 2
#define USB_EP_ISO_USE_RES    3
        };
    };
    union
    {
        uint16_t wMaxPacketSize;
        struct
        {
            uint16_t packet_size : 11;
            uint16_t opportunities : 2;
        };
    };

    uint8_t bInterval;
} usb_ep_desc_t;

typedef struct usb_if_desc
{
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} usb_if_desc_t;

typedef struct usb_conf_desc
{
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    union
    {
        uint8_t bmAttributes;
        struct
        {
            uint8_t reserved : 5;
            uint8_t rmt_wake : 1;
            uint8_t self_power : 1;
            uint8_t one : 1;
        };
    };
    uint8_t bMaxPower;
} usb_conf_desc_t;

typedef struct usb_dev_desc
{
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} usb_dev_desc_t;
#pragma pack(pop)

typedef enum lang_id
{
    LANG_ID_NONE = 0,
    LANG_ID_ENGLISH_US = 0x0409
} lang_id_t;

typedef struct usb_string_desc
{
    uint8_t idx;
    wchar_t* string;
    struct usb_string_desc* next;
} usb_string_desc_t;

typedef void (*usb_ep_to_device)(void*, size_t);
typedef ssize_t (*usb_ep_to_host)(void*, size_t);

typedef struct usb_ep
{
    usb_ep_desc_t desc;
    struct usb_ep* next;
    usb_ep_to_device to_device;
    usb_ep_to_host to_host;
} usb_ep_t;

typedef struct usb_if
{
    usb_if_desc_t desc;
    struct usb_if* next;
    usb_ep_t* endpoints;
} usb_if_t;

typedef struct usb_if_group
{
    usb_if_t* interfaces;
    struct usb_if_group* next;
    int16_t cur_alt_set;
    size_t if_count;
} usb_if_group_t;

typedef struct usb_conf
{
    usb_conf_desc_t desc;
    struct usb_conf* next;
    usb_if_group_t* interfaces;
    int16_t cur_if;
} usb_conf_t;

typedef struct usb_dev
{
    usb_dev_desc_t desc;
    usb_string_desc_t* strings;
    int16_t cur_config;
    usb_conf_t* configurations;
    lang_id_t lang_id;

    // Path to device
    char path[256];
    // Bus ID
    char busid[32];
    // Bus number
    uint32_t busnum;
    // Device number
    uint32_t devnum;
    // Device speed
    uint32_t speed;
} usb_dev_t;

/**
 * @brief Create a new USB device.
 * @param desc, The device descriptor.
 * @param lang_id, The language ID.
 * @return usb_dev_t, The created device.
 */
usb_dev_t usb_dev_create(usb_dev_desc_t* desc, lang_id_t lang_id);

/**
 * @brief Add a string descriptor to the device.
 * @param dev, The device to add the string to.
 * @param string, The string descriptor to add.
 * @return int, -1 on error and errno set, otherwise 0.
 */
int usb_dev_add_string(usb_dev_t* dev, usb_string_desc_t* string);

/**
 * @brief Add a configuration to the device.
 * @param dev, The device to add the configuration to.
 * @param conf, The configuration to add.
 * @return int, -1 on error and errno set, otherwise 0.
 */
int usb_dev_add_config(usb_dev_t* dev, usb_conf_t* conf);

/**
 * @brief Add an interface group to the device.
 * @param dev, The device to add the interface group to.
 * @param conf_idx, The configuration index.
 * @param if_group, The interface group to add.
 * @return int, -1 on error and errno set, otherwise 0.
 */
int usb_dev_add_if_grp(usb_dev_t* dev, uint8_t conf_idx, usb_if_group_t* if_group);

/**
 * @brief Add an endpoint to the device.
 * @param dev, The device to add the endpoint to.
 * @param conf_idx, The configuration index to which the endpoint is added.
 * @param if_idx, The interface index to which the endpoint is added.
 * @param if_alt, The interface alternate setting with which this endpoint is associated.
 * @param ep, The endpoint to add.
 * @return int, -1 on error and errno set, otherwise 0.
 */
int usb_dev_add_ep(usb_dev_t* dev, uint8_t conf_idx, uint8_t if_idx, uint8_t if_alt, usb_ep_t* ep);

/**
 * @brief Add an interface group to the configuration.
 * @param conf, The configuration to add the interface group to.
 * @param if_group, The interface group to add.
 */
int usb_conf_add_if_grp(usb_conf_t* conf, usb_if_group_t* if_group);

/**
 * @brief Add an endpoint to the interface group.
 * @param if_group, The interface group to add the endpoint to.
 * @param ep, The endpoint to add.
 * @return int, -1 on error and errno set, otherwise 0.
 */
int usb_if_add_ep(usb_if_t* interface, usb_ep_t* ep);

/**
 * @brief Add an interface to the interface group.
 * @param group, The interface group to add the interface to.
 * @param interface, The interface to add.
 * @return int, -1 on error and errno set, otherwise 0.
 */
int usb_if_grp_add(usb_if_group_t* group, usb_if_t* interface);

/**
 * @brief Get the configuration descriptor for the device.
 * @param dev, The device to get the configuration descriptor from.
 * @param conf_idx, The configuration index.
 * @return usb_conf_t*, The configuration descriptor or NULL if not found.
 */
usb_conf_t* usb_dev_get_config(usb_dev_t* dev, uint8_t conf_idx);

/**
 * @brief Get the interface from the device.
 * @param dev, The device to get the interface from.
 * @param conf_idx, The configuration index.
 * @param if_idx, The interface index.
 * @param if_alt, The interface alternate setting.
 * @return usb_if_t*, The interface or NULL if not found.
 */
usb_if_t* usb_dev_get_if(usb_dev_t* dev, uint8_t conf_idx, uint8_t if_idx, uint8_t if_alt);

/**
 * @brief Get the interface group from the device.
 * @param dev, The device to get the interface group from.
 * @param conf_idx, The configuration index.
 * @param if_idx, The interface index.
 * @return usb_if_group_t*, The interface group or NULL if not found.
 */
usb_if_group_t* usb_dev_get_if_grp(usb_dev_t* dev, uint8_t conf_idx, uint8_t if_idx);

/**
 * @brief Get the interface from the configuration.
 * @param conf, The configuration to get the interface from.
 * @param if_idx, The interface index.
 * @param if_alt, The interface alternate setting.
 * @return usb_if_t*, The interface or NULL if not found.
 */
usb_if_t* usb_conf_get_if(usb_conf_t* conf, uint8_t if_idx, uint8_t if_alt);

/**
 * @brief Get the interface group from the configuration.
 * @param conf, The configuration to get the interface group from.
 * @param if_idx, The interface index.
 * @return usb_if_group_t*, The interface group or NULL if not found.
 */
usb_if_group_t* usb_conf_get_if_grp(usb_conf_t* conf, uint8_t if_idx);

/**
 * @brief Get the endpoint from the device.
 * @param dev, The device to get the endpoint from.
 * @param conf_idx, The configuration index.
 * @param if_idx, The interface index.
 * @param if_alt, The interface alternate setting.
 * @param ep_idx, The endpoint index.
 * @param ep_dir, The endpoint direction.
 * @return usb_ep_t*, The endpoint or NULL if not found.
 */
usb_ep_t* usb_dev_get_ep(usb_dev_t* dev, uint8_t conf_idx, uint8_t if_idx, uint8_t if_alt,
    uint8_t ep_idx, uint8_t ep_dir);

/**
 * @brief Get the endpoint from the interface.
 * @param interface, The interface to get the endpoint from.
 * @param ep_idx, The endpoint index.
 * @param ep_dir, The endpoint direction.
 * @return usb_ep_t*, The endpoint or NULL if not found.
 */
usb_ep_t* usb_if_get_ep(usb_if_t* interface, uint8_t ep_idx, uint8_t ep_dir);

/**
 * @brief Get the string descriptor from the device.
 * @param dev, The device to get the string descriptor from.
 * @param str_idx, The string index.
 * @return usb_string_desc_t*, The string descriptor or NULL if not found.
 */
usb_string_desc_t* usb_dev_get_str(usb_dev_t* dev, uint8_t str_idx);