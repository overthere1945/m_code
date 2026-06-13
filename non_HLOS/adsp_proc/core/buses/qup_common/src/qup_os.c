/**
    @file  qup_os.c
    @brief OS implementation
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/


/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/
#include "qup_os.h"
#include "qup_alloc.h"
#include "qup_drv.h"
#include "qup_log.h"
#include "qurt.h"
#include "err.h"

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/


/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/


/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/
extern qurt_mutex_t qup_global_lock;
extern qurt_mutex_t qup_pipe_lock;
extern qurt_mutex_t qup_cfg_desc_lock;
extern int process_pid;

/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/

// signals
void   *qup_signal_init (se_dev_cfg *dcfg, qup_signal_type type)
{
    void *s;
    
    if(type == CORE_TRANSFER_SIGNAL)
    {
        s  = qup_mem_alloc (dcfg, sizeof(qurt_signal_t), TRANSFER_SIGNAL_TYPE);
    }
    else if	(type == CORE_CMD_SIGNAL)
    {
        s  = qup_mem_alloc (dcfg, sizeof(qurt_signal_t), CMD_SIGNAL_TYPE);
    }
	else if	(type == CORE_IBI_SIGNAL)
    {
        s  = qup_mem_alloc (dcfg, sizeof(qurt_signal_t), CMD_SIGNAL_TYPE);
    }
    else
    {
        return NULL;
    }
    
    if (s != NULL)
    {
        qurt_signal_init(s);
    }
    
    return s;
    
    
}

void    qup_signal_de_init (se_dev_cfg *dcfg, void *signal)
{
    if (signal != NULL)
    {
        qurt_signal_destroy(signal);
        qup_mem_free(signal, sizeof(qurt_signal_t), dcfg, TRANSFER_SIGNAL_TYPE);
    }
}

//locks
void   *qup_mutex_instance_init (se_dev_cfg *dcfg, qup_lock_type type)
{
    void *cs;
    
    if((type == CORE_R_LOCK) || (type == CORE_LOCK))
    {
        cs  = qup_mem_alloc (dcfg, sizeof(qurt_mutex_t), CORE_MUTEX_TYPE);
    }
    else if	(type == QUEUE_LOCK)
    {
        cs  = qup_mem_alloc (dcfg, sizeof(qurt_mutex_t), Q_MUTEX_TYPE);
    }
//  else if	(type == IRQ_PI_LOCK)
//  {
//      cs  = qup_mem_alloc (dcfg, sizeof(qurt_mutex_t), IRQ_MUTEX_TYPE);
//  }
    else
    {
        return NULL;
    }

    if (cs != NULL)
    {
        if(type == IRQ_PI_LOCK)
        {
            qurt_pimutex_init(cs);
        }
        else if (type == CORE_R_LOCK) 
        {
            qurt_rmutex_init(cs);
        }
        else
        {
            qurt_mutex_init(cs);
        }
    }
    
    return cs;

}
void    qup_mutex_instance_de_init (se_dev_cfg *dcfg, void *lock, qup_lock_type type)
{
    if (lock != NULL)
    {
        if(type == IRQ_PI_LOCK)
        {
            qurt_pimutex_destroy(lock);
        }
        else if (type == CORE_R_LOCK) 
        {
            qurt_rmutex_destroy(lock);
        }
        else
        {
            qurt_mutex_destroy(lock);
        }
        qup_mem_free(lock, sizeof(qurt_mutex_t), dcfg, CORE_MUTEX_TYPE);
    }
}

void qup_os_init(void)
{
    err_cb_info_t  cb_info;
    err_cb_error_t res = ERRCB_E_FAILURE;
    
    /* err_store_info callback registration with error handling */
    cb_info.type = ERRCB_TYPE_VOID;
    cb_info.err_cb.cb = &qup_error_dump_reg;
    cb_info.bucket = ERRCB_BUCKET_NORMAL;
    cb_info.order = ERRCB_ORDER_NORMAL;
 
    res = err_cb_enable(&cb_info);
    if(res != ERRCB_E_SUCCESS)
    {
        QUP_LOG(LEVEL_ERROR, "qup_os_init: err_cb_enable Failed :0x%x",res);
    }

    qurt_mutex_init(&qup_global_lock);
    qurt_mutex_init(&qup_pipe_lock);
    qurt_mutex_init(&qup_cfg_desc_lock);

    process_pid = qurt_getpid();
}


