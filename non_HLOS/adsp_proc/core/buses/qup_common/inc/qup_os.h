/** 
    @file  qup_os.h
    @brief OS interface
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __QUP_OS_H__
#define __QUP_OS_H__
/* *************************************************************** */
/*                          INCLUDE FILES                          */
/* *************************************************************** */

#include "qup_devcfg.h"
#include "qup_log.h"
#include "qup_drv.h"
#include "qurt.h"
#include "err.h"

/* *************************************************************** */
/*                         DATA STRUCTURES                         */
/* *************************************************************** */

/* Synchronisation Lock Type*/
typedef enum qup_lock_type
{
    CORE_LOCK = 0,
    CORE_R_LOCK,
    QUEUE_LOCK,
    IRQ_PI_LOCK,
} qup_lock_type;

/* Signal Type 
 * All signals maintained on a per core basis
 */
typedef enum qup_signal_type
{
    CORE_TRANSFER_SIGNAL = 0,
    CORE_CMD_SIGNAL,
    CORE_IBI_SIGNAL,
} qup_signal_type;

/* Type of Error */
typedef enum qup_assert_type
{
    QUP_ASSERT_TYPE = 0,
    QUP_FATAL_TYPE,
} qup_assert_type;

/* *************************************************************** */
/*                            FUNCTIONS                            */
/* *************************************************************** */

/* Returns True if Process in Interrupt/Exception context*/
boolean qup_os_in_exception_context (void);

/* Block All interrupts*/
void    qup_int_lock (void);
void    qup_int_unlock (void);

/*Global mutex initalised in bootup*/
void    qup_mutex_global_lock (void);
void    qup_mutex_global_unlock (void);

/*Pipe mutex initalised in bootup*/
void    qup_mutex_pipe_lock (void);
void    qup_mutex_pipe_unlock (void);
/* Synchronisation Lock API's*/
void   *qup_mutex_instance_init (se_dev_cfg *dcfg, qup_lock_type type);
void    qup_mutex_instance_lock (void *lock, qup_lock_type type);
void    qup_mutex_instance_unlock (void *lock, qup_lock_type type);
void    qup_mutex_instance_de_init (se_dev_cfg *dcfg, void *lock, qup_lock_type type);

/* Signal API's*/
void   *qup_signal_init (se_dev_cfg *dcfg, qup_signal_type type);
void    qup_signal_de_init (se_dev_cfg *dcfg, void *signal);
boolean qup_wait_for_event (void *signal,uint32 mask);
void    qup_signal_event (void *signal,uint32 mask);
void    qup_clear_signal_event (void *signal,uint32 mask);
/* Assert API */
void    QUP_ASSERT(se_dev_cfg *cfg,qup_assert_type type);

/* MACROs to simplyfy locking API's*/
#define QUP_LOCK_IRQ_PI_LOCK(lock) qurt_pimutex_lock(lock)
#define QUP_LOCK_CORE_R_LOCK(lock) qurt_rmutex_lock(lock)
#define QUP_LOCK_QUEUE_LOCK(lock) qurt_mutex_lock(lock)
#define QUP_LOCK_CORE_LOCK(lock) qurt_mutex_lock(lock)

#define QUP_UNLOCK_IRQ_PI_LOCK(lock) qurt_pimutex_unlock(lock)
#define QUP_UNLOCK_CORE_R_LOCK(lock) qurt_rmutex_unlock(lock)
#define QUP_UNLOCK_QUEUE_LOCK(lock) qurt_mutex_unlock(lock)
#define QUP_UNLOCK_CORE_LOCK(lock) qurt_mutex_unlock(lock)

#define  qup_mutex_instance_lock(lock,type) QUP_LOCK_##type(lock)
#define  qup_mutex_instance_unlock(lock,type) QUP_UNLOCK_##type(lock)

/* Verify if pointer is NULL*/
#define QUP_VERIFY(ptr) if(ptr == NULL){QUP_ASSERT(NULL,QUP_FATAL_TYPE);}

/* Initilaise Global OS Resources*/
void    qup_os_init(void);
int qup_os_get_root_pid(void);
boolean qup_os_is_client_root(qup_client_ctxt *handle);
void qup_set_current_pid(qup_client_ctxt *handle, int pid);
#endif
