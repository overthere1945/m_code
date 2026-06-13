/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#include <stdlib.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>

#ifdef CONFIG_PDTSW_COMMON_HEAP_MGR_ENABLE
extern int pdtsw_heap_init(void);
#endif
#ifdef CONFIG_PDTSW_COMMON_WORKER_THREAD_ENABLE
extern int pdtsw_workq_init(void);
#endif
#ifdef CONFIG_PDTSW_AUDIO_SERVICE_ENABLE
extern int32_t audio_service_init(void);
#endif
#ifdef CONFIG_PDTSW_HAPTICS_SERVICE_ENABLE
extern int32_t haptics_service_init(void);
#endif
#ifdef CONFIG_PDTSW_BATTERY_SERVICE_ENABLE
extern int battery_service_context_init(void);
#endif
#ifdef CONFIG_PDTSW_RSB_SERVICE_ENABLE
extern int rsb_service_init(void);
#endif
#ifdef CONFIG_PDTSW_OFFLOAD_MGR_ENABLE
extern int offload_mgr_init(void);
#endif
#ifdef CONFIG_QC_PDTSW_UTILITY_GLINK_ENABLE
extern int pdtsw_utility_glink_init(); 
#endif
#ifdef CONFIG_QC_PDTSW_PSRAM_ARBITER_ENABLE
extern int pdtsw_psram_arbiter_init();
#endif
#ifdef CONFIG_QC_PDTSW_PSRAM_CLIENT_ENABLE
extern int pdtsw_psram_client_init();
#endif
#if defined(CONFIG_PDTSW_AUDIO_SERVICE_ENABLE) \
    || defined(CONFIG_PDTSW_HAPTICS_SERVICE_ENABLE) \
    || defined(CONFIG_PDTSW_COMMON_UTILS_ENABLE)
extern int offload_utility_hal_init();
#endif

int pdtsw_common_init()
{
    /* Init product sw common utilities*/
#ifdef CONFIG_PDTSW_COMMON_HEAP_MGR_ENABLE
    if(pdtsw_heap_init())
    {
        return -1;
    }
#endif
#ifdef CONFIG_PDTSW_COMMON_WORKER_THREAD_ENABLE
    if(pdtsw_workq_init())
    {
        return -1;
    }
#endif
#if defined(CONFIG_PDTSW_AUDIO_SERVICE_ENABLE) \
    || defined(CONFIG_PDTSW_HAPTICS_SERVICE_ENABLE) \
    || defined(CONFIG_PDTSW_COMMON_UTILS_ENABLE)
    if(offload_utility_hal_init())
    {
        return -1;
    }
#endif
    /* Init product sw services*/
#ifdef CONFIG_PDTSW_AUDIO_SERVICE_ENABLE
    if(audio_service_init())
    {
        return -1;
    }
#endif
#ifdef CONFIG_PDTSW_HAPTICS_SERVICE_ENABLE
    if(haptics_service_init())
    {
        return -1;
    }
#endif
#ifdef CONFIG_PDTSW_BATTERY_SERVICE_ENABLE
    if(battery_service_context_init())
    {
        return -1;
    }
#endif
#ifdef CONFIG_PDTSW_RSB_SERVICE_ENABLE
    if(rsb_service_init())
    {
        return -1;
    }
#endif
#ifdef CONFIG_PDTSW_OFFLOAD_MGR_ENABLE
    if(offload_mgr_init())
    {
        return -1;
    }
#endif
#ifdef CONFIG_QC_PDTSW_PSRAM_ARBITER_ENABLE
    /* PSRAM arbiter should be initialized before utility glink */
    if(pdtsw_psram_arbiter_init())
    {
        return -1;
    }
#endif
#ifdef CONFIG_QC_PDTSW_PSRAM_CLIENT_ENABLE
    /* PSRAM arbiter client should be initialized before utility glink */
    if(pdtsw_psram_client_init())
    {
        return -1;
    }
#endif
#ifdef CONFIG_QC_PDTSW_UTILITY_GLINK_ENABLE
    if(pdtsw_utility_glink_init())
    {
        return -1;
    }
#endif

    return 0;
}

SYS_INIT(pdtsw_common_init, POST_KERNEL,  90);
