/**
    @file  qup_drv_island.c
    @brief Driver implementation
 */

/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
04/15/25   SS      Added changes to support SDA stuck recovery
04/07/25   GKR     Changed EXCLUSIVE_SE to SHARED_SE
07/29/24   GKR     Added sleep vote & devote calls for ADSP PROC
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
#include "qup_log.h"
#include "qup_drv.h"
#include "qup_gpi.h"
#include "qup_plat.h"

#include "i3c_ibi_drv.h"
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
                                          GLOBAL FUNCTIONS
==================================================================================================*/


qup_status qup_power_on(void* handle)
{
    qup_client_ctxt *c_ctxt = (qup_client_ctxt *) handle;
    qup_hw_ctxt *h_ctxt;
    se_dev_cfg *dcfg;
    ibi_core_ctxt     *ibi_ctxt;
    uint8             *se_base;
    qup_status q_status = QUP_SUCCESS;
    boolean    sync_with_irq = FALSE;

    QUP_LOG(LEVEL_PERF, "on S");

    if ((c_ctxt == NULL)         ||
        (c_ctxt->h_ctxt == NULL) ||
        (c_ctxt->h_ctxt->core_iface == NULL) ||
        (c_ctxt->h_ctxt->core_config == NULL))
    {
        QUP_LOG(LEVEL_ERROR, "ERROR on invalid param");
        return QUP_ERROR_INVALID_PARAMETER;
    }

    h_ctxt = c_ctxt->h_ctxt;
    dcfg = (se_dev_cfg *) h_ctxt->core_config;

    qup_mutex_instance_lock (h_ctxt->core_lock, CORE_R_LOCK);

    if(h_ctxt->ibs_controller)
    {
        ibi_ctxt = h_ctxt->ibs_controller;
        sync_with_irq = TRUE;
        qup_mutex_instance_lock (ibi_ctxt->irq_lock, IRQ_PI_LOCK);
    }

    if (h_ctxt->power_ref_count == 0)
    {
        QUP_SE_LOG(dcfg,LEVEL_INFO, "ON 0x%08x", handle);
        if(dcfg->ibi_data)
        {
            dcfg->ibi_data->ibi_bus_contention_status = FALSE;
            dcfg->ibi_data->ibi_bus_error_count = 0;
        }
        /* if GSI interrupts and clocks are voted from IBI handling,do not remove them, but negate the flags */
        if(sync_with_irq && QUP_IS_SET(ibi_ctxt->flags,I3C_DRV_IBI_CLOCK_VOTED) && QUP_IS_SET(ibi_ctxt->flags,I3C_DRV_IBI_QUP_INT_ENABLE))
        {
            QUP_FLAG_UNSET(ibi_ctxt->flags,I3C_DRV_IBI_CLOCK_VOTED);
            QUP_FLAG_UNSET(ibi_ctxt->flags,I3C_DRV_IBI_QUP_INT_ENABLE);
        }
        else
        {
            if (qup_clock_enable(dcfg) == FALSE)
            {
                q_status = QUP_ERROR_PLATFORM_CLOCK_ENABLE_FAIL;
                goto exit;
            }
            h_ctxt->iface_functions.iface_enable(h_ctxt,TRUE);
        }
        if (qup_gpio_enable (dcfg) == FALSE)
        {
            q_status = QUP_ERROR_PLATFORM_GPIO_ENABLE_FAIL;
            goto exit;
        }

        se_base = (uint8*)(dcfg->se_base_addr);
        HWIO_OUTXF(GENI_CFG_BASE(se_base), GENI_OUTPUT_CTRL, IO_OUTPUT_CTRL, 0x7f);

#ifdef QUP_IN_ADSP_PROC
        if (QUP_IS_SET(dcfg->flags, ENABLE_VOTE_AGAINST_ALL_SLEEP))
        {
            qup_set_sleep_votes(h_ctxt, TRUE);
        }
#endif
    }
    h_ctxt->power_ref_count++;

exit:

    if(sync_with_irq)
    {
        qup_mutex_instance_unlock (ibi_ctxt->irq_lock, IRQ_PI_LOCK);
    }

    qup_mutex_instance_unlock (h_ctxt->core_lock, CORE_R_LOCK);
    QUP_LOG(LEVEL_PERF, "on P");

    return q_status;
}

qup_status qup_power_off(void* handle)
{
    qup_client_ctxt *c_ctxt = (qup_client_ctxt *) handle;
    qup_hw_ctxt *h_ctxt;
    se_dev_cfg *dcfg;
    ibi_core_ctxt     *ibi_ctxt;
    qup_status q_status = QUP_SUCCESS;
    boolean    sync_with_irq = FALSE;

    QUP_LOG(LEVEL_PERF, "off S");

    if ((c_ctxt == NULL)         ||
        (c_ctxt->h_ctxt == NULL) ||
        (c_ctxt->h_ctxt->core_iface == NULL) ||
        (c_ctxt->h_ctxt->core_config == NULL))
    {
        QUP_LOG(LEVEL_ERROR, "ERROR off invalid param");
        return QUP_ERROR_INVALID_PARAMETER;
    }
    h_ctxt = c_ctxt->h_ctxt;
    dcfg = (se_dev_cfg *) h_ctxt->core_config;

    qup_mutex_instance_lock (h_ctxt->core_lock, CORE_R_LOCK);

    if(h_ctxt->ibs_controller)
    {
        ibi_ctxt = h_ctxt->ibs_controller;
        sync_with_irq = TRUE;
        qup_mutex_instance_lock (ibi_ctxt->irq_lock, IRQ_PI_LOCK);
    }

    if (h_ctxt->power_ref_count)
    {
        h_ctxt->power_ref_count--;
        if (h_ctxt->power_ref_count == 0)
        {
            QUP_SE_LOG(dcfg,LEVEL_INFO, "OFF 0x%08x", handle);

            h_ctxt->iface_functions.iface_enable(h_ctxt,FALSE);

#ifdef QUP_IN_ADSP_PROC
            /* Remove vote against sleep */
            if (QUP_IS_SET(dcfg->flags, ENABLE_VOTE_AGAINST_ALL_SLEEP))
            {
                qup_set_sleep_votes(h_ctxt, FALSE);
            }
#endif
            if (qup_clock_disable(dcfg) == FALSE)
            {
                q_status = QUP_ERROR_PLATFORM_CLOCK_DISABLE_FAIL;
                goto exit;
            }

            /* Don't Deconfigure GPIOs if multiee usecase 
               Other EE will be using the same SE for bus transfers*/
            if(QUP_IS_NOT_SET(dcfg->flags, SHARED_SE) && dcfg->protocol_in_use != SE_PROTOCOL_I3C_IBI)
            {
                if (qup_gpio_disable (dcfg) == FALSE)
                {
                    q_status = QUP_ERROR_PLATFORM_GPIO_DISABLE_FAIL;
                    goto exit;
                }
            }
        }
    }

exit:

    if(QUP_ERROR(q_status))
    {
        h_ctxt->power_ref_count++;
    }

    if(sync_with_irq)
    {
        qup_mutex_instance_unlock (ibi_ctxt->irq_lock, IRQ_PI_LOCK);
    }

    qup_mutex_instance_unlock (h_ctxt->core_lock, CORE_R_LOCK);
    QUP_LOG(LEVEL_PERF, "off S");

    return q_status;
}

boolean     qup_in_fifo_mode(void* handle)
{
    qup_client_ctxt *c_ctxt = (qup_client_ctxt *) handle;
    qup_hw_ctxt     *h_ctxt = (qup_hw_ctxt *)c_ctxt->h_ctxt;
    se_dev_cfg      *dcfg   = (se_dev_cfg *) h_ctxt->core_config;

    if(QUP_IS_SET(dcfg->flags,ENABLE_FIFO_MODE))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}
