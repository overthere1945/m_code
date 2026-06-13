/**
    @file  qup_log_island.c
    @brief Logging Island implementation
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
#include "ULogFront.h"
#include "qurt_island.h"

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/



/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/


/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/

#ifdef QUP_ENABLE_ISLAND
#include "micro_ULog.h"
#include "micro_diagbuffer.h"

#define QUP_U_LOG_SIZE                 1024

#define QUP_UULOG_BUFFER_SIZE MICRO_DIAGBUFFER_MEM_NEEDED(QUP_U_LOG_SIZE)


micro_ULogHandle uLog_handle;
uint8 qup_uulog_buffer [QUP_UULOG_BUFFER_SIZE];
#endif

/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/

void qup_log (void *cfg, uint8 level, uint32 data_count, const char *format_str, ...)
{
    va_list args;
    va_start(args, format_str);

#ifdef QUP_ENABLE_ISLAND
    if (qurt_island_get_status ())
    {
        micro_ULog_RealTimeVprintf(uLog_handle,
                                   data_count,
                                   format_str,
                                   args);
    }
    else
#endif
    {
        qup_ulog_log(cfg, level, data_count,format_str,args);                             
    }
    va_end(args);
}


