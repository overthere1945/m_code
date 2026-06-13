
/**
  @file  spi_qdi_root_island.c
  @brief SPI root pd island interface to QDI
 */
/*=============================================================================
  Copyright (c) 2020, 2022-2023, 2024 Qualcomm Technologies, Incorporated.
  All rights reserved.
  Qualcomm Technologies, Confidential and Proprietary.
  =============================================================================*/

#include "spi_api.h"
#include "spi_qdi.h"
#include "spi_plat.h"
#include <stdlib.h>
#include <stringl/stringl.h>

#include "qup_alloc.h"
#include "qup_drv.h"
#include "qup_log.h"
#include "err.h"
#include "qup_os.h"
#include "qup_gpi.h"

#if 0
#define TX_BUF 16
#define RX_BUF TX_BUF
#define LOG_SIZE 64

typedef struct
{
  uint32 len;
  uint8  tx_buf[TX_BUF];
  uint8  rx_buf[RX_BUF];
  uint8  slv_idx;
  spi_status_t spi_status;
  qurt_thread_t thread_id;
}spi_log_entry_t;

typedef struct
{
 uint8 curr_idx;
 spi_log_entry_t entries[LOG_SIZE];
}spi_log_t;

/* SPI transaction log */
volatile spi_log_t spi_debug_log ;
volatile boolean spi_loop_err = TRUE;
#endif

spi_qdi_opener   *spi_user_pd_openers[NUM_PDS];
spi_qdi_opener   spi_user_pd_island;
qurt_qdi_obj_t   spi_qdi_root_obj;

void spi_enqueue_user_worker(spi_qdi_opener *opener,int client_handle)
{
  qurt_cb_result_t res;

  res = qurt_qdi_cb_invoke_async(client_handle, opener->spi_user_cb, SPI_CB_THREAD_PRIORITY);
  if ( res != QURT_CB_OK )
  {
    ERR_FATAL("SPI Callback En-queue failed: 0x%x : 0x%xd", res, opener,0);
  }
}

void spi_notify_completion (uint32 status, void *context)
{
    qurt_pipe_data_t data1 = 0;
    qurt_pipe_data_t data2 = 0;
    qurt_pipe_data_t data3 = 0;
    qurt_pipe_data_t data4 = 0;

    qurt_pipe_t         *pipe   = NULL;
    spi_qdi_ctxt        *q_ctxt = NULL;
    spi_qdi_opener      *opener = NULL;
    uint8                i;

    q_ctxt = (spi_qdi_ctxt *) context;

    if (q_ctxt == NULL) { return; }

    opener = q_ctxt->opener;
    if (opener == NULL) { return; }

    if(opener->is_island_process == FALSE && q_ctxt->multi_slave_config == FALSE)
    {        
        for (i = 0; i < q_ctxt->num_desc; i++)
        {
            if(q_ctxt->user_tx_buf[i])
            {
                if (QURT_EOK != qurt_qdi_buffer_unlock(q_ctxt->clnt_handle,
                                                   q_ctxt->user_tx_buf[i],
                                                   (q_ctxt->desc + i)->len,
                                                   NULL))
                {
                    QUP_LOG(LEVEL_ERROR, "[spi_notify_completion] qurt_qdi_buffer_unlock TX FAILED");
                }
            }
            
            if(q_ctxt->user_rx_buf[i])
            {
                if (QURT_EOK != qurt_qdi_buffer_unlock(q_ctxt->clnt_handle,
                                                   q_ctxt->user_rx_buf[i],
                                                   (q_ctxt->desc + i)->len,
                                                   NULL))
                {
                    QUP_LOG(LEVEL_ERROR, "[spi_notify_completion] qurt_qdi_buffer_unlock RX FAILED");
                }
            }
        }
        
    }

    pipe = &opener->pipe;

    memscpy(&data1, sizeof(data1), (void *) &(q_ctxt->cb_fn) ,  sizeof(callback_fn));
    memscpy(&data2, sizeof(data2), (void *) &status,            sizeof(uint32));
    memscpy(&data3, sizeof(data3), (void *) &(q_ctxt->ctxt),    sizeof(void *));
    memscpy(&data4, sizeof(data4), (void *) &(q_ctxt),          sizeof(void *));
    
    QUP_LOG(LEVEL_ERROR, "[spi_notify_completion] context : 0x%x",q_ctxt->ctxt);
    do
    {
        if (QURT_EOK != qurt_pipe_send_cancellable (pipe, data1))
        {
            QUP_LOG(LEVEL_ERROR, "[SPI] spi_notify_completion: qdi pipe send failed for func");
            break;
        }
        if (QURT_EOK != qurt_pipe_send_cancellable (pipe, data2))
        {
            QUP_LOG(LEVEL_ERROR, "[SPI] spi_notify_completion: qdi pipe send failed for status");
            break;
        }
        if (QURT_EOK != qurt_pipe_send_cancellable (pipe, data3))
        {
            QUP_LOG(LEVEL_ERROR,"[SPI] spi_notify_completion: qdi pipe send failed for transferred");
            break;
        }
        if (QURT_EOK != qurt_pipe_send_cancellable (pipe, data4))
        {
            QUP_LOG(LEVEL_ERROR,"[SPI] spi_notify_completion: qdi pipe send failed for transferred");
            break;
        }
    }
    while (0);
    
#ifdef SPI_ENABLE_QURT_CB_FW
    spi_enqueue_user_worker(opener,q_ctxt->clnt_handle);
#endif

    QUP_LOG(LEVEL_VERBOSE, "[SPI] spi_notify_completion: complete");
}

void spi_qdi_root_put_q_ctxt(spi_qdi_opener *opener, spi_qdi_ctxt *q_ctxt, boolean is_multi_desc)
{
    qurt_mutex_lock   (&opener->mutex);

#ifndef ENABLE_XFER_OPTIMIZE_IN_ISLAND
    if (q_ctxt->multi_desc_pairs)
    {
        if(is_multi_desc)
        {
            qurt_free(q_ctxt->multi_desc_pairs);
            q_ctxt->multi_desc_pairs = NULL;
        }
    }
#endif
    q_ctxt->in_use = FALSE;
    QUP_LOG(LEVEL_VERBOSE, "q_ctxt freed 0x%08x", q_ctxt);
    qurt_mutex_unlock (&opener->mutex);
}
spi_qdi_ctxt *spi_qdi_root_get_q_ctxt(spi_qdi_opener *opener, boolean is_multi_desc)
{
    uint16 i = 0;
    boolean found = FALSE;
    spi_qdi_ctxt *q_ctxt = opener->q_ctxt_list;
    uint8 *mem = NULL;

    qurt_mutex_lock   (&opener->mutex);
    for (i = 0; i < MAX_CONCURRENT_CLIENTS; i++)
    {
        if (q_ctxt->in_use == FALSE)
        {
            QUP_LOG(LEVEL_VERBOSE, "q_ctxt alloc 0x%08x", q_ctxt);
            found = TRUE;
            break;
        }
        q_ctxt++;
    }
    if (found)
    {
        mem = (uint8*) q_ctxt;
        /* memset new q_ctxt to 0 */
        for ( i = 0; i < sizeof(spi_qdi_ctxt); i++)
        {
            mem[i] = 0;
        }
        q_ctxt->in_use = TRUE;

#ifndef ENABLE_XFER_OPTIMIZE_IN_ISLAND
        if(is_multi_desc)
        {
            q_ctxt->multi_desc_pairs = qurt_malloc(sizeof(spi_config_desc_pairs) * NUM_QDI_SPI_MAX_DESCS);
        }
        else
        {
            q_ctxt->multi_desc_pairs = NULL;
        }
#endif

    }
    else        { q_ctxt = NULL; }
    qurt_mutex_unlock (&opener->mutex);
    return q_ctxt;
}

int spi_qdi_root_cb_handler (int client_handle, spi_qdi_opener *opener, spi_qdi_callback *cb)
{
    qurt_pipe_data_t callback;
    qurt_pipe_data_t status;
    qurt_pipe_data_t context;
    qurt_pipe_t     *pipe;
    qurt_pipe_data_t ctxt;
    int ret = QURT_EOK;
    spi_qdi_callback cb_local;
    spi_status_t i_status = SPI_SUCCESS;
    spi_qdi_ctxt *q_ctxt = NULL;
    
    pipe = &opener->pipe;
    if ((ret = qurt_pipe_receive_cancellable(pipe, &callback)) != QURT_EOK)    { return ret; }
    if ((ret = qurt_pipe_receive_cancellable(pipe, &status))   != QURT_EOK)    { return ret; }
    if ((ret = qurt_pipe_receive_cancellable(pipe, &context))  != QURT_EOK)    { return ret; }
    if ((ret = qurt_pipe_receive_cancellable(pipe, &ctxt))     != QURT_EOK)    { return ret; }

    cb_local.func        = (callback_fn) callback;
    cb_local.status      = (uint32) status;
    cb_local.ctxt        = (void *) context;
    
    q_ctxt = (spi_qdi_ctxt*) ctxt;

    if (QURT_EOK != qurt_qdi_copy_to_user(client_handle,
                                          cb,
                                          &cb_local,
                                          sizeof(spi_qdi_callback)))
        {
            QUP_LOG(LEVEL_ERROR, "[spi_qdi_root_cb_handler] qdi copying to CB failed");
            i_status = SPI_ERROR_QDI_COPY_FAIL;
            return i_status;
        }
    
    if(q_ctxt->multi_slave_config == FALSE)
    {
        spi_qdi_root_put_q_ctxt(opener, q_ctxt, FALSE);
    }

    return ret;
}

spi_status_t spi_qdi_root_full_duplex
(
    int                 client_handle,
    spi_qdi_opener     *opener,
    void               *spi_handle,
    spi_config_t       *config,
    spi_descriptor_t   *desc,
    uint32              num_descriptors,
    callback_fn         func,
    void               *ctxt,
    uint8               timestamp
)
{
    uint32 i = 0; 
    spi_status_t status  = SPI_SUCCESS;
    spi_qdi_ctxt *q_ctxt = NULL;

#if 0 //debug only	
    uint8 idx; uint8 size;
    idx = spi_debug_log.curr_idx;
    size = desc->len > TX_BUF ? TX_BUF : desc->len; 
    *((uint32 *)(spi_debug_log.entries[idx].tx_buf)) = 0xDEADBEEF;
    *((uint32 *)(spi_debug_log.entries[idx].rx_buf)) = 0xDEADBEEF;
    spi_debug_log.entries[idx].len = desc->len;
    spi_debug_log.entries[idx].slv_idx = config->spi_slave_index;
    spi_debug_log.entries[idx].thread_id = qurt_thread_get_id();
    if (desc->tx_buf)
    {
        memscpy((void *)&spi_debug_log.entries[idx].tx_buf, size, (void *)desc->tx_buf, size);
    }
#endif

    if (num_descriptors > NUM_QDI_SPI_MAX_DESCS)
    {
        QUP_LOG(LEVEL_ERROR,"[SPI] qdi interface supports only %d descriptors per transfer", NUM_QDI_SPI_MAX_DESCS);
        return SPI_ERROR_INVALID_PARAM;
    }

    q_ctxt = spi_qdi_root_get_q_ctxt(opener, FALSE);

    if (q_ctxt == NULL)
    {
        QUP_LOG(LEVEL_ERROR,"[SPI] qdi ctxt allocation failed");
        return SPI_ERROR_QDI_ALLOC_FAIL;
    }
    
    q_ctxt->num_desc = 0;

    if (QURT_EOK != qurt_qdi_copy_from_user(client_handle,
                                            (void *) (&(q_ctxt->config)),
                                            (void *) config,
                                            sizeof(spi_config_t)))
    {
        QUP_LOG(LEVEL_ERROR,"[spi_qdi_root_full_duplex] qdi copying config from user failed");
        status = SPI_ERROR_QDI_COPY_FAIL;
        goto exit;
    }

    if (QURT_EOK != qurt_qdi_copy_from_user(client_handle,
                                            (void *) (q_ctxt->desc),
                                            (void *) desc,
                                            sizeof(spi_descriptor_t) * num_descriptors))
    {
        QUP_LOG(LEVEL_ERROR,"[spi_qdi_root_full_duplex] qdi copying descriptors from user failed");
        status = SPI_ERROR_QDI_COPY_FAIL;
        goto exit;
    }
    
    // validate the buffer address here. note that this is only validation
    if(opener->is_island_process == FALSE)
    {
        for (i = 0; i < num_descriptors; i++)
        {
            q_ctxt->user_tx_buf[i] = (void *)((q_ctxt->desc + i)->tx_buf);
            q_ctxt->user_rx_buf[i] = (void *)((q_ctxt->desc + i)->rx_buf);
    
            
            if(q_ctxt->user_tx_buf[i])
            {
                if (QURT_EOK != qurt_qdi_buffer_lock2(client_handle,
                                                      q_ctxt->user_tx_buf[i],
                                                      (q_ctxt->desc + i)->len,
                                                      QURT_PERM_READ|QURT_PERM_WRITE,
                                                      NULL))
                {
                    QUP_LOG(LEVEL_ERROR, "[spi_qdi_root_full_duplex] qurt_qdi_buffer_lock2 TX Buf SPI_ERROR_QDI_MMAP_FAIL");
                    status = SPI_ERROR_QDI_MMAP_FAIL;
                    goto exit;
                }
            }
            
            if(q_ctxt->user_rx_buf[i])
            {
                if (QURT_EOK != qurt_qdi_buffer_lock2(client_handle,
                                                      q_ctxt->user_rx_buf[i],
                                                      (q_ctxt->desc + i)->len,
                                                      QURT_PERM_READ|QURT_PERM_WRITE,
                                                      NULL))
                {
                    QUP_LOG(LEVEL_ERROR, "[spi_qdi_root_full_duplex] qurt_qdi_buffer_lock2 RX Buf SPI_ERROR_QDI_MMAP_FAIL");
                    status = SPI_ERROR_QDI_MMAP_FAIL;
                    goto exit;
                }
            }   
            q_ctxt->num_desc++;
        }            
    }
    
    q_ctxt->ctxt = ctxt;
    q_ctxt->opener = opener;
    q_ctxt->spi_handle = spi_handle;
    q_ctxt->clnt_handle = client_handle;
    q_ctxt->cb_fn = func;

    status = spi_full_duplex (spi_handle,
                              &q_ctxt->config,
                              q_ctxt->desc,
                              num_descriptors,
                              (func == NULL)?NULL:spi_notify_completion,
                              q_ctxt,
                              timestamp);


#if 0 //debug only	
    if (desc->rx_buf)
    memscpy((void *)&spi_debug_log.entries[idx].rx_buf, size, (void *)desc->rx_buf, size);
 
    spi_debug_log.entries[idx].spi_status = status;
    spi_debug_log.curr_idx = (spi_debug_log.curr_idx + 1) % LOG_SIZE;
#endif  
exit:
    if (SPI_ERROR(status) || func == NULL)
    {
        if (q_ctxt != NULL)
        {
            if(opener->is_island_process == FALSE)
            {
                for (i = 0; i < q_ctxt->num_desc; i++)
                {
                    if(q_ctxt->user_tx_buf[i])
                    {
                        if (QURT_EOK != qurt_qdi_buffer_unlock(client_handle,
                                                               q_ctxt->user_tx_buf[i],
                                                               (q_ctxt->desc + i)->len,
                                                               NULL))
                        {
                            QUP_LOG(LEVEL_ERROR, "[spi_qdi_root_full_duplex] qurt_qdi_buffer_unlock TX FAILED");
                        }
                    }
                    
                    if(q_ctxt->user_rx_buf[i])
                    {
                        if (QURT_EOK != qurt_qdi_buffer_unlock(client_handle,
                                                               q_ctxt->user_rx_buf[i],
                                                               (q_ctxt->desc + i)->len,
                                                               NULL))
                        {
                            QUP_LOG(LEVEL_ERROR, "[spi_qdi_root_full_duplex] qurt_qdi_buffer_unlock RX FAILED");
                        }
                    }
                }
            }

            spi_qdi_root_put_q_ctxt(opener, q_ctxt, FALSE);
        }
    }

    return status;
}


spi_status_t spi_qdi_root_get_timestamp_64 (int client_handle, void *spi_handle, spi_timestamp_type *ts_type, uint64 *timestamp_val)
{
    spi_status_t         status  = SPI_SUCCESS;
    spi_timestamp_type   type;
    uint64               t_stamp;
    
    status = spi_get_timestamp_64(spi_handle, &type, &t_stamp);
    
    if (SPI_SUCCESS(status))
    {
        if (QURT_EOK != qurt_qdi_copy_to_user(client_handle,
                                              (void *) ts_type,
                                              (void *) &type,
                                               sizeof(spi_timestamp_type)))
        {
            QUP_LOG(LEVEL_ERROR,"[spi_qdi_root_get_timestamp_64] qdi copying type failed");
            status = SPI_ERROR_QDI_COPY_FAIL;
            goto exit;
        }
        if (QURT_EOK != qurt_qdi_copy_to_user(client_handle,
                                              (void *) timestamp_val,
                                              (void *) &t_stamp,
                                               sizeof(uint64)))
        {
            QUP_LOG(LEVEL_ERROR,"[spi_qdi_root_get_timestamp_64] qdi copying TS failed");
            status = SPI_ERROR_QDI_COPY_FAIL;
            goto exit;
        }
    }
exit:
    return status;
}

int spi_qdi_root_invoke
(
    int             client_handle,
    qurt_qdi_obj_t *obj,
    int             method,
    qurt_qdi_arg_t  a1,
    qurt_qdi_arg_t  a2,
    qurt_qdi_arg_t  a3,
    qurt_qdi_arg_t  a4,
    qurt_qdi_arg_t  a5,
    qurt_qdi_arg_t  a6,
    qurt_qdi_arg_t  a7,
    qurt_qdi_arg_t  a8,
    qurt_qdi_arg_t  a9
)
{
    int ret = QURT_EOK;
    spi_status_t status = SPI_SUCCESS;
    spi_qdi_opener *opener;
    uint8   handle_idx;
    if (obj == NULL) { return QURT_EINVALID; }

    opener = (spi_qdi_opener *) obj;

    switch(method)
    {
        case SPI_QDI_POWER_ON:
            SPI_QDI_DECODE(a1.ptr,handle_idx,status);
            status = (int) spi_power_on (opener->c_handle[handle_idx].handle);
            break;

        case SPI_QDI_POWER_OFF:
            SPI_QDI_DECODE(a1.ptr,handle_idx,status);
            status = (int) spi_power_off (opener->c_handle[handle_idx].handle);
            break;

        case SPI_QDI_FULL_DUPLEX:
            SPI_QDI_DECODE(a1.ptr,handle_idx,status);
            status = (int) spi_qdi_root_full_duplex (
                      client_handle,
                      opener,
                      opener->c_handle[handle_idx].handle,     // void *spi_handle
                      a2.ptr,     // spi_config_t *config
                      a3.ptr,     // spi_descriptor_t *desc
                      a4.num,     // uint32 num_descriptors
                      a5.ptr,     // callback_fn func
                      a6.ptr,     // void *ctxt
                      a7.num);     // bool get_timestamp
            break;

        case SPI_QDI_GET_TIMESTAMP:
            status = SPI_ERROR;
            break;
            
        case SPI_QDI_GET_TIMESTAMP_64:
            SPI_QDI_DECODE(a1.ptr,handle_idx,status);
            status = spi_qdi_root_get_timestamp_64(client_handle, opener->c_handle[handle_idx].handle, (spi_timestamp_type *)a2.ptr, (uint64 *)a3.ptr);
            break;

        case SPI_QDI_GET_CALLBACK_DATA:
            ret = spi_qdi_root_cb_handler (client_handle,opener, a1.ptr);
            break;
        
        default:
            ret = spi_qdi_root_invoke_non_island(
                      client_handle, obj, method,
                      a1, a2, a3, a4, a5, a6, a7,
                      a8, a9);
            break;
    }
    if (SPI_SUCCESS != status) return status;
    return ret;
}