/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#ifndef __PDTSW_WORKER_THREAD__
#define __PDTSW_WORKER_THREAD__

#include "pdtsw_heap.h"

/* Type definitions */
typedef struct k_work_q pdtsw_workq_t;
typedef struct k_work pdtsw_work_item_t;
typedef void* pdtsw_workq_ctx_t;

/**
 * Does allocation for work handler arguments and return pointer to allocated memory.
 * The handler arguments of the specified size must be copied to the memory pointed
 * to by the pointer.
 * This function is mandatory to call only if arguments need to be passed to handler function.
 * Memory will be allocated from pdtsw worker thread heap.
 * @param size Size of memory to be allocated
 * @param workq_custom workqueue handle as a reference to custom workqueue.
 * @return Pointer to allocated memory
 */
void* pdtsw_allocate_handler_args(int32_t size, pdtsw_workq_ctx_t *workq_custom);

/**
 * Submit work item to execute to workqueue.
 * @param handler_function (mandatory) Handler function to be executed.
 * @param handler_args Handle returned by pdtsw_allocate_handler_args(). NULL if no arguments to the
 *                     the handler function. NULL to be passed if no arguments.
 * @param workq_custom workqueue data as a reference to custom workqueue.
 *                     NULL to be passed if one need to use default workqueue.
 * @return 0 will be returned if submission is success else a negative value.
 */
int pdtsw_submit_to_workq(void *handler_function, void *handler_args,
    pdtsw_workq_ctx_t *workq_custom);

/**
 * Get work handler message data from work item.
 * Memory of message data need to be freed by the handler function using pdtsw_free_handler_args()
 * @param item Work handle
 * @return Pointer to message data required for work handler
 */
void* pdtsw_get_workq_handler_args(pdtsw_work_item_t *item);

/**
 * Free the memory allocated for handler args. Should be called mandatorily
 * by handler functions after fetching and using the args to avoid memory leaks.
 * Memory of message data need to be freed by the handler function.
 * @param item Work handle
 * @param workq_custom workqueue data as a reference to custom workqueue.
 *                     Mandatory arg for custom workqueues.
 *                     NULL to be passed if one need to use default workqueue.
 * @return Pointer to message data required for work handler
 */
void pdtsw_free_handler_args(pdtsw_work_item_t *item, pdtsw_workq_ctx_t *workq_custom);

/* API for services which wants to create and use custom/dedicated workqueue */
/**
 * Initialize custom workqueue.
 * @param heap_type pdtsw heap type to be used for memory allocation for workqueue.
 * @param p_thread_stack Pointer to thread stack object
 * @param thread_stack_size Thread stack size
 * @param thread_priority Priority for workqueue thread.
 * @param cfg Configuration for workqueue.
 * @return workqueue data as a reference to custom workqueue.
 */
pdtsw_workq_ctx_t* pdtsw_custom_workq_init(pdtsw_heap_type_t heap_type,
    k_thread_stack_t *p_thread_stack, size_t thread_stack_size,
    int32_t thread_priority, const struct k_work_queue_config *cfg);

/**
 * Deinitialize default workqueue.
 * @param none
 */
void pdtsw_workq_deinit(void);

/**
 * Deinitialize custom workqueue.
 * @param workq_data workqueue data as a reference to custom workqueue.
 *                   This is a mandatory arg to deinit custom workqueue.
 */
void pdtsw_custom_workq_deinit(pdtsw_workq_ctx_t *workq_data);

#endif //__PDTSW_WORKER_THREAD__