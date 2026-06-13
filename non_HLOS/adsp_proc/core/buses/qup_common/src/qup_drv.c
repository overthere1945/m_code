/**
    @file  qup_drv.c
    @brief Driver implementation
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
#include "qup_plat.h"

#include "qup_drv.h"
#include "qup_gpi.h"
#include "qup_fifo.h"

#include "i3c_drv.h"    // i3c_drv
#include "i3c_ibi_drv.h"

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

qup_hw_ctxt *qup_cores_head = NULL;

qup_hw_ctxt *alloc_hw_ctxt (QUP_TYPE qup, uint32 se_id)
{
    qup_hw_ctxt *h_ctxt = qup_cores_head;

    se_dev_cfg  *dcfg;

    while (h_ctxt != NULL)
    {
        dcfg = (se_dev_cfg *) h_ctxt->core_config;
        if (dcfg->qup_data->qup_id == qup &&
            dcfg->se_id == se_id)
        {
            return h_ctxt;
        }
        h_ctxt = h_ctxt->next;
    }

    dcfg = qup_get_hw_cfg(qup, se_id);

    if (dcfg != NULL)
    {
        h_ctxt = (qup_hw_ctxt *) qup_mem_alloc(dcfg,
                                               sizeof(qup_hw_ctxt),
                                               HW_CTXT_TYPE);
        if (h_ctxt != NULL)
        {
            h_ctxt->core_config = dcfg;

            // attach to the head of the list
            h_ctxt->next    = qup_cores_head;
            qup_cores_head  = h_ctxt;
        }
    }
    return h_ctxt;
}

void free_hw_ctxt (qup_hw_ctxt *h_ctxt)
{
    qup_hw_ctxt *h_curr = qup_cores_head;
    qup_hw_ctxt *prev = NULL;
    boolean      core_found = FALSE;

    while (h_curr != NULL)
    {
        if (h_curr == h_ctxt)
        {
            core_found = TRUE;
            break;
        }
        prev = h_curr;
        h_curr = h_curr->next;
    }

    if (core_found)
    {
        if (prev != NULL)
        {
            // removal from end or middle
            prev->next = h_curr->next;
        }
        else
        {
            // removal from head
            qup_cores_head = h_curr->next;
        }
        qup_mem_free (h_curr, sizeof(qup_hw_ctxt), h_curr->core_config, HW_CTXT_TYPE);
    }
    else
    {
        QUP_ASSERT(NULL,QUP_FATAL_TYPE);
    }
}

qup_status init_hw_ctxt_core_lock (qup_hw_ctxt *h_ctxt)
{
    se_dev_cfg *dcfg = (se_dev_cfg *) h_ctxt->core_config;

    // create an instance lock
    h_ctxt->core_lock = qup_mutex_instance_init (dcfg, CORE_R_LOCK);
    if(!(h_ctxt->core_lock))
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR, "core_lock init failed");
        return QUP_ERROR_NO_MEM;
    }

    return QUP_SUCCESS;
}

static qup_status init_hw_ctxt (qup_hw_ctxt *h_ctxt)
{
    qup_status q_status = QUP_SUCCESS;
    se_dev_cfg *dcfg = (se_dev_cfg *) h_ctxt->core_config;

    // create a lock for the queue
    h_ctxt->queue_lock = qup_mutex_instance_init (dcfg, QUEUE_LOCK);
    if(!(h_ctxt->queue_lock))
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR, "queue_lock init failed");
        return QUP_ERROR_NO_MEM;
    }

    // create a transfer signal
    h_ctxt->transfer_signal = qup_signal_init (dcfg, CORE_TRANSFER_SIGNAL);
    if(!(h_ctxt->transfer_signal))
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR, "transfer_signal init failed");
        return QUP_ERROR_NO_MEM;
    }

    if(QUP_IS_SET(dcfg->flags,ENABLE_TIMEOUT))
    {
        //create a timer handle for bus stuck cases
        if(QUP_IS_SET(dcfg->flags,ENABLE_FIFO_MODE))
        {
            h_ctxt->timer_handle = qup_timer_init(dcfg, qup_fifo_handle_timer_cb, h_ctxt);
        }
        else
        {
            h_ctxt->timer_handle = qup_timer_init(dcfg, qup_gpi_handle_timer_cb, h_ctxt);
        }

        if(!(h_ctxt->timer_handle))
        {
            return QUP_ERROR_NO_MEM; //add new error code
        }
    }

    // initialize bus interface
    if (qup_clock_enable (dcfg) == FALSE)
    {
        return QUP_ERROR_PLATFORM_CLOCK_ENABLE_FAIL;
    }

    if(QUP_IS_SET(dcfg->flags,ENABLE_FIFO_MODE))
    {
        q_status = qup_fifo_iface_init (h_ctxt);
    }
    else
    {
        q_status = qup_gpi_iface_init (h_ctxt);
    }

    if (qup_clock_disable(dcfg) == FALSE)
    {
        return QUP_ERROR_PLATFORM_CLOCK_DISABLE_FAIL;
    }

    // change core state to initialized
    if (QUP_SUCCESS(q_status))
    {
        QUP_FLAG_SET(h_ctxt->core_state,QUP_CORE_INTERFACE_INITIALIZED);
    }

    return q_status;
}

static qup_status de_init_hw_ctxt (qup_hw_ctxt *h_ctxt)
{
    se_dev_cfg    *dcfg = (se_dev_cfg *) h_ctxt->core_config;
    qup_status     q_status = QUP_SUCCESS;

    // de initialize the bus interface
    if (QUP_IS_SET(h_ctxt->core_state,QUP_CORE_INTERFACE_INITIALIZED))
    {
        if(qup_clock_enable (dcfg) == FALSE)
        {
            q_status = QUP_ERROR_PLATFORM_CLOCK_ENABLE_FAIL;
            QUP_SE_LOG(dcfg,LEVEL_ERROR, "ERROR plat_clock_enable failed status %d ", q_status);
            return q_status;
        }

        if(QUP_IS_SET(dcfg->flags,ENABLE_FIFO_MODE))
        {
            q_status = qup_fifo_iface_de_init (h_ctxt);
        }
        else
        {
            q_status = qup_gpi_iface_de_init (h_ctxt);
        }

        if(QUP_ERROR(q_status))
        {
            QUP_LOG(LEVEL_ERROR, "ERROR bus_iface_de_init failed status %d ", q_status);
        }
        else
        {
            QUP_FLAG_UNSET(h_ctxt->core_state,QUP_CORE_INTERFACE_INITIALIZED);
        }

        if(qup_clock_disable(dcfg) == FALSE)
        {
            q_status = QUP_ERROR_PLATFORM_CLOCK_DISABLE_FAIL;
            QUP_SE_LOG(dcfg,LEVEL_ERROR, "ERROR plat_clock_disable failed status %d ", q_status);
            return q_status;
        }

        if (h_ctxt->aux_controller)
        {
            i3c_detach_se(h_ctxt->aux_controller);
        }

        if (h_ctxt->ibs_controller)
        {
            ibi_detach_se(h_ctxt);
        }
    }

    // destroy the lock for the queue
    if (h_ctxt->queue_lock != NULL)
    {
        qup_mutex_instance_de_init (h_ctxt->core_config, h_ctxt->queue_lock, QUEUE_LOCK);
    }

    if (h_ctxt->transfer_signal != NULL)
    {
        qup_signal_de_init (h_ctxt->core_config, h_ctxt->transfer_signal);
    }

    if(dcfg)
    {
        if(QUP_IS_SET(dcfg->flags,ENABLE_TIMEOUT))
        {
            //Release Timer Handle
            if(h_ctxt->timer_handle)
            {
                if(qup_timer_deinit(h_ctxt->core_config,h_ctxt->timer_handle) == FALSE)
                {
                    q_status = QUP_ERROR_PLATFORM_TIMER_DEINIT_FAIL;
                    QUP_SE_LOG(dcfg,LEVEL_ERROR, "ERROR qup_timer_deinit failed status %d ", q_status);
                    return q_status;
                }
            }
        }
    }
    return q_status;
}

qup_status de_init_hw_ctxt_core_lock (qup_hw_ctxt *h_ctxt)
{
    qup_status     q_status = QUP_SUCCESS;

    // destroy the instance lock
    if (h_ctxt->core_lock != NULL)
    {
        qup_mutex_instance_de_init (h_ctxt->core_config, h_ctxt->core_lock, CORE_R_LOCK);
    }

    return q_status;
}

void de_init_client_ctxt (se_dev_cfg *dcfg, qup_client_ctxt *c_ctxt)
{
    qup_client_ctxt *ctxt_curr = c_ctxt;
    qup_client_ctxt *ctxt_prev = NULL;
    while(ctxt_curr)
    {
        ctxt_prev = ctxt_curr;
        ctxt_curr = ctxt_curr->slave_handle_next;
        if (ctxt_prev->t_ctxt.io_ctxt != NULL)
        {
            if(QUP_IS_SET(dcfg->flags,ENABLE_FIFO_MODE))
            {
                qup_fifo_release_io_ctxt (dcfg, ctxt_prev->t_ctxt.io_ctxt);
            }
            else
            {
                qup_gpi_release_io_ctxt (dcfg, (void*)&ctxt_prev->t_ctxt);
            }
        }
        qup_mem_free (ctxt_prev, sizeof(qup_client_ctxt), dcfg, CLIENT_CTXT_TYPE);
    }
}

qup_status init_client_ctxt (qup_hw_ctxt *h_ctxt, qup_client_ctxt **clnt_ctxt)
{
    qup_client_ctxt *c_ctxt = NULL;
    se_dev_cfg *dcfg = h_ctxt->core_config;

    // alloc client context
    c_ctxt = (qup_client_ctxt *) qup_mem_alloc (dcfg,
                                                sizeof(qup_client_ctxt),
                                                CLIENT_CTXT_TYPE);

    if (c_ctxt != NULL)
    {
        c_ctxt->slave_handle_next = NULL;

        if(QUP_IS_SET(dcfg->flags,ENABLE_FIFO_MODE))
        {
            c_ctxt->t_ctxt.io_ctxt = qup_fifo_get_io_ctxt (dcfg);
        }
        else
        {
            c_ctxt->t_ctxt.io_ctxt = qup_gpi_get_io_ctxt (dcfg);
        }
        if (c_ctxt->t_ctxt.io_ctxt == NULL)
        {
            QUP_LOG(LEVEL_ERROR, "qup_open Allocating IO CTXT failed");
            return QUP_ERROR_NO_MEM;
        }
        if (c_ctxt->t_ctxt.io_ctxt != NULL)
        {
            *clnt_ctxt = c_ctxt;
            return QUP_SUCCESS;
        }
    }
    return QUP_ERROR_NO_MEM;
}

qup_status  qup_open(QUP_TYPE qup, uint32 se_id, void** handle, SE_PROTOCOL req_protocol)
{
    qup_status q_status = QUP_SUCCESS;
    qup_client_ctxt *c_ctxt = NULL;
    qup_hw_ctxt *h_ctxt = NULL;
    se_dev_cfg  *dcfg = NULL;

    QUP_LOG(LEVEL_VERBOSE, "open core 0x%08x %d", qup, se_id);
    QUP_LOG(LEVEL_PERF, "open S");

    // param validation
    QUP_VERIFY(handle)

    *handle = NULL;

    if (qup_os_in_exception_context())
    {
        return QUP_ERROR_API_INVALID_EXECUTION_LEVEL;
    }

    qup_mutex_global_lock();

    // get or allocate a hw context
    h_ctxt = alloc_hw_ctxt (qup, se_id);
    if (h_ctxt == NULL)
    {
        q_status = QUP_ERROR_HW_INFO_ALLOCATION;
        goto exit;
    }

    dcfg = (se_dev_cfg*)h_ctxt->core_config;

    if (GET_PROTOCOL_MAJOR(dcfg->protocol_in_use) != req_protocol)
    {
        switch (req_protocol)
        {
            case SE_PROTOCOL_I2C:
                // Check if Protocol is I3C/I3C_IBI before returning error
                if (GET_PROTOCOL_MAJOR(dcfg->protocol_in_use) != SE_PROTOCOL_I3C)
                {
                    QUP_SE_LOG(dcfg,LEVEL_INFO, "open Protocol requested is not matched with protocol_in_use!! handle 0x%08x", c_ctxt);
                    q_status = QUP_ERROR_API_NOT_SUPPORTED;
                    goto exit;
                }
                break;
            case SE_PROTOCOL_SPI:
                // Check if Protocol is QSPI before returning error
                if (GET_PROTOCOL_MAJOR(dcfg->protocol_in_use) != SE_PROTOCOL_SPI_3W)
                {
                    QUP_SE_LOG(dcfg, LEVEL_INFO,
                               "open Protocol requested is not matched with protocol_in_use!! handle 0x%08x",
                                c_ctxt);
                    q_status = QUP_ERROR_API_NOT_SUPPORTED;
                    goto exit;
                }
                break;
            default:
                break;

        }
    }

    if(h_ctxt->core_ref_count == 0)
    {
        if (QUP_HW_CTXT_REF_CNT(h_ctxt) == 0)
        {
            q_status = init_hw_ctxt_core_lock (h_ctxt);
            if (QUP_ERROR(q_status))
            {
                goto exit;
            }
        }
        q_status = init_hw_ctxt (h_ctxt);
        if (QUP_ERROR(q_status))
        {
            goto exit;
        }
    }

    q_status = init_client_ctxt (h_ctxt, &c_ctxt);
    if (QUP_ERROR(q_status) || c_ctxt == NULL)
    {
        QUP_SE_LOG(h_ctxt->core_config,LEVEL_ERROR, "ERROR qup_open init_client_ctxt :  %d", q_status);
        goto exit;
    }

    c_ctxt->h_ctxt = h_ctxt;
    *handle = c_ctxt;
    qup_set_current_pid(c_ctxt,qup_os_get_root_pid());

    QUP_SE_LOG(h_ctxt->core_config,LEVEL_INFO, "open handle 0x%08x count %d", c_ctxt,h_ctxt->core_ref_count);


exit:

    if (QUP_ERROR(q_status))
    {
        QUP_LOG(LEVEL_ERROR, "open core 0x%08x  %d, ERROR %d", qup, se_id, q_status);
        if (h_ctxt != NULL)
        {
            if (c_ctxt != NULL)
            {
                de_init_client_ctxt (h_ctxt->core_config, c_ctxt);
            }
            /* if first qup_open() for given SE is failed */
            if (h_ctxt->core_ref_count == 0)
            {
                if (QUP_SUCCESS != de_init_hw_ctxt (h_ctxt))
                {
                    QUP_LOG(LEVEL_ERROR, "de_init_hw_ctxt failed on ERROR clean up");
                }
                if (QUP_HW_CTXT_REF_CNT(h_ctxt) == 0)
                {
                    if (QUP_SUCCESS != de_init_hw_ctxt_core_lock (h_ctxt))
                    {
                        QUP_LOG(LEVEL_ERROR, "de_init_hw_ctxt_core_lock failed on ERROR clean up");
                    }
                    free_hw_ctxt (h_ctxt);
                }
            }
        }
    }
    else
    {
        /* increment ref count only if qup_open() is successful */
        h_ctxt->core_ref_count++;
    }

    qup_mutex_global_unlock();

    QUP_LOG(LEVEL_PERF, "open P");

    return q_status;
}

qup_status qup_close(void* handle, SE_PROTOCOL req_protocol)
{
    qup_client_ctxt  *c_ctxt = (qup_client_ctxt *) handle;
    qup_hw_ctxt      *h_ctxt;
    qup_status        q_status = QUP_SUCCESS;
    se_dev_cfg       *dcfg;

    QUP_LOG(LEVEL_PERF, "close S");

    if (qup_os_in_exception_context())
    {
        return QUP_ERROR_API_INVALID_EXECUTION_LEVEL;
    }

    if ((c_ctxt == NULL) ||
        (c_ctxt->h_ctxt == NULL) ||
        (c_ctxt->h_ctxt->core_config == NULL))
    {
        QUP_LOG(LEVEL_ERROR, "ERROR close invalid param");
        return QUP_ERROR_INVALID_PARAMETER;
    }

    h_ctxt = c_ctxt->h_ctxt;
    dcfg   = (se_dev_cfg *) h_ctxt->core_config;

    if(GET_PROTOCOL_MAJOR(dcfg->protocol_in_use) != req_protocol)
	{
		switch(req_protocol)
		{
			case SE_PROTOCOL_I2C:
     			 // Check if Protocol is I3C/I3C_IBI before returning error
                 if (GET_PROTOCOL_MAJOR(dcfg->protocol_in_use) != SE_PROTOCOL_I3C)
                 {
                     QUP_SE_LOG(dcfg,
					            LEVEL_INFO,
								"close Protocol requested is not matched with protocol_in_use!! handle 0x%08x",
								c_ctxt);
                     return QUP_ERROR_API_NOT_SUPPORTED;
                 }
			    break;
			case SE_PROTOCOL_SPI:
                 // Check if Protocol is QSPI before returning error
                 if (GET_PROTOCOL_MAJOR(dcfg->protocol_in_use) != SE_PROTOCOL_SPI_3W)
                 {
                     QUP_SE_LOG(dcfg,
             					LEVEL_INFO,
                                "close Protocol requested is not matched with protocol_in_use!! handle 0x%08x",
                                 c_ctxt);
                     return QUP_ERROR_API_NOT_SUPPORTED;
                 }			    
			    break;
			default:
			    break;			
		}
	}

    QUP_SE_LOG(dcfg,LEVEL_INFO, "close core 0x%08x %d handle 0x%08x", dcfg->qup_data->qup_id, dcfg->se_id ,c_ctxt);

    qup_mutex_global_lock();

    if (h_ctxt->core_ref_count)
    {
        h_ctxt->core_ref_count--;
        if (h_ctxt->core_ref_count == 0)
        {
            q_status = de_init_hw_ctxt (h_ctxt);
            if(QUP_ERROR(q_status))
            {
                h_ctxt->core_ref_count++;
                qup_mutex_global_unlock();

                QUP_LOG(LEVEL_ERROR, "ERROR de_init_iface_hw_ctxt failed status %d ", q_status);
                return q_status;
            }
            if (QUP_HW_CTXT_REF_CNT(h_ctxt) == 0)
            {
                q_status = de_init_hw_ctxt_core_lock (h_ctxt);
                if(QUP_ERROR(q_status))
                {
                    h_ctxt->core_ref_count++;
                    qup_mutex_global_unlock();

                    QUP_LOG(LEVEL_ERROR, "ERROR de_init_hw_ctxt failed status %d ", q_status);
                    return q_status;
                }
                free_hw_ctxt (h_ctxt);
            }
        }
    }

    if (c_ctxt != NULL)
    {
        de_init_client_ctxt (dcfg,c_ctxt);
    }

    QUP_SE_LOG(dcfg,LEVEL_INFO, "close handle 0x%08x", c_ctxt);

    qup_mutex_global_unlock();

    QUP_LOG(LEVEL_PERF, "close P");


    return q_status;
}


void  qup_error_dump_reg(void)
{
    qup_hw_ctxt *h_ctxt = qup_cores_head;
    se_dev_cfg  *dcfg;

    if(h_ctxt != NULL)
    {
        if(!qup_is_power_domain_on(h_ctxt->core_config))
        {
            QUP_LOG(LEVEL_ERROR, "qup_error_dump_reg : Qup Power Domain disabled");
            return;
        }

        do
        {
            dcfg = (se_dev_cfg *) h_ctxt->core_config;

#ifdef QUP_HANDLE_GPI_PDR
            qup_gpi_core_ctxt *g_ctxt = h_ctxt->core_iface;
            if(h_ctxt->power_ref_count == 0)
            {
                qup_clock_enable(dcfg);
            }
            else
            {
                gpi_iface_active(g_ctxt->gpi_handle, FALSE);
            }
            gpi_iface_set_irq_mode(g_ctxt->gpi_handle, FALSE);
            QUP_FLAG_SET(dcfg->flags,POLLED_MODE);
            qup_gpi_iface_de_init(h_ctxt);
            qup_clock_disable(dcfg);
#endif

            qup_gpio_dump_stat(dcfg);
            h_ctxt = h_ctxt->next;

        }while(h_ctxt != NULL);
    }

}

