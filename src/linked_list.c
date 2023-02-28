#include <errno.h>
#include <stdint.h>

#include "linked_list.h"

int linked_list_init(alloc_fn allocator, free_fn free, linked_list_t* list)
{
    if (allocator == NULL || free == NULL || list == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    list->size      = 0;
    list->first     = NULL;
    list->last      = NULL;
    list->allocator = allocator;
    list->free      = free;

    return 0;
}

int linked_list_push(linked_list_t* list, void* data)
{
    node_t* new_node = list->allocator(sizeof(node_t));

    if (new_node == NULL)
    {
        return -1;
    }

    if (list->size == 0)
    {
        new_node->next = NULL;
        new_node->prev = NULL;
        list->first    = new_node;
    }
    else
    {
        new_node->next   = NULL;
        list->last->next = new_node;
        new_node->prev   = list->last;
    }

    new_node->data = data;

    list->last     = new_node;
    list->size++;

    return 0;
}

node_t* linked_list_find(linked_list_t* list, size_t i)
{
    if (i < list->size)
    {
        uint8_t direction = 0;
        node_t* cur;
        if (i >= list->size / 2)
        {
            cur       = list->last;
            i         = list->size - i - 1;
            direction = 1;
        }
        else
        {
            cur = list->first;
        }

        for (; i > 0; --i)
        {
            if (direction)
            {
                cur = cur->prev;
            }
            else
            {
                cur = cur->next;
            }
        }

        return cur;
    }

    return NULL;
}

void* linked_list_get(linked_list_t* list, size_t i)
{
    node_t* node = linked_list_find(list, i);

    if (node != NULL)
    {
        return node->data;
    }

    return NULL;
}

void* linked_list_rem(linked_list_t* list, size_t i)
{
    node_t* node = linked_list_find(list, i);

    if (node != NULL)
    {
        if (node->prev != NULL)
        {
            node->prev->next = node->next;
        }

        if (node->next != NULL)
        {
            node->next->prev = node->prev;
        }

        if (i == list->size - 1)
        {
            list->last = node->prev;
        }
        else if (i == 0)
        {
            list->first = node->next;
        }

        list->size--;

        node->next = NULL;
        node->prev = NULL;

        void* data = node->data;

        list->free(node);

        return data;
    }

    return NULL;
}

void linked_list_iter(linked_list_t* list, iter_cb_t iter_cb, void* ctx)
{
    node_t* cur  = list->first;
    node_t* last = NULL;
    for (size_t i = 0; i < list->size; ++i)
    {
        if (iter_cb(cur->data, i, ctx))
        {
            // Current pointer is invalidated.
            if (i == 0)
            {
                cur = list->first;
            }
            else if (i == 1)
            {
                cur = list->first->next;
            }
            else
            {
                cur = last->next;
            }
        }
        else
        {
            cur = cur->next;
        }

        if (cur != NULL)
        {
            last = cur->prev;
        }
    }
}