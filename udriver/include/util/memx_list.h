/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_LIST_H_
#define MEMX_LIST_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "memx_platform.h"

typedef struct _MemxRingBufferData
{
  void *buffer;     /* data buffer */
  uint32_t length;	/* data length */
} MemxRingBufferData;


/**
 * @brief The link node here stores user data with a pointer to next node as
 * basic linked-list structure.
 */
typedef struct _MemxListNode
{
  void *data; /* user data */
  struct _MemxListNode *next; /* pointer to next node */
} MemxListNode;

/**
 * @brief The list head stores no user data but total node number, head and
 * tail pointer for basic linked-list operations.
 */
typedef struct _MemxList
{
  MemxListNode *head;
  MemxListNode *tail;
  int32_t count;
  platform_mutex_t guard;
} MemxList;

/**
 * @brief Allocate list head and initialize all the variables. Always remember
 * use 'memx_list_destroy' to clean-up the resources allocated later.
 *
 * @return list object new allocated
 */
MemxList* memx_list_create(void);

/**
 * @brief Release all the list nodes stored within list and destroy the given
 * list-head.
 *
 * @param list                list object
 *
 * @return none
 */
void memx_list_destroy(MemxList *list);

/**
 * @brief Release all the list node stored within list but will not destroy
 * the given list-head.
 *
 * @param list                list object
 *
 * @return none
 */
void memx_list_clear(MemxList *list);

/**
 * @brief Create a new list-node which stores user data and add to tail of the
 * list.
 *
 * @param list                list object
 * @param data                user data
 *
 * @return 0 on success, otherwise -1
 */
int32_t memx_list_push(MemxList *list, void *data);

/**
 * @brief Pop out the first node from the list, will destroy the list-node and
 * return the user data stored only.
 *
 * @param list                list object
 *
 * @return user data
 */
void *memx_list_pop(MemxList *list);

/**
 * @brief Get total number of list-nodes which currently stores within list.
 *
 * @param list                list object
 *
 * @return number of nodes stored within list
 */
int32_t memx_list_count(MemxList *list);

/**
 * @brief Returns the first node from the list, but will NOT pop-out it from
 * list.
 *
 * @param list                list object
 *
 * @return user data
 */
void *memx_list_front(MemxList *list);

/**
 * @brief Returns the node at specified index from list. Will NOT pop-out it
 * from list.
 *
 * @param list                list object
 * @param index               index of the object to get
 *
 * @return user data
 */
void *memx_list_peek(MemxList *list, int32_t index);

/**
 * @brief Search if specific data user is already stored within list.
 *
 * @param list                list object
 * @param data                user data
 *
 * @return 1 if user data exists, otherwise 0
 */
int32_t memx_list_exists(MemxList *list, void *data);

/**
 * @brief Remove the first match data from list.
 *
 * @param list                list object
 * @param data                user data
 *
 * @return user data on success, otherwise nullptr
 */
void *memx_list_remove(MemxList *list, void *data);


#ifdef __cplusplus
}
#endif

#endif /* MEMX_LINKLIST_H_ */

