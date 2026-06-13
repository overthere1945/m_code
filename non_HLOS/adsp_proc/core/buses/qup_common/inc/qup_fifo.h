/** 
    @file  qup_fifo.h
    @brief FIFO interface
 */
/*=============================================================================
            Copyright (c) 2020 Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __QUP_FIFO_H__
#define __QUP_FIFO_H__

/* *************************************************************** */
/*                          INCLUDE FILES                          */
/* *************************************************************** */
#include "qup_drv.h"

/* *************************************************************** */
/*                 DATA STRUCTURES and MACRO DEFINITONS            */
/* *************************************************************** */

/* Flags to mark State of Geni in qup_fifo_core_ctxt */
typedef enum _fifo_interface_flags
{
    FIFO_FLAGS_RESET            = 0,
    FIFO_IFACE_REGISTERED       = (1 << 0),
    FIFO_WAIT_FOR_CMD_COMP      = (1 << 1),
    
    FIFO_IN_ERROR_PATH          = (1 << 2),
    
    /*Channel Related flags*/
    FIFO_UNEXPECTED_EVT         = (1 << 3),
    
    /*Clean Related flags*/
    FIFO_CLEAN_IN_FLIGHT        = (1 << 4),
    FIFO_CLEAN_CANCELLED        = (1 << 5),
    FIFO_CLEAN_ABORTED          = (1 << 6),
    FIFO_CLEAN_COMPLETED        = (1 << 7),
    FIFO_CLEAN_FLAGS            = FIFO_CLEAN_IN_FLIGHT | FIFO_CLEAN_CANCELLED | FIFO_CLEAN_ABORTED | FIFO_CLEAN_COMPLETED,

}fifo_interface_flags;

/* Flags to mark status of transfer pointed by qup_fifo_iface_ctxt */
typedef enum _fifo_io_flags
{   
    FIFO_IO_UNUSED              =  0,
    FIFO_IO_IN_USE              = (1 << 0),
    FIFO_TX_COMPLETED           = (1 << 1),
    FIFO_RX_COMPLETED           = (1 << 2),
    FIFO_WAIT_FOR_CMD_DONE      = (1 << 3),
    FIFO_WAIT_FOR_CLEAN         = (1 << 4),
    FIFO_ALL_TRANSFER_FLAGS     = FIFO_TX_COMPLETED | FIFO_RX_COMPLETED | FIFO_WAIT_FOR_CMD_DONE | FIFO_WAIT_FOR_CLEAN ,
}fifo_io_flags;

/* Client Callback type of GP Error from M_IRQ_STATUS and DMA_IRQ_STAT */
typedef boolean (*GP_err_cb_type)(qup_client_ctxt *,uint32);

/* Client Callback type for Watermark interrupt*/
typedef void    (*wm_cb)(qup_client_ctxt *,boolean);

/* Client Callback type for Transfer completion*/
typedef void    (*tf_completion_cb)(qup_client_ctxt *);

/* Client Callback type for IN-Band Interrupt*/
typedef void    (*gp_sync_fifo_cb)(qup_hw_ctxt *,uint32 );

/* Structure held per core to bookmark client data / GENI status    
 * Need to be filled as a part of Interface initializations.
 */
typedef struct qup_fifo_core_ctxt
{
    uint16                  tx_fifo_depth;
    uint16                  rx_fifo_depth;
    uint32                  m_irq_stat_err_log;
    wm_cb                   watermark_cb;
    // rx_wm_cb                rx_watermark_cb;
    tf_completion_cb        tf_comp_cb;
    GP_err_cb_type          gp_err_cb;
    gp_sync_fifo_cb         ibs_cb;
    void                   *command_wait_signal;
    void                   *client_to_be_cancelled;
    fifo_interface_flags    flags;
} qup_fifo_core_ctxt;

/* MACRO's used to attach Callbacks to qup_fifo_core_ctxt structure */
#define QUP_FIFO_ATTACH_INBAND_CB(ctxt,cb)          ((qup_fifo_core_ctxt *)(ctxt))->ibs_cb = cb
#define QUP_FIFO_ATTACH_WATERMARK_CB(ctxt,cb)       ((qup_fifo_core_ctxt *)(ctxt))->watermark_cb = cb
#define QUP_FIFO_ATTACH_CMD_COMPLETION_CB(ctxt,cb)  ((qup_fifo_core_ctxt *)(ctxt))->tf_comp_cb = cb
#define QUP_FIFO_ATTACH_GP_ERR_CB(ctxt,cb)          ((qup_fifo_core_ctxt *)(ctxt))->gp_err_cb = cb

/* Structure held per client to bookmark transfer status
 * Need to be filled as a part of transfer queue.
 */
typedef struct qup_fifo_iface_ctxt
{
    uint16               tx_bytes_written;
    uint16               rx_bytes_read;
    qup_status           tf_status;
    fifo_io_flags        flags;
} qup_fifo_iface_ctxt;


/* *************************************************************** */
/*                            FUNCTIONS                            */
/* *************************************************************** */

/* Initialize FIFO interface */
qup_status qup_fifo_iface_init    (qup_hw_ctxt *h_ctxt);

/* De - Initialize FIFO interface */
qup_status qup_fifo_iface_de_init (qup_hw_ctxt *h_ctxt);

/* Cancel/Abort Present transfer FIFO interface */
qup_status qup_fifo_cancel        (qup_hw_ctxt *h_ctxt, qup_client_ctxt *c_ctxt,boolean force_reset);

/* Enable/Disable FIFO interface*/
boolean qup_fifo_enable        (qup_hw_ctxt *h_ctxt, boolean active);

/*****************************************************************
* Internal Functions to be used across Island and Non-Island Files
******************************************************************/

/* Polling function for fifo ISR*/
void  qup_fifo_isr (void *int_handle);

/* Fifo IO context allocator*/
void *qup_fifo_get_io_ctxt (se_dev_cfg *dcfg);

/* De-Allocate Fifo IO context*/
void  qup_fifo_release_io_ctxt (se_dev_cfg *dcfg, void *ptr);

/* Write Data to fifo
   Returns No. of bytes written*/
uint32  qup_write_to_fifo (qup_hw_ctxt *h_ctxt, uint8 *ptr, uint32 length);

/* Read Data from fifo
   Returns No. of bytes read*/
uint32  qup_read_from_fifo (qup_hw_ctxt *h_ctxt, uint8 *ptr, uint32 length);

/* Handle Transfer Clean from Timer Callback */
void  qup_fifo_handle_timer_cb(void *data);

#endif

