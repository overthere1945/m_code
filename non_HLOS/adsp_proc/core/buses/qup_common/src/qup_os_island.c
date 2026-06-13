/**
    @file  qup_os_island.c
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
qurt_mutex_t qup_global_lock;
qurt_mutex_t qup_pipe_lock;
/*==================================================================================================
                                          GLOBAL VARIABLES
==================================================================================================*/
int process_pid;
/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/

boolean qup_os_in_exception_context (void)
{
    return FALSE;
}
void qup_int_lock (void)
{
    return ;
}

void qup_int_unlock (void)
{
    return ;
}
/* Presently Pre Proccessed at header
void qup_mutex_instance_lock (void *lock, qup_lock_type type)
{
    if(type == IRQ_PI_LOCK)
    {
        qurt_pimutex_lock(lock);
    }
    else if (type == CORE_R_LOCK) 
    {
        qurt_rmutex_lock(lock);
    }
    else
    {
        qurt_mutex_lock(lock);
    }
}
void qup_mutex_instance_unlock (void *lock, qup_lock_type type)
{
    if(type == IRQ_PI_LOCK)
    {
        qurt_pimutex_unlock(lock);
    }
    else if (type == CORE_R_LOCK) 
    {
        qurt_rmutex_unlock(lock);
    }
    else
    {
        qurt_mutex_unlock(lock);
    }
}
*/
boolean qup_wait_for_event (void *signal,uint32 mask)
{
    unsigned int ret_mask = 0;
    if (QURT_EOK == qurt_signal_wait_cancellable (signal,
                    mask,
                    QURT_SIGNAL_ATTR_WAIT_ALL,
                    &ret_mask))
    {
        qurt_signal_clear(signal,
                ret_mask);
        return TRUE;
    }
    return FALSE;
}

void qup_clear_signal_event (void *signal,uint32 mask)
{
    qurt_signal_clear(signal,(unsigned int)mask);
}

void qup_signal_event (void *signal,uint32 mask)
{
    qurt_signal_set(signal,(unsigned int)mask);
}

void qup_mutex_global_lock (void)
{
    qurt_mutex_lock(&qup_global_lock);
}


void qup_mutex_global_unlock (void)
{
    qurt_mutex_unlock(&qup_global_lock);
}

void qup_mutex_pipe_lock (void)
{
    qurt_mutex_lock(&qup_pipe_lock);
}

void qup_mutex_pipe_unlock (void)
{
    qurt_mutex_unlock(&qup_pipe_lock);
}

void qup_set_current_pid(qup_client_ctxt *handle, int pid)
{
    if(handle)  //For KW
    {
        handle->pid = pid;
    }
}

boolean qup_os_is_client_root(qup_client_ctxt *handle)
{
    if(qup_os_get_root_pid() == handle->pid)
        return TRUE;
    else
        return FALSE;
}


int qup_os_get_root_pid (void)
{
    return process_pid;
}

void QUP_ASSERT(se_dev_cfg *cfg,qup_assert_type type)
{
    if(type == QUP_ASSERT_TYPE)
    {
        //get assert api
    }
    else
    {
        ERR_FATAL("QUP_ERR_FATAL ",0,0,0);
    }
}

