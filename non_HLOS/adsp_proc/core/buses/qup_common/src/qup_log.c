/**
    @file  qup_log.c
    @brief Logging implementation
 */
/*
===============================================================================

Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
10/07/24   GKR     Added new input argument level to api qup_ulog_log()
07/12/24   gkr     Corrected incorrect datatype comparision to avoid warnings

*/
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/


/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/
#include "qup_log.h"
#include "qup_devcfg.h"
#include "qup_common.h"
#include "ULogFront.h"
#include "qurt_island.h"
#include <stdio.h>

#define QUP_LOG_NAME_SIZE              16
#define QUP_B_LOG_SIZE                 2048
#define QUP_U_LOG_SIZE                 1024


/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/


/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/


/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/

ULogHandle comm_log_handle;

#ifdef QUP_ENABLE_ISLAND
#include "micro_ULog.h"
#include "micro_diagbuffer.h"

extern micro_ULogHandle uLog_handle;
extern uint8 qup_uulog_buffer [];
#endif

/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/

#ifdef QUP_ENABLE_ISLAND
static boolean qup_comm_uulog_init (void)
{
    uint32                     actual_buf_size;
    micro_diagbuffer_result    res;

    res = micro_diagbuffer_create(&uLog_handle,
                                "uQUP_COMMON",
                                (char*) qup_uulog_buffer,
                                &actual_buf_size,
                                MICRO_DIAGBUFFER_MEM_NEEDED(QUP_U_LOG_SIZE),
                                MICRO_DIAGBUFFER_LOCKABLE);
    
    if ((res != MICRO_DIAGBUFFER_SUCCESS) || (actual_buf_size == 0))
    {
        return FALSE;
    }

    return TRUE;
}
#endif



boolean qup_comm_ulog_init (void)
{
    ULogResult          res;

    res = ULogFront_RealTimeInit(&comm_log_handle,
                                 "QUP_COMMON",
                                 2048,
                                 ULOG_MEMORY_LOCAL,
                                 ULOG_LOCK_OS);
    if (res)
    {
        return FALSE;
    }
    
#ifdef QUP_ENABLE_ISLAND
    qup_comm_uulog_init();
#endif

    return TRUE;
}



static boolean qup_ulog_init (se_dev_cfg  *dcfg)
{
    ULogResult          res;
    char                log_name[ULOG_MAX_NAME_SIZE];

    snprintf(log_name, ULOG_MAX_NAME_SIZE, "%s_QUP_%d_SE_%d", QUP_name[GET_QUP_MAJOR(dcfg->qup_data->qup_id)], GET_QUP_MINOR(dcfg->qup_data->qup_id), dcfg->se_id);


    res = ULogFront_RealTimeInit(((ULogHandle *)&(dcfg->log_handle)),
                                 log_name,
                                 QUP_B_LOG_SIZE,
                                 ULOG_MEMORY_LOCAL,
                                 ULOG_LOCK_OS);
    if (res)
    {
        return FALSE;
    }
    
    return TRUE;
}


void qup_ulog_log (void *cfg, uint8 level, uint32 data_count, const char *format_str, va_list args)
{
    se_dev_cfg  *dcfg = cfg;

    if(dcfg)
    {
        if(!(dcfg->log_handle))
        {
            qup_ulog_init(dcfg);
        }
        if (QUP_IS_SET(dcfg->log_level, level))
        { 
            ULogFront_RealTimeVprintf ((ULogHandle)(dcfg->log_handle),
                                    data_count,
                                    format_str,
                                    args);
        }
    }
    else
    {
        ULogFront_RealTimeVprintf (comm_log_handle,
                                   data_count,
                                   format_str,
                                   args);
    }
}



