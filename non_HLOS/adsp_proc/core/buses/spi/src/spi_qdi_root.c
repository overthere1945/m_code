
/**
  @file  spi_qdi_root.c
  @brief SPI root pd interface to QDI
 */
/*=============================================================================
  Copyright (c) Qualcomm Technologies, Incorporated.
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
#include "qup_os.h"
#include "qup_log.h"

extern spi_qdi_opener   *spi_user_pd_openers[];
extern spi_qdi_opener   spi_user_pd_island;
extern qurt_qdi_obj_t   spi_qdi_root_obj;

int  spi_qdi_open(int client_handle);
void spi_qdi_root_release (qurt_qdi_obj_t *obj);

spi_status_t
spi_qdi_root_prepare_transfer
(
    int                 client_handle,
    spi_qdi_opener     *opener,
    void               *handle,
    spi_config_desc_pairs *spi_cfg_desc_pairs,
    uint32              num_desc_pairs,
    callback_fn         func,
    void               *ctxt,
    uint8               timestamp,
    void **slave_transfer_handle
)
{
    uint32 i = 0, j = 0, k = 0;
    spi_status_t status  = SPI_SUCCESS;
    spi_qdi_ctxt *q_ctxt = NULL;
    spi_qdi_ctxt *curr_q_ctxt = NULL;
    qup_client_ctxt     *c_ctxt;
    qup_hw_ctxt         *h_ctxt;
    se_dev_cfg          *dcfg;
    spi_clnt_qctxt_map *c_handle = (spi_clnt_qctxt_map*)handle;
    void *spi_handle = c_handle->handle;
    c_ctxt = (qup_client_ctxt *) spi_handle;
    h_ctxt = c_ctxt->h_ctxt;
    dcfg   = (se_dev_cfg *) h_ctxt->core_config;


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

    q_ctxt = spi_qdi_root_get_q_ctxt(opener, TRUE);

    if (q_ctxt == NULL)
    {
        QUP_LOG(LEVEL_ERROR,"[SPI] qdi ctxt allocation failed");
        return SPI_ERROR_QDI_ALLOC_FAIL;
    }

    q_ctxt->num_desc = 0;

    spi_config_desc_pairs cfg_dec_pairs[NUM_QDI_SPI_MAX_DESCS];

    //copy locally first and then populate q_ctxt
    if (QURT_EOK != qurt_qdi_copy_from_user(client_handle,
                                        (void *) cfg_dec_pairs,
                                        (void *) spi_cfg_desc_pairs,
                                        sizeof(spi_config_desc_pairs) * num_desc_pairs))
    {
        QUP_LOG(LEVEL_ERROR,"[spi_qdi_root_full_duplex] qdi copying local from user failed");\
        status = SPI_ERROR_QDI_COPY_FAIL;
        goto exit;
    }
    for ( i = 0; i <  num_desc_pairs; i++)
    {
        q_ctxt->multi_desc_pairs[i].num_desc = cfg_dec_pairs[i].num_desc;
        qup_mutex_global_lock();
        q_ctxt->multi_desc_pairs[i].config = qup_mem_alloc(dcfg, sizeof(spi_config_t), SPI_CONFIG_TYPE);
        qup_mutex_global_unlock();
        if (QURT_EOK != qurt_qdi_copy_from_user(client_handle,
                                            (void *) q_ctxt->multi_desc_pairs[i].config,
                                            (void *) cfg_dec_pairs[i].config,
                                            sizeof(spi_config_t)))
        {
            QUP_LOG(LEVEL_ERROR,"[spi_qdi_root_full_duplex] qdi copying config from user failed");
            status = SPI_ERROR_QDI_COPY_FAIL;
            goto exit;
        }

        qup_mutex_global_lock();
        q_ctxt->multi_desc_pairs[i].desc = qup_mem_alloc(dcfg, cfg_dec_pairs[i].num_desc * sizeof(spi_descriptor_t), SPI_DESC_TYPE);
        qup_mutex_global_unlock();
        if (QURT_EOK != qurt_qdi_copy_from_user(client_handle,
                                            (void *) (q_ctxt->multi_desc_pairs[i].desc),
                                            (void *)cfg_dec_pairs[i].desc,
                                            sizeof(spi_descriptor_t) * cfg_dec_pairs[i].num_desc))
        {
            QUP_LOG(LEVEL_ERROR,"[spi_qdi_root_full_duplex] qdi copying descriptors from user failed");
            status = SPI_ERROR_QDI_COPY_FAIL;
            goto exit;
        }

    // validate the buffer address here. note that this is only validation
        if(opener->is_island_process == FALSE)
        {
            for (j = 0; j <  cfg_dec_pairs[i].num_desc; j++)
            {
                q_ctxt->user_tx_buf[k] = (void *)((q_ctxt->multi_desc_pairs[i].desc + j)->tx_buf);
                q_ctxt->user_rx_buf[k] = (void *)((q_ctxt->multi_desc_pairs[i].desc + j)->rx_buf);


                if(q_ctxt->user_tx_buf[k])
                {
                    if (QURT_EOK != qurt_qdi_buffer_lock2(client_handle,
                                                        q_ctxt->user_tx_buf[k],
                                                        (q_ctxt->multi_desc_pairs[i].desc + j)->len,
                                                        QURT_PERM_READ|QURT_PERM_WRITE,
                                                        NULL))
                    {
                        QUP_LOG(LEVEL_ERROR, "[spi_qdi_root_full_duplex] qurt_qdi_buffer_lock2 TX Buf SPI_ERROR_QDI_MMAP_FAIL");
                        status = SPI_ERROR_QDI_MMAP_FAIL;
                        goto exit;
                    }
                }

                if(q_ctxt->user_rx_buf[k])
                {
                    if (QURT_EOK != qurt_qdi_buffer_lock2(client_handle,
                                                        q_ctxt->user_rx_buf[i],
                                                        (q_ctxt->multi_desc_pairs[i].desc + j)->len,
                                                        QURT_PERM_READ|QURT_PERM_WRITE,
                                                        NULL))
                    {
                        QUP_LOG(LEVEL_ERROR, "[spi_qdi_root_full_duplex] qurt_qdi_buffer_lock2 RX Buf SPI_ERROR_QDI_MMAP_FAIL");
                        status = SPI_ERROR_QDI_MMAP_FAIL;
                        goto exit;
                    }
                }
                q_ctxt->num_desc++;
                k++;
            }
        }
    }

    q_ctxt->ctxt = ctxt;
    q_ctxt->opener = opener;
    q_ctxt->spi_handle = spi_handle;
    q_ctxt->clnt_handle = client_handle;
    q_ctxt->cb_fn = func;
    q_ctxt->multi_slave_config = TRUE;
    q_ctxt->num_desc_pairs = num_desc_pairs;

    status = spi_prepare_transfer (spi_handle,
                              q_ctxt->multi_desc_pairs,
                              num_desc_pairs,
                              (func == NULL)?NULL:spi_notify_completion,
                              q_ctxt,
                              timestamp,
                              slave_transfer_handle);

    curr_q_ctxt = c_handle->q_ctxt;
    if(curr_q_ctxt == NULL)
    {
        c_handle->q_ctxt =  q_ctxt;
        q_ctxt->next = NULL;
    }
    else
    {
        while(curr_q_ctxt->next)
        {
            curr_q_ctxt = curr_q_ctxt->next;
        }
        curr_q_ctxt->next = q_ctxt;
        q_ctxt->next = NULL;
    }

#if 0 //debug only
    if (desc->rx_buf)
    memscpy((void *)&spi_debug_log.entries[idx].rx_buf, size, (void *)desc->rx_buf, size);

    spi_debug_log.entries[idx].spi_status = status;
    spi_debug_log.curr_idx = (spi_debug_log.curr_idx + 1) % LOG_SIZE;
#endif
exit:
    if (SPI_ERROR(status))
    {
        k=0;
        if (q_ctxt != NULL)
        {
            if(opener->is_island_process == FALSE)
            {
                for( i = 0; i <  num_desc_pairs; i++)
                {
                    for(j = 0; j <  spi_cfg_desc_pairs[i].num_desc; j++)
                    {
                        if(q_ctxt->user_tx_buf[k])
                        {
                            if (QURT_EOK != qurt_qdi_buffer_unlock(client_handle,
                                                                q_ctxt->user_tx_buf[k],
                                                                (q_ctxt->multi_desc_pairs[i].desc + j)->len,
                                                                NULL))
                            {
                                QUP_LOG(LEVEL_ERROR, "[spi_qdi_root_full_duplex] qurt_qdi_buffer_unlock TX FAILED");
                            }
                        }

                        if(q_ctxt->user_rx_buf[k])
                        {
                            if (QURT_EOK != qurt_qdi_buffer_unlock(client_handle,
                                                                q_ctxt->user_rx_buf[k],
                                                                (q_ctxt->multi_desc_pairs[i].desc + j)->len,
                                                                NULL))
                            {
                                QUP_LOG(LEVEL_ERROR, "[spi_qdi_root_full_duplex] qurt_qdi_buffer_unlock RX FAILED");
                            }
                        }
                        k++;
                     }

                }
                qup_mutex_global_lock();
                qup_mem_free (q_ctxt->multi_desc_pairs[i].desc , sizeof(spi_descriptor_t) * q_ctxt->multi_desc_pairs[i].num_desc , dcfg, SPI_DESC_TYPE);
                qup_mem_free (q_ctxt->multi_desc_pairs[i].config , sizeof(spi_config_t) , dcfg, SPI_CONFIG_TYPE);
                qup_mutex_global_unlock();
            }

            spi_qdi_root_put_q_ctxt(opener, q_ctxt, TRUE);
        }
    }

    return status;
}

spi_status_t spi_qdi_root_submit_transfer
(
    int client_handle,
    spi_qdi_opener     *opener,
    void *spi_handle,
    void *slave_transfer_handle
)
{
    return spi_submit_transfer(
    spi_handle,
    slave_transfer_handle);
}

int spi_qdi_root_invoke_non_island
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
    int ret = QURT_EOK, i,j,k = 0;
    spi_status_t status = SPI_SUCCESS;
    spi_qdi_opener *opener;
    uint8   handle_idx,slave_handle_idx;
    void*   encoded_h;
    void*   spi_handle;
    void *slave_xfer_handle;
    uint32  temp_var;
    qup_client_ctxt     *c_ctxt = NULL;
    qup_hw_ctxt *hctxt   = NULL;
    se_dev_cfg *dcfg = NULL;
    spi_qdi_ctxt *q_ctxt = NULL;
    spi_qdi_ctxt *curr_q_ctxt = NULL;

    if (obj == NULL) { return QURT_EINVALID; }

    opener = (spi_qdi_opener *) obj;

    switch(method)
    {
        case QDI_OPEN:
            ret = spi_qdi_open (client_handle);
            break;

        case SPI_QDI_OPEN:
        case SPI_QDI_OPEN_EX:
            if(method == SPI_QDI_OPEN)
            {
                if(opener->is_island_process && (a1.num > 0))
                {
                    status = (int) spi_open_ex (QUP_SSC, a1.num - 1, &spi_handle);
                }
                else
                {
                    status = (int) spi_open (a1.num, &spi_handle);
                }
            }
            else
            {
                status = (int) spi_open_ex (a1.num, a2.num, &spi_handle);
            }
            if (SPI_SUCCESS(status))
            {
                qurt_mutex_lock (&opener->mutex);
                for (i = 0; i < NUM_CLIENTS_PER_PD; i++)
                {
                    if (opener->c_handle[i].handle == NULL)
                    {
                        break;
                    }
                }
                if (i >= NUM_CLIENTS_PER_PD)
                {
                    status = SPI_ERROR_MEM_ALLOC;
                    QUP_LOG(LEVEL_ERROR,"SPI: Exceeding max supported clients per pd");
                    qurt_mutex_unlock (&opener->mutex);
                    spi_close (spi_handle);
                    break;
                }
                opener->c_handle[i].handle = spi_handle;
                qup_set_current_pid(opener->c_handle[i].handle,opener->pid);
                encoded_h = (void *)SPI_QDI_ENCODE(i);
                if (QURT_EOK != qurt_qdi_copy_to_user(client_handle,
                                        (void *) (method == SPI_QDI_OPEN)? a2.ptr : a3.ptr,
                                        (void *) &encoded_h,
                                        sizeof(void *)))
                {
                    QUP_LOG(LEVEL_ERROR, "qdi copying Open Handle failed");
                    status = SPI_ERROR_QDI_MMAP_FAIL;
                    opener->c_handle[i].handle = NULL;
                    qurt_mutex_unlock (&opener->mutex);
                    spi_close (spi_handle);
                    break;
                }

                qurt_mutex_unlock (&opener->mutex);
            }
            break;

        case SPI_QDI_CLOSE:
            SPI_QDI_DECODE(a1.ptr,handle_idx,status);
            c_ctxt = (qup_client_ctxt *) opener->c_handle[handle_idx].handle;
            hctxt   = c_ctxt->h_ctxt;
            dcfg = (se_dev_cfg*)hctxt->core_config;

            status = (int) spi_close (opener->c_handle[handle_idx].handle);

            qurt_mutex_lock   (&opener->mutex);
            if (SPI_SUCCESS(status))
            {
                opener->c_handle[handle_idx].handle = NULL;
                q_ctxt = opener->c_handle[handle_idx].q_ctxt;
                while(q_ctxt)
                {
                    k=0;
                    for(i = 0; i < q_ctxt->num_desc_pairs; i++)
                    {
                        if(opener->is_island_process == FALSE)
                        {
                            if(q_ctxt->multi_slave_config)
                            {
                                for(j = 0; j <  q_ctxt->multi_desc_pairs[i].num_desc; j++)
                                {
                                    if(QURT_EOK != qurt_qdi_buffer_unlock(client_handle,
                                                                      q_ctxt->user_tx_buf[k],
                                                                      (q_ctxt->multi_desc_pairs[i].desc + j)->len,
                                                                      NULL))
                                    {
                                        QUP_LOG(LEVEL_ERROR, "[SPI_QDI_CLOSE] qurt_qdi_buffer_unlock TX FAILED");
                                    }
                                    if(QURT_EOK != qurt_qdi_buffer_unlock(client_handle,
                                                                      q_ctxt->user_rx_buf[k],
                                                                      (q_ctxt->multi_desc_pairs[i].desc + j)->len,
                                                                      NULL))
                                    {
                                        QUP_LOG(LEVEL_ERROR, "[SPI_QDI_CLOSE] qurt_qdi_buffer_unlock TX FAILED");
                                    }
                                    k++;
                                }
                            }
                        }
                        qup_mutex_global_lock();
                        qup_mem_free (q_ctxt->multi_desc_pairs[i].desc , sizeof(spi_descriptor_t) * q_ctxt->multi_desc_pairs[i].num_desc , dcfg, SPI_DESC_TYPE);
                        qup_mem_free (q_ctxt->multi_desc_pairs[i].config , sizeof(spi_config_t) , dcfg, SPI_CONFIG_TYPE);
                        q_ctxt->multi_desc_pairs[i].desc = NULL;
                        q_ctxt->multi_desc_pairs[i].config = NULL;
                        q_ctxt->multi_desc_pairs[i].num_desc = 0;
                        qup_mutex_global_unlock();
                    }


                    q_ctxt->num_desc_pairs = 0;
#ifndef ENABLE_XFER_OPTIMIZE_IN_ISLAND
                    if (q_ctxt->multi_desc_pairs)
                    {
                        qurt_free(q_ctxt->multi_desc_pairs);
                        q_ctxt->multi_desc_pairs = NULL;
                    }
#endif
                    opener->c_handle[(uint32)q_ctxt->spi_handle].handle = NULL;
                    curr_q_ctxt =  q_ctxt;
                    q_ctxt = q_ctxt->next;
                    qup_memset(curr_q_ctxt, 0, sizeof(spi_qdi_ctxt));
                }
                    opener->c_handle[handle_idx].q_ctxt = NULL;
            }
            qurt_mutex_unlock (&opener->mutex);
            break;

        case SPI_QDI_RESET_ISLAND_RESOURCE:
            if(opener->is_island_process && (a1.num > 0))
            {
                status = (int) spi_reset_island_resource_ex (QUP_SSC, a1.num - 1, 0);
            }
            else
            {
                status = (int) spi_reset_island_resource (a1.num);
            }
            break;

        case SPI_QDI_RESET_ISLAND_RESOURCE_EX:
            status = (int) spi_reset_island_resource_ex (a1.num, a2.num, a3.num);
            break;

        case SPI_QDI_SETUP_ISLAND_RESOURCE:
            if(opener->is_island_process && (a1.num > 0))
            {
                status = (int) spi_setup_island_resource_ex (QUP_SSC, a1.num - 1, a2.num);
            }
            else
            {
                status = (int) spi_setup_island_resource (a1.num, a2.num);
            }
            break;

        case SPI_QDI_SETUP_ISLAND_RESOURCE_EX:
            status = (int) spi_setup_island_resource_ex (a1.num, a2.num, a3.num);
            break;

        case SPI_QDI_SET_USER_CB_DATA:
            opener->spi_user_cb = (qurt_cb_data_t *)a1.ptr;
            break;

        case SPI_QDI_ENABLE_SLAVE_INDEX:
            SPI_QDI_DECODE(a1.ptr,handle_idx,status);
            status = (int) spi_enable_slave_index (opener->c_handle[handle_idx].handle, a2.num);
            break;

        case SPI_QDI_PREPARE_TRANSFER:
            SPI_QDI_DECODE(a1.ptr,handle_idx,status);
            status = spi_qdi_root_prepare_transfer(
                client_handle,
                opener,
                (void*)&opener->c_handle[handle_idx],
                a2.ptr,
                a3.num,
                a4.ptr,
                a5.ptr,
                a6.num,
                &slave_xfer_handle);
            if(SPI_SUCCESS(status))
            {
                qurt_mutex_lock   (&opener->mutex);
                for (i = 0; i < NUM_CLIENTS_PER_PD; i++)
                {
                    if (opener->c_handle[i].handle == NULL)
                    {
                        break;
                    }
                }
                if (i >= NUM_CLIENTS_PER_PD)
                {
                    status = SPI_ERROR_MEM_ALLOC;
                    QUP_LOG(LEVEL_ERROR, "exceeding max supported clients per pd");
                    qurt_mutex_unlock (&opener->mutex);
                    //release c_ctxt
                    break;
                }
                opener->c_handle[i].handle = slave_xfer_handle;
                //qup_set_current_pid(opener->c_handle[i].handle,opener->pid);
                temp_var = SPI_QDI_ENCODE(i);
                if (QURT_EOK != qurt_qdi_copy_to_user(client_handle,
                                            (void *) (a7.ptr),
                                            (void *) &temp_var,
                                            sizeof(void *)))
                                            {
                                                QUP_LOG(LEVEL_ERROR, "[spi_qdi_root_transfer] qurt_qdi_copy_to_user for slave xfer handle");
                                                status = SPI_ERROR_QDI_COPY_FAIL;
                                                opener->c_handle[i].handle = NULL;
                                                qurt_mutex_unlock (&opener->mutex);
                                                //release ctxt
                                            }
                q_ctxt = opener->c_handle[handle_idx].q_ctxt;
                while(q_ctxt->next)
                {
                    q_ctxt = q_ctxt->next;
                }
                q_ctxt->spi_handle = (void*)i;
                qurt_mutex_unlock (&opener->mutex);
            }
            break;
        case SPI_QDI_SUBMIT_TRANSFER:
            SPI_QDI_DECODE(a1.ptr,handle_idx,status);
            SPI_QDI_DECODE(a2.ptr,slave_handle_idx,status);
            status =  spi_qdi_root_submit_transfer(
                client_handle,
                opener,
                opener->c_handle[handle_idx].handle,
                opener->c_handle[slave_handle_idx].handle);

            break;

        default:
            ret = qurt_qdi_method_default(
                      client_handle, obj, method,
                      a1, a2, a3, a4, a5, a6, a7,
                      a8, a9);
            break;
    }
    if (SPI_SUCCESS != status) return status;
    return ret;
}

int spi_qdi_root_pipe_init (spi_qdi_opener *opener)
{
    qurt_pipe_t         *pipe = &opener->pipe;
    qurt_pipe_attr_t    *attr = &opener->attr;
    qurt_pipe_data_t    *data = opener->data;

    qurt_pipe_attr_init(attr);
    qurt_pipe_attr_set_buffer(attr, data);
    qurt_pipe_attr_set_elements(attr, CB_PIPE_NUM_PARAMETERS * MAX_CONCURRENT_CLIENTS);
    qurt_pipe_attr_set_buffer_partition(attr, 1);

    return qurt_pipe_init(pipe, attr);
}

void spi_qdi_root_release (qurt_qdi_obj_t *obj)
{
    int i;
    spi_status_t status = SPI_SUCCESS;
    spi_qdi_opener *opener = (spi_qdi_opener *) obj;
    spi_qdi_ctxt *q_list;

    if (opener == NULL) { return; }

    q_list = opener->q_ctxt_list;
	/* Pipe read thread is gracefully exited at this time.
	   To avoid pipe writes, destroying the pipe */
    qurt_pipe_destroy(&opener->pipe);


    for (i = 0; i < MAX_CONCURRENT_CLIENTS; i++)
    {
        if (q_list[i].in_use)
        {
            //Should we explicitly stop GSI channels?
            q_list[i].in_use = FALSE;
#ifndef ENABLE_XFER_OPTIMIZE_IN_ISLAND
            if (q_list[i].multi_desc_pairs)
            {
                qurt_free(q_list[i].multi_desc_pairs);
                q_list[i].multi_desc_pairs = NULL;
            }
#endif
        }
    }

    qurt_mutex_lock   (&opener->mutex);
    for (i = 0; i < NUM_CLIENTS_PER_PD; i++)
    {
        if (opener->c_handle[i].handle != NULL)
        {
            status = spi_power_off (opener->c_handle[i].handle);
            if (status != SPI_SUCCESS)
            {
                QUP_LOG(LEVEL_INFO, "qdi release: power off status %d", status);
            }
            status = spi_close (opener->c_handle[i].handle);
            QUP_LOG(LEVEL_INFO, "qdi release: close status %d", status);
            opener->c_handle[i].handle = NULL;
        }
    }
    qurt_mutex_unlock (&opener->mutex);

    qurt_mutex_destroy(&opener->mutex);

    qup_mutex_global_lock();
    for (i = 0; i < NUM_PDS; i++)
    {
        if(opener == spi_user_pd_openers[i])
        {
            if(opener == &spi_user_pd_island)
            {
                opener->in_use = FALSE;
            }
            else
            {
                qurt_free(opener);
            }
            spi_user_pd_openers[i] = NULL;
            break;
        }
    }
    qup_mutex_global_unlock();
}

int spi_qdi_open(int client_handle)
{
    int i       = 0;
    int ret     = QURT_EOK;
    int handle  = -1;
    int pid;
    spi_qdi_opener *opener = NULL;
    qurt_process_attr_t  p_attr;

    ret = qurt_process_get_pid(client_handle, &pid);
    if(QURT_EOK != ret)
    {
        QUP_LOG(LEVEL_ERROR, "ERROR spi_qdi_open qurt_process_get_pid failed %d",ret);
        goto error;
    }

    ret = qurt_process_attr_get(pid,&p_attr);
    if(QURT_EOK != ret)
    {
        QUP_LOG(LEVEL_ERROR, "ERROR spi_qdi_open qurt_process_attr_get failed %d",ret);
        goto error;
    }

    qup_mutex_global_lock();

    while (i < NUM_PDS)
    {
        if (spi_user_pd_openers[i] == NULL)
        {
            if(0 == strncmp("qsh_process", p_attr.name, 11))
            {
                if(spi_user_pd_island.in_use  == FALSE)
                {
                    spi_user_pd_openers[i] = &spi_user_pd_island;
                    opener = spi_user_pd_openers[i];
                    opener->in_use = TRUE;
                    opener->is_island_process = TRUE;
                    opener->pid = pid;
                }
            }
            else
            {
                spi_user_pd_openers[i] = qurt_malloc(sizeof(spi_qdi_opener));
                opener = spi_user_pd_openers[i];
                QUP_VERIFY(opener);
                memset(opener, 0, sizeof(spi_qdi_opener));
                opener->in_use = TRUE;
                opener->is_island_process = FALSE;
                opener->pid = pid;
            }
            break;
        }
        i++;
    }
    qup_mutex_global_unlock();

    if (opener == NULL)
    {
        goto error;
    }

    opener->obj.invoke  = spi_qdi_root_invoke;
    opener->obj.refcnt  = QDI_REFCNT_INIT;
    opener->obj.release = spi_qdi_root_release;

    ret = spi_qdi_root_pipe_init (opener);
    if(QURT_EOK != ret)
    {
        goto error;
    }

    qurt_mutex_init (&opener->mutex);

    handle = qurt_qdi_handle_create_from_obj_t(client_handle, &opener->obj);
    if (handle < 0)
    {
        goto error;
    }

error:
    if (opener != NULL && ((handle < 0) || (ret != QURT_EOK)))
    {
        opener->in_use = FALSE;
    }
    return handle;
}

void spi_qdi_root_init(void)
{
    // qdi interface init
    spi_qdi_root_obj.invoke   = spi_qdi_root_invoke;
    spi_qdi_root_obj.refcnt   = QDI_REFCNT_INIT;
    spi_qdi_root_obj.release  = spi_qdi_root_release;
    qurt_qdi_register_devname (SPI_DRIVER_NAME, &spi_qdi_root_obj);
}
