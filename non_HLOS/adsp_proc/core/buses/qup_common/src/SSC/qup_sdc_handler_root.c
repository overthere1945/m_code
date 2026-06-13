/**
    @file  qup_sdc_handler_root.c
    @brief QUP SDC Handler Root implementation
 */

/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
07/12/24   GKR     Changes to align register access to auto generated HWIO & avoid manual editing
06/26/24   GKR     Enabled support for multple SSC QUPs Via DT
*/
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/


/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/
#include "qup_sdc_handler.h"
#include "qup_common_internal.h"
#include "qup_hal.h"
#include "qup_devcfg.h"
#include "qup_log.h"
#include "qup_os.h"
#include "qup_plat.h"
#include "gpi.h"
#include "Clock.h"
#include "ClockImage.h"
#include "HALhwio.h"
#include "DTBExtnLib.h"
/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/
#define MAX_IBI_CONTROLLER_INSTANCES   3
#define MAX_IBI_CONTROLLER_SLAVES      8
#define POLL_TIME_DEFAULT_US           400
#define POLL_INTERVAL_US               10

/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/

int qdi_qup_sdc_handler_invoke(int,qurt_qdi_obj_t *,int ,qurt_qdi_arg_t,qurt_qdi_arg_t,qurt_qdi_arg_t,qurt_qdi_arg_t, qurt_qdi_arg_t, qurt_qdi_arg_t,qurt_qdi_arg_t, qurt_qdi_arg_t, qurt_qdi_arg_t);

/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/

static const qurt_qdi_obj_t  opener_object = {qdi_qup_sdc_handler_invoke,QDI_REFCNT_PERM,0};
qup_sdc_cfg_t   ssc_sdc_cfg[NUM_SSC_QUP];
qup_sdc_object  root_ctxt = {0};
/*==================================================================================================
                                          GLOBAL VARIABLES
==================================================================================================*/
extern ClockHandle    qup_clock_handle;
extern qup_dev_cfg*   qup_cfg_list[];
extern ibi_dev_cfg*   ssc_sdc_ibi_dcfg_list[];
/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/
static void qdi_qup_sdc_handler_release(qurt_qdi_obj_t *obj)
{
#ifdef QUP_SDC_PROC_SUPPORT
    uint8  *base = NULL;
    ibi_dev_cfg *ibi_cfg = NULL;
    uint8   se_id,i,j,n,k;
    uint32  config_reg_val = 0;
    uint32 timeout_us = 0, size = 0;
    uint32 irq_status = 0;
    uint8 valid, cmd_done = 0;
    GPI_RETURN_STATUS gpi_status;
    ClockResult res = CLOCK_SUCCESS;
    qup_dev_cfg* qcfg = NULL;
    ClockIdType se_clk_id = 0;

    fdt_node_handle node;
    char     dt_node_name[50];
    int      ret_value = 0;

    QUP_LOG(LEVEL_INFO, "qdi_qup_sdc_handler_release  release obj 0x%x",obj);

    if(!qup_enable_power_domain(TRUE))
    {
        QUP_LOG(LEVEL_INFO, "qdi_qup_sdc_handler_release  release : GDSC ON failed");
        return;
    }

    for ( i = 0; i < NUM_SSC_QUP; i++)
    {
        qcfg = qup_cfg_list[qup_idx_offset[SSC_QUP]+i];
        if(qcfg == NULL) continue;

        if (ssc_sdc_cfg[i].num_gpii == 0)
        {

            node = qcfg->qup_node;

            ret_value = fdt_get_prop_size(&node, "sdc_gpii_list", &size);
            if (ret_value || size == 0) continue;

            ssc_sdc_cfg[i].num_gpii = (uint8) size;

            ret_value = fdt_get_uint8_prop_list(&node, "sdc_gpii_list", &ssc_sdc_cfg[i].sdc_gpii_list[0], size);
            if (ret_value) continue;
        }

        for (j = 0; j < QUP_COMMON_CLOCKS_MAX ; j++)
        {
            res = Clock_Enable(qup_clock_handle, qcfg->common_clk_id[j]);
            if(CLOCK_SUCCESS != res)
            {
               QUP_LOG(LEVEL_INFO, "ERROR qdi_qup_sdc_handler_release Common Clock Enable failed");
               root_ctxt.in_use = FALSE;
               goto exit;
            }
        }

        for (j = 0; j < ssc_sdc_cfg[i].num_gpii; j++)
        {
            if(GPI_STATUS_SUCCESS != gpi_query_remote_gpii(ssc_sdc_cfg[i].sdc_gpii_list[j],SET_QUP_TYPE(SSC_QUP,i),&se_id))
            {
                continue;
            }

            snprintf(dt_node_name, 50, "/soc/SSC_QUP_%hhu/SSC_QUP_%hu_SE_%hu", i,i,se_id);
            ret_value = fdt_get_node_handle(&node, NULL, dt_node_name);
            if (ret_value)
            {
                QUP_LOG(LEVEL_ERROR, "qup_sdc_handler_release Node not found %s",dt_node_name);
                continue;
            }

            ret_value = Clock_GetIdDT(qup_clock_handle, &node, "se-clk", &se_clk_id);

            if(CLOCK_SUCCESS != Clock_Enable(qup_clock_handle, se_clk_id))
            {
                QUP_LOG(LEVEL_ERROR, "ERROR qdi_qup_sdc_handler_release Se Clock Enable failed %d",se_id);
                continue;
            }

            gpi_status = gpi_iface_disable(ssc_sdc_cfg[i].sdc_gpii_list[j],SET_QUP_TYPE(SSC_QUP, i));

            if (gpi_status != GPI_STATUS_SUCCESS)
            {
                QUP_LOG(LEVEL_ERROR, "qdi_qup_sdc_handler_release [%d] gpi_iface_disable : %d",ssc_sdc_cfg[i].sdc_gpii_list[j],gpi_status);
            }
            else
            {
                QUP_LOG(LEVEL_INFO, "qdi_qup_sdc_handler_release clean up gpii : [%d] gpi_iface_disable : %d",ssc_sdc_cfg[i].sdc_gpii_list[j],gpi_status);
            }
            if(CLOCK_SUCCESS != Clock_Disable(qup_clock_handle, se_clk_id))
            {
                QUP_LOG(LEVEL_ERROR, "ERROR qdi_qup_sdc_handler_release Se Clock Disable failed %d",se_id);
            }
        }

        for (j = QUP_COMMON_CLOCKS_MAX; j > 0 ; j--)
        {
            res = Clock_Disable(qup_clock_handle, qcfg->common_clk_id[j-1]);
            if(CLOCK_SUCCESS != res)
            {
                QUP_LOG(LEVEL_ERROR, "qdi_qup_sdc_handler_release: Common Clock Disable failed for idx :%d ", j-1);
            }
        }
    }


    for(n = 0; ssc_sdc_ibi_dcfg_list[n] != NULL; n++)
    {
        ibi_cfg = ssc_sdc_ibi_dcfg_list[n];
        if(ibi_cfg && ibi_cfg->core_base_addr)
        {
            base = ibi_cfg->core_base_addr;
            for(k = 0; k < MAX_IBI_CONTROLLER_SLAVES; k++)
            {
                //read SDC IBI table entries
                valid = HWIO_INXFI2(base, I3C_IBI_CONFIGn_ENTRYk,SDC_GPII, k, VALID);
                if(valid)
                {
                    //if entry is valid, mark it invalid
                    config_reg_val = HWIO_INXI2(base, I3C_IBI_CONFIGn_ENTRYk,SDC_GPII, k);

                    //Mask all HW interrupt and poll on status bit

                    timeout_us = POLL_TIME_DEFAULT_US;
                    HWIO_OUTXI(base,I3C_IBI_IRQ_ENn,SDC_GPII,0);

                    irq_status = HWIO_INXI(ibi_cfg->core_base_addr,I3C_IBI_IRQ_STATUSn,SDC_GPII);

                    //If IBI is sent by slave, clear it and send cmd
                    if(irq_status & IBI_RECEIVED)
                    {
                        HWIO_OUTXI(base, I3C_IBI_RECEIVED_IBI_CLRn, SDC_GPII, HWIO_RMSK(I3C_IBI_RECEIVED_IBI_CLRn));
                        HWIO_OUTXI(base, I3C_IBI_IRQ_CLRn, SDC_GPII, IBI_RECEIVED_CLR);
                    }

                    //give command to mark entry invalid in IBI table
                    HWIO_OUTXI(base, I3C_IBI_CMDn, SDC_GPII, config_reg_val) ;

                    while ((timeout_us != 0) && !cmd_done)
                    {
                        //if cmd_done is also received then exit else send cmd again
                        if(irq_status & COMMAND_DONE)
                        {
                            cmd_done = (irq_status & COMMAND_DONE);
                        }

                        qup_delay_us (POLL_INTERVAL_US);
                        timeout_us -= POLL_INTERVAL_US;
                    }
                    if(cmd_done)
                    {
                        HWIO_OUTXI(base,I3C_IBI_IRQ_CLRn,SDC_GPII,0xffffff);
                    }
                    else
                    {
                        QUP_LOG(LEVEL_INFO, "sdc_handler_release cmd_done not completed");
                    }
                }
            }
        }
    }


    root_ctxt.in_use = FALSE;

exit:
    if(!qup_enable_power_domain(FALSE))
    {
        QUP_LOG(LEVEL_INFO, "qdi_qup_sdc_handler_release  release : GDSC OFF failed");
        return;
    }
#endif // #ifdef QUP_SDC_PROC_SUPPORT
}

static int qup_sdc_handler_qdi_open(int client_handle,int pid)
{
    qurt_process_attr_t  p_attr;
    int                  ret;

    ret = qurt_process_attr_get(pid,&p_attr);
    if(QURT_EOK != ret)
    {
        QUP_LOG(LEVEL_INFO, "ERROR qup_sdc_handler_qdi_open qurt_process_attr_get failed %d",ret);
        return ret;
    }

    if(strncmp("qsh_process", p_attr.name, 11) || root_ctxt.in_use)
    {
        QUP_LOG(LEVEL_INFO, "ERROR qup_sdc_handler_qdi_open called from non-sensor user / driver already in use");
        return QURT_EFAILED;
    }

    root_ctxt.obj.invoke  = qdi_qup_sdc_handler_invoke;
    root_ctxt.obj.refcnt  = QDI_REFCNT_INIT;
    root_ctxt.obj.release = qdi_qup_sdc_handler_release;

    ret = qurt_qdi_handle_create_from_obj_t(client_handle, &root_ctxt.obj);
    if (ret < 0)
    {
        return ret;
    }

    root_ctxt.in_use = TRUE;

    return ret;

}

int qdi_qup_sdc_handler_invoke(int client_handle, qurt_qdi_obj_t *obj, int method,
                                qurt_qdi_arg_t a1, qurt_qdi_arg_t a2, qurt_qdi_arg_t a3,
                                qurt_qdi_arg_t a4, qurt_qdi_arg_t a5, qurt_qdi_arg_t a6,
                                qurt_qdi_arg_t a7, qurt_qdi_arg_t a8, qurt_qdi_arg_t a9)

{
    int ret = QURT_EOK;
    switch(method)
    {
        case QDI_OPEN:
            ret = qup_sdc_handler_qdi_open (client_handle,a2.num);
            break;
        default:
            ret = qurt_qdi_method_default(
                    client_handle,
                    obj, method,
                    a1, a2, a3, a4,
                    a5, a6, a7, a8,
                    a9);
            break;
    }
    return ret;
}

void qup_sdc_handler_init_root(void)
{
     qurt_qdi_register_devname(QUP_SDC_DRIVER_NAME, &opener_object);
}
