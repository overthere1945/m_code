/**
    @file  qup_gpi_island.c
    @brief GPI interface Island implementation
 */

/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
02/04/26   KSN     Channel deinit update if PDR and timeout happens simultaneously
10/16/25   JAY     Enabled IEOB based optimization for KPI improvement
08/18/25   GKR     Changes to support SFE
07/15/25   GKR     Moved ERR FATAL under feature Flag. update geni ios read logic
07/02/25   SS      KW Fixes.
06/10/25   SS      Added changes to not wait for unlock TRE if lock TRE is not processed.
04/28/25   SS      Reverted the SWA for IEOB not getting generated.
04/15/25   SS      Added changes to return to client to issue bus recovery for SDA getting stuck
04/07/25   GKR     Changed noop_tre logic to handle corner cases
06/26/24   GKR     Support for multiple SSC QUPS
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
#include "qup_error.h"
#include "qup_plat.h"
#include "qup_gpi.h"
#include "qup_hal.h"
#include "err.h"

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/

#define GP_IRQ(x,offset) ((x >> offset) & 0xFF)

#define POLL_TIME_DEFAULT_US    409600
#define POLL_CMD_DEFAULT_US     5000
#define POLL_INTERVAL_US        10

#define LOG_CLEAN_STATE_DUMP_SIZE   32

#define MAX_GPII_RECOVERY_ATTEMPTS  3
/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/

typedef struct gpi_clean_dumps
{
    GPI_CLIENT_HANDLE      handle;
    gpi_clean_state        previous;
    GPI_CHAN_CMD           cmd;
    GPI_CHAN_TYPE          chan;
    gpi_clean_state        next;
}gpi_clean_dumps;

/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/


gpi_clean_dumps log_gpi_clean[LOG_CLEAN_STATE_DUMP_SIZE];

/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/
/**
 * @brief Logs the Transfer Request Elements (TREs) for both TX and RX requests.
 *
 * This function logs the TREs for the transmit (TX) and receive (RX) requests
 * in a QUP (Qualcomm Universal Peripheral) context. It iterates through the
 * list of TREs and logs each one using the QUP_SE_LOG macro.
 *
 * @param c_ctxt Pointer to the QUP client context.
 */
static void qup_log_tres(qup_client_ctxt *c_ctxt)
{
    qup_hw_ctxt         *h_ctxt  = c_ctxt->h_ctxt;
    se_dev_cfg          *dcfg    = (se_dev_cfg *) (h_ctxt->core_config);
    qup_gpi_iface_ctxt  *io_ctxt = (qup_gpi_iface_ctxt *)c_ctxt->t_ctxt.io_ctxt;
    qup_gpi_core_ctxt   *g_ctxt  = (qup_gpi_core_ctxt *) io_ctxt->g_ctxt;
    gpi_ring_elem *tre = NULL;
    gpi_ring_elem *tx_base_tre = (gpi_ring_elem *) g_ctxt->ring_info[GPI_OUTBOUND_CHAN].ring_base_va;
    gpi_ring_elem *rx_base_tre = (gpi_ring_elem *) g_ctxt->ring_info[GPI_INBOUND_CHAN].ring_base_va;
    uint32 i = 0;

    tre = io_ctxt->tx_tre_req.tre_list;
    for ( i = 0; i < io_ctxt->tx_tre_req.num_tre; i++)
    {
        if (i == 0) QUP_SE_LOG(dcfg,LEVEL_ERROR, "TX");
        QUP_SE_LOG(dcfg,LEVEL_ERROR,"%08x: %08x %08x %08x %08x", tre, tre->dword_0, tre->dword_1, tre->dword_2, tre->ctrl);
        tre++;
        if ((tx_base_tre + io_ctxt->tx_tre_req.num_elem) == tre) { tre = tx_base_tre; }
    }

    tre = io_ctxt->rx_tre_req.tre_list;
    for ( i = 0; i < io_ctxt->rx_tre_req.num_tre; i++)
    {
        if (i == 0) QUP_SE_LOG(dcfg,LEVEL_ERROR, "RX");
        QUP_SE_LOG(dcfg,LEVEL_ERROR,"%08x: %08x %08x %08x %08x", tre, tre->dword_0, tre->dword_1, tre->dword_2, tre->ctrl);
        tre++;
        if ((rx_base_tre + io_ctxt->rx_tre_req.num_elem) == tre) { tre = rx_base_tre; }
    }
}

static qup_client_ctxt *qup_gpi_get_client_to_be_cancelled(qup_hw_ctxt *h_ctxt)
{
    qup_client_ctxt *c_ctxt_list = h_ctxt->client_ctxt_head;
    qup_transfer_ctxt t_ctxt;
    qup_transfer_state t_state;

    while(c_ctxt_list != NULL)
    {
        t_ctxt    = c_ctxt_list->t_ctxt;
        t_state   = t_ctxt.transfer_state;
        if(t_state == TRANSFER_TO_BE_CANCELLED)
        {
            return c_ctxt_list;
        }
        c_ctxt_list = c_ctxt_list->next;
    }
    return NULL;
}

void noop_tres (qup_hw_ctxt *h_ctxt, qup_client_ctxt *c_ctxt)
{
    uint32 i, ctrl_mask = 0, stuck_tx_tre_idx = 0xFF;
    se_dev_cfg          *dcfg    = (se_dev_cfg *) (h_ctxt->core_config);
    qup_gpi_iface_ctxt  *io_ctxt = (qup_gpi_iface_ctxt *)c_ctxt->t_ctxt.io_ctxt;
    qup_gpi_core_ctxt   *g_ctxt  = (qup_gpi_core_ctxt *) io_ctxt->g_ctxt;
    qup_mem_region_type  region  = REGION_NONE;
    chan_status_info     chan_info;

    gpi_ring_elem *tx_ring_base = (gpi_ring_elem *) g_ctxt->ring_info[GPI_OUTBOUND_CHAN].ring_base_va;
    gpi_ring_elem *rx_ring_base = (gpi_ring_elem *) g_ctxt->ring_info[GPI_INBOUND_CHAN].ring_base_va;
    gpi_ring_elem *tx_curr_rp = NULL;
    gpi_ring_elem *tx_tre = (&(io_ctxt->tx_tre_req))->tre_list;
    gpi_ring_elem *rx_tre = (&(io_ctxt->rx_tre_req))->tre_list;

    uint32 tx_num_tre = (io_ctxt->tx_tre_req).num_tre;
    uint32 rx_num_tre = (io_ctxt->rx_tre_req).num_tre;

    region = dcfg->flags & USES_INTERNAL_DDR_MEM ? REGION_DDR : REGION_PRAM;

    if (GPI_STATUS_SUCCESS == gpi_query_chan_status(g_ctxt->gpi_handle,GPI_OUTBOUND_CHAN,&chan_info))
    {
        tx_curr_rp = tx_ring_base + chan_info.rp_index;
        QUP_SE_LOG(dcfg,LEVEL_INFO, "noop tx current rp : 0x%x", tx_curr_rp);
    }

    // noop TX
    for (i = 0; i < tx_num_tre; i++)
    {
        if (QUP_IS_SET(dcfg->flags, SHARED_SE))
        {
            // condition to check which TRE RP is pointing
            if (tx_curr_rp == tx_tre) stuck_tx_tre_idx = i;

            // condition to check current tre in the loop is unlock
            if (i == tx_num_tre - 1)
            {
                /* current rp idx indicating GSI has processed lock TRE
                   so we should skip nooping unlock TRE */
                if (stuck_tx_tre_idx > 0 && stuck_tx_tre_idx < tx_num_tre)
                {
                    break; //skip nooping unlock TRE if lock is processed
                }
                else
                {
                    QUP_SE_LOG(dcfg,LEVEL_INFO, "unlock TRE nooped");
                    /* Dont wait for unlock TRE event as it will be nooped */
                    QUP_FLAG_UNSET(io_ctxt->flags, GPI_WAIT_FOR_UNLOCK_TRE);
                }
            }
        }

        // mask for link_rx and chain
        ctrl_mask = GPI_BUILD_TRE_CTRL(0, 0, 1, 0, 0, 0, 1);
        // clear all fields except link_rx and chain
        tx_tre->ctrl = tx_tre->ctrl & ctrl_mask;
        // add NOOP
        tx_tre->ctrl = tx_tre->ctrl | GPI_BUILD_TRE_CTRL(TRE_NOOP_MAJOR, TRE_NOOP_MINOR, 0, 0, 0, 0, 0);
        qup_mem_flush_cache(tx_tre, sizeof(gpi_ring_elem), ATTRIB_TRE, region);
        tx_tre++;
        if ((tx_ring_base + io_ctxt->tx_tre_req.num_elem) == tx_tre) { tx_tre = tx_ring_base; }
    }

    QUP_SE_LOG(dcfg,LEVEL_VERBOSE, "TX noop");

    // noop RX
    for (i = 0; i < rx_num_tre; i++)
    {
        // mask for link_rx and chain
        ctrl_mask = GPI_BUILD_TRE_CTRL(0, 0, 1, 0, 0, 0, 1);
        // clear all fields except link_rx and chain
        rx_tre->ctrl = rx_tre->ctrl & ctrl_mask;
        // add NOOP
        rx_tre->ctrl = rx_tre->ctrl | GPI_BUILD_TRE_CTRL(TRE_NOOP_MAJOR, TRE_NOOP_MINOR, 0, 0, 0, 0, 0);
        qup_mem_flush_cache(rx_tre, sizeof(gpi_ring_elem), ATTRIB_TRE, region);
        rx_tre++;
        if ((rx_ring_base + io_ctxt->rx_tre_req.num_elem) == rx_tre) { rx_tre = rx_ring_base; }
    }
    QUP_SE_LOG(dcfg,LEVEL_VERBOSE, "RX noop");
}

qup_status qup_gpi_cancel (qup_hw_ctxt *h_ctxt, qup_client_ctxt *c_ctxt,boolean force_reset)
{
    qup_status  status = QUP_SUCCESS;

    if(h_ctxt == NULL || c_ctxt == NULL)
    {
        status = QUP_ERROR_INVALID_PARAMETER;
        goto exit;
    }

    qup_transfer_ctxt    *t_ctxt  = (qup_transfer_ctxt *)  &(c_ctxt->t_ctxt);
    qup_gpi_iface_ctxt   *io_ctxt = (qup_gpi_iface_ctxt *) t_ctxt->io_ctxt;
    qup_gpi_core_ctxt    *g_ctxt  = io_ctxt->g_ctxt;
    se_dev_cfg           *dcfg    = h_ctxt->core_config;

    if(t_ctxt->transfer_state == TRANSFER_DONE      ||
       t_ctxt->transfer_state == TRANSFER_CANCELED  ||
       t_ctxt->transfer_state == TRANSFER_TIMED_OUT)
    {
        QUP_SE_LOG(dcfg,LEVEL_INFO, "qup_gpi_cancel:Transfer Already completed for ctxt 0x%x state 0x%x",c_ctxt,t_ctxt->transfer_state);
        status = QUP_TRANSFER_COMPLETED;
        goto exit;
    }
    if(!(g_ctxt->flags & GPI_CLEAN_IN_FLIGHT))
    {
        if(force_reset)
        {
            SET_FORCE_RESET(g_ctxt->clean_state);
        }
        t_ctxt->transfer_state = TRANSFER_TO_BE_CANCELLED;
        g_ctxt->client_to_be_cancelled = qup_gpi_get_client_to_be_cancelled(h_ctxt);
        if(g_ctxt->client_to_be_cancelled != NULL)
        {
            if(dcfg->flags & ENABLE_TIMEOUT)
            {
               qup_timer_clear(dcfg, h_ctxt->timer_handle); // clear timer in case of bus errors
               qup_timer_set(dcfg, h_ctxt->timer_handle, MAX_CANCEL_TIMEOUT); // set timer for 50msec for recovery sequence
            }
            qup_gpi_iface_clean(g_ctxt->client_to_be_cancelled);
        }
        QUP_FLAG_UNSET(g_ctxt->flags,GPI_ERROR_FLAGS);
        QUP_FLAG_SET(io_ctxt->flags,GPI_WAIT_FOR_CLEAN);
    }
    else
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR, "Clean command Already set for  ctxt 0x%x",g_ctxt->client_to_be_cancelled);
        status = QUP_ERROR_HANDLE_ALREADY_IN_QUEUE;
        goto exit;
    }

 exit:
    return status;
}

/**
 * @brief Reads the GENI_IOS (Input/Output Status) register for a Serial Engine (SE).
 *
 * This function checks the current state of the I/O lines for a given Serial Engine (SE)
 * configured for I2C or I3C protocol. If the lines are not in the expected high state and
 * the `force_default` flag is set, it attempts to reset the lines to their default state
 * by writing to the GENI_FORCE_DEFAULT register and re-reading the IOS register.
 *
 * @param[in] dcfg Pointer to the SE device configuration structure. This includes
 *                base addresses and protocol information.
 * @param[in] force_default Boolean flag indicating whether to force the default
 *                          state if the I/O lines are not in the expected state.
 *
 * @return qup_geni_ios The current state of the GENI_IOS register after optional
 *                      force default operation. Returns QUP_GENI_IO_INVALID if the
 *                      protocol is not I2C or I3C.
 *
 * @note This function is specific to SEs using I2C or I3C protocols. For other
 *       protocols, it returns QUP_GENI_IO_INVALID without performing any operations.
 */
qup_geni_ios qup_read_geni_ios(se_dev_cfg* dcfg, boolean force_default)
{
    uint8* se_base = dcfg->se_base_addr;
    qup_geni_ios geni_ios = QUP_GENI_IO_INVALID;

    if ((GET_PROTOCOL_MAJOR(dcfg->protocol_in_use) == SE_PROTOCOL_I2C) ||
        (GET_PROTOCOL_MAJOR(dcfg->protocol_in_use) == SE_PROTOCOL_I3C))
    {
        geni_ios = HWIO_INX(GENI_DATA_BASE(se_base), GENI_IOS) & QUP_GENI_IOS_MASK;

        if (geni_ios != QUP_GENI_IO_HIGH && force_default)
        {
            /* Lines are not proper. Write Force Default. */
            HWIO_OUTX(GENI_CFG_BASE(se_base), GENI_FORCE_DEFAULT_REG, 0x1);

            /* Read IOS again after forcing default. */
            geni_ios = HWIO_INX(GENI_DATA_BASE(se_base), GENI_IOS) & QUP_GENI_IOS_MASK;
        }
    }

    return geni_ios;
}

void qup_gpi_handle_timer_cb(void *data)
{
    qup_hw_ctxt         *h_ctxt       = (qup_hw_ctxt *)data;
    qup_client_ctxt     *c_ctxt       = NULL;
    qup_gpi_iface_ctxt  *io_ctxt      = NULL;
    qup_transfer_ctxt   *t_ctxt       = NULL;
    se_dev_cfg          *dcfg         = NULL;
    qup_gpi_core_ctxt   *g_ctxt       = NULL;
    chan_status_info     chan_info;
    qup_geni_ios geni_ios = QUP_GENI_IO_INVALID;

    if (h_ctxt == NULL) { QUP_LOG(LEVEL_ERROR, "bus_iface_timer_callback : failed to initialize hw_ctxt"); return; }

    c_ctxt = h_ctxt->client_ctxt_head;
    dcfg   = (se_dev_cfg *) h_ctxt->core_config;

    QUP_SE_LOG(dcfg,LEVEL_ERROR, "bus_iface_timer_callback :0x%08x",h_ctxt);

    if (c_ctxt != NULL)
    {
        t_ctxt  = (qup_transfer_ctxt *)  &c_ctxt->t_ctxt;
        io_ctxt = (qup_gpi_iface_ctxt *)   t_ctxt->io_ctxt;
        g_ctxt  = io_ctxt->g_ctxt;
    }
    else
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR, "bus_iface_timer_callback : No active client");
        return;
    }

    // capture HW registers
    if(dcfg->flags & ENABLE_REGDUMP)
    {
//        bus_iface_reg_capture(h_ctxt);
    }

    if(dcfg->flags & ENABLE_FATAL || io_ctxt->recovery_attempts >= MAX_GPII_RECOVERY_ATTEMPTS )
    {
        if(GET_QUP_MAJOR(dcfg->qup_data->qup_id) == SSC_QUP)
        {
            ERR_FATAL("SSC QUP bus recovery fatal QUP id:%d, SE id: %d, recovery_count : %d ",dcfg->qup_data->qup_id, dcfg->se_id, io_ctxt->recovery_attempts);
        }
        else
        {
            ERR_FATAL("TOP QUP bus recovery fatal QUP id:%d, SE id: %d recovery_count : %d ",dcfg->qup_data->qup_id, dcfg->se_id, io_ctxt->recovery_attempts);
        }
    }

    qup_log_tres(c_ctxt);

    io_ctxt->recovery_attempts++;

    geni_ios = qup_read_geni_ios(dcfg, TRUE);

    if (geni_ios < QUP_GENI_IO_HIGH )
    {
        if(geni_ios == QUP_GENI_SDA_LOW)
        {
            /* Update error status to issue bus clear */
            t_ctxt->cfg.i2c.transfer_status = I2C_ERROR_SLAVE_BAD_STATE;
        }
        else if (dcfg->flags & ENABLE_FATAL)
        {
            ERR_FATAL("QUP Fatal IO Line Bad state QUP id:%d, SE id: %d, IOS : %d ",dcfg->qup_data->qup_id, dcfg->se_id, geni_ios);
        }
        else
        {
            QUP_SE_LOG(dcfg,LEVEL_ERROR, "bus_iface_timer_callback : IO Line bad state  :  QUP id:%d, SE id: %d, IOS : %d",dcfg->qup_data->qup_id, dcfg->se_id, geni_ios);
        }
    }

    qup_mutex_instance_lock(h_ctxt->queue_lock,QUEUE_LOCK);

    if (GPI_STATUS_SUCCESS == gpi_query_chan_status(g_ctxt->gpi_handle,GPI_INBOUND_CHAN,&chan_info))
    {
        g_ctxt->chan_state[GPI_INBOUND_CHAN] = chan_info.chan_state;
        if (GPI_STATUS_SUCCESS == gpi_query_chan_status(g_ctxt->gpi_handle,GPI_OUTBOUND_CHAN,&chan_info))
        {
            if(QUP_IS_NOT_SET(g_ctxt->flags,GPI_CLEAN_IN_FLIGHT))
            {
                if (chan_info.rp_index == chan_info.wp_index)
                {
                    if (dcfg->flags & ENABLE_FATAL)
                    {
                        ERR_FATAL("QUP Fatal due to irq miss post xfer complete QUP id:%d, SE id: %d, %d ",dcfg->qup_data->qup_id, dcfg->se_id, 0);
                    }
                    else
                    {
                        QUP_SE_LOG(dcfg,LEVEL_ERROR, "bus_iface_timer_callback : IRQ Missed even after all TREs processed by GSI :  QUP id:%d, SE id: %d, %d",dcfg->qup_data->qup_id, dcfg->se_id, 0);
                    }
                }
            }
            g_ctxt->chan_state[GPI_OUTBOUND_CHAN] = chan_info.chan_state;
            t_ctxt->transfer_state = TRANSFER_TO_BE_CANCELLED;
            g_ctxt->client_to_be_cancelled = qup_gpi_get_client_to_be_cancelled(h_ctxt);
            
			 //adding this condition to prioritize close call over timercallback
	         //GPI_DEINIT_INPROGRESS is set in close and we are ending any callbacks triggered while close is running
            if (QUP_IS_SET(g_ctxt->flags,GPI_DEINIT_INPROGRESS) && QUP_IS_SET(g_ctxt->flags,GPI_CLEAN_IN_FLIGHT))
            {
                QUP_FLAG_UNSET(g_ctxt->flags, GPI_CLEAN_IN_FLIGHT);
                return;
            }

            if(g_ctxt->client_to_be_cancelled != NULL)
            {
                qup_gpi_iface_clean(g_ctxt->client_to_be_cancelled);
            }

            QUP_FLAG_UNSET(g_ctxt->flags,GPI_ERROR_FLAGS);
            QUP_FLAG_SET(io_ctxt->flags,GPI_WAIT_FOR_CLEAN);

            qup_mutex_instance_unlock(h_ctxt->queue_lock,QUEUE_LOCK);

            qup_timer_set    (dcfg, h_ctxt->timer_handle, MAX_CANCEL_TIMEOUT); //reset timer

            return;
        }

    }

    ERR_FATAL("bus_iface_timer_callback : error getting GPI Channel Status",0,0,0);

    return;

}

static void qup_gpi_iface_check_completion(qup_gpi_core_ctxt *g_ctxt,qup_client_ctxt *c_ctxt)
{
    qup_hw_ctxt         *h_ctxt    = g_ctxt->h_ctxt;
    qup_client_ctxt     *temp_ctxt = NULL;
    qup_gpi_iface_ctxt  *io_ctxt   = NULL;
	se_dev_cfg          *dcfg      = (se_dev_cfg *) h_ctxt->core_config;;
	
    

    qup_mutex_instance_lock(h_ctxt->queue_lock,QUEUE_LOCK);

    if(QUP_IS_SET(g_ctxt->flags,GPI_CLEAN_COMPLETED))
    {
        if(QUP_IS_SET(g_ctxt->flags,GPI_CLEAN_CH_RESTARTED))
        {
            temp_ctxt = g_ctxt->client_to_be_cancelled;
            io_ctxt = temp_ctxt->t_ctxt.io_ctxt;
            g_ctxt = ((qup_gpi_iface_ctxt*)temp_ctxt->t_ctxt.io_ctxt)->g_ctxt ;
            if(QUP_IS_NOT_SET(io_ctxt->flags,GPI_WAIT_FOR_UNLOCK_TRE))
            {
                QUP_FLAG_UNSET(g_ctxt->flags,GPI_CLEAN_FLAGS);
                QUP_FLAG_UNSET(io_ctxt->flags,GPI_ALL_TRANSFER_FLAGS);
                io_ctxt->recovery_attempts = 0;
                temp_ctxt->t_ctxt.transfer_state = TRANSFER_CANCELED;
                g_ctxt->client_to_be_cancelled = qup_gpi_get_client_to_be_cancelled(h_ctxt);
                qup_mutex_instance_unlock(h_ctxt->queue_lock,QUEUE_LOCK);
                g_ctxt->tf_comp_cb(temp_ctxt);
				g_ctxt->client_to_be_cancelled = NULL;
                g_ctxt->in_use = FALSE;
                return;
            }
        }
        else if(QUP_IS_SET(g_ctxt->flags,GPI_CLEAN_CH_RESET))
        {
            QUP_FLAG_UNSET(g_ctxt->flags,GPI_CLEAN_FLAGS);
            qup_mutex_instance_unlock(h_ctxt->queue_lock,QUEUE_LOCK);

            while (h_ctxt->client_ctxt_head)
            {
                temp_ctxt = h_ctxt->client_ctxt_head;
                io_ctxt = (qup_gpi_iface_ctxt*)temp_ctxt->t_ctxt.io_ctxt;
                g_ctxt = io_ctxt->g_ctxt ;
                io_ctxt->recovery_attempts = 0;
                temp_ctxt->t_ctxt.transfer_state  = TRANSFER_BUS_RESET;
                QUP_FLAG_UNSET(io_ctxt->flags,GPI_ALL_TRANSFER_FLAGS);
                g_ctxt->tf_comp_cb(temp_ctxt);
				g_ctxt->client_to_be_cancelled = NULL;
                g_ctxt->in_use = FALSE;
            }
            return;
        }
        else
        {
            ERR_FATAL("bus_iface_check_completion : Unknown Clean Kind",0,0,0);
        }
    }
	
	//adding this condition to prioritize close call over timercallback
	//GPI_DEINIT_INPROGRESS is set in close and we are ending any callbacks triggered while close is running
    else if (QUP_IS_SET(g_ctxt->flags,GPI_DEINIT_INPROGRESS) && QUP_IS_SET(g_ctxt->flags,GPI_CLEAN_IN_FLIGHT))
    {
        temp_ctxt = g_ctxt->client_to_be_cancelled;
        io_ctxt = temp_ctxt->t_ctxt.io_ctxt;
        g_ctxt = ((qup_gpi_iface_ctxt*)temp_ctxt->t_ctxt.io_ctxt)->g_ctxt ;		
		qup_timer_clear(dcfg,h_ctxt->timer_handle);
        QUP_FLAG_UNSET(g_ctxt->flags,GPI_CLEAN_FLAGS);
        QUP_FLAG_UNSET(io_ctxt->flags,GPI_ALL_TRANSFER_FLAGS);
    }
    else if(QUP_IS_SET(g_ctxt->flags, GPI_IN_FAILURE_PATH)   ||
           (QUP_IS_SET(g_ctxt->flags, GPI_IN_ERROR_PATH)  &&
             (g_ctxt->chan_state[GPI_OUTBOUND_CHAN] == GPI_CHAN_ERROR)     &&
             (g_ctxt->chan_state[GPI_INBOUND_CHAN]  == GPI_CHAN_ERROR)))
    {
        if(QUP_IS_NOT_SET(g_ctxt->flags,GPI_CLEAN_IN_FLIGHT))
        {
            qup_gpi_cancel(h_ctxt,g_ctxt->client_to_be_cancelled,FALSE);
        }
    }
    else if(c_ctxt)
    {
        io_ctxt = c_ctxt->t_ctxt.io_ctxt;

        if(QUP_IS_SET(io_ctxt->flags,GPI_TX_COMPLETED)      &&
           QUP_IS_SET(io_ctxt->flags,GPI_RX_COMPLETED)      &&
           QUP_IS_NOT_SET(g_ctxt->flags,GPI_ERROR_FLAGS)         &&
           QUP_IS_NOT_SET(io_ctxt->flags,GPI_WAIT_FOR_CLEAN)
          )
        {
            c_ctxt->t_ctxt.transfer_state = TRANSFER_DONE;
            qup_mutex_instance_unlock(h_ctxt->queue_lock,QUEUE_LOCK);
            g_ctxt->tf_comp_cb(c_ctxt);
            g_ctxt->in_use = FALSE;
            return;
        }
    }

    qup_mutex_instance_unlock(h_ctxt->queue_lock,QUEUE_LOCK);

}

boolean qup_gpi_clean_get_cmd(qup_gpi_core_ctxt         *g_ctxt,
                              GPI_CHAN_STATE             curr_tx_state,
                              GPI_CHAN_STATE             curr_rx_state,
                              GPI_CHAN_CMD              *next_cmd,
                              GPI_CHAN_TYPE             *next_cmd_ch,
                              boolean                   *noop)
{

    static uint8 log_index = 0;

    if(QUP_IS_SET(g_ctxt->flags,GPI_CLEAN_IN_FLIGHT))
    {
        if((g_ctxt->clean_state.data.tx_chan_state == (curr_tx_state & GPI_CHAN_STATE_MASK)) &&
           (g_ctxt->clean_state.data.rx_chan_state == (curr_rx_state & GPI_CHAN_STATE_MASK))
        )
        {
            ERR_FATAL("bus_iface_clean_get_cmd : Channel states same",0,0,0);
        }
    }
    else
    {
        QUP_FLAG_SET(g_ctxt->flags,GPI_CLEAN_IN_FLIGHT);
    }

    log_gpi_clean[log_index].handle = g_ctxt->gpi_handle;
    log_gpi_clean[log_index].previous.raw = g_ctxt->clean_state.raw;

    SET_CURR_STATE(g_ctxt->clean_state,curr_tx_state,curr_rx_state)

    if(NOOP_TRE(g_ctxt->clean_state))
    {
        *noop = TRUE;
        g_ctxt->flags |= GPI_CLEAN_CH_RESTARTED;
    }

    if (START_RX(g_ctxt->clean_state))
    {
        *next_cmd      = GPI_CHAN_CMD_START;
        *next_cmd_ch   = GPI_INBOUND_CHAN;
    }
    else if (START_TX(g_ctxt->clean_state))
    {
        *next_cmd      = GPI_CHAN_CMD_START;
        *next_cmd_ch   = GPI_OUTBOUND_CHAN;
    }
    else if (STOP_TX(g_ctxt->clean_state))
    {
        *next_cmd      = GPI_CHAN_CMD_STOP;
        *next_cmd_ch   = GPI_OUTBOUND_CHAN;
    }
    else if (STOP_RX(g_ctxt->clean_state))
    {
        *next_cmd      = GPI_CHAN_CMD_STOP;
        *next_cmd_ch   = GPI_INBOUND_CHAN;
    }
    else if (RESET_TX(g_ctxt->clean_state))
    {
        *next_cmd      = GPI_CHAN_CMD_RESET;
        *next_cmd_ch   = GPI_OUTBOUND_CHAN;
        g_ctxt->flags |= GPI_CLEAN_CH_RESET;
    }
    else if (RESET_RX(g_ctxt->clean_state))
    {
        *next_cmd    = GPI_CHAN_CMD_RESET;
        *next_cmd_ch = GPI_INBOUND_CHAN;
    }
    else
    {
        return FALSE;
    }

    SET_CURR_CMD_CHID(g_ctxt->clean_state,*next_cmd_ch)

    log_gpi_clean[log_index].next.raw = g_ctxt->clean_state.raw;
    log_gpi_clean[log_index].cmd = *next_cmd;
    log_gpi_clean[log_index].chan = *next_cmd_ch;
    log_index++;
    log_index &= (LOG_CLEAN_STATE_DUMP_SIZE - 1);

    return TRUE;
}


void qup_gpi_iface_clean(qup_client_ctxt *c_ctxt)
{
    qup_gpi_iface_ctxt  *io_ctxt = c_ctxt->t_ctxt.io_ctxt;
    qup_gpi_core_ctxt   *g_ctxt  = io_ctxt->g_ctxt;
    se_dev_cfg          *dcfg    = (c_ctxt->h_ctxt)->core_config;
    GPI_CHAN_STATE       t_state,r_state;
    GPI_CHAN_CMD         cmd;
    GPI_CHAN_TYPE        cmd_ch;
    boolean              noop = FALSE;

    t_state = g_ctxt->chan_state[GPI_OUTBOUND_CHAN];
    r_state = g_ctxt->chan_state[GPI_INBOUND_CHAN];

    if(g_ctxt->flags & GPI_CLEAN_IN_FLIGHT)
    {
        if((t_state == GPI_CHAN_RUNNING) &&
           (r_state == GPI_CHAN_RUNNING) )
        {
            CLEAR_CLEAN_STATE(g_ctxt->clean_state);
            QUP_FLAG_SET(g_ctxt->flags,GPI_CLEAN_COMPLETED);
            return;
        }
    }

    if(qup_gpi_clean_get_cmd(g_ctxt,t_state,r_state,&cmd,&cmd_ch,&noop))
    {
        if(noop)
        {
            noop_tres(c_ctxt->h_ctxt,g_ctxt->client_to_be_cancelled);

            //Make sure all Error Flags are cleared once Channels in Stopped State
            QUP_FLAG_UNSET(g_ctxt->flags,GPI_ERROR_FLAGS);
        }
        if (!qup_gpi_issue_cmd_ex(dcfg, g_ctxt, cmd,0, GPI_CHAN_STATE_NONE,cmd_ch,TRUE, g_ctxt))
        { QUP_SE_LOG(dcfg,LEVEL_ERROR, "handle_gpi_restart : CLEANUP ERROR: CMD 0x%x failed on chan 0x%x",cmd,cmd_ch); }
    }
    else
    {
        ERR_FATAL("bus_iface_handle_clean : Next CMD unknown",0,0,0);
    }
}

void qup_gpi_cmd_callback(gpi_result_type *result)
{
    qup_hw_ctxt         *h_ctxt  = NULL;
    qup_gpi_core_ctxt   *g_ctxt  = NULL;
    se_dev_cfg          *dcfg    = NULL;

    QUP_LOG(LEVEL_VERBOSE, "result 0x%08x", result);

    if (result == NULL) { return; }

    g_ctxt  = result->user_data;
    h_ctxt  = g_ctxt->h_ctxt;
    dcfg    = h_ctxt->core_config;


    QUP_SE_LOG(dcfg,LEVEL_INFO, "chan %d: type %d: code %d: length %d: tre_idx %d: dword_0 0x%08x: dword_1 0x%08x: status 0x%08x: user_data 0x%08x",
                result->chan,
                result->type,
                result->code,
                result->length,
                result->tre_idx,
                result->dword_0,
                result->dword_1,
                result->status,
                result->user_data);

    switch(result->type)
    {
        case EVT_CMD_COMPLETION:
        case EVT_STATE_CHANGE:
            if(result->dword_0 < GPI_CHAN_STATE_MAX)
            {
                g_ctxt->chan_state[result->chan] = (GPI_CHAN_STATE) result->dword_0;
            }
        break;
        case GPI_EVT_OTHER:
            if(result->dword_0 == GPI_ERROR_GLOBAL)
            {
                g_ctxt->gpi_err_log = result->dword_1;
                QUP_SE_LOG(dcfg,LEVEL_ERROR, "GPI_ERROR_GLOBAL 0x%08x", result->dword_1);
            }
            else
            {
                QUP_SE_LOG(dcfg,LEVEL_ERROR, "GPI GENERAL ERROR 0x%08x", result->dword_0);
            }
            //Probably initiate a Reset post checking devcfg
            break;
        default :
            QUP_SE_LOG(dcfg,LEVEL_ERROR, "Unknown result->type 0x%08x", result->type);
    }


    if(g_ctxt->flags & GPI_CLEAN_IN_FLIGHT)
    {
        qup_gpi_iface_clean(g_ctxt->client_to_be_cancelled);
        qup_gpi_iface_check_completion(g_ctxt, NULL);
    }
    else if(result->type == EVT_CMD_COMPLETION)
    {
        QUP_FLAG_UNSET(g_ctxt->flags,GPI_WAIT_FOR_CMD_COMP);
        qup_signal_event (g_ctxt->command_wait_signal,CMD_SIGNAL_MASK);
    }
    else
    {
        qup_gpi_iface_check_completion(g_ctxt, NULL);
    }
}


void qup_gpi_evr_callback (gpi_result_type *result)
{
    qup_hw_ctxt             *h_ctxt  = NULL;
    qup_client_ctxt         *c_ctxt  = NULL;
    qup_transfer_ctxt       *t_ctxt  = NULL;
    qup_gpi_core_ctxt       *g_ctxt  = NULL;
    qup_gpi_iface_ctxt      *io_ctxt = NULL;
    se_dev_cfg              *dcfg    = NULL;

    QUP_LOG(LEVEL_VERBOSE, "result 0x%08x", result);

    if (result == NULL) { return; }


    if((result->type == EVT_IMM_DATA) || (result->type == EVT_XFER_COMPLETION))
    {
        c_ctxt  = result->user_data;
        h_ctxt  = c_ctxt->h_ctxt;
        t_ctxt  = &(c_ctxt->t_ctxt);
        io_ctxt = t_ctxt->io_ctxt;
        g_ctxt  = io_ctxt->g_ctxt;
    }
    else
    {
        g_ctxt  = result->user_data;
        h_ctxt  = g_ctxt->h_ctxt;
        if(result->type != EVT_I3C_IBI_REC)
        {
            c_ctxt  = h_ctxt->client_ctxt_head;
            if(c_ctxt != NULL)
            {
                t_ctxt  = &(c_ctxt->t_ctxt);
                io_ctxt = t_ctxt->io_ctxt;
            }
            else
            {
                QUP_LOG(LEVEL_ERROR, "Unable to fetch the client context: c_ctxt\n");
                return; //Can't proceed with error recording
            }
        }
    }

    if(h_ctxt == NULL) {
        QUP_LOG(LEVEL_ERROR, "Unable to dereference h_ctxt\n");
        return;
    }

    dcfg    = h_ctxt->core_config;

    QUP_SE_LOG(dcfg,LEVEL_INFO, "chan %d: type %d: code %d: length %d: tre_idx %d: dword_0 0x%08x: dword_1 0x%08x: status 0x%08x: user_data 0x%08x",
             result->chan,
             result->type,
             result->code,
             result->length,
             result->tre_idx,
             result->dword_0,
             result->dword_1,
             result->status,
             result->user_data);

    if(result->code == EVT_COMPLETION_UNDEF)
    {
        if(g_ctxt->gp_err_cb)
        {
            if(g_ctxt->gp_err_cb(c_ctxt,GP_IRQ(result->status,M_DMA_GP_ERR_SHFT)))
            {
                QUP_FLAG_SET(g_ctxt->flags,GPI_IN_ERROR_PATH);
            }
            else
            {
                QUP_FLAG_SET(g_ctxt->flags,GPI_IN_FAILURE_PATH);
            }
        }
        else
        {
            QUP_FLAG_SET(g_ctxt->flags,GPI_IN_FAILURE_PATH);
        }
        if(c_ctxt)
        {
            qup_log_tres(c_ctxt);
        }

        if(t_ctxt != NULL)
        {
            t_ctxt->transfer_state = TRANSFER_TO_BE_CANCELLED;
        }
        g_ctxt->client_to_be_cancelled = qup_gpi_get_client_to_be_cancelled(h_ctxt);
    }

    switch(result->type)
    {
        case EVT_XFER_COMPLETION:
        case EVT_IMM_DATA:

            t_ctxt->transferred += result->length;

            if(result->status & M_DMA_GP_ERR_MASK)
            {
                if(g_ctxt->gp_err_cb)
                {
                    g_ctxt->gp_err_cb(c_ctxt,GP_IRQ(result->status,M_DMA_GP_ERR_SHFT));
                }
                else
                {
                    //Do Error
                }
            }

            if (result->is_last_evt)
            {
                if(result->chan == GPI_OUTBOUND_CHAN)
                {
                    QUP_FLAG_SET(io_ctxt->flags,GPI_TX_COMPLETED);
                    QUP_FLAG_UNSET(io_ctxt->flags,GPI_WAIT_FOR_UNLOCK_TRE);
                }
                else if (result->chan == GPI_INBOUND_CHAN)
                {
                    QUP_FLAG_SET(io_ctxt->flags,GPI_RX_COMPLETED);
                }
                else
                {
                    //Add error
                }
            }
            break;

        case EVT_TIMESTAMP:
            if(t_ctxt)
            {
                t_ctxt->start_stop_bit_timestamp = QUP_TS_64(result->dword_1 , result->dword_0);
            }
            break;

        case EVT_QUP_NOTIF:
            //result->dword_0 is M/S_IRQ_STATUS
            if (result->dword_0 & STATUS_TIMESTAMP)
            {
                if(t_ctxt)
                {
                    t_ctxt->start_stop_bit_timestamp  = result->dword_1;
                }
            }
            if(result->dword_0 & M_IRQ_GP_ERR_MASK)
            {
                qup_log_tres(c_ctxt);
                if(g_ctxt->gp_err_cb)
                {
                    g_ctxt->gp_err_cb(c_ctxt,GP_IRQ(result->dword_0,M_IRQ_GP_ERR_SHFT));
                }
                else
                {
                    //Do Error
                }
            }
            break;
        case EVT_I3C_IBI_REC:
            if(g_ctxt->ibs_cb)
            {
                g_ctxt->ibs_cb(h_ctxt,result);
                return;
            }
        default : ;
            //Print Error
    }

    if(g_ctxt != NULL && c_ctxt != NULL)
    {
        qup_gpi_iface_check_completion(g_ctxt,c_ctxt);
    }
    else
    {
        QUP_LOG(LEVEL_ERROR, "qup_gpi_evr_callback: h_ctxt - hw context & c_ctxt - client context are not valid!\n");
    }
}

static void qup_gpi_isr_ex (void *int_handle)
{
    qup_gpi_core_ctxt       *g_ctxt = (qup_gpi_core_ctxt *)  int_handle;

    gpi_iface_poll(g_ctxt->gpi_handle);
}


// keep it simple. commands execute synchronously and one command at a time
boolean qup_gpi_issue_cmd (qup_hw_ctxt        *h_ctxt,
                           GPI_CHAN_CMD       gpi_chan_cmd,
                           uint32             cmd_param,
                           GPI_CHAN_STATE     gpi_exp_state,
                           GPI_CHAN_TYPE      gpi_chan_type,
                           boolean            asynchronous)
{
    se_dev_cfg *dcfg = (se_dev_cfg *) h_ctxt->core_config;
    qup_gpi_core_ctxt *g_ctxt = (qup_gpi_core_ctxt *) h_ctxt->core_iface;

    return qup_gpi_issue_cmd_ex(dcfg,g_ctxt,gpi_chan_cmd,cmd_param,gpi_exp_state,gpi_chan_type,asynchronous,g_ctxt);
}



boolean qup_gpi_issue_cmd_ex (se_dev_cfg        *dcfg,
                              qup_gpi_core_ctxt *g_ctxt,
                              GPI_CHAN_CMD       gpi_chan_cmd,
                              uint32             cmd_param,
                              GPI_CHAN_STATE     gpi_exp_state,
                              GPI_CHAN_TYPE      gpi_chan_type,
                              boolean            asynchronous,
                              void              *user_data)
{
    uint32 timeout_us = POLL_CMD_DEFAULT_US;
    chan_status_info chan_info;

    QUP_SE_LOG(dcfg,LEVEL_PERF, "gpicmd S");

    if ((gpi_exp_state != GPI_CHAN_STATE_NONE) && (g_ctxt->chan_state[gpi_chan_type] == gpi_exp_state))
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR, "ERROR chan_state = %d already as expeted = %d", g_ctxt->chan_state[gpi_chan_type], gpi_exp_state);
        return TRUE;
    }

    QUP_FLAG_SET(g_ctxt->flags,GPI_WAIT_FOR_CMD_COMP);

    if (GPI_STATUS_SUCCESS != gpi_issue_cmd(g_ctxt->gpi_handle,
                                            gpi_chan_type,
                                            gpi_chan_cmd,
                                            cmd_param,
                                            user_data))
    {
        return FALSE;
    }

    if (asynchronous) { return TRUE; }

    if (QUP_IS_NOT_SET(dcfg->flags,POLLED_MODE))
    {

        /*
         * As per HPG, there is a possibility for the STOP command to get stuck.
         * The software needs to poll the channel status after issuing the STOP command.
         * If there is no change in the channel state within a given time or if the channel state moves to STOP_IN_PROC,
         * return an error to the caller function. The caller function must then issue a RESET command.*/

        if (gpi_chan_cmd == GPI_CHAN_CMD_STOP)
        {
            while(timeout_us != 0)
            {
                qup_delay_us (POLL_INTERVAL_US);
                if (GPI_STATUS_SUCCESS != gpi_query_chan_status(g_ctxt->gpi_handle, gpi_chan_type, &chan_info))
                {
                    QUP_SE_LOG(dcfg,LEVEL_ERROR, "gpi_query_chan_status failed");
                }
                if (chan_info.chan_state == GPI_CHAN_STOPPED)
                {
                    break;
                }
                timeout_us -= POLL_INTERVAL_US;
            }
            if(timeout_us == 0)
            {
                QUP_SE_LOG(dcfg,LEVEL_ERROR, " GPII STOP CMD timed out for Chan :%d Curr state : %d",
                                gpi_chan_type, chan_info.chan_state);
                return FALSE;
            }
        }
        if(FALSE == qup_wait_for_event(g_ctxt->command_wait_signal,CMD_SIGNAL_MASK))
        {
            while (QUP_IS_SET(g_ctxt->flags,GPI_WAIT_FOR_CMD_COMP) && (timeout_us != 0))
            {
                qup_delay_us (POLL_INTERVAL_US);
                timeout_us -= POLL_INTERVAL_US;
            }
            qup_clear_signal_event(g_ctxt->command_wait_signal,CMD_SIGNAL_MASK);
         }
    }
    else
    {
        while (QUP_IS_SET(g_ctxt->flags,GPI_WAIT_FOR_CMD_COMP) && (timeout_us != 0))
        {
            qup_gpi_isr_ex (g_ctxt);
            qup_delay_us (POLL_INTERVAL_US);
            timeout_us -= POLL_INTERVAL_US;
        }
    }

    if(gpi_exp_state != GPI_CHAN_STATE_NONE)
    {
        if (g_ctxt->chan_state[gpi_chan_type] != gpi_exp_state)
        {
            QUP_SE_LOG(dcfg,LEVEL_ERROR, "ERROR chan_state = %d: expected = %d", g_ctxt->chan_state, gpi_exp_state);
            return FALSE;
        }
    }

    return TRUE;
}

void qup_gpi_isr (void *int_handle)
{
    qup_hw_ctxt             *h_ctxt = (qup_hw_ctxt *) int_handle;
    return qup_gpi_isr_ex(h_ctxt->core_iface);
}


uint32 qup_gpi_tre_available (qup_hw_ctxt *h_ctxt, GPI_CHAN_TYPE chan, gpi_ring_elem **next_available_tre,uint8 *wp_idx)
{
    qup_gpi_core_ctxt *g_ctxt = h_ctxt->core_iface;
    se_dev_cfg        *dcfg   = h_ctxt->core_config;

    return qup_gpi_tre_available_ex(dcfg,g_ctxt,chan,next_available_tre,wp_idx);
}
uint32 qup_gpi_tre_available_ex (se_dev_cfg *dcfg, qup_gpi_core_ctxt *g_ctxt, GPI_CHAN_TYPE chan, gpi_ring_elem **next_available_tre,uint8 *wp_idx)
{
    chan_status_info chan_info;

    if (GPI_STATUS_SUCCESS == gpi_query_chan_status(g_ctxt->gpi_handle,
                                                    chan,
                                                    &chan_info))
    {
        *next_available_tre = ((gpi_ring_elem *) g_ctxt->ring_info[chan].ring_base_va) + chan_info.wp_index;
        *wp_idx = chan_info.wp_index;
        QUP_SE_LOG(dcfg,LEVEL_INFO, "available 0x%08x free in chan[%d] = %d @ %d state : %d", *next_available_tre, chan, chan_info.num_avail_tre,*wp_idx, chan_info.chan_state);
        return chan_info.num_avail_tre;
    }

    return 0;
}

void qup_gpi_set_tre_lock (gpi_ring_elem *tre)
{
    tre->dword_0 = 0;
    tre->dword_1 = 0;
    tre->dword_2 = 0;
    tre->ctrl    = GPI_BUILD_TRE_CTRL(TRE_LOCK_MAJOR, TRE_LOCK_MINOR, 0, 0, 0, 0, 1);
}

void qup_gpi_set_tre_unlock (gpi_ring_elem *tre)
{
    tre->dword_0 = 0;
    tre->dword_1 = 0;
    tre->dword_2 = 0;
    tre->ctrl    = GPI_BUILD_TRE_CTRL(TRE_UNLOCK_MAJOR, TRE_UNLOCK_MINOR, 0, 0, 0, 1, 0);
}

boolean qup_gpi_enable  (qup_hw_ctxt *h_ctxt, boolean active)
{
    se_dev_cfg        *dcfg   = (se_dev_cfg *)h_ctxt->core_config;
    qup_gpi_core_ctxt *g_ctxt = (qup_gpi_core_ctxt *)h_ctxt->core_iface;

    if(GPI_STATUS_SUCCESS == gpi_iface_active(g_ctxt->gpi_handle, active))
    {
        return TRUE;
    }
    else
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR, "ERROR : gpi_iface_active failed");
        return FALSE;
    }
}

qup_status qup_wait_for_gpii_clean (void* gpii_handle)
{
    uint8  gpi_clean_wait_cnt   = 0;
    qup_gpi_core_ctxt*   g_ctxt = (qup_gpi_core_ctxt*)gpii_handle;

    while (QUP_IS_SET(g_ctxt->flags,GPI_CLEAN_IN_FLIGHT))
    {
        if (gpi_clean_wait_cnt < 2)
        {
            // gpii clean up is in progress wait for sometime
             busywait(MAX_CANCEL_TIMEOUT);
        }
        else // exceeded max wait time
        {
            return QUP_ERROR_DMA_INSUFFICIENT_RESOURCES;
        }
        gpi_clean_wait_cnt++;
    }

    return QUP_SUCCESS;
}
/*
qup_status qup_gpi_is_active    (qup_hw_ctxt *h_ctxt);
{

}
*/

