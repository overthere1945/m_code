/**
    @file  spi_gpi_ex.c
    @brief SPI GPI Extended driver implementation
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#include "qup_drv.h"
#include "i2c_iface.h"
#include "i2c_plat.h"
#include "qup_log.h"
#include "i2c_api.h"
#include "qup_common.h"
#include "gpi.h"
#include "assert.h"
#include "qup_os.h"
#include "qup_alloc.h"
#include "qup_hal.h"
#include "spi_gpi.h"

spi_status_t spi_gpi_prepare_transfer (qup_client_ctxt *c_ctxt)
{
    qup_hw_ctxt         *h_ctxt  =  c_ctxt->h_ctxt;
    qup_transfer_ctxt   *t_ctxt  = &c_ctxt->t_ctxt;
    qup_gpi_iface_ctxt  *io_ctxt = (qup_gpi_iface_ctxt *)t_ctxt->io_ctxt;
    se_dev_cfg          *dcfg    = (se_dev_cfg *) h_ctxt->core_config;
    spi_config_desc_pairs  *spi_cfg_desc_pairs = t_ctxt->cfg.spi.config_desc_pairs;
    spi_descriptor_t *dc = NULL;

    spi_status_t status = SPI_SUCCESS;
    uint16 i,j;
    uint32 *t_count, *r_count;

    gpi_ring_elem tx_tre_local[50] = {0};
    gpi_ring_elem rx_tre_local[10] = {0};

    gpi_ring_elem *t_tre = tx_tre_local;
    gpi_ring_elem *r_tre = rx_tre_local;
    gpi_ring_elem *last_tx_go_tre = NULL;

    (io_ctxt->tx_tre_req).num_tre = 0;
    (io_ctxt->rx_tre_req).num_tre = 0;

    t_count = (uint32*) &((io_ctxt->tx_tre_req).num_tre);
    r_count = (uint32*) &((io_ctxt->rx_tre_req).num_tre);

    QUP_FLAG_SET(io_ctxt->flags,GPI_TX_COMPLETED | GPI_RX_COMPLETED);

    if(QUP_IS_SET(dcfg->flags, SHARED_SE))
    {
        qup_gpi_set_tre_lock (t_tre);
        t_tre++;
        (*t_count)++;
    }

    if (!(h_ctxt->core_state & QUP_CORE_CONFIG_TRE_ISSUED) || (!(dcfg->flags & OPTIMISE_TRANSFERS)))
    {
        spi_gpi_set_tre_config_0 (t_tre, c_ctxt);
        t_tre++;
        (*t_count)++;
        if(spi_cfg_desc_pairs->num_desc == 1)
        {
            h_ctxt->core_state |= QUP_CORE_CONFIG_TRE_ISSUED;
        }
    }

    uint8 num_config_desc_pairs = t_ctxt->cfg.spi.num_descs_pairs;

    for(i = 0; i < num_config_desc_pairs; i++)
    {
        t_ctxt->cfg.spi.config = (spi_cfg_desc_pairs + i)->config;
        t_ctxt->cfg.spi.num_descs  = (spi_cfg_desc_pairs + i)->num_desc;

        for (j = 0; j < t_ctxt->cfg.spi.num_descs; j++)
        {
            //QUP_SE_LOG(dcfg,LEVEL_ERROR,"bus_iface_prepare_transfer_gpi enter : i :%d, j %d", i, j);
            t_ctxt->cfg.spi.desc   = ((spi_cfg_desc_pairs + i)->desc) + j;
            dc = spi_cfg_desc_pairs->desc + j;

            if (dcfg->protocol_in_use == SE_PROTOCOL_SPI_3W && dc->rx_buf && last_tx_go_tre)
            {
                spi3w_update_go_tre_for_rx(last_tx_go_tre, t_ctxt, dc);
                last_tx_go_tre = NULL;
            }
            else
            {
                //set the GO tre for every descriptor. We can have
                //tx only, rx only or tx and rx transfers.
                spi_gpi_set_tre_go (t_tre, t_ctxt, dcfg);

                /* QUP supports 3 non-transfer GO commands:
                1. Keep CS asserted
                2. Keep CS deasserted and
                3. Send free SCLK cycles.
                At this time, none of the descriptors are for purely non-data transfer,
                so we shouldnt have to worry about handling this. The below if condition is
                for completion sake. */

                if (spi_gpi_tre_non_data_opcode (t_tre))
                {
                    /*Q: This is the only TRE. Chain will be 0. This probably needs to update
                    control bits to enable IEOB*/
                    break;
                }
                if (dcfg->protocol_in_use == SE_PROTOCOL_SPI_3W)
                {
                // save tx tre pointer for updating later for RX
                    last_tx_go_tre = t_tre;
                }

                t_tre++;
                (*t_count)++;
            }

            if (dc->tx_buf != NULL)
            {
                spi_gpi_set_tre_dma (c_ctxt, t_tre, dc->tx_buf, dc);
                t_tre++;
                (*t_count)++;
            }
            if (dc->rx_buf != NULL)
            {
                spi_gpi_set_tre_dma (c_ctxt, r_tre, dc->rx_buf, dc);
                r_tre++;
                (*r_count)++;
            }

            t_ctxt->transfer_count++;
        }
    }

    if(QUP_IS_SET(dcfg->flags, SHARED_SE))
    {
        qup_gpi_set_tre_unlock (t_tre);
        t_tre++;
        (*t_count)++;
        io_ctxt->flags |= GPI_WAIT_FOR_UNLOCK_TRE;
    }

    if (*r_count)
    {
        t_ctxt->buffer_list[GPI_INBOUND_CHAN]  = qup_mem_alloc(dcfg, *r_count*sizeof(gpi_ring_elem), QUP_TRE_LIST_ISLAND);
        qup_mem_copy(t_ctxt->buffer_list[GPI_INBOUND_CHAN], (*r_count)*sizeof(gpi_ring_elem), rx_tre_local, (*r_count)*sizeof(gpi_ring_elem));
        io_ctxt->flags &= (~GPI_RX_COMPLETED);
        QUP_LOG(LEVEL_PERF, "gpirx S");
    }

    if (*t_count)
    {
        t_ctxt->buffer_list[GPI_OUTBOUND_CHAN] = qup_mem_alloc(dcfg, *t_count*sizeof(gpi_ring_elem), QUP_TRE_LIST_ISLAND);
        qup_mem_copy(t_ctxt->buffer_list[GPI_OUTBOUND_CHAN],  (*t_count)*sizeof(gpi_ring_elem), tx_tre_local, (*t_count)*sizeof(gpi_ring_elem));
        io_ctxt->flags &= (~GPI_TX_COMPLETED);
        QUP_LOG(LEVEL_PERF, "gpitx S");
    }

    (io_ctxt->tx_tre_req).num_tre = *t_count;
    (io_ctxt->rx_tre_req).num_tre = *r_count;

    return status;
}


spi_status_t spi_gpi_submit_transfer (qup_client_ctxt *c_ctxt)
{
    qup_hw_ctxt         *h_ctxt  =  c_ctxt->h_ctxt;
    qup_transfer_ctxt   *t_ctxt  = &c_ctxt->t_ctxt;
    qup_gpi_iface_ctxt  *io_ctxt = (qup_gpi_iface_ctxt *)t_ctxt->io_ctxt;
    qup_gpi_core_ctxt   *g_ctxt  = (qup_gpi_core_ctxt *)h_ctxt->core_iface;
    se_dev_cfg          *dcfg    = (se_dev_cfg *) h_ctxt->core_config;
    GPI_RETURN_STATUS gpi_ret_val = GPI_STATUS_SUCCESS;

    spi_status_t status = SPI_SUCCESS;
    uint8   t_wp_idx,r_wp_idx;
    uint32 t_count, r_count;
    uint32 tx_num_elem, rx_num_elem;
    qup_mem_region_type region;
    uint8 elem_left = 0;

    gpi_ring_elem *t_tre = NULL;
    gpi_ring_elem *r_tre = NULL;

    //traverse to find g_ctxt with sufficient TRE space left
    while(g_ctxt && QUP_IS_SET(g_ctxt->flags,GPI_IFACE_REGISTERED) && g_ctxt->in_use == TRUE)
    {
        if (((qup_gpi_tre_available_ex(dcfg, g_ctxt, GPI_OUTBOUND_CHAN, &t_tre, &t_wp_idx)) < (io_ctxt->tx_tre_req).num_tre) ||
             (qup_gpi_tre_available_ex(dcfg, g_ctxt, GPI_INBOUND_CHAN, &r_tre, &r_wp_idx) < (io_ctxt->rx_tre_req).num_tre))
        {
            g_ctxt = g_ctxt->next;
        }
        else
        {
            break;
        }
    }

    //traverse to find unused g_ctxt, this will have whole TRE memory unused
    if(g_ctxt == NULL)
    {
        g_ctxt = (qup_gpi_core_ctxt *)  h_ctxt->core_iface;
        while(g_ctxt && QUP_IS_SET(g_ctxt->flags,GPI_IFACE_REGISTERED))
        {
            if(g_ctxt->in_use == FALSE)
            {
                break;
            }
            g_ctxt = g_ctxt->next;
        }
    }
    if(g_ctxt != NULL)
    {
        qup_gpi_tre_available_ex(dcfg, g_ctxt, GPI_OUTBOUND_CHAN, &t_tre, &t_wp_idx);
        qup_gpi_tre_available_ex(dcfg, g_ctxt, GPI_INBOUND_CHAN, &r_tre, &r_wp_idx);
    }
    else return SPI_ERROR_DMA_INSUFFICIENT_RESOURCES;

    io_ctxt->g_ctxt                  = g_ctxt;
    io_ctxt->tx_tre_req.handle       = g_ctxt->gpi_handle;
    io_ctxt->tx_tre_req.chan         = GPI_OUTBOUND_CHAN;

    io_ctxt->rx_tre_req.handle       = g_ctxt->gpi_handle;
    io_ctxt->rx_tre_req.chan         = GPI_INBOUND_CHAN;

    QUP_FLAG_SET(io_ctxt->flags,GPI_TX_COMPLETED | GPI_RX_COMPLETED);

    gpi_ring_elem *tx_base_tre = (gpi_ring_elem *) g_ctxt->ring_info[GPI_OUTBOUND_CHAN].ring_base_va;
    gpi_ring_elem *rx_base_tre = (gpi_ring_elem *) g_ctxt->ring_info[GPI_INBOUND_CHAN].ring_base_va;

    if(GPI_STATUS_SUCCESS != gpi_validate_channels(&(io_ctxt->tx_tre_req)) || GPI_STATUS_SUCCESS != gpi_validate_channels(&(io_ctxt->rx_tre_req)))
    {
        QUP_SE_LOG(dcfg, LEVEL_VERBOSE, "spi_gpi_submit_transfer invalid channels: QUP : %d, SE : %d",dcfg->qup_data->qup_id, dcfg->se_id);
        return SPI_ERROR_DMA_INSUFFICIENT_RESOURCES;
    }

    gpi_ring_elem *t_tre_local =  t_ctxt->buffer_list[GPI_OUTBOUND_CHAN];
    gpi_ring_elem *r_tre_local =  t_ctxt->buffer_list[GPI_INBOUND_CHAN];

    io_ctxt->tx_tre_req.tre_list = t_tre;
    io_ctxt->rx_tre_req.tre_list = r_tre;

    t_count = (io_ctxt->tx_tre_req).num_tre;
    r_count = (io_ctxt->rx_tre_req).num_tre;

    QUP_SE_LOG(dcfg, LEVEL_VERBOSE, "spi_gpi_submit_transfer t_tre : 0x%x, t_wp_idx : %d, r_tre : 0x%x, r_wp_idx : %d ",t_tre, t_wp_idx, r_tre, r_wp_idx);

    QUP_SE_LOG(dcfg, LEVEL_VERBOSE, "spi_gpi_submit_transfer after update t_tre : 0x%x, r_tre : 0x%x ",t_tre, r_tre);

    tx_num_elem = gpi_get_chan_num_elem(io_ctxt->tx_tre_req.handle, GPI_OUTBOUND_CHAN);
    rx_num_elem = gpi_get_chan_num_elem(io_ctxt->rx_tre_req.handle, GPI_INBOUND_CHAN);

    io_ctxt->rx_tre_req.num_elem = rx_num_elem;
    io_ctxt->tx_tre_req.num_elem = tx_num_elem;

    QUP_SE_LOG(dcfg, LEVEL_ERROR, "spi_gpi_submit_transfer tx_num_elem : %d, rx_num_elem : %d ",tx_num_elem, rx_num_elem);

    if((tx_num_elem - t_wp_idx) < (io_ctxt->tx_tre_req).num_tre)
    {
        qup_mem_copy(t_tre, (tx_num_elem - t_wp_idx)*sizeof(gpi_ring_elem), t_tre_local, (tx_num_elem - t_wp_idx)*sizeof(gpi_ring_elem));
        QUP_SE_LOG(dcfg, LEVEL_VERBOSE, "spi_gpi_submit_transfer t_tre_local : 0x%x", t_tre_local);
        t_tre_local += (tx_num_elem - t_wp_idx);
        QUP_SE_LOG(dcfg, LEVEL_VERBOSE, "spi_gpi_submit_transfer after update t_tre_local : 0x%x", t_tre_local);
        t_tre = tx_base_tre;
        elem_left = (io_ctxt->tx_tre_req).num_tre - (tx_num_elem - t_wp_idx);
        QUP_SE_LOG(dcfg, LEVEL_VERBOSE, "spi_gpi_submit_transfer elem_left : %d", elem_left);
        if (elem_left)
        {
            qup_mem_copy(t_tre, (elem_left)*sizeof(gpi_ring_elem), t_tre_local, elem_left*sizeof(gpi_ring_elem));
        }
    }
    else qup_mem_copy(t_tre, (io_ctxt->tx_tre_req).num_tre * sizeof(gpi_ring_elem), t_tre_local, (io_ctxt->tx_tre_req).num_tre * sizeof(gpi_ring_elem));

    if((rx_num_elem - r_wp_idx) < (io_ctxt->rx_tre_req).num_tre)
    {
        qup_mem_copy(r_tre, (rx_num_elem - r_wp_idx)*sizeof(gpi_ring_elem), r_tre_local, (rx_num_elem - r_wp_idx)*sizeof(gpi_ring_elem));
        r_tre_local += (rx_num_elem - r_wp_idx);
        r_tre = rx_base_tre;
        elem_left = (io_ctxt->rx_tre_req).num_tre - (rx_num_elem - r_wp_idx);
        if (elem_left)
        {
            qup_mem_copy(r_tre, elem_left*sizeof(gpi_ring_elem), r_tre_local, elem_left*sizeof(gpi_ring_elem));
        }
    }
    else qup_mem_copy(r_tre, (io_ctxt->rx_tre_req).num_tre * sizeof(gpi_ring_elem), r_tre_local, (io_ctxt->rx_tre_req).num_tre * sizeof(gpi_ring_elem));

    region = dcfg->flags & USES_INTERNAL_DDR_MEM ? REGION_DDR : REGION_PRAM;
/*
    if (qurt_island_get_status ())
    {
        spi_island_calls++;
    }*/

    if (r_count)
    {
        io_ctxt->rx_tre_req.user_data = c_ctxt;
        io_ctxt->flags &= (~GPI_RX_COMPLETED);
        qup_mem_flush_cache (rx_base_tre, sizeof(gpi_ring_elem) * MAX_RX_TRE_LIST_SIZE_PER_CORE,ATTRIB_TRE,region);
        spi_gpi_dump_tres(dcfg,io_ctxt->rx_tre_req.tre_list, rx_base_tre, r_count, FALSE,region,rx_num_elem);
        gpi_ret_val = gpi_process_tre(&io_ctxt->rx_tre_req, r_wp_idx);
        if(GPI_STATUS_SUCCESS != gpi_ret_val)
        {
            QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_gpi_submit_transfer : gpi_process_tre for RX failed with val %d, handle 0x%x",gpi_ret_val, c_ctxt);
            return SPI_ERROR_DMA_PROCESS_TRE_FAIL;
        }
    }

    if (t_count)
    {
        io_ctxt->tx_tre_req.user_data    = c_ctxt;
        io_ctxt->flags &= (~GPI_TX_COMPLETED);
        /* the tre list may wrap around, instead of special handling for the wrapped-around portion,
        lets just flush the full tre list*/
        qup_mem_flush_cache (tx_base_tre, sizeof(gpi_ring_elem) * MAX_TX_TRE_LIST_SIZE_PER_CORE,ATTRIB_TRE,region);
        spi_gpi_dump_tres(dcfg,io_ctxt->tx_tre_req.tre_list, tx_base_tre, t_count, TRUE,region,tx_num_elem);
        gpi_ret_val = gpi_process_tre(&io_ctxt->tx_tre_req, t_wp_idx);
        if(GPI_STATUS_SUCCESS != gpi_ret_val)
        {
            QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_gpi_submit_transfer : gpi_process_tre for TX failed with val %d, handle 0x%x",gpi_ret_val, c_ctxt);
            return SPI_ERROR_DMA_PROCESS_TRE_FAIL;
        }
    }
    g_ctxt->in_use = TRUE;
    //if polled mode = FALSE, do nothing else. spi_gpi_callback will be invoked from GSI interrupt. else..
    if(dcfg->flags & POLLED_MODE)
    {
        uint32 timeout = GPI_POLLING_TIMEOUT_US;
        while ((t_ctxt->transfer_state != TRANSFER_DONE) && timeout)
        {
            qup_gpi_isr (h_ctxt);
            qup_delay_us (GPI_POLLING_INTERVAL_US);
            timeout -= GPI_POLLING_INTERVAL_US;
        }
        if ((timeout == 0) && (t_ctxt->transfer_state != TRANSFER_DONE))
        {
        //spi_gpi_callback did not happen, we still owe the client a callback.
        status = SPI_ERROR_TRANSFER_TIMEOUT;
        t_ctxt->transfer_state = TRANSFER_TIMED_OUT;
        if(t_ctxt->cfg.spi.async)
        {
            t_ctxt->cfg.spi.callback(status,t_ctxt->cfg.spi.ctxt);
            g_ctxt->in_use = FALSE;
        }
        }
        else if (t_ctxt->cfg.spi.callback == NULL)
        {
            // if we did not time out and callback is NULL, update the status
            status = t_ctxt->cfg.spi.xfer_status;
        }
    }

    return status;
}