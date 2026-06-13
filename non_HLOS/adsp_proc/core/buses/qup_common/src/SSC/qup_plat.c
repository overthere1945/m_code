/**
    @file  qup_plat.c
    @brief platform implementation
 */
/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
06/26/24   GKR     Generalized QUP GPIO Handles
*/
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/


/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/
#include "qup_plat_ssc_internal.h"
#include "qurt.h"
#include "qurt_island.h"
#include "ClockDefs.h"
#include "err.h"
#include <stringl/stringl.h>
#include "DDIPlatformInfo.h"
#include "utimer.h"
#include "qup_alloc.h"
#include "island_user.h"
#include "uSleep_islands.h"
#include "qurt_isr.h"
#include "GPIO.h"

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/


/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/


/*==================================================================================================
                                          EXTERN VARIABLES
==================================================================================================*/
extern GPIOClientHandleType  qup_gpio_handle[];
/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/


void *ssc_qup_timer_init(se_dev_cfg *dcfg, void (*timer_cb) (void *), void *data)
{
    utimer_error_type         t_error;
    utimer_type              *timer_handle = NULL;
    qup_timer_callback_data  *cb_container;
    uint16 se_island_spec = 0;
    
    timer_handle =  (utimer_type *)qup_mem_alloc (dcfg, sizeof(utimer_type), OS_TIMER_TYPE);
    cb_container  =  (qup_timer_callback_data *)qup_mem_alloc (dcfg, sizeof(qup_timer_callback_data), OS_TIMER_CB_DATA_TYPE);
    
    if((timer_handle == NULL) || (cb_container == NULL))
    {
        return NULL;
    }


    cb_container->cb_data = data;
    cb_container->cb_func = timer_cb;

#ifdef QUP_ENABLE_ISLAND
    se_island_spec = qup_common_get_island_spec(dcfg);
    if(se_island_spec != 0)
    {
        t_error = utimer_def_osal_ex(timer_handle, UTIMER_FUNC1_CB_TYPE, &internal_qup_timer_expiry, (unsigned long)cb_container, se_island_spec);
    }
    else
    {
        t_error = utimer_def_osal(timer_handle, UTIMER_FUNC1_CB_TYPE, &internal_qup_timer_expiry, (unsigned long)cb_container);
    }
#else
    t_error = utimer_def_osal(timer_handle, UTIMER_FUNC1_CB_TYPE, &internal_qup_timer_expiry, (unsigned long)cb_container);
#endif

    if(UTE_SUCCESS == t_error)
    {
        return (void *) timer_handle;
    }
    else
    {
        return NULL;
    }

}

boolean  ssc_qup_timer_deinit(se_dev_cfg *dcfg, void *handle)
{
    utimer_type *timer_handle= (utimer_type *)handle;
    utimer_error_type t_error;
    
    t_error = utimer_undef(timer_handle);
    if(UTE_SUCCESS == t_error)
    {
        return TRUE;
    }
    else
    {
        QUP_LOG(LEVEL_ERROR, "qup_timer_deinit : timer undef fail 0x%x", handle);
        return FALSE;
    }
}

void   ssc_qup_gpio_dump_stat (se_dev_cfg *config)
{
    int i;
    GPIOResult   res;

    GPIOConfigType      curr_cfg;
    GPIOValueType       curr_val;
    GPIOKeyType         gpio_key;
    
    for(i = (MAX_IO_PADS - 1); i >= 0; i--)
    {
        if(config->io[i].valid)
        {
            res = GPIO_RegisterPinExplicit(qup_gpio_handle[SSC_QUP], config->io[i].pin_no, GPIO_ACCESS_SHARED, &gpio_key);
            if (GPIO_SUCCESS == res) 
            {
                res = GPIO_GetPinConfig(qup_gpio_handle[SSC_QUP], gpio_key, &curr_cfg);
                if (GPIO_SUCCESS == res) 
                {
                        QUP_SE_LOG(config,LEVEL_ERROR, "ssc_qup_gpio_dump_stat : GPIO_GetPinConfig  GPIO:%d CFG:0x%x ", config->io[i].pin_no, curr_cfg); 
                        QUP_SE_LOG(config,LEVEL_ERROR, "ssc_qup_gpio_dump_stat : func: %d, dir : %d, pull : %d, drive : %d, strongpull : %d",
                                   curr_cfg.func, curr_cfg.dir, curr_cfg.pull, curr_cfg.drive, curr_cfg.strongpull); 
                }
                else
                {
                    QUP_SE_LOG(config,LEVEL_ERROR, "ssc_qup_gpio_dump_stat : GPIO_GetPinConfig failed with res : %d", res); 
                    return;
                }

                res = GPIO_ReadPin(qup_gpio_handle[SSC_QUP], gpio_key, &curr_val);
                if (GPIO_SUCCESS == res)
                {
                    QUP_SE_LOG(config,LEVEL_ERROR, "ssc_qup_gpio_dump_stat : GPIO_ReadPin  GPIO:%d VAL:0x%x ", config->io[i].pin_no, curr_val);
                }
                else
                {
                    QUP_SE_LOG(config,LEVEL_ERROR, "ssc_qup_gpio_dump_stat : GPIO_ReadPin failed with res : %d", res);
                    return;
                }
            }
            else
            {
                QUP_SE_LOG(config,LEVEL_ERROR, "ssc_qup_gpio_dump_stat : GPIO_RegisterPinExplicit failed with res : %d", res);
                return;
            }
        }
    }
}
