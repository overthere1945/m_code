/**
    @file  spi_gpi_island.c
    @brief SPI GSI implementation
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#include "qup_drv.h"
#include "spi_gpi.h"
#include "spi_plat.h"
#include "qup_log.h"
#include "qurt.h"
#include "qurt_island.h"
#include "qup_common.h"
#include "qup_alloc.h"
#include "qup_os.h"
#include "qup_plat.h"

#ifdef QUP_KPI_PROFILING
#include "CoreTime.h"
#endif

#define ADVANCE_TRE(dir, count, num_avail_tre, tre, base_tre, max_tre_list_size)           \
    if ((count) > num_avail_tre)                                        \
    {                                                                   \
        return SPI_ERROR_DMA_INSUFFICIENT_RESOURCES;                    \
    }                                                                   \
    else                                                                \
    {                                                                   \
        (count)++;                                                      \
        (tre)++;                                                        \
        if ((base_tre + max_tre_list_size) == tre)     \
        {                                                               \
            tre = base_tre;                                             \
        }                                                               \
    }

// TODO: set this to 8 to enable IMM DMA
//#define SPI_IMM_DMA_LENGTH 0



uint32 spi_island_calls = 0;
/*
static void get_imm_read_data (uint8 *buffer, uint32 dword0, uint32 dword1, uint32 length)
{
    uint8 i, shift;

    for (i = 0; i < length; i++)
    {
        shift = (i & 3) << 3;
        if (i < 4) { buffer[i] = (uint8) (dword0 >> shift); }
        else       { buffer[i] = (uint8) (dword1 >> shift); }
    }
}
*/

void spi_handle_tf_completion (qup_client_ctxt *c_ctxt)
{

    qup_hw_ctxt         *h_ctxt = c_ctxt->h_ctxt;
    qup_client_ctxt     *temp_curr,*temp_prev = NULL;
    qup_transfer_ctxt   *t_ctxt;
    se_dev_cfg          *dcfg = (se_dev_cfg *) h_ctxt->core_config;
    spi_status_t         status = SPI_SUCCESS;

    boolean              found = FALSE;
//    boolean has_internal_queueing = bus_iface_has_queueing(h_ctxt);

//
//
    qup_mutex_instance_lock (h_ctxt->queue_lock, QUEUE_LOCK);

//  if(dcfg->flags & ENABLE_TIMEOUT)
//  {
//       //Clear Timer for present transfer
//      qup_timer_clear(h_ctxt->timer_handle);
//  }
//
    // remove the client from the queue
    temp_curr = h_ctxt->client_ctxt_head;
    if (temp_curr == NULL)
    {
        QUP_LOG(LEVEL_INFO, "NOTE: client_ctxt_head already completed");
        qup_mutex_instance_unlock (h_ctxt->queue_lock, QUEUE_LOCK);
        return;
    }

        while(temp_curr)
        {
            if(temp_curr == c_ctxt)
            {
                found = TRUE;
                break;
            }
            temp_prev = temp_curr;
            temp_curr = temp_curr->next;
        }

        if(!found )
    {
        QUP_LOG(LEVEL_INFO, "NOTE: client_ctxt not found in linked list");
        qup_mutex_instance_unlock (h_ctxt->queue_lock, QUEUE_LOCK);
        return;
    }

        if(temp_prev == NULL)
        {
            h_ctxt->client_ctxt_head = temp_curr->next;
        }
        else
        {
            temp_prev->next = temp_curr->next;
        }
        temp_curr->next = NULL;

        // get transfer context
        t_ctxt = &(temp_curr->t_ctxt);

        //Restart the timer if next client is queued in HW
//        if (h_ctxt->client_ctxt_head != NULL)
//        {
//            if(dcfg->flags & ENABLE_TIMEOUT)
//            {
//                qup_timer_set(h_ctxt->timer_handle, MAX_TRANSFER_TIMEOUT);
//            }
//        }

        // this has to be done in the end to avoid race between polling thread
        // and gpi thread

        if(t_ctxt->transfer_state == TRANSFER_DONE)
        {
            status = SPI_SUCCESS;
        }
        else if ((t_ctxt->transfer_state == TRANSFER_TIMED_OUT) ||
                 (t_ctxt->transfer_state == TRANSFER_BUS_RESET))
        {
            status = SPI_ERROR_TRANSFER_TIMEOUT;
        }
        else
        {
            //FATAL
        }

        //invalidate_read_descriptors if not in island.
        if(dcfg->flags & USES_DDR_BUFFER)
        {
            uint32 i =0;
            for (i = 0; i < t_ctxt->cfg.spi.num_descs; i++)
            {
                spi_descriptor_t *dc = &(t_ctxt->cfg.spi.desc[i]);
                if (dc->rx_buf != NULL && qup_os_is_client_root(c_ctxt))
                {
                    qup_mem_invalidate_cache((void *) dc->rx_buf, dc->len,ATTRIB_BUFFER,REGION_DDR);
                }
            }
        }

        qup_mutex_instance_unlock (h_ctxt->queue_lock, QUEUE_LOCK);

        t_ctxt->cfg.spi.xfer_status = status;

        if(t_ctxt->cfg.spi.async)
        {
            QUP_LOG(LEVEL_INFO, "spi_notify_completion_cb");
            t_ctxt->cfg.spi.callback(status,t_ctxt->cfg.spi.ctxt);
#ifdef QUP_KPI_PROFILING
            t_ctxt-> xfer_end_timestamp = CoreTimetick_Get64();
#endif
            if(!t_ctxt->multi_transfer_config)
            {
                qup_dealloc_cfg_desc_pair (dcfg, t_ctxt->cfg.spi.config_desc_pairs, sizeof(spi_config_desc_pairs));
            }
        }
        else if (!(dcfg->flags & POLLED_MODE))
        {
           qup_signal_event(h_ctxt->transfer_signal,TRANSFER_SIGNAL_MASK);
        }
//
//        // check if more clients are queued
//        if ((h_ctxt->client_ctxt_head  != NULL) && (has_internal_queueing == FALSE))
//        {
//            i_status = bus_iface_queue_transfer (h_ctxt->client_ctxt_head );
//            if (I2C_SUCCESS(i_status))
//            {
//                t_ctxt = &(h_ctxt->client_ctxt_head->t_ctxt);
//                t_ctxt->transfer_state = TRANSFER_IN_PROGRESS;
//                break;
//            }
//        }
//    }
//    while ((h_ctxt->client_ctxt_head != NULL) && (has_internal_queueing == FALSE));
}

/**
 * @brief Update a Transfer Ring Element (TRE) for SPI 3W RX case
 *
 * This function updates the given TRE for SPI communication by setting the appropriate
 * command, flags, and receive length based on the provided transfer context and descriptor.
 *
 * @param tre Pointer to the gpi_ring_elem structure representing the TRE.
 * @param t_ctxt Pointer to the qup_transfer_ctxt structure containing the transfer context.
 * @param dc Pointer to the spi_descriptor_t structure containing the SPI descriptor.
 */
void spi3w_update_go_tre_for_rx(gpi_ring_elem *tre, qup_transfer_ctxt *t_ctxt, spi_descriptor_t *dc)
{
    uint8 N = t_ctxt->cfg.spi.config->spi_bits_per_word;
    uint32 rx_len = (N) ? ((dc->len * 8) / N) : dc->len;
    uint8 flags = 0;
    uint32 dword0_mask = GPI_FIELD_MASK(TRE_SPI_GO_CS) | GPI_FIELD_MASK(TRE_SPI_GO_FLAGS_EX) | GPI_FIELD_MASK(TRE_SPI_GO_FLAGS);

    // Check if the timestamp configuration includes GET_SPI_CS_DE_ASSERT_TIMESTAMP
    if (t_ctxt->cfg.spi.timestamp & GET_SPI_CS_DE_ASSERT_TIMESTAMP)
    {
        // Set the timestamp after flag
        flags |= (1 << GPI_TRE_SPI_GO_FLAGS_TSTAMP_AFTER__MASK);
    }

    // Clear the cmd field in tre->dword_0 using the mask
    tre->dword_0 &= dword0_mask;

    // Update tre->dword_0 with the new command and flags
    tre->dword_0 |= GPI_BUILD_TRE_SPI_GO_0(GPI_SPI_GO_CMD_TX_RX, 0, 0, flags);

    // Set tre->dword_2 with the receive length
    tre->dword_2 = GPI_BUILD_TRE_SPI_GO_2(rx_len);

    // Update the control field with link rx 
    tre->ctrl |= GPI_BUILD_TRE_CTRL(0, 0, 1, 0, 0, 0, 0);
}

void spi_gpi_set_tre_go (gpi_ring_elem *tre, qup_transfer_ctxt *t_ctxt, se_dev_cfg *dcfg )
{
    spi_descriptor_t *dc = NULL;
    uint8 cs = t_ctxt->cfg.spi.config->spi_slave_index;
    uint8 N = t_ctxt->cfg.spi.config->spi_bits_per_word;
    uint8 flags = 0; // TBD: Use this for configurable timestamps and delays.
    uint16 flags_ex = 0;
    uint8 cmd = 0;
    uint8 chain = 1;
    uint8 link_rx = 0;
    uint32 rx_len = 0;
    if(t_ctxt->multi_transfer_config == TRUE)
    {
        dc = t_ctxt->cfg.spi.desc ;
    }
    else dc =  (t_ctxt->cfg.spi.desc + t_ctxt->transfer_count);

    if(dcfg->protocol_in_use == SE_PROTOCOL_SPI_3W)
    {
        flags_ex |= (1<< GPI_TRE_SPI_GO_FLAGS_EX_MODE_SEL__SHIFT);

        if (dc->tx_buf && dc->rx_buf == NULL)
        {
            cmd = GPI_SPI_GO_CMD_TX_ONLY;
        }
    }
    else
    {
        if (dc->rx_buf && dc->tx_buf)
        {
            cmd = GPI_SPI_GO_CMD_FULL_DUPLEX;
            link_rx = 1;
            rx_len = (N)?( (dc->len * 8) / N) : dc->len;
        }
        else if (dc->tx_buf && dc->rx_buf == NULL)
        {
            cmd = GPI_SPI_GO_CMD_TX_ONLY;
        }
        else if (dc->rx_buf && dc->tx_buf == NULL)
        {
            cmd = GPI_SPI_GO_CMD_RX_ONLY;
            chain = 0; link_rx = 1;
            rx_len = (N)?( (dc->len * 8) / N) : dc->len;
        }

        if (dc->leave_cs_asserted)
        {
           flags |= (1 << GPI_TRE_SPI_GO_FLAGS_FRAG__SHIFT);
        }
   }

    if(t_ctxt->cfg.spi.timestamp)
    {
        if((t_ctxt->cfg.spi.timestamp & GET_SPI_CS_ASSERT_TIMESTAMP) &&
           (t_ctxt->transfer_count == 0))
        {
            flags |= (1<< GPI_TRE_SPI_GO_FLAGS_TSTAMP_BEFORE__SHIFT);
        }
        else if ((t_ctxt->cfg.spi.timestamp & GET_SPI_CS_DE_ASSERT_TIMESTAMP) &&
                 (t_ctxt->transfer_count == ((t_ctxt->cfg.spi.num_descs *t_ctxt->cfg.spi.num_descs_pairs) - 1)))
        {
            flags |= (1<< GPI_TRE_SPI_GO_FLAGS_TSTAMP_AFTER__MASK);
        }
    }

    tre->dword_0 = GPI_BUILD_TRE_SPI_GO_0 (cmd, cs, flags_ex,flags);
    tre->dword_1 = 0;
    tre->dword_2 = GPI_BUILD_TRE_SPI_GO_2 (rx_len);
    tre->ctrl    = GPI_BUILD_TRE_CTRL (TRE_GO_MAJOR, TRE_GO_MINOR,
                       link_rx, /*LINK_RX*/
                       0,       /*BEI*/
                       0,       /*IEOT*/
                       0,       /*IEOB*/
                       chain);
}

void spi_gpi_set_tre_config_1 (gpi_ring_elem *tre, qup_client_ctxt *c_ctxt) {
    tre->dword_0 = GPI_BUILD_TRE_SPI_CONFIG_1_0;
    tre->ctrl    = GPI_BUILD_TRE_CTRL (TRE_CONFIG_1_MAJOR,
                    TRE_CONFIG_1_MINOR,
                    0,   /*LINK_RX*/
                    0,   /*Block Event Interrupt*/
                    0,   /*IEOT*/
                    0,   /*IEOB*/
                    1);  /*CH*/
}

void spi_gpi_set_tre_config_0 (gpi_ring_elem *tre, qup_client_ctxt *c_ctxt)
{
    qup_transfer_ctxt *t_ctxt  = &c_ctxt->t_ctxt;
    qup_hw_ctxt *h_ctxt = (qup_hw_ctxt *)c_ctxt->h_ctxt;
    se_dev_cfg  *cfg = (se_dev_cfg  *)h_ctxt->core_config;
    spi_config_t *c        = t_ctxt->cfg.spi.config;
    uint8 config_0_flags = 0, cpol = 0, cpha = 0, coded_spi_word_len;
    uint32 div = 0, dfs_index = 0;
    config_0_flags |= c->loopback_mode;
    config_0_flags |= c->cs_toggle << 3;


    switch (t_ctxt->cfg.spi.config->spi_mode)
    {
       case SPI_MODE_1 : cpol = 0; cpha = 1; break;
       case SPI_MODE_2 : cpol = 1; cpha = 0; break;
       case SPI_MODE_3 : cpol = 1; cpha = 1; break;
       case SPI_MODE_0 :
       default         : cpol = 0; cpha = 0; break;
    }
    config_0_flags |= cpha << 4;
    config_0_flags |= cpol << 5;

    /*c->spi_bits_per_word is already validated in api.c*/
    coded_spi_word_len =  c->spi_bits_per_word - 4;

    tre->dword_0 = GPI_BUILD_TRE_SPI_CONFIG_0_0 (coded_spi_word_len, config_0_flags, SPI_RX_TX_PACKING_ENABLE);
    tre->dword_1 = GPI_BUILD_TRE_SPI_CONFIG_0_1 (c->inter_word_delay_cycles, c->cs_clk_delay_cycles, 0);


    spi_plat_get_div_dfs(cfg, c->clk_freq_hz, &dfs_index, &div);
    tre->dword_2 = GPI_BUILD_TRE_SPI_CONFIG_0_2 (div, dfs_index);

    tre->ctrl    = GPI_BUILD_TRE_CTRL (TRE_CONFIG_0_MAJOR, TRE_CONFIG_0_MINOR,
                       0,   /*LINK_RX*/
                       0,   /*Block Event Interrupt*/
                       0,   /*IEOT*/
                       0,   /*IEOB*/
                       1);  /*CH*/
}

boolean spi_gpi_tre_non_data_opcode (gpi_ring_elem *tre)
{
    uint8 command = (tre->dword_0 >> GPI_FIELD_SHIFT(TRE_SPI_GO_CMD)) &
                                     GPI_FIELD_MASK (TRE_SPI_GO_CMD);

    if ((command == GPI_SPI_GO_CMD_CS_ASSERT)    ||
        (command == GPI_SPI_GO_CMD_CS_DEASSERT)  ||
        (command == GPI_SPI_GO_CMD_SCK_ONLY))
    {
        return TRUE;
    }

    return FALSE;
}


void spi_gpi_set_tre_dma (qup_client_ctxt *c_ctxt, gpi_ring_elem *tre, uint8 *buf, spi_descriptor_t *dc)
{
    qup_hw_ctxt       *h_ctxt =  c_ctxt->h_ctxt;
    //qup_transfer_ctxt *t_ctxt = &(c_ctxt->t_ctxt);
    se_dev_cfg        *dcfg   = (se_dev_cfg *) h_ctxt->core_config;
    uint8 bei                 = 0;
    /*uint32 data = 0;
    uint16 i;

    if (dc->length <= SPI_IMM_DMA_LENGTH)
    {
        if (dc->flags & I2C_FLAG_WRITE)
        {
            for (i = 0; (i < dc->length) && (i < 4); i++)
            {
                data = data | (dc->buffer[i] << ((3 - i) << 3));
            }
            tre->dword_0 = GPI_BUILD_DMA_IMM_TRE_0(data);
            if (dc->length > 4)
            {
                for (i = 4; (i < dc->length) && (i < 8); i++)
                {
                    data = data | (dc->buffer[i] << ((7 - i) << 3));
                }
                tre->dword_1 = GPI_BUILD_DMA_IMM_TRE_1(data);
            }
        }
        else
        {
            tre->dword_0 = GPI_BUILD_DMA_IMM_TRE_0(data);
            tre->dword_1 = GPI_BUILD_DMA_IMM_TRE_1(data);
        }
        tre->dword_2 = GPI_BUILD_DMA_IMM_TRE_2(dc->length);
        tre->ctrl    = GPI_BUILD_TRE_CTRL(TRE_DMA_IMM_MAJOR, TRE_DMA_IMM_MINOR, 0, 0, 0, 0);
    }
    else*/

    uint64 dma_addr = (uint64) buf;
    qup_mem_region_type region  = REGION_PRAM;

    dma_addr = (uint64 )qup_mem_virt_to_phys(c_ctxt->pid, (void *)buf, ATTRIB_BUFFER, REGION_PRAM);
    if(qup_os_is_client_root(c_ctxt))
    {
        if(dcfg->flags & USES_DDR_BUFFER)
        {
            region = REGION_DDR;
        }
        qup_mem_flush_cache ((void *)buf, dc->len, ATTRIB_BUFFER, region);
    }

    /* For Non Multi EE case BEI must not be set because DMA with leave_cs_asserted
       will be the last TRE */
    if (QUP_IS_NOT_SET(dcfg->flags, SHARED_SE) && (dc->leave_cs_asserted)) bei = 0;

    tre->dword_0 = GPI_BUILD_DMA_W_BUFFER_TRE_0((uint32) dma_addr);
    tre->dword_1 = GPI_BUILD_DMA_W_BUFFER_TRE_1((uint32) (dma_addr >> 32));
    tre->dword_2 = GPI_BUILD_DMA_W_BUFFER_TRE_2(dc->len);
    //each of our descriptors consists only one 1 buf of any direction OR 1 buf of each direction.
    //EOT = 1, and CH = 0 is mandatory, since the next descriptor starts with a GO and is
    //therefore a fresh transfer.
    tre->ctrl    = GPI_BUILD_TRE_CTRL(TRE_DMA_W_BUFFER_MAJOR, TRE_DMA_W_BUFFER_MINOR,
                        0,   /*LINK_RX*/
                        bei, /*Block Event Interrupt*/
                        1,   /*IEOT*/
                        0,   /*IEOB*/
                        0);  /*CHAIN*/

    //t_ctxt->transferred += dc->len;

}

void spi_gpi_dump_tres(se_dev_cfg *cfg,gpi_ring_elem *tre_list, gpi_ring_elem *base, uint32 count, boolean is_tx, qup_mem_region_type region, uint32 tre_list_size)
{
    uint32 i = 0;

    if(region == REGION_DDR)
    {
        while (i < count)
        {
            QUP_SE_LOG(cfg,LEVEL_INFO, "spi_gpi_dump_tres : %08x %08x %08x %08x\n", tre_list->dword_0, tre_list->dword_1,
                    tre_list->dword_2, tre_list->ctrl);
            i++;
            tre_list++;
            if (tre_list == (base + tre_list_size))
            {
                tre_list = base;
            }
        }
    }
}

spi_status_t spi_gpi_queue_transfer (qup_client_ctxt *c_ctxt)
{
    qup_hw_ctxt         *h_ctxt  =  c_ctxt->h_ctxt;
    qup_transfer_ctxt   *t_ctxt  = &c_ctxt->t_ctxt;
    qup_gpi_iface_ctxt  *io_ctxt = (qup_gpi_iface_ctxt *)t_ctxt->io_ctxt;
    qup_gpi_core_ctxt   *g_ctxt  = (qup_gpi_core_ctxt *)h_ctxt->core_iface;
    se_dev_cfg          *dcfg    = (se_dev_cfg *) h_ctxt->core_config;
    GPI_RETURN_STATUS gpi_ret_val = GPI_STATUS_SUCCESS;
    uint8 tx_num_elem, rx_num_elem;
    spi_descriptor_t *dc = NULL;

    spi_status_t status = SPI_SUCCESS;
    uint16 i,j;
    uint8   t_wp_idx,r_wp_idx;
    uint32 *t_count, *r_count;
    uint32 num_avail_tx_tre;
    uint32 num_avail_rx_tre;
    qup_mem_region_type region;
    gpi_ring_elem *t_tre = NULL;
    gpi_ring_elem *r_tre = NULL;

    gpi_ring_elem *tx_base_tre = (gpi_ring_elem *) g_ctxt->ring_info[GPI_OUTBOUND_CHAN].ring_base_va;
    gpi_ring_elem *rx_base_tre = (gpi_ring_elem *) g_ctxt->ring_info[GPI_INBOUND_CHAN].ring_base_va;

    gpi_ring_elem *last_tx_go_tre = NULL;

    if ((num_avail_tx_tre = qup_gpi_tre_available (h_ctxt, GPI_OUTBOUND_CHAN, &t_tre, &t_wp_idx)) == 0)
    {
        return SPI_ERROR_DMA_INSUFFICIENT_RESOURCES;
    }
    io_ctxt->tx_tre_req.tre_list = t_tre;

    if ((num_avail_rx_tre = qup_gpi_tre_available (h_ctxt, GPI_INBOUND_CHAN, &r_tre, &r_wp_idx)) == 0)
    {
        return SPI_ERROR_DMA_INSUFFICIENT_RESOURCES;
    }

    if(NULL == t_tre)
    {
        return SPI_ERROR_DMA_INSUFFICIENT_RESOURCES;
    }

    io_ctxt->rx_tre_req.tre_list = r_tre;

    (io_ctxt->tx_tre_req).num_tre = 0;
    (io_ctxt->rx_tre_req).num_tre = 0;

    t_count = (uint32*) &((io_ctxt->tx_tre_req).num_tre);
    r_count = (uint32*) &((io_ctxt->rx_tre_req).num_tre);

    io_ctxt->g_ctxt                  = g_ctxt;
    io_ctxt->tx_tre_req.handle       = g_ctxt->gpi_handle;
    io_ctxt->tx_tre_req.chan         = GPI_OUTBOUND_CHAN;

    io_ctxt->rx_tre_req.handle       = g_ctxt->gpi_handle;
    io_ctxt->rx_tre_req.chan         = GPI_INBOUND_CHAN;


    QUP_FLAG_SET(io_ctxt->flags,GPI_TX_COMPLETED | GPI_RX_COMPLETED);

    tx_num_elem = gpi_get_chan_num_elem(io_ctxt->tx_tre_req.handle, GPI_OUTBOUND_CHAN);
    rx_num_elem = gpi_get_chan_num_elem(io_ctxt->rx_tre_req.handle, GPI_INBOUND_CHAN);

    if(QUP_IS_SET(dcfg->flags, SHARED_SE))
    {
        qup_gpi_set_tre_lock (t_tre);
        ADVANCE_TRE(TX, *t_count, num_avail_tx_tre, t_tre, tx_base_tre, tx_num_elem)
    }

    if (!(h_ctxt->core_state & QUP_CORE_CONFIG_TRE_ISSUED) || (!(dcfg->flags & OPTIMISE_TRANSFERS)))
    {
        spi_gpi_set_tre_config_0 (t_tre, c_ctxt);
        ADVANCE_TRE(TX, *t_count, num_avail_tx_tre, t_tre, tx_base_tre, tx_num_elem)
        h_ctxt->core_state |= QUP_CORE_CONFIG_TRE_ISSUED;

        if(dcfg->is_pipeline_enable && (t_ctxt->cfg.spi.config->loopback_mode == 1))
        {
            spi_gpi_set_tre_config_1(t_tre, c_ctxt);
            ADVANCE_TRE(TX, *t_count, num_avail_tx_tre, t_tre, tx_base_tre, tx_num_elem)
        }
    }

    uint8 num_config_desc_pairs = t_ctxt->cfg.spi.num_descs_pairs;
    uint8 num_descs = 0;

    for(i = 0; i < num_config_desc_pairs; i++)
    {
        t_ctxt->cfg.spi.config = (t_ctxt->cfg.spi.config_desc_pairs + i)->config;
        t_ctxt->cfg.spi.desc   = (t_ctxt->cfg.spi.config_desc_pairs + i)->desc;
        num_descs  = (t_ctxt->cfg.spi.config_desc_pairs + i)->num_desc;

        for (j = 0; j < num_descs; j++)
        {
            dc = (t_ctxt->cfg.spi.desc + j);

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
                ADVANCE_TRE(TX, *t_count, num_avail_tx_tre, t_tre, tx_base_tre, tx_num_elem)
            }
            if (dc->tx_buf != NULL)
            {
                spi_gpi_set_tre_dma (c_ctxt, t_tre, dc->tx_buf, dc);
                ADVANCE_TRE(TX, *t_count, num_avail_tx_tre, t_tre, tx_base_tre, tx_num_elem)
            }

            if (dc->rx_buf != NULL)
            {
                spi_gpi_set_tre_dma (c_ctxt, r_tre, dc->rx_buf, dc);
                ADVANCE_TRE(RX, *r_count, num_avail_rx_tre, r_tre, rx_base_tre, rx_num_elem)
            }
            t_ctxt->transfer_count++;
        }
    }

    if(QUP_IS_SET(dcfg->flags, SHARED_SE))
    {
        qup_gpi_set_tre_unlock (t_tre);
        ADVANCE_TRE(TX, *t_count, num_avail_tx_tre, t_tre, tx_base_tre, tx_num_elem)
        io_ctxt->flags |= GPI_WAIT_FOR_UNLOCK_TRE;
    }

     region = dcfg->flags & USES_INTERNAL_DDR_MEM ? REGION_DDR : REGION_PRAM;

    if (QUP_SUCCESS != qup_wait_for_gpii_clean(g_ctxt))
    {
        QUP_SE_LOG(dcfg, LEVEL_ERROR, "ERROR! gpii is not recovered for next transfers ");
        return SPI_ERROR_DMA_INSUFFICIENT_RESOURCES;
    }

    if (*t_count)
    {
        io_ctxt->tx_tre_req.user_data    = c_ctxt;
        io_ctxt->flags &= (~GPI_TX_COMPLETED);
        /* the tre list may wrap around, instead of special handling for the wrapped-around portion,
        lets just flush the full tre list*/
        qup_mem_flush_cache (tx_base_tre, sizeof(gpi_ring_elem) * (*t_count),ATTRIB_TRE,region);
        spi_gpi_dump_tres(dcfg,io_ctxt->tx_tre_req.tre_list, tx_base_tre, *t_count, TRUE, region, tx_num_elem);
    }

    if (*r_count)
    {
        io_ctxt->rx_tre_req.user_data = c_ctxt;
        io_ctxt->flags &= (~GPI_RX_COMPLETED);
        qup_mem_flush_cache (rx_base_tre, sizeof(gpi_ring_elem) * (*r_count),ATTRIB_TRE,region);
        spi_gpi_dump_tres(dcfg,io_ctxt->rx_tre_req.tre_list, rx_base_tre, *r_count, FALSE, region, rx_num_elem);
    }

    if (qurt_island_get_status ())
    {
        spi_island_calls++;
    }

    if (*r_count)
    {
        gpi_ret_val = gpi_process_tre(&io_ctxt->rx_tre_req,r_wp_idx);
        if(GPI_STATUS_SUCCESS != gpi_ret_val)
        {
            QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_gpi_queue_transfer : gpi_process_tre for RX failed with val %d, handle 0x%x",gpi_ret_val, c_ctxt);
            return SPI_ERROR_DMA_PROCESS_TRE_FAIL;
        }
    }

    if (*t_count)
    {
        gpi_ret_val = gpi_process_tre(&io_ctxt->tx_tre_req,t_wp_idx);
        if(GPI_STATUS_SUCCESS != gpi_ret_val)
        {
            QUP_SE_LOG(dcfg,LEVEL_ERROR,"spi_gpi_queue_transfer : gpi_process_tre for TX failed with val %d, handle 0x%x",gpi_ret_val, c_ctxt);
            return SPI_ERROR_DMA_PROCESS_TRE_FAIL;
        }
    }

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
