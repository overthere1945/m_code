/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#include "pdtsw_heap.h"
#include <zephyr/kernel.h>
#include <string.h>

/* Static byte array for heaps */
//TODO: All heap sizes are to be fine tuned later based on actual usage by each service.
#ifdef CONFIG_PDTSW_OFFLOAD_MGR_ENABLE
static uint8_t pdtsw_heap_mem_offload[CONFIG_PDTSW_OFFLOAD_MGR_HEAP_SIZE];
#endif
#ifdef CONFIG_PDTSW_RSB_SERVICE_ENABLE
static uint8_t pdtsw_heap_mem_rsb[CONFIG_PDTSW_RSB_SERVICE_HEAP_SIZE];
#endif
#ifdef CONFIG_PDTSW_QSH_GLINK_ROUTER_ENABLE
static uint8_t pdtsw_heap_mem_qsh[CONFIG_PDTSW_QSH_GLINK_ROUTER_HEAP_SIZE];
#endif
#ifdef CONFIG_PDTSW_QSH_DISPLAY_BRIDGE_ENABLE
static uint8_t pdtsw_heap_mem_qsh_bridge[CONFIG_PDTSW_QSH_DISPLAY_BRIDGE_HEAP_SIZE];
#endif
#if defined(CONFIG_PDTSW_HAPTICS_SERVICE_ENABLE) || defined(CONFIG_PDTSW_AUDIO_SERVICE_ENABLE)
__attribute__((section(".qc_psram_am_svc_data"))) static uint8_t pdtsw_psram_heap_mem_audio_haptics[\
    CONFIG_PDTSW_AUDIO_SERVICE_OFFLOAD_HEAP_SIZE+CONFIG_PDTSW_HAPTICS_SERVICE_OFFLOAD_HEAP_SIZE];
#endif
#if defined(CONFIG_QC_SHIM_DIAG) || defined(CONFIG_QC_DIAG_ENABLED) || (CONFIG_QC_DIAG_TRACER_PASSTHRU)
static uint8_t pdtsw_heap_mem_debug[CONFIG_PDTSW_DEBUG_SERVICE_HEAP_SIZE];
#endif

static uint8_t pdtsw_heap_mem_common[CONFIG_PDTSW_COMMON_HEAP_SIZE];
static uint8_t pdtsw_heap_mem_glink[CONFIG_PDTSW_COMMON_GLINK_HEAP_SIZE];

/* Heap info struct to maintain heap info for heap type */
typedef struct
{
    struct k_heap heap;
    void* start_addr;            /**< Starting address of memory region            */
    size_t size;                 /**< Total size of memory region                  */
    pdtsw_heap_type_t heap_type; /**< ID of heap memory region                     */
} pdtsw_heap_info_t;

/* Initialize a heap info table */
static pdtsw_heap_info_t pdtsw_heap_info_table[PDTSW_HEAP_TYPE_MAX] =
{
#ifdef CONFIG_PDTSW_OFFLOAD_MGR_ENABLE
    [PDTSW_OFFLOAD_MGR_HEAP] = {
        {},
        (void*)pdtsw_heap_mem_offload,
        CONFIG_PDTSW_OFFLOAD_MGR_HEAP_SIZE,
        PDTSW_OFFLOAD_MGR_HEAP
    },
#endif
#if defined(CONFIG_PDTSW_HAPTICS_SERVICE_ENABLE) || defined(CONFIG_PDTSW_AUDIO_SERVICE_ENABLE)
    [PDTSW_AUDIO_HAPTICS_SVC_HEAP_PSRAM] = {
        {},
        (void*)pdtsw_psram_heap_mem_audio_haptics,
        CONFIG_PDTSW_AUDIO_SERVICE_OFFLOAD_HEAP_SIZE + CONFIG_PDTSW_HAPTICS_SERVICE_OFFLOAD_HEAP_SIZE,
        PDTSW_AUDIO_HAPTICS_SVC_HEAP_PSRAM
    },
#endif
#ifdef CONFIG_PDTSW_RSB_SERVICE_ENABLE
    [PDTSW_RSB_SVC_HEAP] = {
        {},
        (void*)pdtsw_heap_mem_rsb,
        CONFIG_PDTSW_RSB_SERVICE_HEAP_SIZE,
        PDTSW_RSB_SVC_HEAP
    },
#endif
#ifdef CONFIG_PDTSW_QSH_GLINK_ROUTER_ENABLE
    [PDTSW_QSH_GLINK_ROUTER_HEAP] = {
        {},
        (void*)pdtsw_heap_mem_qsh,
        CONFIG_PDTSW_QSH_GLINK_ROUTER_HEAP_SIZE,
        PDTSW_QSH_GLINK_ROUTER_HEAP
    },
#endif
#ifdef CONFIG_PDTSW_QSH_DISPLAY_BRIDGE_ENABLE
    [PDTSW_QSH_DISPLAY_BRIDGE_HEAP] = {
        {},
        (void*)pdtsw_heap_mem_qsh_bridge,
        CONFIG_PDTSW_QSH_DISPLAY_BRIDGE_HEAP_SIZE,
        PDTSW_QSH_DISPLAY_BRIDGE_HEAP
    },
#endif
#if defined(CONFIG_QC_SHIM_DIAG) || defined(CONFIG_QC_DIAG_ENABLED) || (CONFIG_QC_DIAG_TRACER_PASSTHRU)
    [PDTSW_DEBUG_SVC_HEAP] = {
        {},
        (void*)pdtsw_heap_mem_debug,
        CONFIG_PDTSW_DEBUG_SERVICE_HEAP_SIZE,
        PDTSW_DEBUG_SVC_HEAP
    },
#endif
    [PDTSW_COMMON_GLINK_HEAP] = {
        {},
        (void*)pdtsw_heap_mem_glink,
        CONFIG_PDTSW_COMMON_GLINK_HEAP_SIZE,
        PDTSW_COMMON_GLINK_HEAP
    },
    [PDTSW_COMMON_HEAP] = {
        {},
        (void*)pdtsw_heap_mem_common,
        CONFIG_PDTSW_COMMON_HEAP_SIZE,
        PDTSW_COMMON_HEAP
    }
};

int pdtsw_heap_init(void)
{
    for (uint8_t idx = 0; idx < PDTSW_HEAP_TYPE_MAX; idx++)
    {
        if (pdtsw_heap_info_table[idx].size && pdtsw_heap_info_table[idx].start_addr)
        {
            k_heap_init(&(pdtsw_heap_info_table[idx].heap),
                            pdtsw_heap_info_table[idx].start_addr,
                            pdtsw_heap_info_table[idx].size);
        }
    }

    return 0;
}

void* pdtsw_heap_alloc(pdtsw_heap_type_t heap_type, size_t num_bytes)
{
    if (PDTSW_HEAP_TYPE_MAX <= heap_type)
    {
        return NULL;
    }
    void *mem_p = k_heap_alloc(&(pdtsw_heap_info_table[heap_type].heap),
                            num_bytes,
                            K_NO_WAIT);
    return mem_p;
}

void* pdtsw_heap_calloc(pdtsw_heap_type_t heap_type, size_t num_obj, size_t size_obj)
{
    if (PDTSW_HEAP_TYPE_MAX <= heap_type)
    {
        return NULL;
    }
    size_t num_bytes = num_obj * size_obj;
    void *mem_p = k_heap_alloc(&(pdtsw_heap_info_table[heap_type].heap),
                        num_bytes,
                        K_NO_WAIT);
    if (mem_p)
    {
        memset(mem_p, 0, num_bytes);
    }

    return mem_p;
}

void* pdtsw_heap_aligned_alloc(pdtsw_heap_type_t heap_type, size_t num_bytes, size_t align)
{
    if (PDTSW_HEAP_TYPE_MAX <= heap_type)
    {
        return NULL;
    }
    void *mem_p =
    k_heap_aligned_alloc(&(pdtsw_heap_info_table[heap_type].heap),
                            align,
                            num_bytes,
                            K_NO_WAIT);
    return mem_p;
}

void* pdtsw_heap_realloc(pdtsw_heap_type_t heap_type, void *mem_p, size_t num_bytes)
{
    if (NULL == mem_p || (PDTSW_HEAP_TYPE_MAX <= heap_type))
    {
        return NULL;
    }
    void *realloc_mem_p =
    k_heap_realloc(&(pdtsw_heap_info_table[heap_type].heap),
                            mem_p,
                            num_bytes,
                            K_NO_WAIT);

    return realloc_mem_p;
}

void pdtsw_heap_free(pdtsw_heap_type_t heap_type, void *mem_p)
{
    if (PDTSW_HEAP_TYPE_MAX <= heap_type)
    {
        return;
    }
    k_heap_free(&(pdtsw_heap_info_table[heap_type].heap), mem_p);

    return;
}

//TODO: Boundary check, memory overlap handling etc
__no_optimization void pdtsw_heap_aligned_memcpy(void *dest, size_t dst_size, void *src, size_t src_size)
{

    if ((!dest || !src || dst_size == 0 || src_size == 0) || (dst_size < src_size))
    {
        return;
    }

    uint32_t *d = (uint32_t *)dest;
    uint32_t *s = (uint32_t *)src;
    size_t words = src_size / 4;
    size_t remaining = src_size % 4;

    /* Copy word-aligned data */
    while(words > 0)
    {
        *d = *s;
        d++; s++;
        words--;
    }

    /* Copy remaining bytes */
    if (remaining > 0)
    {
        uint8_t *d8 = (uint8_t *)d;
        uint8_t *s8 = (uint8_t *)s;
        while(remaining > 0)
        {
            *d8 = *s8;
            d8++; s8++;
            remaining--;
        }
    }
}