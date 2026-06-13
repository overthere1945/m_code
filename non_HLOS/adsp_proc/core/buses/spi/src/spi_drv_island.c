/** 
    @file  i2c_drv_island.c
    @brief I2C implementation
 */
/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
11/03/25   SS      Added changes to return error for full duplex descriptor on SPI3W
07/06/24   GKR     converted gpi_data from ptr to array for better memory handling
*/
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/
#include "spi_api.h"    // implement the apis defined here
#include "spi_gpi.h"  // fifo/dma interface definitions
#include "spi_plat.h"   // platform apis
#include "qup_log.h"    // logging
#include "qup_os.h"
#include "qup_drv.h"
#include "qup_alloc.h"
#include "spi_drv.h"

boolean spi_validate_transfer_params (qup_client_ctxt *c_ctxt, 
        spi_config_desc_pairs *cfg_desc_pairs, 
        uint16 num_desc_pairs,       
        callback_fn c_fn,
        void *ctxt
        )
{
    uint16 i,j;
    se_dev_cfg	*dcfg = NULL;
    spi_descriptor_t *dc = NULL;
    uint8 total_num_desc = 0;
    uint8 num_ring_elements = 0;
    spi_config_t *config = NULL;
	
    if ((c_ctxt == NULL)                        ||
        (c_ctxt->h_ctxt == NULL)                ||
        (c_ctxt->h_ctxt->core_config == NULL)   || 
        num_desc_pairs == 0
        )
    {
        QUP_LOG(LEVEL_ERROR,"spi_full_duplex : invalid param");
        return FALSE;
    }
    dcfg =	(se_dev_cfg	*)c_ctxt->h_ctxt->core_config;    
    config = cfg_desc_pairs -> config;
	
	if( config -> loopback_mode == 1 && dcfg -> protocol_in_use ==  SE_PROTOCOL_SPI_3W)
	{
        QUP_LOG(LEVEL_ERROR,"spi_full_duplex : invalid param, Loop_back is not supported in spi_3w ");
        return FALSE;		
	}
	
    QUP_SE_LOG(dcfg,LEVEL_VERBOSE, "num_desc_pairs %d",
                num_desc_pairs);
    QUP_SE_LOG(dcfg,LEVEL_INFO, "func 0x%08x: ctxt 0x%08x",
                c_fn, ctxt);
    
    num_ring_elements = dcfg->gpi_data.ring_size_multiplier * 16;
    
    // validate descriptors
    for (i = 0; i < num_desc_pairs; i++)
    {
        dc = cfg_desc_pairs[i].desc;
        for(j = 0; j < cfg_desc_pairs[i].num_desc; j++)
        {
            if (dc == NULL) 
            {
                QUP_SE_LOG(dcfg,LEVEL_ERROR, "SPI Transfer Desc NULL, Invalid 0x%x", c_ctxt);
                return FALSE; 
            }
                    
            if (dc->len == 0)
            {
                // len 0 check
                QUP_SE_LOG(dcfg,LEVEL_ERROR, "SPI Transfer Len is 0, Invalid 0x%x", c_ctxt);
                return FALSE;
            }
            if  (cfg_desc_pairs[i].config->spi_slave_index >= SPI_CHIP_SELECT_INVALID)
            {
                QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_full_duplex : invalid spi_slave_index");
                return FALSE;
            }
			if(dcfg->protocol_in_use == SE_PROTOCOL_SPI_3W)
			{
				if((dc[j].tx_buf != NULL) && (dc[j].rx_buf != NULL))
				{
					/* Full duplex is not supported for SPI 3W*/
					QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_full_duplex : FUll duplex is not supported on 3W SPI");
					return FALSE;
				}
				
			}
            
            dc++;
            total_num_desc++;
        }
    }

    if((total_num_desc*2) + 3 > num_ring_elements) // 3 to account for LOCK, CONFIG and UNLOCK TRE, 
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR, "Number of descriptors more than expected");
        return FALSE;
    }
    
    return TRUE;
}

spi_status_t spi_add_client_ctxt_to_queue(qup_hw_ctxt *h_ctxt, qup_client_ctxt *c_ctxt)
{
    spi_status_t status = SPI_SUCCESS;
    se_dev_cfg *dcfg = (se_dev_cfg*)h_ctxt->core_config;

    qup_mutex_instance_lock (h_ctxt->queue_lock,QUEUE_LOCK);
    if (h_ctxt->client_ctxt_head == NULL)
    {
        // if we are here, it means there are no active transfers in the core
        h_ctxt->client_ctxt_head = c_ctxt;
    }
    else
    {
        // check if the same client is queueing twice
        qup_client_ctxt *temp_curr = h_ctxt->client_ctxt_head;
        qup_client_ctxt *temp_prev = NULL;
    
        while (temp_curr != NULL)
        {
            if (temp_curr == c_ctxt)
            {
                QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_full_duplex : client already has queued a transfer, handle 0x%x", c_ctxt);
                status = SPI_ERROR_PENDING_TRANSFER;
                break;
            }
            temp_prev = temp_curr;
            temp_curr = temp_curr->next;
        }
    
        // if the client handle is not in queue, then add it
        if (status != SPI_ERROR_PENDING_TRANSFER)
        {
            temp_prev->next = c_ctxt;    
        }
    }
    
    qup_mutex_instance_unlock (h_ctxt->queue_lock,QUEUE_LOCK);
    return status;
}

void spi_update_transfer_ctxt(qup_client_ctxt *c_ctxt, spi_config_desc_pairs *cfg_desc_pairs, callback_fn c_fn, void *caller_ctxt,uint32 num_desc_pairs, uint8 timestamp)
{
    qup_hw_ctxt *h_ctxt = c_ctxt->h_ctxt;
    qup_transfer_ctxt *t_ctxt = &(c_ctxt->t_ctxt);
    spi_config_t *config = cfg_desc_pairs->config;;    

    t_ctxt->cfg.spi.config      = cfg_desc_pairs->config;
    t_ctxt->cfg.spi.desc        = cfg_desc_pairs->desc;
    t_ctxt->cfg.spi.num_descs   = cfg_desc_pairs->num_desc;    

    t_ctxt->cfg.spi.num_descs_pairs    = num_desc_pairs;
    t_ctxt->cfg.spi.config_desc_pairs  = cfg_desc_pairs,    
    t_ctxt->cfg.spi.ctxt               = caller_ctxt;
    t_ctxt->cfg.spi.callback    = c_fn;
    t_ctxt->cfg.spi.async = (c_fn == NULL) ? FALSE : TRUE;
    
    t_ctxt->transfer_count           = 0;
    t_ctxt->transfer_state           = TRANSFER_INVALID;
    t_ctxt->start_stop_bit_timestamp = 0;
    t_ctxt->cfg.spi.xfer_status      = SPI_SUCCESS;
    t_ctxt->cfg.spi.timestamp        = timestamp;
    
    if(num_desc_pairs == 1)
    {
        if(memcmp(&h_ctxt->curr_bus_cfg.spi_bus_cfg, config, sizeof(spi_config_t)) != 0)
        {
            h_ctxt->core_state &= ~(QUP_CORE_CONFIG_TRE_ISSUED);
            h_ctxt->curr_bus_cfg.spi_bus_cfg = *config;
        }
        else
        {
            h_ctxt->core_state |= QUP_CORE_CONFIG_TRE_ISSUED;
        }
    } 
    else h_ctxt->core_state &= ~(QUP_CORE_CONFIG_TRE_ISSUED); //do not put config TRE optimization logic for multiple slaves

}