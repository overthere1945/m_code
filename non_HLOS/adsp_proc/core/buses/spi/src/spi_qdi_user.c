/**
    @file  spi_qdi_user.c
    @brief SPI User PD interface to QDI
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#include "spi_api.h"
#include "spi_qdi.h"
#include "qurt.h"
#include "qurt_qdi.h"
#include "qup_common_internal.h"
#include <stringl/stringl.h>

#define QUP_LOG(...) 

#ifdef SPI_ENABLE_QURT_CB_FW
qurt_cb_data_t  spi_user_cb;
#else
#define SPI_CB_THREAD_STACK_SIZE   2048
static uint8 spi_cb_thread_stack[SPI_CB_THREAD_STACK_SIZE];
static qurt_thread_t spi_cb_thread;
#endif

static int spi_qdi_handle = -1;
int spi_get_callback (spi_qdi_callback *cb);

#ifdef SPI_ENABLE_QURT_CB_FW
void spi_qdi_user_cb_handler (void *arg)
{
    spi_qdi_callback cb = {NULL, 0, NULL};
    if (QURT_EOK == spi_get_callback (&cb))
    {
        if (cb.func != NULL)
        {
            cb.func(cb.status, cb.ctxt);
        }
        else
        {
            QUP_LOG("\n[SPI] qdi callback func is null\n");
        }
    }
}
#else
void spi_qdi_user_cb_handler (void *arg)
{
    while (1)
    {
        spi_qdi_callback cb = {NULL, 0, NULL};
        if (QURT_EOK == spi_get_callback (&cb))
        {
            if (cb.func != NULL)
            {
                cb.func(cb.status, cb.ctxt);
            }
            else
            {
                QUP_LOG("\n[SPI] qdi callback func is null\n");
            }
        }
    }
}
#endif


#ifdef SPI_ENABLE_QURT_CB_FW
void spi_qdi_user_init (void)
{
    if (spi_qdi_handle == -1)
    {
        spi_qdi_handle = qurt_qdi_open(SPI_DRIVER_NAME);
    }

    if(spi_qdi_handle < 0)
    {
        QUP_LOG("\n[SPI] qdi open failed %d\n", spi_qdi_handle);
        return;
    }
    
    qurt_cb_data_init(&spi_user_cb);
    qurt_cb_data_set_cbfunc(&spi_user_cb, (void*)spi_qdi_user_cb_handler);
    qurt_cb_data_set_cbarg(&spi_user_cb, (unsigned)0);
    
    qurt_qdi_handle_invoke(
           spi_qdi_handle,
           SPI_QDI_SET_USER_CB_DATA,
           &spi_user_cb);
}
#else
void spi_qdi_user_init (void)
{
    int ret = 0;
    qurt_thread_attr_t tattr;
    unsigned int stackbase;
	user_pd_features* qup_user_pd_feature_enabled;
	
    if (spi_qdi_handle == -1)
    {
        spi_qdi_handle = qurt_qdi_open(SPI_DRIVER_NAME);
    }

    if(spi_qdi_handle < 0)
    {
        QUP_LOG("\n[SPI] qdi open failed %d\n", spi_qdi_handle);
        return;
    }

    qup_user_pd_feature_enabled = qup_get_user_pd_features();

    stackbase = (unsigned int) &spi_cb_thread_stack;
    qurt_thread_attr_init (&tattr);
    qurt_thread_attr_set_stack_size (&tattr, (SPI_CB_THREAD_STACK_SIZE - 8));
    qurt_thread_attr_set_stack_addr (&tattr, (void*)((stackbase + 7) & (~7)));
    qurt_thread_attr_set_priority (&tattr, SPI_CB_THREAD_PRIORITY - 1);
    if((qup_user_pd_feature_enabled -> user_pd_island_enabled))
    {
	    qurt_thread_attr_set_tcb_partition (&tattr, 1);
	}

    qurt_thread_attr_set_name (&tattr, SPI_THREAD_NAME);
    ret = qurt_thread_create (&spi_cb_thread, &tattr, spi_qdi_user_cb_handler, NULL);

    if(QURT_EOK != ret)
    {
        QUP_LOG("\n[SPI] thread init failed %d\n", ret);
        qurt_qdi_close(spi_qdi_handle);
        return;
    }
}
#endif

spi_status_t spi_open (spi_instance_t instance, void **spi_handle)
{
    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_OPEN,
            instance,
            spi_handle);
}

spi_status_t spi_open_ex (QUP_TYPE qup, uint32 se_index, void **spi_handle)
{
    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_OPEN_EX,
            qup,
            se_index,
            spi_handle);
}

spi_status_t spi_close (void *spi_handle)
{
    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_CLOSE,
            spi_handle);
}

spi_status_t spi_power_on (void *spi_handle)
{
    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_POWER_ON,
            spi_handle);
}

spi_status_t spi_power_off (void *spi_handle)
{
    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_POWER_OFF,
            spi_handle);
}

spi_status_t spi_full_duplex (void *spi_handle,
                              spi_config_t *config,
                              spi_descriptor_t *desc,
                              uint32 num_descriptors,
                              callback_fn c_fn,
                              void *c_ctxt,
                              uint8 timestamp)
{
    uint32 i = 0;
    for (i = 0; i < num_descriptors; i++)
    {
        if((desc + i)->rx_buf != NULL)
        {
            qurt_mem_cache_clean((qurt_addr_t) (desc + i)->rx_buf,
                                    (qurt_size_t) (desc + i)->len,
                                    QURT_MEM_CACHE_INVALIDATE,
                                    QURT_MEM_DCACHE);
        }
        if((desc + i)->tx_buf !=NULL)
        {
            qurt_mem_cache_clean((qurt_addr_t) (desc + i)->tx_buf,
                                    (qurt_size_t) (desc + i)->len,
                                    QURT_MEM_CACHE_FLUSH,
                                    QURT_MEM_DCACHE);
        }
        
    }
    
    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_FULL_DUPLEX,
            spi_handle,
            config,
            desc,
            num_descriptors,
            c_fn,
            c_ctxt,
            timestamp);
}

spi_status_t
spi_prepare_transfer
(
    void *spi_handle,
    spi_config_desc_pairs *spi_cfg_desc_pairs,
    uint16 num_desc_pairs,
    callback_fn c_fn,
    void *ctxt,
    uint8 timestamp,
    void **slave_transfer_handle    
)
{
    uint16 i = 0, j = 0;
    for ( i = 0; i < num_desc_pairs; i++)
    {
        spi_descriptor_t *desc = spi_cfg_desc_pairs[i].desc;
        uint16 num_desc = spi_cfg_desc_pairs[i].num_desc;

        for(j = 0; j < num_desc; j++)
        {
            if((desc+j)->rx_buf != NULL)
            {
                qurt_mem_cache_clean((qurt_addr_t) (desc+j)->rx_buf,
                                    (qurt_size_t) (desc+j)->len,
                                    QURT_MEM_CACHE_INVALIDATE,
                                    QURT_MEM_DCACHE);
            }
            if(desc->tx_buf !=NULL)
             {
                qurt_mem_cache_clean((qurt_addr_t) (desc+j)->tx_buf,
                                        (qurt_size_t) (desc+j)->len,
                                        QURT_MEM_CACHE_FLUSH,
                                        QURT_MEM_DCACHE);
            }
        }
    }    

    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_PREPARE_TRANSFER,
            spi_handle, 
            spi_cfg_desc_pairs, 
            num_desc_pairs, 
            c_fn,
            ctxt,
            timestamp,
            slave_transfer_handle);
}

spi_status_t
spi_submit_transfer
(
    void *spi_handle,
    void *slave_transfer_handle
)
{
     return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle, 
            SPI_QDI_SUBMIT_TRANSFER,
            spi_handle, 
            slave_transfer_handle);
}

spi_status_t spi_get_timestamp (void *spi_handle, uint64 *start_time, uint64 *completed_time)
{
    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_GET_TIMESTAMP,
            spi_handle,
            start_time,
            completed_time);
}

int spi_get_callback (spi_qdi_callback *cb)
{
    return (uint32) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_GET_CALLBACK_DATA,
            cb);
}

spi_status_t spi_setup_island_resource (spi_instance_t instance, uint32 frequency_hz)
{
    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_SETUP_ISLAND_RESOURCE,
            instance,
            frequency_hz);
}

spi_status_t spi_setup_island_resource_ex (QUP_TYPE qup, uint32 se_index, uint32 frequency_hz)
{
    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_SETUP_ISLAND_RESOURCE_EX,
            qup,
            se_index,
            frequency_hz);
}

spi_status_t spi_reset_island_resource (spi_instance_t instance)
{
    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_RESET_ISLAND_RESOURCE,
            instance);
}

spi_status_t spi_reset_island_resource_ex (QUP_TYPE qup, uint32 se_index, uint32 frequency_hz)
{
    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_RESET_ISLAND_RESOURCE_EX,
            qup,
            se_index,
            frequency_hz);
}

spi_status_t spi_get_timestamp_64 (void *spi_handle, spi_timestamp_type *ts_type, uint64 *timestamp_val)
{
    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_GET_TIMESTAMP_64,
            spi_handle,
            ts_type,
            timestamp_val);
}

spi_status_t spi_enable_slave_index (void *spi_handle, spi_cs_index cs_index)
{
    return (spi_status_t) qurt_qdi_handle_invoke(
            spi_qdi_handle,
            SPI_QDI_ENABLE_SLAVE_INDEX,
            spi_handle,
            cs_index);
}

