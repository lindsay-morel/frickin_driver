/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include "memx_list.h"
#include "memx_common.h"

#include <stdlib.h>
/***************************************************************************//**
 * implementation
 ******************************************************************************/
MemxList *memx_list_create(void)
{
    MemxList *list = (MemxList *)malloc(sizeof(MemxList));
    if (!list) { return NULL; }

    // create mutex
    if (platform_mutex_create(&list->guard, NULL)) { free(list); list = NULL; return NULL; }
    // initialize variables
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;

    return list;
}

void memx_list_destroy(MemxList *list)
{
    // skip if is nullptr
    if (!list) { return; }

    // release all list-nodes within list first
    platform_mutex_lock(&list->guard);
    MemxListNode *node = list->head;
    while (node) {
        MemxListNode *next_node = node->next;
        free(node);
        node = NULL;
        node = next_node;
    }
    platform_mutex_unlock(&list->guard);
    // destroy the list-head given
    platform_mutex_destory(&list->guard);
    free(list);
    list = NULL;
}

void memx_list_clear(MemxList *list)
{
    // skip if is nullptr
    if (!list) { return; }

    // release all list-nodes and reset list-head variables
    platform_mutex_lock(&list->guard);
    MemxListNode *node = list->head;
    while (node) {
        MemxListNode *next_node = node->next;
        free(node);
        node = NULL;
        node = next_node;
    }
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    platform_mutex_unlock(&list->guard);
}

int memx_list_push(MemxList *list, void *data)
{
    // skip if list is nullptr
    if (!list || !data) { return 1; }
    // counter wrap around
    if (list->count + 1 < 0) { return 2; }

    // create a new list-node
    MemxListNode *node = (MemxListNode *)malloc(sizeof(MemxListNode));
    if (!node) { return 3; }

    node->data = data;
    node->next = NULL;

    // push list-node to tail
    platform_mutex_lock(&list->guard);
    if (list->count == 0) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
    list->count++;
    platform_mutex_unlock(&list->guard);
    return 0;
}

void *memx_list_pop(MemxList *list)
{
    // skip if list is nullptr
    if (!list) { return NULL; }

    // we need to make sure no other one access list at the same time
    platform_mutex_lock(&list->guard);
    // skip if no data case
    if (list->count == 0) {
        platform_mutex_unlock(&list->guard);
        return NULL;
    }

    // we pop from head(i.e first node)
    MemxListNode *node = list->head;
    if (!node) {
        platform_mutex_unlock(&list->guard);
        return NULL;
    }
    // move the secod node to head
    list->head = node->next;
    // check the only one node case(head == tail)
    if (list->tail == node) { list->tail = NULL; }

    list->count--;
    platform_mutex_unlock(&list->guard);

    // get user data and release node
    void *data = node->data;
    free(node);
    node = NULL;
    return data;
}

int memx_list_count(MemxList *list)
{
    int count = 0;
    if (!list) { return 0; }
    platform_mutex_lock(&list->guard);
    count = list->count;
    platform_mutex_unlock(&list->guard);
    return count;
}

void *memx_list_front(MemxList *list)
{
    void *data = NULL;
    if (!list || !list->head) { return NULL; }
    platform_mutex_lock(&list->guard);
    if (list->head) {
        data = list->head->data;
    }
    platform_mutex_unlock(&list->guard);
    return data;
}

void *memx_list_peek(MemxList *list, int zero_based_index)
{
    void *data = NULL;
    if (!list || zero_based_index < 0) { return NULL; }
    platform_mutex_lock(&list->guard);
    if (zero_based_index >= list->count) {
        platform_mutex_unlock(&list->guard);
        return NULL;
    }

    MemxListNode *node = list->head;
    for (int idx = 0; idx < zero_based_index; ++idx) { node = node->next; }
    if (node) {data = node->data;}
    platform_mutex_unlock(&list->guard);
    return data;
}

int memx_list_exists(MemxList *list, void *data)
{
    // 1: exist  0 : non-exist
    if (!list || !data) { return 0; }
    platform_mutex_lock(&list->guard);
    MemxListNode *node = list->head;
    while (node) {
        if (node->data == data) { return 1; }
        node = node->next;
    }
    platform_mutex_unlock(&list->guard);
    return 0;
}

void *memx_list_remove(MemxList *list, void *data)
{
    // skip if list is nullptr
    if (!list || !data) { return NULL; }
    platform_mutex_lock(&list->guard);

    // skip if no data
    if (list->count == 0) {
        platform_mutex_unlock(&list->guard);
        return NULL;
    }

    MemxListNode *node_need_to_remove = NULL;
    // search for the 1st match data
    if (list->count == 1) {
        if (list->head->data != data) {
            // case only one node but not our target
            platform_mutex_unlock(&list->guard);
            return NULL;
        }
        // case only one node and it is our target
        node_need_to_remove = list->head;
        list->head = NULL;
        list->tail = NULL;
        list->count = 0;
    } else {
        // case multiple nodes and the 1st one is our target
        if (list->head->data == data) {
            node_need_to_remove = list->head;
            list->head = node_need_to_remove->next;
            list->count--;
        } else {
            // case multiple nodes and need to find our target exists or not
            MemxListNode *prev = list->head; // point to the first node
            node_need_to_remove = prev->next; // point to the second node
            while (node_need_to_remove && node_need_to_remove->data != data) {
                prev = node_need_to_remove;
                node_need_to_remove = node_need_to_remove->next;
            }
            if (node_need_to_remove) { // only adjust the list when target is found
                prev->next = node_need_to_remove->next;
                list->count--;
            }
        }
    }
    platform_mutex_unlock(&list->guard);
    // get user data and release node
    void *udata = NULL;
    if (node_need_to_remove) { udata = node_need_to_remove->data; free(node_need_to_remove);  node_need_to_remove = NULL; }
    return udata;
}

