/*================================================================================================
 * @file pdtsw_heap.h
 *
 * This file contains the declarations for all the heap memory allocation APIs
 * exposed to product software services.
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===============================================================================================*/

#ifndef __PDTSW_HEAP_H__
#define __PDTSW_HEAP_H__

#include <stddef.h>
#include <stdint.h>

/* Enum to indicate type of heap */
typedef enum
{
#ifdef CONFIG_PDTSW_OFFLOAD_MGR_ENABLE
    PDTSW_OFFLOAD_MGR_HEAP,
#endif
#ifdef CONFIG_PDTSW_RSB_SERVICE_ENABLE
    PDTSW_RSB_SVC_HEAP,
#endif
#if defined(CONFIG_PDTSW_HAPTICS_SERVICE_ENABLE) || defined(CONFIG_PDTSW_AUDIO_SERVICE_ENABLE)
    PDTSW_AUDIO_HAPTICS_SVC_HEAP_PSRAM,
#endif

#ifdef CONFIG_PDTSW_QSH_GLINK_ROUTER_ENABLE
    PDTSW_QSH_GLINK_ROUTER_HEAP,
#endif
#ifdef CONFIG_PDTSW_QSH_DISPLAY_BRIDGE_ENABLE
    PDTSW_QSH_DISPLAY_BRIDGE_HEAP,
#endif
#if defined(CONFIG_QC_SHIM_DIAG) || defined(CONFIG_QC_DIAG_ENABLED) || (CONFIG_QC_DIAG_TRACER_PASSTHRU)
    PDTSW_DEBUG_SVC_HEAP,
#endif
    /* All services must use this heap type for glink transactions */
    PDTSW_COMMON_GLINK_HEAP,

    /* Common heap across services for misc usage */
    PDTSW_COMMON_HEAP,
    PDTSW_HEAP_TYPE_MAX
} pdtsw_heap_type_t;

/**
 * @brief Allocates the chunk of memory block for requested number of bytes
 * from the requested heap memory section type passed.
 * @param[in] heap_type The heap type to allocate from.
 *                      Should be one among pdtsw_heap_type_t enum values only
 * @param[in] num_bytes The number of bytes to be allocated.
 * @return Returns pointer to allocated memory if operation was success,
 *         otherwise NULL.
 * @note This API does not perform any memory alignment.
 */
void* pdtsw_heap_alloc(pdtsw_heap_type_t heap_type, size_t num_bytes);

/**
 * @brief Allocates the chunk of memory block for requested number of bytes
 * from the requested heap memory section type passed & initializes all bytes to zero.
 * @param[in] heap_type The heap type to allocate from.
 *                      Should be one among pdtsw_heap_type_t enum values only
 * @param[in] num_obj The number of objects to allocate.
 * @param[in] size_obj The size of each object to allocate.
 * @return Returns pointer to allocated memory if operation was success,
 *         otherwise NULL.
 * @note This API does not perform any memory alignment.
 */
void* pdtsw_heap_calloc(pdtsw_heap_type_t heap_type, size_t num_obj, size_t size_obj);

/**
 * @brief Allocates the chunk of memory block for requested number of bytes
 * from the requested heap memory section type passed.
 * @param[in] heap_type The heap type to allocate from.
 *                      Should be one among pdtsw_heap_type_t enum values only
 * @param[in] num_bytes The number of bytes to be allocated.
 * @param[in] align     Number of bytes by which to align.
 *                      Mandatory: Alignment in bytes, must be a power of two.
 * @return Returns pointer to allocated memory if operation was success, otherwise NULL.
 */
void* pdtsw_heap_aligned_alloc(pdtsw_heap_type_t heap_type, size_t num_bytes, size_t align);

/**
 * @brief Reallocates the chunk of memory block for requested number of bytes
 * from the requested heap memory section type passed.
 * @param[in] heap_type The heap type to allocate from.
 *                      Should be one among pdtsw_heap_type_t enum values only
 * @param[in] mem_p Pointer to original memory location which needs to be rellocated.
 * @param[in] num_bytes The number of bytes to be allocated during reallocation.
 * @return Returns pointer to allocated memory if operation was success,
 *         otherwise NULL.
 * @note This API does not perform any memory alignment.
 */
void* pdtsw_heap_realloc(pdtsw_heap_type_t heap_type, void *mem_p, size_t num_bytes);

 /**
 * @brief Frees the chunk of memory block.
 * @param[in] heap_type The heap type to allocate from.
 *                      Should be one among pdtsw_heap_type_t enum values only
 * @param[in] mem_p Pointer to memory location to be freed.
 * @return None
 */
void pdtsw_heap_free(pdtsw_heap_type_t heap_type, void *mem_p);

 /**
 * @brief Performs aligned memory copy from src to dst address
 * @param[in] dest      Destination address
 * @param[in] dst_size  memory size at destination address
 * @param[in] src       Source address
 * @param[in] src_size  data size to be copied to dest address
 * @return None
 */
void pdtsw_heap_aligned_memcpy(void *dest, size_t dst_size, void *src, size_t src_size);

#endif //__PDTSW_HEAP_H__
