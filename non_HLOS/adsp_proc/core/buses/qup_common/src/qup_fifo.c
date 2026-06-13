/**
    @file  qup_fifo.c
    @brief Qup interface implementation
 */


/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
06/26/24   GKR     Multi SSC QUP Support

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
#include "qup_plat.h"
#include "qup_alloc.h"
#include "qup_log.h"
#include "qup_hal.h"
#include "qup_error.h"
#include "qup_fifo.h"

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


void *qup_fifo_get_io_ctxt (se_dev_cfg *dcfg)
{
    void *ptr = NULL;

    ptr =  qup_mem_alloc (dcfg,
                          sizeof(qup_fifo_iface_ctxt),
                          IO_CTXT_TYPE);
    return ptr;
}

void qup_fifo_release_io_ctxt (se_dev_cfg *dcfg, void *ptr)
{
    qup_mem_free (ptr, sizeof(qup_fifo_iface_ctxt), dcfg, IO_CTXT_TYPE);
}

qup_status qup_fifo_iface_init (qup_hw_ctxt *h_ctxt)
{
    se_dev_cfg          *dcfg   = (se_dev_cfg *) h_ctxt->core_config;
    qup_fifo_core_ctxt  *f_ctxt;
    qup_status           q_status = QUP_SUCCESS;
    int                  irq_idx;
    uint8               *se_base;
    boolean              enable_island   = FALSE;
    
    f_ctxt = (qup_fifo_core_ctxt *) qup_mem_alloc (dcfg,
                                                  sizeof(qup_fifo_core_ctxt),
                                                  CORE_IFACE_TYPE);
                                                  
    if (f_ctxt == NULL) { return QUP_ERROR_NO_MEM; }
    
    h_ctxt->core_iface = f_ctxt;
    se_base  = (uint8 *)dcfg->se_base_addr;

    f_ctxt->tx_fifo_depth = HWIO_INXF(GENI_SE_DMA_BASE(se_base),SE_HW_PARAM_0,TX_FIFO_DEPTH);
    f_ctxt->rx_fifo_depth = HWIO_INXF(GENI_SE_DMA_BASE(se_base),SE_HW_PARAM_1,RX_FIFO_DEPTH);


    HWIO_OUTXF(GENI_DATA_BASE(se_base),GENI_RX_RFR_WATERMARK_REG,RX_RFR_WATERMARK,(f_ctxt->rx_fifo_depth - 1));
    HWIO_OUTXF(GENI_DATA_BASE(se_base),GENI_RX_WATERMARK_REG,RX_WATERMARK,(f_ctxt->rx_fifo_depth - 2));

    if(GET_QUP_MAJOR(dcfg->qup_data->qup_id) == SSC_QUP)
    {
        enable_island = TRUE;
    }
    
    if(QUP_IS_NOT_SET(dcfg->flags,POLLED_MODE))
    {
        if(QUP_IS_NOT_SET(dcfg->iface_data.interface_supported,CORE_IRQ))
        {
            irq_idx = qup_config_tcsr(dcfg->qup_data->qup_id,QUP_SE_IRQ,dcfg->se_id);
            if(irq_idx >= 0)
            {
                dcfg->iface_data.core_irq = irq_idx;
                QUP_FLAG_SET(dcfg->iface_data.interface_supported,CORE_IRQ);
            }
            else
            {
                QUP_SE_LOG(dcfg,LEVEL_ERROR, "CORE Interrupt not supported");
                q_status =  QUP_ERROR_UNSUPPORTED_CORE_INSTANCE;
                goto error;
            }
        }
        
        if(!qup_interrupt_register_ex(dcfg->iface_data.core_irq, LEVEL_HIGH_TRIGGER, (void*)qup_fifo_isr,
                                      (void*)h_ctxt,dcfg->island_spec_in_use, enable_island))
        {
            QUP_SE_LOG(dcfg,LEVEL_ERROR, "qup_interrupt_register_ex failed.");
            q_status =  QUP_ERROR_PLATFORM_REG_INT_FAIL;
            goto error;
        }
        qup_interrupt_disable(dcfg->iface_data.core_irq);
        
        HWIO_OUTXM(GENI_DATA_BASE(se_base),GENI_M_IRQ_ENABLE,
                HWIO_FMSK(GENI_M_IRQ_ENABLE, M_CMD_OVERRUN_EN      ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, M_ILLEGAL_CMD_EN      ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, M_CMD_FAILURE_EN      ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, M_CMD_CANCEL_EN       ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, M_CMD_ABORT_EN        ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, M_TIMESTAMP_EN        ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, IO_DATA_ASSERT_EN     ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, IO_DATA_DEASSERT_EN   ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, RX_FIFO_RD_ERR_EN     ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, RX_FIFO_WR_ERR_EN     ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, TX_FIFO_RD_ERR_EN     ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, TX_FIFO_WR_ERR_EN     ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, RX_FIFO_WATERMARK_EN  ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, TX_FIFO_WATERMARK_EN  ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, RX_FIFO_LAST_EN       ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, M_CMD_DONE_EN         ) |
                HWIO_FMSK(GENI_M_IRQ_ENABLE, SEC_IRQ_EN            ) ,
                HWIO_FVAL(GENI_M_IRQ_ENABLE, M_CMD_OVERRUN_EN,    1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, M_ILLEGAL_CMD_EN,    1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, M_CMD_FAILURE_EN,    1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, M_CMD_CANCEL_EN,     1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, M_CMD_ABORT_EN,      1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, M_TIMESTAMP_EN,      1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, IO_DATA_ASSERT_EN,   1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, IO_DATA_DEASSERT_EN, 1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, RX_FIFO_RD_ERR_EN,   1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, RX_FIFO_WR_ERR_EN,   1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, TX_FIFO_RD_ERR_EN,   1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, TX_FIFO_WR_ERR_EN,   1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, RX_FIFO_WATERMARK_EN,1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, TX_FIFO_WATERMARK_EN,1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, RX_FIFO_LAST_EN,     1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, M_CMD_DONE_EN,       1) |
                HWIO_FVAL(GENI_M_IRQ_ENABLE, SEC_IRQ_EN,          1));
                
        HWIO_OUTXM(GENI_DATA_BASE(se_base),GENI_S_IRQ_ENABLE,
                HWIO_FMSK(GENI_S_IRQ_ENABLE, S_CMD_OVERRUN_EN       ) |
                HWIO_FMSK(GENI_S_IRQ_ENABLE, S_ILLEGAL_CMD_EN       ) |
                HWIO_FMSK(GENI_S_IRQ_ENABLE, S_CMD_CANCEL_EN        ) |
                HWIO_FMSK(GENI_S_IRQ_ENABLE, S_CMD_ABORT_EN         ) |
                HWIO_FMSK(GENI_S_IRQ_ENABLE, S_GP_IRQ_0_EN          ) |
                HWIO_FMSK(GENI_S_IRQ_ENABLE, S_GP_IRQ_1_EN          ) |
                HWIO_FMSK(GENI_S_IRQ_ENABLE, S_GP_IRQ_2_EN          ) |
                HWIO_FMSK(GENI_S_IRQ_ENABLE, S_GP_IRQ_3_EN          ) |
                HWIO_FMSK(GENI_S_IRQ_ENABLE, RX_FIFO_WR_ERR_EN      ) |
                HWIO_FMSK(GENI_S_IRQ_ENABLE, RX_FIFO_RD_ERR_EN      ) |
                HWIO_FMSK(GENI_S_IRQ_ENABLE, S_CMD_DONE_EN          ) ,
                HWIO_FVAL(GENI_S_IRQ_ENABLE, S_CMD_OVERRUN_EN,     1) |
                HWIO_FVAL(GENI_S_IRQ_ENABLE, S_ILLEGAL_CMD_EN,     1) |
                HWIO_FVAL(GENI_S_IRQ_ENABLE, S_CMD_CANCEL_EN,      1) |
                HWIO_FVAL(GENI_S_IRQ_ENABLE, S_CMD_ABORT_EN,       1) |
                HWIO_FVAL(GENI_S_IRQ_ENABLE, S_GP_IRQ_0_EN,        1) |
                HWIO_FVAL(GENI_S_IRQ_ENABLE, S_GP_IRQ_1_EN,        1) |
                HWIO_FVAL(GENI_S_IRQ_ENABLE, S_GP_IRQ_2_EN,        1) |
                HWIO_FVAL(GENI_S_IRQ_ENABLE, S_GP_IRQ_3_EN,        1) |
                HWIO_FVAL(GENI_S_IRQ_ENABLE, RX_FIFO_WR_ERR_EN,    1) |
                HWIO_FVAL(GENI_S_IRQ_ENABLE, RX_FIFO_RD_ERR_EN,    1) |
                HWIO_FVAL(GENI_S_IRQ_ENABLE, S_CMD_DONE_EN,        1) );
    }
    
    f_ctxt->flags        = FIFO_FLAGS_RESET;
    
    f_ctxt->command_wait_signal = qup_signal_init (dcfg,CORE_CMD_SIGNAL);

    QUP_FLAG_SET(f_ctxt->flags,FIFO_IFACE_REGISTERED);

    error:
    
    if (QUP_ERROR(q_status))
    {
        qup_fifo_iface_de_init(h_ctxt);
    }
    else
    {
        h_ctxt->iface_functions.iface_enable = qup_fifo_enable;
        h_ctxt->iface_functions.cancel_transfer = qup_fifo_cancel;
    }
    
    return q_status;
}

qup_status qup_fifo_iface_de_init (qup_hw_ctxt *h_ctxt)
{
    se_dev_cfg          *dcfg            = (se_dev_cfg *) h_ctxt->core_config;
    qup_fifo_core_ctxt  *f_ctxt          = (qup_fifo_core_ctxt *) h_ctxt->core_iface;
    qup_status           q_status        = QUP_SUCCESS;
    boolean              enable_island   = FALSE;
    
    if (f_ctxt == NULL) { return QUP_ERROR_INVALID_PARAMETER; }
    
    if(QUP_IS_SET(f_ctxt->flags,FIFO_IFACE_REGISTERED))
    {
        if(GET_QUP_MAJOR(dcfg->qup_data->qup_id) == SSC_QUP)
        {
            enable_island = TRUE;
        }
        
        if(!qup_interrupt_deregister_ex(dcfg->iface_data.core_irq, enable_island))
        {
            QUP_SE_LOG(dcfg,LEVEL_ERROR, "qup_interrupt_deregister_ex core_irq failed.");
            q_status = QUP_ERROR_PLATFORM_DE_REG_INT_FAIL;
        }
        
        if (f_ctxt->command_wait_signal != NULL)
        {
            qup_signal_de_init (dcfg, f_ctxt->command_wait_signal);
            f_ctxt->command_wait_signal = NULL;
        }

        qup_mem_free (f_ctxt,
                      sizeof(qup_fifo_core_ctxt),
                      dcfg,
                      CORE_IFACE_TYPE);
        
    }
    return q_status;
}

