/**
    @file  qup_fifo_island.c
    @brief FIFO interface Island implementation
 */
/*=============================================================================
            Copyright (c) 2020-2021 Qualcomm Technologies, Incorporated.
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
#include "qup_fifo.h"
#include "qup_hal.h"
#include "err.h"

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/

#define GP_IRQ(x,offset) ((x >> offset) & 0xFF)

#define POLL_TIME_DEFAULT_US    409600
#define POLL_CMD_DEFAULT_US     5000
#define POLL_INTERVAL_US        10
#define MAX_CANCEL_TIMEOUT      50


/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/

/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/

/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/

/* Should be called with QUEUE LOCK*/
qup_status qup_fifo_cancel        (qup_hw_ctxt *h_ctxt, qup_client_ctxt *c_ctxt,boolean force_reset)
{
    qup_fifo_core_ctxt    *f_ctxt  = (qup_fifo_core_ctxt *)  h_ctxt->core_iface;
    qup_transfer_ctxt     *t_ctxt  = (qup_transfer_ctxt *)  &(c_ctxt->t_ctxt);
    qup_fifo_iface_ctxt   *io_ctxt = (qup_fifo_iface_ctxt *)  t_ctxt->io_ctxt;
    se_dev_cfg            *dcfg    = (se_dev_cfg *)h_ctxt->core_config;
    uint8                 *se_base = (uint8 *)dcfg->se_base_addr;
    qup_status             status  = QUP_SUCCESS;
    
    if(t_ctxt->transfer_state == TRANSFER_DONE      || 
       t_ctxt->transfer_state == TRANSFER_CANCELED  ||
       t_ctxt->transfer_state == TRANSFER_TIMED_OUT)
    {
        QUP_SE_LOG(dcfg,LEVEL_INFO, "qup_fifo_cancel:Transfer Already completed for ctxt 0x%x state 0x%x",c_ctxt,t_ctxt->transfer_state);
        status = QUP_TRANSFER_COMPLETED;
        goto exit;
    }
    if(!(f_ctxt->flags & FIFO_CLEAN_IN_FLIGHT))
    {
        if(HWIO_INXF(GENI_CFG_BASE(se_base),GENI_STATUS,M_GENI_CMD_ACTIVE))
        {
            QUP_FLAG_SET(f_ctxt->flags,FIFO_CLEAN_IN_FLIGHT);
            QUP_FLAG_SET(io_ctxt->flags,FIFO_WAIT_FOR_CLEAN);
            f_ctxt->client_to_be_cancelled = c_ctxt;
            
            /* Reset TX WM when in cancel path*/
            HWIO_OUTXF(GENI_DATA_BASE(se_base),GENI_TX_WATERMARK_REG,TX_WATERMARK,0);
            
            if(force_reset)
            {
                HWIO_OUTXF(GENI_DATA_BASE(se_base),GENI_M_CMD_CTRL_REG, M_GENI_CMD_ABORT,1);
            }
            else
            {
                HWIO_OUTXF(GENI_DATA_BASE(se_base),GENI_M_CMD_CTRL_REG, M_GENI_CMD_CANCEL,1);
            }
        }
        else
        {
            QUP_SE_LOG(dcfg,LEVEL_ERROR, "qup_fifo_cancel : Geni command not active : 0x%08x",se_base);
            status = QUP_TRANSFER_COMPLETED;
        }
    }   
    else
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR, "Clean command Already set for  ctxt 0x%x",f_ctxt->client_to_be_cancelled);
        status = QUP_ERROR_HANDLE_ALREADY_IN_QUEUE;
    }
    
 exit:
    return status;
}

void  qup_fifo_handle_timer_cb(void *data)
{
    qup_hw_ctxt         *h_ctxt       = (qup_hw_ctxt *)data;
    qup_client_ctxt     *c_ctxt       = NULL;
    se_dev_cfg          *dcfg         = NULL;
    qup_fifo_core_ctxt  *f_ctxt       = NULL;
    uint8               *se_base      = NULL;
    
    if (h_ctxt == NULL) { QUP_LOG(LEVEL_ERROR, "bus_iface_timer_callback : failed to initialize hw_ctxt"); return; }
    
    c_ctxt = h_ctxt->client_ctxt_head;
    dcfg   = (se_dev_cfg *) h_ctxt->core_config;
    f_ctxt = h_ctxt->core_iface;
    se_base      = (uint8 *)dcfg->se_base_addr;
    
    QUP_SE_LOG(dcfg,LEVEL_ERROR, "bus_iface_timer_callback :0x%08x",h_ctxt);
    
    if (c_ctxt == NULL)   
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR, "bus_iface_timer_callback : No active client");
        return;
    }
    
    if(dcfg->flags & ENABLE_FATAL)
    {
        ERR_FATAL("bus recovery fatal",h_ctxt,0,0);
    }
    
    qup_mutex_instance_lock(h_ctxt->queue_lock,QUEUE_LOCK);
    
    if(QUP_IS_NOT_SET(f_ctxt->flags,FIFO_CLEAN_IN_FLIGHT))
    {
        qup_fifo_cancel(h_ctxt,c_ctxt,TRUE);
    }
    else
    {
        HWIO_OUTXF(GENI_DATA_BASE(se_base),GENI_M_CMD_CTRL_REG, M_GENI_CMD_ABORT,1);
    }
    
    qup_mutex_instance_unlock(h_ctxt->queue_lock,QUEUE_LOCK);
}

void  qup_fifo_isr (void *int_handle)
{
    qup_hw_ctxt             *h_ctxt = (qup_hw_ctxt *) int_handle;
    se_dev_cfg              *dcfg;
    qup_client_ctxt         *c_ctxt;
    qup_transfer_ctxt       *t_ctxt;
    qup_fifo_core_ctxt      *f_ctxt;
    qup_fifo_iface_ctxt     *f_io_ctxt;
    uint8                   *se_base;
    uint32                   m_irq_status;
    uint32                   m_irq_clr = 0;
    
    if( h_ctxt == NULL               ||
        h_ctxt->core_config == NULL  ||
        h_ctxt->core_iface  == NULL 
      )
    {
        QUP_LOG(LEVEL_ERROR, "qup_fifo_isr : h_ctxt parameters NULL");
        return;
    }
    
    dcfg     = h_ctxt->core_config;
    f_ctxt   = h_ctxt->core_iface;
    se_base  = (uint8 *)dcfg->se_base_addr;
    
    if(!h_ctxt->power_ref_count)
    {
        QUP_SE_LOG(dcfg, LEVEL_ERROR, "qup_fifo_isr : clocks off: 0x%08x",h_ctxt);
        return;
    }
    
    m_irq_status = HWIO_INX(GENI_DATA_BASE(se_base), GENI_M_IRQ_STATUS);
    QUP_SE_LOG(dcfg, LEVEL_INFO, "qup_fifo_isr : isr 0x%08x",m_irq_status); //debug
    
    if(m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS,M_GP_SYNC_IRQ_0))
    {
        if(f_ctxt->ibs_cb)
        {
            f_ctxt->ibs_cb(h_ctxt,HWIO_INX(GENI_DATA_BASE(se_base),GENI_I3C_IBI_STATUS));
        }
        HWIO_OUTX(GENI_DATA_BASE(se_base), GENI_M_IRQ_CLEAR, HWIO_FMSK(GENI_M_IRQ_STATUS,M_GP_SYNC_IRQ_0));
        return;
    }
    
    if(h_ctxt->client_ctxt_head == NULL)
    {
        QUP_SE_LOG(dcfg, LEVEL_ERROR, "qup_fifo_isr : client_ctxt_head NULL : 0x%08x",m_irq_status);
        HWIO_OUTX(GENI_DATA_BASE(se_base),GENI_M_IRQ_CLEAR,m_irq_status);
        return;
    }
    else
    {
        c_ctxt = h_ctxt->client_ctxt_head;
        t_ctxt  = &(c_ctxt->t_ctxt);
        f_io_ctxt = t_ctxt->io_ctxt;
    }
    
    if(m_irq_status & M_IRQ_GP_ERR_MASK)
    {
        if(f_ctxt->gp_err_cb)
        {
            f_ctxt->gp_err_cb(c_ctxt,GP_IRQ(m_irq_status,M_IRQ_GP_ERR_SHFT));
        }
        m_irq_clr = m_irq_status & M_IRQ_GP_ERR_MASK;
    }
    
    if(m_irq_status & M_IRQ_FIFO_ERR_MASK)
    {
        if (m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS,M_CMD_FAILURE))
        {
            f_io_ctxt->tf_status = QUP_ERROR_COMMAND_FAIL;
            m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS,M_CMD_FAILURE);
        }
        else if (m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS,RX_FIFO_RD_ERR))
        {
            f_io_ctxt->tf_status = QUP_ERROR_INPUT_FIFO_UNDER_RUN;
            m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS,RX_FIFO_RD_ERR);
        }
        else if (m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS,RX_FIFO_WR_ERR))
        {
            f_io_ctxt->tf_status = QUP_ERROR_INPUT_FIFO_OVER_RUN;
            m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS,RX_FIFO_WR_ERR);
        }
        else if (m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS,TX_FIFO_RD_ERR))
        {
            f_io_ctxt->tf_status = QUP_ERROR_OUTPUT_FIFO_UNDER_RUN;
            m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS,TX_FIFO_RD_ERR);
        }
        else if (m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS,TX_FIFO_WR_ERR))
        {
            f_io_ctxt->tf_status = QUP_ERROR_OUTPUT_FIFO_OVER_RUN;
            m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS,TX_FIFO_WR_ERR);
        }
        else if (m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS,M_CMD_OVERRUN))
        {
            f_io_ctxt->tf_status = QUP_ERROR_COMMAND_OVER_RUN;
            m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS,M_CMD_OVERRUN);
        }
        else if (m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS,M_ILLEGAL_CMD))
        {
            f_io_ctxt->tf_status = QUP_ERROR_COMMAND_ILLEGAL;
            m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS,M_ILLEGAL_CMD);
        }
        
        qup_mutex_instance_lock(h_ctxt->queue_lock,QUEUE_LOCK);
        
        qup_fifo_cancel(h_ctxt,c_ctxt,FALSE);
        
        qup_mutex_instance_unlock(h_ctxt->queue_lock,QUEUE_LOCK);
    }
    
    if(m_irq_status & (HWIO_FMSK(GENI_M_IRQ_STATUS,M_CMD_CANCEL) & HWIO_FMSK(GENI_M_IRQ_STATUS,M_CMD_ABORT)))
    {
        
        if(QUP_IS_NOT_SET(f_ctxt->flags,FIFO_CLEAN_IN_FLIGHT))
        {
            ERR_FATAL("qup_fifo_isr : Cancel not initiated",h_ctxt,0,0);
        }
        
        qup_mutex_instance_lock(h_ctxt->queue_lock,QUEUE_LOCK);
        
        QUP_FLAG_UNSET(f_ctxt->flags,FIFO_CLEAN_FLAGS);
        QUP_FLAG_UNSET(f_io_ctxt->flags,FIFO_ALL_TRANSFER_FLAGS);
        
        if(m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS,M_CMD_CANCEL))
        {
            t_ctxt->transfer_state = TRANSFER_CANCELED;
            m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS,M_CMD_CANCEL);
        }
        else
        {
            t_ctxt->transfer_state = TRANSFER_BUS_RESET;
            m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS,M_CMD_ABORT);
        }
        
        f_ctxt->client_to_be_cancelled = NULL;
        
        qup_mutex_instance_unlock(h_ctxt->queue_lock,QUEUE_LOCK);
        
        if(f_ctxt->tf_comp_cb)
        {
            f_ctxt->tf_comp_cb(c_ctxt);
        }
    }
    
    if(m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS,M_TIMESTAMP))
    {
        if(t_ctxt)
        {
            t_ctxt->start_stop_bit_timestamp = QUP_TS_64(HWIO_INX(GENI_DATA_BASE(se_base),GENI_TIMESTAMP_MSB),HWIO_INX(GENI_DATA_BASE(se_base),GENI_TIMESTAMP));
        }
        m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS,M_TIMESTAMP);
    }
    
    if (m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS,M_CMD_DONE))
    {
        if (m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS, RX_FIFO_LAST))
        {
            if(f_ctxt->watermark_cb)
            {
                f_ctxt->watermark_cb(c_ctxt,TRUE);
            }
            m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS, RX_FIFO_LAST);
        }
        
        qup_mutex_instance_lock(h_ctxt->queue_lock,QUEUE_LOCK);
        
        if(QUP_IS_NOT_SET(f_io_ctxt->flags,FIFO_WAIT_FOR_CLEAN))
        {
            t_ctxt->transfer_state = TRANSFER_DONE;
        }
        
        qup_mutex_instance_unlock(h_ctxt->queue_lock,QUEUE_LOCK);
        
        if(f_ctxt->tf_comp_cb)
        {
            f_ctxt->tf_comp_cb(c_ctxt);
        }
        
        m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS,M_CMD_DONE);
    }
   else
   {
        if (m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS, TX_FIFO_WATERMARK))
        {
            // QUP_SE_LOG(dcfg, LEVEL_INFO, "qup_fifo_isr TX_FIFO_WATERMARK: isr 0x%08x",f_ctxt->watermark_cb); //debug
            if(f_ctxt->watermark_cb)
            {
                f_ctxt->watermark_cb(c_ctxt,FALSE);
            }
            m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS, TX_FIFO_WATERMARK);
        }
        if (m_irq_status & HWIO_FMSK(GENI_M_IRQ_STATUS, RX_FIFO_WATERMARK))
        {
            // QUP_SE_LOG(dcfg, LEVEL_INFO, "qup_fifo_isr RX_FIFO_WATERMARK: isr 0x%08x",f_ctxt->watermark_cb); //debug
            if(f_ctxt->watermark_cb)
            {
                f_ctxt->watermark_cb(c_ctxt,TRUE);
            }
            m_irq_clr |= HWIO_FMSK(GENI_M_IRQ_STATUS, RX_FIFO_WATERMARK);
        }
   }
   
   if(m_irq_clr != m_irq_status)
   {
       QUP_SE_LOG(dcfg, LEVEL_INFO, "qup_fifo_isr : interrupt not handled 0x%08x",m_irq_clr ^ m_irq_status);
   }
   
   HWIO_OUTX(GENI_DATA_BASE(se_base),GENI_M_IRQ_CLEAR,m_irq_status);
}

 
boolean qup_fifo_enable        (qup_hw_ctxt *h_ctxt, boolean active)
{
    se_dev_cfg        *dcfg   = (se_dev_cfg *)h_ctxt->core_config;
    
    if(QUP_IS_NOT_SET(dcfg->flags,POLLED_MODE))
    {
        if(active)
        {
            return qup_interrupt_enable(dcfg->iface_data.core_irq);
        }
        else
        {
            return qup_interrupt_disable(dcfg->iface_data.core_irq);
        }
    }
    
    return TRUE;
}

/* Write Data to fifo
   Returns No. of bytes written*/
uint32  qup_write_to_fifo (qup_hw_ctxt *h_ctxt, uint8 *ptr, uint32 length)
{
    se_dev_cfg          *dcfg     = (se_dev_cfg *)h_ctxt->core_config;
    qup_fifo_core_ctxt  *f_ctxt   = (qup_fifo_core_ctxt *)h_ctxt->core_iface;
    uint8               *se_base  = (uint8 *)dcfg->se_base_addr;

    uint32 word = 0, i = 0;
    uint32 tx_watermark = HWIO_INXF(GENI_DATA_BASE(se_base), GENI_TX_WATERMARK_REG, TX_WATERMARK);
    uint32 bytes_to_write = (f_ctxt->tx_fifo_depth << 2) - (tx_watermark << 2);
    
    if (length < bytes_to_write)
    {
        bytes_to_write = length;
        // if all data will be written, then disable watermark
        HWIO_OUTXF(GENI_DATA_BASE(se_base),GENI_TX_WATERMARK_REG,TX_WATERMARK,0);
    }
    
    for (i = 0; i < bytes_to_write; i++)
    {
        if (i && ((i & 0x3) == 0))
        {
            HWIO_OUTXI(GENI_DATA_BASE(se_base), GENI_TX_FIFOn, 0, word);
            word = 0;
        }
        word = word | (ptr[i] << ((i & 0x3) << 3));
    }
    HWIO_OUTXI(GENI_DATA_BASE(se_base), GENI_TX_FIFOn, 0, word);
    
    return bytes_to_write;
}

/* Read Data from fifo
   Returns No. of bytes read*/
uint32  qup_read_from_fifo (qup_hw_ctxt *h_ctxt, uint8 *ptr, uint32 length)
{
    se_dev_cfg  *dcfg     = (se_dev_cfg *)h_ctxt->core_config;
    uint8       *se_base  = (uint8 *)dcfg->se_base_addr;
    
    uint32       rx_fifo_status =  HWIO_INX(GENI_DATA_BASE(se_base), GENI_RX_FIFO_STATUS);
    uint8        bytes_in_last_word = 0;
    uint32       bytes_to_read =  QUP_PARSEF(GENI_RX_FIFO_STATUS, RX_FIFO_WC, rx_fifo_status) << 2;
    uint32       word = 0, i = 0;
    
    if (QUP_PARSEF(GENI_RX_FIFO_STATUS, RX_LAST, rx_fifo_status))
    {
        bytes_in_last_word = QUP_PARSEF(GENI_RX_FIFO_STATUS, RX_LAST_BYTE_VALID, rx_fifo_status);
        bytes_to_read = (bytes_to_read - 4) + bytes_in_last_word;
    }
    
    if (length < bytes_to_read)
    {
        bytes_to_read = length;
    }
    
    for (i = 0; i < bytes_to_read; i++)
    {
        if ((i & 0x3) == 0)
        {
            word = HWIO_INXI(GENI_DATA_BASE(se_base), GENI_RX_FIFOn, 0);
        }
        ptr[i] = (word >> ((i & 0x3) << 3)) & 0xFF;
    }
    return bytes_to_read;
}

