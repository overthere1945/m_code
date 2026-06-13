/**
  @file spi_api_island.c
  @brief SPI Driver API implementation
*/
/*
  ===========================================================================

  Copyright (c) 2016-2018, 2020,2022,2023 Qualcomm Technologies Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary

  ===========================================================================

  $Header: //components/rel/core.qdsp6/8.2.3/buses/spi/src/spi_api_island.c#1 $
  $DateTime: 2025/11/18 04:23:34 $
  $Author: pwbldsvc $

  ===========================================================================
*/
#include "spi_plat.h"
#include "spi_gpi.h"
#include "spi_drv.h"
#include "qup_common.h"
#include "qup_log.h"
#include <stdlib.h>
#include <stringl/stringl.h>

#include "qup_os.h"
#include "qup_alloc.h"
#include "qup_plat.h"
#include "qup_drv.h"

#ifdef QUP_KPI_PROFILING
#include "CoreTime.h"
#endif

/*===========================================================================
                              FUNCTION DEFINITIONS
===========================================================================*/
/* spi_power_on and off */

spi_status_t spi_power_on (void *spi_handle)
{
    qup_status q_status;
    
    q_status = qup_power_on(spi_handle);
    
    if(QUP_ERROR(q_status))
    {
        QUP_LOG(LEVEL_ERROR, "qup_internal_p_on error handle 0x%08x q_status 0x%x",spi_handle,q_status);
        return SPI_DRIVER_ERROR(q_status);
    }
    
    return SPI_SUCCESS;
}

spi_status_t spi_power_off (void *spi_handle)

{
    qup_status q_status;
    
    q_status = qup_power_off(spi_handle);
    
    if(QUP_ERROR(q_status))
    {
        QUP_LOG(LEVEL_ERROR, "qup_internal_p_off error handle 0x%08x q_status 0x%x",spi_handle,q_status);
        return SPI_DRIVER_ERROR(q_status);
    }
    
    return SPI_SUCCESS;
}


spi_status_t spi_full_duplex
(
   void *spi_handle,
   spi_config_t *config,
   spi_descriptor_t *desc,
   uint32 num_descriptors,
   callback_fn c_fn,
   void *caller_ctxt,
   uint8 timestamp
)
{
    spi_status_t status = SPI_SUCCESS;
    qup_client_ctxt *c_ctxt = (qup_client_ctxt *) spi_handle;
    qup_hw_ctxt *h_ctxt = NULL;
    qup_transfer_ctxt *t_ctxt = NULL;
    se_dev_cfg *dcfg = NULL;

    spi_config_desc_pairs *spi_cfg_desc_pairs = NULL;
    if(c_ctxt != NULL)
    {
        h_ctxt = c_ctxt->h_ctxt;
        dcfg   = (se_dev_cfg *) (h_ctxt->core_config);
        t_ctxt = &(c_ctxt->t_ctxt);
    }
    else
    {
        QUP_LOG(LEVEL_ERROR,"spi_full_duplex : invalid params");
        return SPI_ERROR_INVALID_PARAM;
    }

    qup_mutex_instance_lock (h_ctxt->core_lock,CORE_R_LOCK);

#ifdef QUP_KPI_PROFILING
    t_ctxt->xfer_start_timestamp = CoreTimetick_Get64();
#endif

    spi_cfg_desc_pairs = (spi_config_desc_pairs*) qup_alloc_cfg_desc_pair (dcfg, sizeof(spi_config_desc_pairs));

    if(spi_cfg_desc_pairs == NULL)
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR, "ERROR : i2c_transfer NO MEM for config desc pairs");
        status = SPI_ERROR_INVALID_PARAM;
        goto on_exit;
    }

    spi_cfg_desc_pairs->config = config;
    spi_cfg_desc_pairs->desc = desc;
    spi_cfg_desc_pairs->num_desc =  num_descriptors;
    
    if(!spi_validate_transfer_params(spi_handle, 
        spi_cfg_desc_pairs,  
        1,      
        c_fn,
        caller_ctxt))
    {
        QUP_LOG(LEVEL_ERROR,"spi_full_duplex : invalid params");
        status = SPI_ERROR_INVALID_PARAM;
        goto on_exit;
    }

    // qurt_printf ("SPI transfer handle 0x%x\n", spi_handle);
    if (qup_os_in_exception_context())
    {
        QUP_LOG(LEVEL_ERROR,"spi_full_duplex : invalid execution level, handle 0x%x", spi_handle);
        status = SPI_ERROR_INVALID_EXECUTION_LEVEL;
        goto on_exit;
    }

    t_ctxt->multi_transfer_config = FALSE;

    // queue transfer 
    // ==============================================================    
    status = spi_add_client_ctxt_to_queue(h_ctxt, c_ctxt);
    // ==============================================================

    if(SPI_SUCCESS != status)
    {
        goto on_exit;
    }

    spi_update_transfer_ctxt(c_ctxt, spi_cfg_desc_pairs, c_fn, caller_ctxt,1,timestamp);
    
    /*timestamp stuff- tbd*/
    c_ctxt->next = NULL;
    
    if (h_ctxt->power_ref_count == 0) //core is powered off
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_full_duplex : unclocked accesss , handle 0x%x", spi_handle);
        status = SPI_ERROR_UNCLOCKED_ACCESS;
        goto on_exit;
    }

    status = h_ctxt->iface_functions.queue_transfer(c_ctxt);
    if(SPI_SUCCESS != status)
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_full_duplex :gpi queue transfer failed with status : %d, handle 0x%x", status, spi_handle);
    }
    
    if (SPI_SUCCESS(status) && (c_fn == NULL))
    {
        if (!(dcfg->flags & POLLED_MODE))
        {
            qup_wait_for_event (h_ctxt->transfer_signal, TRANSFER_SIGNAL_MASK);
        }
    }
#ifdef QUP_KPI_PROFILING
        t_ctxt-> xfer_end_timestamp = CoreTimetick_Get64();
#endif

on_exit:

    if (!SPI_SUCCESS(status) || (c_fn == NULL))
    {
        if (spi_cfg_desc_pairs)
        {
            qup_dealloc_cfg_desc_pair (dcfg, spi_cfg_desc_pairs, sizeof(spi_config_desc_pairs));
        }
    }

    qup_mutex_instance_unlock (h_ctxt->core_lock,CORE_R_LOCK);

    QUP_LOG(LEVEL_VERBOSE,"spi_full_duplex : handle 0x%x, ret %d", spi_handle, status);

    return status;
}

/*Get the timestamps on the last executed SPI transfer which had get_timestamp enabled.
If the transfer consisted of a number of descriptors, only the timestamp of the LAST
transferred buffer is captured*/
spi_status_t spi_get_timestamp (void *spi_handle, uint64 *start_time, uint64 *completed_time)
{
    return SPI_ERROR;
}

spi_status_t spi_get_timestamp_64 (void *spi_handle, spi_timestamp_type *ts_type, uint64 *timestamp_val)
{
    qup_client_ctxt *c_ctxt = (qup_client_ctxt *) spi_handle;
    qup_transfer_ctxt *t_ctxt = NULL;
    
    if (spi_handle == NULL || ts_type == NULL || timestamp_val == NULL)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    t_ctxt = &(c_ctxt->t_ctxt);
    if (t_ctxt != NULL)
    {
        if(t_ctxt->transfer_state == TRANSFER_DONE)
        {
            *timestamp_val     = t_ctxt->start_stop_bit_timestamp;
            *ts_type           = spi_plat_get_timestamp_supported();
        }
        else
        {
            return SPI_ERROR_PENDING_TRANSFER;
        }
    }
    
    return SPI_SUCCESS;
}
