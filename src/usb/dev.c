#include "dev.h"

usb_dev_t usb_dev_create(usb_dev_desc_t* desc, lang_id_t lang_id)
{
    usb_dev_t dev = {
        .desc = *desc,
        .configurations = NULL,
        .strings = NULL,
        .cur_config = -1,
        .lang_id = lang_id,
    };

    dev.desc.bNumConfigurations = 0;

    return dev;
}

int usb_dev_add_config(usb_dev_t* dev, usb_conf_t* conf)
{
    if (dev == NULL || conf == NULL)
    {
        return 0;
    }

    conf->cur_if = -1;
    conf->desc.bNumInterfaces = 0;
    conf->next = NULL;
    if (dev->configurations == NULL)
    {
        dev->configurations = conf;
        dev->desc.bNumConfigurations = 1;
    }
    else
    {
        usb_conf_t* cur = dev->configurations;

        while (cur->next != NULL)
        {
            cur = cur->next;
        }

        cur->next = conf;

        dev->desc.bNumConfigurations++;
    }

    return 1;
}

int usb_conf_add_if_grp(usb_conf_t* conf, usb_if_group_t* if_group)
{
    if (conf == NULL || if_group == NULL)
    {
        return 0;
    }

    if_group->next = NULL;

    if (conf->interfaces == NULL)
    {
        conf->interfaces = if_group;
        conf->desc.bNumInterfaces += if_group->if_count;
    }
    else
    {
        usb_if_group_t* cur = conf->interfaces;

        while (cur->next != NULL)
        {
            cur = cur->next;
        }

        cur->next = if_group;

        conf->desc.bNumInterfaces += if_group->if_count;
    }

    return 1;
}

int usb_dev_add_if_grp(usb_dev_t* dev, uint8_t conf_idx, usb_if_group_t* if_group)
{
    usb_conf_t* conf = usb_dev_get_config(dev, conf_idx);

    return usb_conf_add_if_grp(conf, if_group);
}

int usb_if_grp_add(usb_if_group_t* group, usb_if_t* interface)
{
    if (group == NULL || interface == NULL)
    {
        return 0;
    }

    interface->next = NULL;

    if (group->interfaces == NULL)
    {
        group->if_count = 1;
        group->cur_alt_set = interface->desc.bAlternateSetting;
        group->interfaces = interface;
    }
    else
    {
        usb_if_t* cur = group->interfaces;

        while (cur->next != NULL)
        {
            cur = cur->next;
        }

        cur->next = interface;
        group->if_count++;
    }

    return 1;
}

int usb_if_add_ep(usb_if_t* interface, usb_ep_t* ep)
{
    if (interface == NULL || ep == NULL)
    {
        return 0;
    }
    ep->next = NULL;

    if (interface->endpoints == NULL)
    {
        interface->endpoints = ep;
        interface->desc.bNumEndpoints++;
    }
    else
    {
        usb_ep_t* cur = interface->endpoints;

        while (cur->next != NULL)
        {
            cur = cur->next;
        }

        cur->next = ep;
        interface->desc.bNumEndpoints++;
    }

    return 1;
}

int usb_dev_add_ep(usb_dev_t* dev, uint8_t conf_idx, uint8_t if_idx, uint8_t if_alt, usb_ep_t* ep)
{
    usb_if_t* interface = usb_dev_get_if(dev, conf_idx, if_idx, if_alt);

    return usb_if_add_ep(interface, ep);
}

usb_conf_t* usb_dev_get_config(usb_dev_t* dev, uint8_t conf_idx)
{
    if (dev == NULL)
    {
        return NULL;
    }

    usb_conf_t* cur = dev->configurations;

    while (cur != NULL)
    {
        if (cur->desc.bConfigurationValue == conf_idx)
        {
            break;
        }
        cur = cur->next;
    }

    return cur;
}

usb_if_t* usb_dev_get_if(usb_dev_t* dev, uint8_t conf_idx, uint8_t if_idx, uint8_t if_alt)
{
    if (dev == NULL)
    {
        return NULL;
    }

    usb_conf_t* conf = usb_dev_get_config(dev, conf_idx);

    return usb_conf_get_if(conf, if_idx, if_alt);
}

usb_if_group_t* usb_dev_get_if_grp(usb_dev_t* dev, uint8_t conf_idx, uint8_t if_idx)
{
    usb_conf_t* conf = usb_dev_get_config(dev, conf_idx);

    return usb_conf_get_if_grp(conf, if_idx);
}

usb_if_group_t* usb_conf_get_if_grp(usb_conf_t* conf, uint8_t if_idx)
{
    if (conf == NULL)
    {
        return NULL;
    }

    usb_if_group_t* cur = conf->interfaces;

    while (cur != NULL)
    {
        if (cur->interfaces != NULL && cur->interfaces->desc.bInterfaceNumber == if_idx)
        {
            break;
        }
        cur = cur->next;
    }

    return cur;
}

usb_if_t* usb_conf_get_if(usb_conf_t* conf, uint8_t if_idx, uint8_t if_alt)
{
    if (conf == NULL)
    {
        return NULL;
    }

    usb_if_group_t* cur = conf->interfaces;

    while (cur != NULL)
    {
        if (cur->cur_alt_set == if_alt)
        {
            usb_if_t* cur_if = cur->interfaces;

            while (cur_if != NULL)
            {
                if (cur_if->desc.bInterfaceNumber == if_idx
                    && cur_if->desc.bAlternateSetting == if_alt)
                {
                    return cur_if;
                }
                cur_if = cur_if->next;
            }
        }

        cur = cur->next;
    }

    return NULL;
}

usb_ep_t* usb_dev_get_ep(usb_dev_t* dev, uint8_t conf_idx, uint8_t if_idx, uint8_t if_alt,
    uint8_t ep_idx, uint8_t ep_dir)
{
    if (dev == NULL)
    {
        return NULL;
    }

    usb_if_t* interface = usb_dev_get_if(dev, conf_idx, if_idx, if_alt);

    return usb_if_get_ep(interface, ep_idx, ep_dir);
}

usb_ep_t* usb_if_get_ep(usb_if_t* interface, uint8_t ep_idx, uint8_t ep_dir)
{
    if (interface == NULL)
    {
        return NULL;
    }

    usb_ep_t* cur = interface->endpoints;

    while (cur != NULL)
    {
        if (cur->desc.ep_nb == ep_idx && cur->desc.dir == ep_dir)
        {
            break;
        }
        cur = cur->next;
    }

    return cur;
}

usb_string_desc_t* usb_dev_get_str(usb_dev_t* dev, uint8_t str_idx)
{
    usb_string_desc_t* str = dev->strings;

    while (str != NULL)
    {
        if (str->idx == str_idx)
        {
            return str;
        }
        str = str->next;
    }
    return NULL;
}