/**
    @file  qup_plat_island.c
    @brief platform implementation
 */

/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
10/15/25   MG      Added qup pipe init support
09/23/25   MG      Added qup pipe support
08/16/24   GKR     Moved qup_enable_power_domain() to non island file
06/26/24   GKR     Generalized GPIO Handles for TOP & SSC QUPS from two different variable names for better driver design
*/
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/


/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/
#include "qup_plat.h"
#include "qup_os.h"
#include "busywait.h"
#include "GPIO.h"
#include "qurt_isr.h"
#include "Clock.h"
/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/

/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/

/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/

/*==================================================================================================
                                          GLOBAL VARIABLES
==================================================================================================*/

ClockHandle  qup_clock_handle = 0;
ClockIdType gdsc_pwr_domain_id = 0;
GPIOClientHandleType  qup_gpio_handle[QUP_MAJOR_MAX-1]; // one handle for TOP & one handle for SSC 
qurt_pipe_t           qup_pipe = {0};
qurt_pipe_data_t      qup_data[QUP_PIPE_NUM_PARAMETERS * QUP_PIPE_MAX_CONCURRENT_CLIENTS] = {0};

/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/

boolean qup_interrupt_enable (uint32 irq_num)
{
    if(QURT_EOK != qurt_interrupt_enable(irq_num))
    {
        QUP_LOG(LEVEL_ERROR, "qup_interrupt_enable : enable fail %d", irq_num);
        return FALSE;
    }
    return TRUE;
}

boolean qup_interrupt_clear (uint32 irq_num)
{
    if(QURT_EOK != qurt_interrupt_clear(irq_num))
    {
        QUP_LOG(LEVEL_ERROR, "qup_interrupt_clear : Clear fail %d", irq_num);
        return FALSE;
    }
    return TRUE;
}

boolean qup_interrupt_disable (uint32 irq_num)
{
    if(QURT_EOK != qurt_interrupt_disable(irq_num))
    {
        QUP_LOG(LEVEL_ERROR, "qup_interrupt_disable : disable fail %d", irq_num);
        return FALSE;
    }
    return TRUE;
}


void qup_delay_us (uint32 us)
{
    busywait (us);
}


void internal_qup_timer_expiry(int data)
{
    qup_timer_callback_data   *cb_retainer = (qup_timer_callback_data *)data;
    
    if(cb_retainer->cb_func != NULL)
    {
        cb_retainer->cb_func(cb_retainer->cb_data);
    }
}

boolean   qup_is_power_domain_on (se_dev_cfg *config)
{
    boolean isOn = TRUE;

#ifdef SSC_QUP_IN_ADSP
    ClockResult res = CLOCK_SUCCESS;

    if (!gdsc_pwr_domain_id)
    {
        return FALSE;
    }

    res = Clock_IsOn(qup_clock_handle, gdsc_pwr_domain_id, (bool *) &isOn);
    if (res != CLOCK_SUCCESS) return FALSE;
#endif

    return isOn;
}

qurt_pipe_t *qup_get_pipe_handle(void)
{
    return (&qup_pipe);
}

int qup_pipe_init (void)
{
    qurt_pipe_attr_t    attr = {0};
    qurt_pipe_attr_init(&attr);
    qurt_pipe_attr_set_buffer(&attr, qup_data);
    qurt_pipe_attr_set_buffer_partition(&attr, 1);
    qurt_pipe_attr_set_elements(&attr, QUP_PIPE_NUM_PARAMETERS * QUP_PIPE_MAX_CONCURRENT_CLIENTS);

    return qurt_pipe_init(&qup_pipe, &attr);
}

void qup_pipe_de_init (void)
{
    return qurt_pipe_destroy(&qup_pipe);
}