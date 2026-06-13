/*==============================================================================
                 Copyright (c) 2025-2026 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include "err.h"
#include "pdtsw_worker_thread.h"

#if !defined(__FILENAME__)
/**
 * @brief Find the base filename using __FILE__ macro.
 * Fall back to generic run-time computation for other compilers.
 *
 */
#include <string.h>
#define __FILENAME__                                                           \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define PDTSW_ERR_SUCCESS                              (0)

/* Define the stack for PDTSW default workqueue */
K_THREAD_STACK_DEFINE(pdtsw_default_workq_stack, CONFIG_PDTSW_DEFAULT_WORKQ_STACKSIZE);

/* Define the stack for PDTSW garbage collector workqueue */
K_THREAD_STACK_DEFINE(pdtsw_gc_workq_stack, CONFIG_QC_PDTSW_GC_WORKQ_STACKSIZE);

/**
 * Workqueue data
 */
typedef struct
{
    pdtsw_workq_t workq_handle; /* Ensure to keep this as the first element always */
    pdtsw_heap_type_t heap_type;
} pdtsw_workq_data_t;

/**
 * Default workqueue handle
 */
static pdtsw_workq_data_t *pdtsw_workq_default;

/**
 * Garbage collector workqueue handle
 */
static pdtsw_workq_data_t *pdtsw_gc_workq;

/**
 * Workqueue handler arguments.
 */
typedef struct
{
    pdtsw_work_item_t work;
    void *msg;
} pdtsw_workq_handler_args_t;

/*
* Data structure for garbage collector FIFO
*/
typedef struct
{
    void *fifo_reserved; /* mandatory requirement from zephyr */
    void *data_ptr;
    pdtsw_heap_type_t heap_type;
} pdtsw_workq_gc_fifo_data_t;

/*
* Garbage collector context
*/
typedef struct
{
    pdtsw_work_item_t work;
    struct k_fifo ptr_fifo;
} pdtsw_workq_gc_t;

/*
* Pointer to the workqueue garbage collector context
*/
pdtsw_workq_gc_t *pdtsw_workq_gc_context;

/**
 * Work handler for the garbage collector
 *
 * @param[in] p_work_item Pointer to the work item
 *
 * @return None
 */
static void pdtsw_workq_gc_process(pdtsw_work_item_t *p_work_item)
{
    ARG_UNUSED(p_work_item);

    while(!k_fifo_is_empty(&pdtsw_workq_gc_context->ptr_fifo))
    {
        pdtsw_workq_gc_fifo_data_t *p_data = \
            (pdtsw_workq_gc_fifo_data_t*)k_fifo_get(&pdtsw_workq_gc_context->ptr_fifo, K_NO_WAIT);
        if(p_data != NULL)
        {
            pdtsw_heap_free(p_data->heap_type, p_data->data_ptr);
            pdtsw_heap_free(p_data->heap_type, p_data);
        }
    }

    return;
}

/**
 * Initialize default workqueue.
 * Function is invoked during system initialization.
 * @param none
 */
int pdtsw_workq_init(void)
{
    /* initialize the productsw worker thread */
    pdtsw_workq_default =
        (pdtsw_workq_data_t*) pdtsw_heap_alloc(PDTSW_COMMON_HEAP, sizeof(pdtsw_workq_data_t));
    if (!pdtsw_workq_default)
    {
        ERR_FATAL("pdtsw_workq_default alloc failed\n", 0, 0, 0);
    }

    pdtsw_workq_default->heap_type = PDTSW_COMMON_HEAP;

    k_work_queue_init(&pdtsw_workq_default->workq_handle);
    const struct k_work_queue_config pdtsw_workq_default_cfg = {
        .name = "PDTSW Worker Thread",
        .no_yield = false,
        .essential = false
    };

    k_work_queue_start(&pdtsw_workq_default->workq_handle, pdtsw_default_workq_stack,
        K_THREAD_STACK_SIZEOF(pdtsw_default_workq_stack),
        CONFIG_PDTSW_DEFAULT_WORKQ_THREAD_PRIORITY, &pdtsw_workq_default_cfg);

    /* intialize the pdtsw garbage collector workqueue */
    pdtsw_gc_workq =
        (pdtsw_workq_data_t*) pdtsw_heap_alloc(PDTSW_COMMON_HEAP, sizeof(pdtsw_workq_data_t));
    if (!pdtsw_gc_workq)
    {
        pdtsw_heap_free(PDTSW_COMMON_HEAP, pdtsw_workq_default);
        ERR_FATAL("pdtsw_gc_workq alloc failed\n", 0, 0, 0);
    }

    pdtsw_gc_workq->heap_type = PDTSW_COMMON_HEAP;

    pdtsw_workq_gc_context = (pdtsw_workq_gc_t*)pdtsw_heap_alloc(PDTSW_COMMON_HEAP, sizeof(pdtsw_workq_gc_t));
    if(!pdtsw_workq_gc_context)
    {
        pdtsw_heap_free(PDTSW_COMMON_HEAP, pdtsw_workq_default);
        pdtsw_heap_free(PDTSW_COMMON_HEAP, pdtsw_gc_workq);
        ERR_FATAL("pdtsw_workq_gc_context alloc failed\n", 0, 0, 0);
    }

    k_work_init(&pdtsw_workq_gc_context->work, pdtsw_workq_gc_process);

    k_fifo_init(&pdtsw_workq_gc_context->ptr_fifo);

    k_work_queue_init(&pdtsw_gc_workq->workq_handle);
    const struct k_work_queue_config pdtsw_gc_workq_cfg = {
        .name = "PDTSW GC Thread",
        .no_yield = false,
        .essential = false
    };

    k_work_queue_start(&pdtsw_gc_workq->workq_handle, pdtsw_gc_workq_stack,
        K_THREAD_STACK_SIZEOF(pdtsw_gc_workq_stack),
        CONFIG_QC_PDTSW_GC_WORKQ_THREAD_PRIORITY, &pdtsw_gc_workq_cfg);

    return PDTSW_ERR_SUCCESS;
}

pdtsw_workq_ctx_t* pdtsw_custom_workq_init(pdtsw_heap_type_t heap_type,
    k_thread_stack_t *p_thread_stack, size_t thread_stack_size,
    int32_t thread_priority, const struct k_work_queue_config *cfg)
{
    pdtsw_workq_data_t *pdtsw_workq_custom = (pdtsw_workq_data_t*) pdtsw_heap_alloc(heap_type,
        sizeof(pdtsw_workq_data_t));
    if (!pdtsw_workq_custom)
    {
        return NULL;
    }

    pdtsw_workq_custom->heap_type = heap_type;

    k_work_queue_init(&pdtsw_workq_custom->workq_handle);
    k_work_queue_start(&pdtsw_workq_custom->workq_handle, p_thread_stack,
        thread_stack_size, thread_priority, cfg);

    return (pdtsw_workq_ctx_t)pdtsw_workq_custom;
}

void pdtsw_custom_workq_deinit(pdtsw_workq_ctx_t *workq_ctx)
{
    //TODO: stop api not supported. Need to check if there is other way to deinit workq
    //k_work_queue_stop(&workq_data->workq_handle, K_FOREVER);
}

void pdtsw_workq_deinit(void)
{
    //TODO: stop api not supported. Need to check if there is other way to deinit workq
    //k_work_queue_stop(pdtsw_workq_default, K_FOREVER);
}

void* pdtsw_allocate_handler_args(int32_t size, pdtsw_workq_ctx_t *workq_ctx)
{
    pdtsw_workq_data_t *pdtsw_workq = workq_ctx
                                        ? (pdtsw_workq_data_t*) workq_ctx
                                        : pdtsw_workq_default;

    /* Allocates memory for both handler arg container and msg data */
    int8_t *ptr =
        (int8_t*) pdtsw_heap_alloc(pdtsw_workq->heap_type,
            sizeof(pdtsw_workq_handler_args_t) + size);
    if (!ptr)
    {
        return NULL;
    }
    else
    {
        /* Returns pointer to the msg data to be appended */
        return (void*)(ptr + sizeof(pdtsw_workq_handler_args_t));
    }
}

int pdtsw_submit_to_workq(void *handler_function, void *handler_args,
    pdtsw_workq_ctx_t *workq_ctx)
{
    int ret = PDTSW_ERR_SUCCESS;
    pdtsw_workq_handler_args_t *args = NULL;
    pdtsw_workq_data_t *pdtsw_workq = workq_ctx
                                        ? (pdtsw_workq_data_t*) workq_ctx
                                        : pdtsw_workq_default;

    if(handler_function)
    {
        /* Get the pointer to handler arg container */
        args = handler_args
        ? ((pdtsw_workq_handler_args_t*)(((int8_t*)handler_args) - \
            sizeof(pdtsw_workq_handler_args_t)))
        : (pdtsw_workq_handler_args_t*)pdtsw_heap_alloc(pdtsw_workq->heap_type,
            sizeof(pdtsw_workq_handler_args_t));
        if (!args)
        {
            ret = -ENOSR;
        }
        else
        {
            /* Point to the message data within handler arg container */
		    args->msg = handler_args;
            k_work_init(&args->work, handler_function);
            ret = k_work_submit_to_queue(&pdtsw_workq->workq_handle, &args->work);
            if((ret >= 0) && (ret <= 2))
            {
                ret = PDTSW_ERR_SUCCESS;
            }
        }
    }
    else
    {
        ret = -EINVAL;
    }

    /* Free handler args memory upon failure to submit the work item on to the work queue */
    if (PDTSW_ERR_SUCCESS != ret && NULL != args)
    {
        pdtsw_heap_free(pdtsw_workq->heap_type, args);
    }

    return ret;
}

void* pdtsw_get_workq_handler_args(pdtsw_work_item_t *item)
{
    pdtsw_workq_handler_args_t *data = CONTAINER_OF(item, pdtsw_workq_handler_args_t, work);

    return data->msg;
}

void pdtsw_free_handler_args(pdtsw_work_item_t *item, pdtsw_workq_ctx_t *workq_ctx)
{
    pdtsw_workq_data_t *pdtsw_workq = workq_ctx
                                        ? (pdtsw_workq_data_t*) workq_ctx
                                        : pdtsw_workq_default;
    pdtsw_workq_handler_args_t *data = CONTAINER_OF(item, pdtsw_workq_handler_args_t, work);

    /* allocate a garbage collector FIFO entry */
    pdtsw_workq_gc_fifo_data_t *p_fifo_data = (pdtsw_workq_gc_fifo_data_t*)pdtsw_heap_alloc(
        pdtsw_workq->heap_type, sizeof(pdtsw_workq_gc_fifo_data_t));
    if(p_fifo_data == NULL)
    {
        /* fallback to immediate cleanup if GC FIFO entry allocation fails */
        pdtsw_heap_free(pdtsw_workq->heap_type, data);
        ERR_FATAL("PDTSW GC FIFO Entry Alloc Failed\n", 0, 0, 0);
    }

    /* store the pointer-to-free and its heap type into the FIFO entry */
    p_fifo_data->data_ptr = (void*)data;
    p_fifo_data->heap_type = pdtsw_workq->heap_type;

    /* queue this entry in the garbage collector FIFO */
    k_fifo_put(&pdtsw_workq_gc_context->ptr_fifo, p_fifo_data);

    /* Schedule the garbage collection work for deferring freeing of the handler args */
    int ret = k_work_submit_to_queue(&pdtsw_gc_workq->workq_handle, &pdtsw_workq_gc_context->work);
    if(ret < 0)
    {
        ERR_FATAL("Work Item Submit Failed: %d\n", ret, 0, 0);
    }
    
    return;
}
