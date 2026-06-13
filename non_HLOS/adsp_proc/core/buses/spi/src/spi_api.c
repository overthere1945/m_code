/**
  @file spi_api.c
  @brief SPI Driver API implementation
*/
/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
10/22/25   SS      SPI 3 Wire fixes.
07/02/25   SS      KW Fixes.
04/07/25   GKR     Changed EXCLUSIVE_SE to SHARED_SE
06/26/24   GKR     Changes to Support for multiple SSC QUPs
*/
/*
  ===========================================================================

  Copyright (c) Qualcomm Technologies Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary

  ===========================================================================

  $Header: //components/rel/core.qdsp6/8.2.3/buses/spi/src/spi_api.c#1 $
  $DateTime: 2025/11/18 04:23:34 $
  $Author: pwbldsvc $

  ===========================================================================
*/

#include "spi_plat.h"
#include "spi_drv.h"
#include "spi_gpi.h"
#include "qup_common.h"
#include "qup_log.h"
#include <stdlib.h>
#include <stringl/stringl.h>
#include "qup_os.h"

#include "qup_drv.h"
#include "qup_alloc.h"
#include "qup_plat.h"
#include "qup_devcfg.h"

spi_status_t spi_open (spi_instance_t instance, void **spi_handle)
{
    QUP_TYPE qup;
    uint32 se_index;
    
    if(!qup_config_get_info(instance,&qup,&se_index))
    {
        return SPI_ERROR;
    }
    
    return spi_open_ex(qup,se_index,spi_handle);
}

spi_status_t spi_open_ex (QUP_TYPE qup, uint32 se_index, void **spi_handle)
{
    qup_status q_status;
    qup_client_ctxt     *c_ctxt = NULL;
    qup_hw_ctxt         *h_ctxt = NULL;
    
    q_status = qup_open(qup, se_index, spi_handle, SE_PROTOCOL_SPI);
    
    if((QUP_ERROR(q_status)) ||(*spi_handle == NULL))
    {
        QUP_LOG(LEVEL_ERROR, "qup_internal_open error 0x%08x %d q_status 0x%x",qup,se_index,q_status);
        return SPI_DRIVER_ERROR(q_status);
    }
    
    c_ctxt = (qup_client_ctxt *) *spi_handle;
    h_ctxt = c_ctxt->h_ctxt;
    
    qup_mutex_instance_lock (h_ctxt->core_lock, CORE_R_LOCK);
    
    if(QUP_IS_NOT_SET(h_ctxt->core_state,QUP_CORE_IFACE_FUNCTIONS_INITIALIZED))
    {
        QUP_GPI_ATTACH_TRANSFER_COMPLETION_CB(h_ctxt->core_iface,spi_handle_tf_completion);
        h_ctxt->iface_functions.queue_transfer = (uint32 (*)(qup_client_ctxt *))spi_gpi_queue_transfer;
        h_ctxt->iface_functions.submit_transfer = (uint32 (*)(qup_client_ctxt *))spi_gpi_submit_transfer;
        h_ctxt->iface_functions.prepare_transfer = (uint32 (*)(qup_client_ctxt *))spi_gpi_prepare_transfer;
        
        QUP_FLAG_SET(h_ctxt->core_state,QUP_CORE_HW_HAS_INTERNAL_QUEUE);
        QUP_FLAG_SET(h_ctxt->core_state,QUP_CORE_IFACE_FUNCTIONS_INITIALIZED);
    }
    
    qup_mutex_instance_unlock (h_ctxt->core_lock, CORE_R_LOCK);
    
    return SPI_SUCCESS;
}

spi_status_t spi_close (void *spi_handle)
{
    qup_status q_status;

    q_status = qup_close((void *)spi_handle, SE_PROTOCOL_SPI);

    if(QUP_ERROR(q_status))
    {
        QUP_LOG(LEVEL_ERROR, "qup_internal_close error handle 0x%08x q_status 0x%x",spi_handle,q_status);
        return SPI_DRIVER_ERROR(q_status);
    }
    
    return SPI_SUCCESS;
}

//Set power resources required for the specific SPI instance before entering low power island mode.
spi_status_t spi_setup_island_resource (spi_instance_t instance, uint32 frequency_hz)
{
    QUP_TYPE qup;
    uint32 se_index;
    
    if(!qup_config_get_info(instance,&qup,&se_index))
    {
        return SPI_ERROR;
    }
    
    return spi_setup_island_resource_ex(qup,se_index,frequency_hz);
}

//Set power resources required for the specific SPI instance before entering low power island mode.
spi_status_t spi_setup_island_resource_ex (QUP_TYPE qup, uint32 se_index, uint32 frequency_hz)
{
    spi_status_t status = SPI_SUCCESS;
    se_dev_cfg *dcfg;
    uint32 src_freq = 0;
    uint32 core_freq = 0; 
    
    if (frequency_hz == 0)
    {
        QUP_LOG(LEVEL_ERROR,"spi_setup_island_resource : invalid param");
        return SPI_ERROR_INVALID_PARAM;
    }
    
    qup_mutex_global_lock();
    
    dcfg = qup_get_hw_cfg (qup,se_index);
    if (dcfg == NULL)
    {
        QUP_LOG(LEVEL_ERROR,"spi_setup_island_resource : core is not enabled, 0x%08x %d",qup,se_index);
        status =  SPI_ERROR_CORE_NOT_OPEN;
        goto exit;
    }


    if ((GET_PROTOCOL_MAJOR(dcfg->protocol_in_use) != (SE_PROTOCOL_SPI)) && (GET_PROTOCOL_MAJOR(dcfg->protocol_in_use) != (SE_PROTOCOL_SPI_3W)))
    {
        QUP_LOG(LEVEL_ERROR, "ERROR lpi setup protocol loaded is not SPI !!");
        status = SPI_ERROR;
        goto exit;
    }

    if ( GET_QUP_MAJOR(qup) == SSC_QUP)
    {
        if(dcfg->gdsc_ref_count == 0)
        {
            if(qup_enable_power_domain(TRUE))
            {
                dcfg->gdsc_ref_count++;
            }
            else 
            {
                QUP_LOG(LEVEL_ERROR,"spi_setup_island_resource 0x%08x %d GDSC ON failed",qup,se_index);
                status = SPI_ERROR_PLAT_SET_RESOURCE_FAIL;
                goto exit;
            }
        }
        else
        {
            dcfg->gdsc_ref_count++;
        }
    }

    core_freq = dcfg->qup_data->core_freq_hz;

    if (0 == (src_freq = spi_plat_clock_get_frequency (dcfg, frequency_hz)))
    {
        QUP_LOG(LEVEL_ERROR,"spi_setup_island_resource : get src frequency failed 0x%08x %d",qup,se_index);
        status = SPI_ERROR_PLAT_SET_RESOURCE_FAIL;
        goto exit;
    }
    
    if (FALSE == qup_setup_se_clock(dcfg,src_freq))
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_setup_island_resource : qup_setup_se_clock failed 0x%08x %d",qup,se_index);
        status = SPI_ERROR_PLAT_RESET_RESOURCE_FAIL;
        goto exit;

    }

    if (FALSE == qup_setup_common_clock_id (dcfg))
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_setup_island_resource : qup_setup_common_clock_id failed 0x%08x %d",qup,se_index);
        status = SPI_ERROR_PLAT_RESET_RESOURCE_FAIL;
        goto exit;

    }

    if (FALSE == qup_setup_core_clock (dcfg, core_freq))
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_setup_island_resource : qup_setup_core_clock failed 0x%08x %d",qup,se_index);
        status = SPI_ERROR_PLAT_RESET_RESOURCE_FAIL;
        goto exit;
    }

    dcfg->resources.island_setup_votes++;

exit:
    qup_mutex_global_unlock();
    QUP_SE_LOG(dcfg,LEVEL_INFO,"spi_setup_island_resource : 0x%08x %d, status %d",qup,se_index,status);
    
    return status;
}

//If a resource was previously set up for island operation, this releases that resource.
spi_status_t spi_reset_island_resource (spi_instance_t instance)
{
    QUP_TYPE qup;
    uint32 se_index;
    
    if(!qup_config_get_info(instance,&qup,&se_index))
    {
        return SPI_ERROR;
    }
    
    return spi_reset_island_resource_ex(qup,se_index, 0);
}

//If a resource was previously set up for island operation, this releases that resource.
spi_status_t spi_reset_island_resource_ex (QUP_TYPE qup, uint32 se_index, uint32 frequency_hz)
{
    spi_status_t status = SPI_SUCCESS;
    se_dev_cfg *dcfg = NULL;
    uint32 src_freq = 0;

    qup_mutex_global_lock();
    
    dcfg = qup_get_hw_cfg (qup,se_index);
    if (dcfg == NULL)
    {
        QUP_LOG(LEVEL_ERROR,"spi_reset_island_resource : core is not enabled 0x%08x %d",qup,se_index);
        status = SPI_ERROR_CORE_NOT_OPEN;
        goto exit;
    }

    if ((GET_PROTOCOL_MAJOR(dcfg->protocol_in_use) != (SE_PROTOCOL_SPI)) && (GET_PROTOCOL_MAJOR(dcfg->protocol_in_use) != (SE_PROTOCOL_SPI_3W)))
    {
        QUP_LOG(LEVEL_ERROR, "ERROR lpi reset protocol loaded is not SPI !!");
        status = SPI_ERROR;
        goto exit;
    }

    if (frequency_hz)  // if frequnecy is not passed by client then src_freq will be 0
    {
        if (0 == (src_freq = spi_plat_clock_get_frequency (dcfg, frequency_hz)))
        {
            QUP_LOG(LEVEL_ERROR,"spi_reset_island_resource : get src frequency failed 0x%08x %d",qup,se_index);
            status = SPI_ERROR_PLAT_SET_RESOURCE_FAIL;
            goto exit;
        }
    }

    if (dcfg->resources.island_setup_votes)
    {
        if (FALSE == qup_unset_se_clock(dcfg, src_freq))
        {
            QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_reset_island_resource : se clk reset failed 0x%08x %d",qup,se_index);
            status = SPI_ERROR_PLAT_RESET_RESOURCE_FAIL;
            goto exit;
    
        }
        
        if (FALSE == qup_unset_core_clock(dcfg))
        {
            QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_reset_island_resource : core clk reset failed 0x%08x %d",qup,se_index);
            status = SPI_ERROR_PLAT_RESET_RESOURCE_FAIL;
            goto exit;
        }
        
        //de-configure GPIO's only if the island_setup_votes is 1.
        //applicable for both shared PD and multi EE usecase.
        if(QUP_IS_SET(dcfg->flags, SHARED_SE) && (dcfg->resources.island_setup_votes == 1) && (GET_QUP_MAJOR(dcfg->qup_data->qup_id) == SSC_QUP))
        {
            if (qup_gpio_disable (dcfg) == FALSE)
            {
                QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_setup_island_resource 0x%08x %d GDSC OFF Failed",qup,se_index);
                status = SPI_ERROR_PLAT_GPIO_ENABLE_FAIL;
                goto exit;
            }
        }
        if(dcfg->gdsc_ref_count) 
        {
            dcfg->gdsc_ref_count--;
            if(dcfg->gdsc_ref_count == 0)
            {
                if(!qup_enable_power_domain(FALSE))
                {
                    QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_setup_island_resource 0x%08x %d GDSC OFF Failed",qup,se_index);
                    dcfg->gdsc_ref_count++;
                    status = SPI_ERROR_PLAT_GPIO_ENABLE_FAIL;
                    goto exit;
                }
            }
        }

        dcfg->resources.island_setup_votes--;
    }

exit:
    qup_mutex_global_unlock();
    
    QUP_SE_LOG(dcfg,LEVEL_VERBOSE,"spi_reset_island_resource : 0x%08x %d, status %d", qup,se_index,status);
    return status;
}

spi_status_t spi_enable_slave_index (void *spi_handle, spi_cs_index cs_index)
{
    spi_status_t status     = SPI_SUCCESS;
    qup_client_ctxt *c_ctxt = (qup_client_ctxt *) spi_handle;
    qup_hw_ctxt *h_ctxt;
    se_dev_cfg *dcfg;
    
    if (spi_handle == NULL || cs_index >= SPI_CHIP_SELECT_INVALID)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    h_ctxt = c_ctxt->h_ctxt;
    dcfg   = (se_dev_cfg *) (h_ctxt->core_config);
    
    //For KW
    if(GET_PROTOCOL_MAJOR(dcfg->protocol_in_use) >= PROTOCOL_MAJOR_MAX) 
    {
        return SPI_ERROR_HW_INFO_ALLOCATION;
    }
    
    qup_mutex_instance_lock (h_ctxt->core_lock,CORE_R_LOCK);
    
    if(config_qup_io(dcfg,(cs_index + 3)))
    {
        if(h_ctxt->power_ref_count > 0)
        {
            if (qup_gpio_enable_io(dcfg, (cs_index + 3)) == FALSE)
            {
                status = SPI_ERROR_PLAT_GPIO_ENABLE_FAIL ;
            }
        }
    }
    else
    {
        status = SPI_ERROR_HW_INFO_ALLOCATION;
        QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_enable_slave_index : config_qup_io failed %d", (cs_index + 3));
    }
    qup_mutex_instance_unlock (h_ctxt->core_lock,CORE_R_LOCK);
    
    return status;
}

spi_status_t
spi_prepare_transfer
(
    void *spi_handle,
    spi_config_desc_pairs *spi_cfg_desc_pairs,
    uint16 num_desc_pairs,
    callback_fn c_fn,
    void *caller_ctxt,
    uint8 timestamp,
    void **slave_transfer_handle    
)
{
    spi_status_t status = SPI_SUCCESS;
    qup_client_ctxt *c_ctxt = (qup_client_ctxt *) spi_handle;
    qup_hw_ctxt *h_ctxt;
    se_dev_cfg *dcfg;
    qup_client_ctxt *c_ctxt_new = NULL;
    qup_client_ctxt     *head = c_ctxt;
    
    if(!spi_validate_transfer_params(c_ctxt, 
        spi_cfg_desc_pairs,        
        num_desc_pairs,
        c_fn,
        caller_ctxt
        ))
    {
        return SPI_ERROR_INVALID_PARAM;
    }

    // qurt_printf ("SPI transfer handle 0x%x\n", spi_handle);
    if (qup_os_in_exception_context())
    {
        QUP_LOG(LEVEL_ERROR,"spi_full_duplex : invalid execution level, handle 0x%x", spi_handle);
        return SPI_ERROR_INVALID_EXECUTION_LEVEL;
    }
    
    h_ctxt = c_ctxt->h_ctxt;
    dcfg   = (se_dev_cfg *) (h_ctxt->core_config);
    
    qup_mutex_instance_lock (h_ctxt->core_lock,CORE_R_LOCK);
    
    if (h_ctxt->power_ref_count == 0) //core is powered off
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_full_duplex : unclocked accesss , handle 0x%x", spi_handle);        
        status = SPI_ERROR_UNCLOCKED_ACCESS;
        goto exit;
    }   

    qup_mutex_global_lock();
    // alloc new client context
    c_ctxt_new = (qup_client_ctxt *) qup_mem_alloc (dcfg,
                                                sizeof(qup_client_ctxt),
                                                CLIENT_CTXT_TYPE);
    qup_mutex_global_unlock();

    if (c_ctxt_new != NULL)
    {
        c_ctxt_new->h_ctxt = h_ctxt;
        c_ctxt_new->pid = c_ctxt->pid;
        c_ctxt_new->slave_handle_next = NULL;

        while(head->slave_handle_next)
        {
            head = head->slave_handle_next;
        }
        head->slave_handle_next = c_ctxt_new;

        c_ctxt_new->t_ctxt.io_ctxt = qup_gpi_get_io_ctxt (dcfg);
        
        if (c_ctxt_new->t_ctxt.io_ctxt != NULL)
        {
            *slave_transfer_handle = c_ctxt_new;
            c_ctxt_new->t_ctxt.multi_transfer_config = TRUE;
        }
        else
        {
            QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_prepare_transfer : io_ctxt allocation failed , handle 0x%x", spi_handle);
            status =  SPI_ERROR_MEM_ALLOC;
        }
    }
    else
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_prepare_transfer : c_ctxt allocation failed , handle 0x%x", spi_handle);
        status = SPI_ERROR_MEM_ALLOC;
        goto exit;
    }

    spi_update_transfer_ctxt(c_ctxt_new, spi_cfg_desc_pairs, c_fn, caller_ctxt,num_desc_pairs,timestamp);  
    
    status = h_ctxt->iface_functions.prepare_transfer(c_ctxt_new);
    if(SPI_SUCCESS != status)
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_full_duplex :gpi prepare transfer failed with status : %d, handle 0x%x", status, spi_handle);
    }

exit:   
    if(!SPI_SUCCESS(status))
    {
        if(c_ctxt_new)
        {
            qup_mutex_global_lock();
            if (c_ctxt_new->t_ctxt.io_ctxt)
            {
                qup_gpi_release_io_ctxt(dcfg, c_ctxt_new->t_ctxt.io_ctxt);
            }
            qup_mem_free (c_ctxt_new, sizeof(qup_client_ctxt), dcfg, CLIENT_CTXT_TYPE);
            qup_mutex_global_unlock();
        }

    }
    qup_mutex_instance_unlock (h_ctxt->core_lock,CORE_R_LOCK);
    
    QUP_LOG(LEVEL_VERBOSE,"spi_full_duplex : handle 0x%x, ret %d", spi_handle, status);
    return status;
}

spi_status_t
spi_submit_transfer
(
    void *spi_handle,
    void *slave_transfer_handle
)
{
    spi_status_t status = SPI_SUCCESS;
    qup_client_ctxt *c_ctxt = (qup_client_ctxt *) slave_transfer_handle;
    qup_transfer_ctxt *t_ctxt = NULL;
    qup_hw_ctxt *h_ctxt = NULL;
    se_dev_cfg *dcfg = NULL;

    if (c_ctxt != NULL &&
        c_ctxt->h_ctxt != NULL &&
        c_ctxt->h_ctxt->core_config != NULL)
    {
        h_ctxt = c_ctxt->h_ctxt;
        dcfg = (se_dev_cfg *) (h_ctxt->core_config);
        t_ctxt = &(c_ctxt->t_ctxt);
    }
    else
    {
        QUP_LOG(LEVEL_ERROR,"spi_submit_transfer : Invalid Input Params");
        return SPI_ERROR_INVALID_PARAM;
    }
    qup_mutex_instance_lock (h_ctxt->core_lock,CORE_R_LOCK);

    t_ctxt->transfer_state           = TRANSFER_INVALID;
    t_ctxt->cfg.spi.xfer_status      = SPI_SUCCESS;
    t_ctxt->transferred              = 0;

    // queue transfer 
    // ==============================================================
    status = spi_add_client_ctxt_to_queue(h_ctxt, c_ctxt);
    // ==============================================================
    
    if (!SPI_SUCCESS(status))
    {
       qup_mutex_instance_unlock (h_ctxt->core_lock, CORE_R_LOCK);
       QUP_SE_LOG(dcfg, LEVEL_ERROR, "spi_submit_transfer spi_add_client_ctxt_to_queue failed : %d", status); 
       return status;
    }

    status = h_ctxt->iface_functions.submit_transfer(c_ctxt);
    if(SPI_SUCCESS != status)
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_full_duplex :gpi submit transfer failed with status : %d, handle 0x%x", status, spi_handle);
    }
    
    if (SPI_SUCCESS(status) && (t_ctxt->cfg.spi.callback == NULL))
    {
        if (!(dcfg->flags & POLLED_MODE))
            qup_wait_for_event (h_ctxt->transfer_signal, TRANSFER_SIGNAL_MASK);
    }
    qup_mutex_instance_unlock (h_ctxt->core_lock,CORE_R_LOCK);
    
    QUP_LOG(LEVEL_VERBOSE,"spi_full_duplex : handle 0x%x, ret %d", spi_handle, status);
    return status;
}