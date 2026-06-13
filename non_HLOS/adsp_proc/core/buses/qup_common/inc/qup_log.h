/** 
    @file  qup_log.h
    @brief QUP logging interface
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __QUP_LOG_H__
#define __QUP_LOG_H__


/* *************************************************************** */
/*                          INCLUDE FILES                          */
/* *************************************************************** */
#include <stdarg.h>
#include "qurt.h"
#include "comdef.h"
#include "micro_ULog.h"
#include "qup_drv.h"


/* *************************************************************** */
/*                            CONSTANTS                            */ 
/* *************************************************************** */

#define QUP_LEVEL_ERROR   (1<<0)
#define QUP_LEVEL_WARNING (1<<1)
#define QUP_LEVEL_INFO    (1<<2)
#define QUP_LEVEL_DATA    (1<<3)
#define QUP_LEVEL_REGS    (1<<4)
#define QUP_LEVEL_VERBOSE (1<<5)
#define QUP_LEVEL_PERF    (1<<6)

#define QUP_MAX_LOG_LEVELS 7

#define QUP_DEFAULT_LOG_LEVEL (QUP_LEVEL_ERROR|QUP_LEVEL_WARNING|QUP_LEVEL_INFO)
#define QUP_ALL_LOG_LEVELS    ((1 << QUP_MAX_LOG_LEVELS) - 1)

#define QUP_LOG_NUM_ARGS(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,N,...) (N - 1)


// to enable logs, set QUP_LOG_FLAGS to an OR of the above bits in scons
//#ifdef QUP_LOG_RUMI
//#include "qurt_printf.h"
//#define QUP_LOG(level,msg,args...)  \
//    if (level & QUP_LOG_LEVEL)      \
//    {                               \
//        qurt_printf(msg,##args);    \
//    }

//#elif  QUP_ENABLE_ULOG


#define QUP_LOG(_level_,msg, args...) QUP_LOG_##_level_(NULL,QUP_##_level_,msg,##args)

#define QUP_SE_LOG(cfg,_level_,msg, args...) QUP_LOG_##_level_(cfg,QUP_##_level_,msg,##args)



#define qup_log_ex(cfg,level,dataCount, format, ...)                         \
{                                                                      \
    static const char str[] MICRO_ULOG_STR_ATTR = format;              \
    qup_log( cfg, level, dataCount, str, ##__VA_ARGS__);                      \
}


#ifdef QUP_LOG_ENABLE_DATA
#define QUP_LOG_LEVEL_DATA(cfg,level,...) qup_log_ex(cfg,level,QUP_LOG_NUM_ARGS(__VA_ARGS__,10,9,8,7,6,5,4,3,2,1), __VA_ARGS__)
#else
#define QUP_LOG_LEVEL_DATA(...)
#endif

#ifdef QUP_LOG_ENABLE_ERROR
#define QUP_LOG_LEVEL_ERROR(cfg,level,...) qup_log_ex(cfg,level,QUP_LOG_NUM_ARGS(__VA_ARGS__,10,9,8,7,6,5,4,3,2,1), __VA_ARGS__); \
    if (!qurt_island_get_status ()){ qup_log_ex(NULL,level,QUP_LOG_NUM_ARGS(__VA_ARGS__,10,9,8,7,6,5,4,3,2,1), __VA_ARGS__);} 
    //If in island dont reprint log in common else reprint log in common log for error
#else
#define QUP_LOG_LEVEL_ERROR(...)
#endif

#ifdef QUP_LOG_ENABLE_INFO
#define QUP_LOG_LEVEL_INFO(cfg,level,...) qup_log_ex(cfg,level,QUP_LOG_NUM_ARGS(__VA_ARGS__,10,9,8,7,6,5,4,3,2,1), __VA_ARGS__)
#else
#define QUP_LOG_LEVEL_INFO(...)
#endif

#ifdef QUP_LOG_ENABLE_REGS
#define QUP_LOG_LEVEL_REGS(cfg,level,...) qup_log_ex(cfg,level,QUP_LOG_NUM_ARGS(__VA_ARGS__,10,9,8,7,6,5,4,3,2,1), __VA_ARGS__)
#else
#define QUP_LOG_LEVEL_REGS(...)
#endif

#ifdef QUP_LOG_ENABLE_PERF
#define QUP_LOG_LEVEL_PERF(cfg,level,...) qup_log_ex(cfg,level,QUP_LOG_NUM_ARGS(__VA_ARGS__,10,9,8,7,6,5,4,3,2,1), __VA_ARGS__)
#else
#define QUP_LOG_LEVEL_PERF(...)
#endif

#ifdef QUP_LOG_ENABLE_VERBOSE
#define QUP_LOG_LEVEL_VERBOSE(cfg,level,...) qup_log_ex(cfg,level,QUP_LOG_NUM_ARGS(__VA_ARGS__,10,9,8,7,6,5,4,3,2,1), __VA_ARGS__)
#else
#define QUP_LOG_LEVEL_VERBOSE(...)
#endif


/* *************************************************************** */
/*                            FUNCTIONS                            */
/* *************************************************************** */
/*Common log init : To be called from Boot sequence*/
boolean qup_comm_ulog_init (void);

/*Logging API's*/
void qup_log (void *cfg,uint8 level, uint32 data_count, const char *format_str,...);
void qup_ulog_log (void *cfg,uint8 level,uint32 data_count, const char *format_str, va_list arg);


#endif
