/**
    @file  qup_plat.c
    @brief platform implementation
 */

/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
09/09/24   MG      Added SE_GENI_FORCE_DEFAULT_REG for I3C TOP QUP 
06/26/24   GKR     Generalized QUP GPIO Handles
*/
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/


/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/
#include "qup_plat.h"
#include "qup_os.h"
#include "qup_alloc.h"
#include "Clock.h"
#include "GPIO.h"
#include "ClockDefs.h"
#include "DALDeviceId.h"
#include "timer.h"
#include "icbarb.h"
#include "npa.h"
#include "DTBExtnLib.h"
#include "GPIOImage.h"

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/

/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/
typedef struct _qup_npa_struct_
{
    npa_client_handle  qup_core_clk_handle;
    npa_client_handle  qup_ddr_handle;
    npa_client_handle  lpass_slave_handle;
}qup_npa_struct;

#define DEFAULT_BUS_WIDTH    4
#define DEFAULT_SE_CLK       19200000
#define QUP_BW_VOTE_AB      (DEFAULT_SE_CLK * DEFAULT_BUS_WIDTH)

#define LPASS_QUP_SLAVE_BW_AB      26023
#define LPASS_QUP_SLAVE_BW_IB      26023


ICBArb_RequestType qup_request_ddr [] =
{
    { ICBARB_REQUEST_TYPE_3, { 0, QUP_BW_VOTE_AB, 0} },
};

ICBArb_RequestType qup_request_clk [] =
{
    { ICBARB_REQUEST_TYPE_3, {0, 0, 0} },
};

ICBArb_RequestType lpass_request_slave [] =
{
    { ICBARB_REQUEST_TYPE_3, {LPASS_QUP_SLAVE_BW_IB, LPASS_QUP_SLAVE_BW_AB, 0} },
};

ICBArb_MasterSlaveType qup_master_slave_list [TOP_QUP_MAX] =
{
    {ICBID_MASTER_QUP_CORE_0, ICBID_SLAVE_QUP_CORE_0}, //requesting bandwidth for the core/core2x clocks
    {ICBID_MASTER_QUP_CORE_1, ICBID_SLAVE_QUP_CORE_1},
    {ICBID_MASTER_QUP_CORE_2, ICBID_SLAVE_QUP_CORE_2},
    {ICBID_MASTER_QUP_CORE_3, ICBID_SLAVE_QUP_CORE_3},
    {ICBID_MASTER_QUP_CORE_4, ICBID_SLAVE_QUP_CORE_4},
};

ICBArb_MasterSlaveType ddr_master_slave_list [TOP_QUP_MAX] =
{
    {ICBID_MASTER_QUP_0, ICBID_SLAVE_EBI1 }, //requesting bandwidth for the path from QUP to DDR
    {ICBID_MASTER_QUP_1, ICBID_SLAVE_EBI1 },
    {ICBID_MASTER_QUP_2, ICBID_SLAVE_EBI1 },
    {ICBID_MASTER_QUP_3, ICBID_SLAVE_EBI1 },
    {ICBID_MASTER_QUP_4, ICBID_SLAVE_EBI1 },
};

ICBArb_MasterSlaveType lpass_to_slave_list [TOP_QUP_MAX] =
{
    {ICBID_MASTER_LPASS_PROC, ICBID_SLAVE_QUP_0 }, //requesting bandwidth for the path from LPASS to QUP_SLAVE
    {ICBID_MASTER_LPASS_PROC, ICBID_SLAVE_QUP_1 },
    {ICBID_MASTER_LPASS_PROC, ICBID_SLAVE_QUP_2 },
    {ICBID_MASTER_LPASS_PROC, ICBID_SLAVE_QUP_3 },
    {ICBID_MASTER_LPASS_PROC, ICBID_SLAVE_QUP_4 },
};


static boolean    top_qup_clock_disable (se_dev_cfg *config);
static boolean    top_qup_clock_enable (se_dev_cfg *config);
static boolean    top_qup_gpio_enable_io (se_dev_cfg *config, uint8 io_idx);
static boolean    top_qup_gpio_enable (se_dev_cfg *config);
static boolean    top_qup_gpio_disable (se_dev_cfg *config);
static void       top_qup_gpio_dump_stat (se_dev_cfg *config);
static boolean    top_qup_timer_set(se_dev_cfg *dcfg, void *handle, uint32 time_out_msec);
static boolean    top_qup_timer_clear(se_dev_cfg *dcfg, void *handle);
static void*      top_qup_timer_init(se_dev_cfg *dcfg,void (*timer_cb) (void *), void *data);
static boolean    top_qup_timer_deinit(se_dev_cfg *dcfg, void *handle);


/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/
const QupPlatFcnTable top_qup_functions = 
{
    top_qup_clock_enable    ,
    top_qup_clock_disable   ,
    top_qup_gpio_enable_io  ,
    top_qup_gpio_enable     ,
    top_qup_gpio_disable    ,
    top_qup_gpio_dump_stat  ,
    top_qup_timer_init      ,
    top_qup_timer_set       ,
    top_qup_timer_clear     ,
    top_qup_timer_deinit    ,
};

/*==================================================================================================
                                          EXTERN VARIABLES
==================================================================================================*/

extern GPIOClientHandleType         qup_gpio_handle[];
extern ClockHandle                  qup_clock_handle;

/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/

static qup_npa_struct *qup_get_npa_handle(se_dev_cfg *cfg)
{
    static qup_npa_struct hNpa[TOP_QUP_MAX][MAX_SE_PER_QUP] = {{0}};
    uint32  qup_idx = (GET_QUP_MAJOR(cfg->qup_data->qup_id) == TOP_QUP)? GET_QUP_MINOR(cfg->qup_data->qup_id) : 0xFF;
    uint32  se_id = cfg->se_id;
    char   qup_handle_name[25] = {0};
    
    //For KW
    if ((qup_idx < TOP_QUP_MAX) && (se_id < MAX_SE_PER_QUP))
    {
        if ((!hNpa[qup_idx][se_id].qup_core_clk_handle) ||
            (!hNpa[qup_idx][se_id].qup_ddr_handle)||
            (!hNpa[qup_idx][se_id].lpass_slave_handle))
        {
            snprintf(qup_handle_name, 20, "TOP_QUP_CLK_%lu_%lu", qup_idx, se_id);

            // ICBArb_CreateClientVectorType vector;
    
            hNpa[qup_idx][se_id].qup_core_clk_handle = npa_create_sync_client_ex("/icb/arbiter",
                                                                                                    qup_handle_name,
                                                                                                    NPA_CLIENT_VECTOR,
                                                                                                    sizeof(ICBArb_MasterSlaveType),
                                                                                                    (void *)&qup_master_slave_list[qup_idx]);

            snprintf(qup_handle_name, 20, "TOP_QUP_DDR_%lu_%lu", qup_idx, se_id);

            hNpa[qup_idx][se_id].qup_ddr_handle      = npa_create_sync_client_ex("/icb/arbiter",
                                                                                                    qup_handle_name,
                                                                                                    NPA_CLIENT_VECTOR,
                                                                                                    sizeof(ICBArb_MasterSlaveType), 
                                                                                                    (void *)&ddr_master_slave_list[qup_idx]);

            snprintf(qup_handle_name, 25, "TOP_QUP_LPASS_SLAVE_%lu_%lu", qup_idx, se_id);

            hNpa[qup_idx][se_id].lpass_slave_handle      = npa_create_sync_client_ex("/icb/arbiter",
                                                                                                    qup_handle_name,
                                                                                                    NPA_CLIENT_VECTOR,
                                                                                                    sizeof(ICBArb_MasterSlaveType), 
                                                                                                    (void *)&lpass_to_slave_list[qup_idx]);

        if ((!hNpa[qup_idx][se_id].qup_core_clk_handle) ||
            (!hNpa[qup_idx][se_id].qup_ddr_handle) ||
            (!hNpa[qup_idx][se_id].lpass_slave_handle))
            {
                QUP_SE_LOG(cfg,LEVEL_ERROR, "ERROR: NPA handles not found for core %d %d", qup_idx, se_id);
                hNpa[qup_idx][se_id].qup_core_clk_handle = 0;
                hNpa[qup_idx][se_id].qup_ddr_handle = 0;
                hNpa[qup_idx][se_id].lpass_slave_handle = 0;
                return NULL;
            }
            
            return &(hNpa[qup_idx][se_id]);
        }
        else
        {
            return &(hNpa[qup_idx][se_id]);
        }
    }
    return NULL;
}



static boolean top_qup_clock_disable (se_dev_cfg *config)
{
    ClockResult      res = CLOCK_SUCCESS;
    qup_npa_struct   *pNpa = NULL;
    
    QUP_SE_LOG(config,LEVEL_VERBOSE, "clock_OFF ALL");
    
    if(QUP_IS_SET(config->resources.state,QUP_CLOCK_ON_FAILURE))
    {
        QUP_FLAG_UNSET(config->resources.state,QUP_CLOCK_ON_FAILURE);
    }
    else
    {
        config->resources.state = QUP_ALL_CLOCKS_ON;
    }
    
    if(config->resources.state == QUP_SE_CLOCK_ON)
    {
        res = Clock_Disable(qup_clock_handle, config->se_clk_id);
        if(CLOCK_SUCCESS != res)
        {
            QUP_SE_LOG(config,LEVEL_ERROR, "clock_OFF failed for QUP_SE_CLOCK, id : %d, error: 0x%x",config->se_clk_id, res);
            goto exit_error;
        }

        config->resources.state--;
    }

        res = Clock_Disable(qup_clock_handle, config->qup_data->common_clk_id[QUP_M_AHB_CLOCK_IDX]);
        if(CLOCK_SUCCESS != res)
        {
            QUP_SE_LOG(config,LEVEL_ERROR, "clock_OFF failed for QUP_M_AHB_CLOCK, id : %d, error: 0x%x",config->qup_data->common_clk_id[QUP_M_AHB_CLOCK_IDX],res);
            goto exit_error;
        }
        res = Clock_Disable(qup_clock_handle, config->qup_data->common_clk_id[QUP_S_AHB_CLOCK_IDX]);
        if(CLOCK_SUCCESS != res)
        {
            QUP_SE_LOG(config,LEVEL_ERROR, "clock_OFF failed for QUP_S_AHB_CLOCK, id : %d, error: 0x%x",config->qup_data->common_clk_id[QUP_S_AHB_CLOCK_IDX],res);
            goto exit_error;
        }
        res = Clock_Disable(qup_clock_handle, config->qup_data->common_clk_id[QUP_CORE_CLOCK_IDX]);
        if(CLOCK_SUCCESS != res)
        {
            QUP_SE_LOG(config,LEVEL_ERROR, "clock_OFF failed for QUP_CORE_CLOCK, id : %d, error: 0x%x",config->qup_data->common_clk_id[QUP_CORE_CLOCK_IDX],res);
            goto exit_error;
        }
        res = Clock_Disable(qup_clock_handle, config->qup_data->common_clk_id[QUP_CORE_2X_CLOCK_IDX]);
        if(CLOCK_SUCCESS != res)
        {
            QUP_SE_LOG(config,LEVEL_ERROR, "clock_OFF failed for QUP_CORE2X_CLOCK, id : %d, error: 0x%x",config->qup_data->common_clk_id[QUP_CORE_2X_CLOCK_IDX],res);
            goto exit_error;
        }

        config->resources.state = QUP_NO_CLOCK_ON;

    pNpa = qup_get_npa_handle(config);
    if(pNpa == NULL)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "qup_get_npa_handle is NULL");
        goto exit_error;
    }
    
    npa_complete_request(pNpa->qup_core_clk_handle);
    npa_complete_request(pNpa->qup_ddr_handle);
    npa_complete_request(pNpa->lpass_slave_handle);

    return TRUE;
    
exit_error:
    
    //ERR FATAL::Probable SW race condition
    QUP_ASSERT(config,QUP_FATAL_TYPE);
    return FALSE;
}

static boolean top_qup_clock_enable (se_dev_cfg *config)
{
    ClockResult      res = CLOCK_SUCCESS;
    qup_npa_struct   *pNpa = NULL;

    uint32 core_2x_vote_ib = 0;
    uint32 core_2x_vote_ab = 0;

    core_2x_vote_ib = config->qup_data->core_freq_hz/1000;     //Converting Hz to Khz.
    core_2x_vote_ab = core_2x_vote_ib/100;

    if (qup_clock_handle == NULL)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "ERROR: top_qup_clock_enable handle NULL");
        return FALSE;
    }
    
    pNpa = qup_get_npa_handle(config);
    if(pNpa == NULL)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "top qup_get_npa_handle is NULL");
        goto exit_error;
    }

    qup_request_clk[0].arbData.type3.uIb = core_2x_vote_ib;
    qup_request_clk[0].arbData.type3.uAb = core_2x_vote_ab;

    npa_issue_vector_request(pNpa->qup_core_clk_handle, sizeof(qup_request_clk)/(sizeof(npa_resource_state)), (npa_resource_state *) qup_request_clk);
    npa_issue_vector_request(pNpa->qup_ddr_handle, sizeof(qup_request_ddr)/(sizeof(npa_resource_state)), (npa_resource_state *) qup_request_ddr);
    npa_issue_vector_request(pNpa->lpass_slave_handle, sizeof(lpass_request_slave)/(sizeof(npa_resource_state)), (npa_resource_state *) lpass_request_slave);

    QUP_SE_LOG(config,LEVEL_VERBOSE, "top qup clock_ON ALL");

    //clocks = config->qup_data->common_clocks;
    config->resources.state = QUP_NO_CLOCK_ON;
    
    res = Clock_Enable(qup_clock_handle, config->qup_data->common_clk_id[QUP_CORE_2X_CLOCK_IDX]);
    if(CLOCK_SUCCESS != res)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "top clock_ON failed for QUP_CORE_2X_CLOCK, id : %d, error: 0x%x",config->qup_data->common_clk_id[QUP_CORE_2X_CLOCK_IDX],res);
        goto exit_error;
    }
    config->resources.state++;// = QUP_CORE_2X_CLOCK_ON;
    
    res = Clock_Enable(qup_clock_handle, config->qup_data->common_clk_id[QUP_CORE_CLOCK_IDX]);
    if(CLOCK_SUCCESS != res)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "clock_ON failed for QUP_CORE_CLOCK, id  : %d, error: 0x%x",config->qup_data->common_clk_id[QUP_CORE_CLOCK_IDX],res);
        goto exit_error;
    }
    config->resources.state++;// = QUP_CORE_CLOCK_ON;	

    res = Clock_Enable(qup_clock_handle, config->qup_data->common_clk_id[QUP_S_AHB_CLOCK_IDX]);
    if(CLOCK_SUCCESS != res)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "top clock_ON failed for QUP_S_AHB_CLOCK, id : %d,error: 0x%x",config->qup_data->common_clk_id[QUP_S_AHB_CLOCK_IDX],res);
        goto exit_error;
    }
    config->resources.state++;// = QUP_S_AHB_CLOCK_ON;	
    
    res = Clock_Enable(qup_clock_handle, config->qup_data->common_clk_id[QUP_M_AHB_CLOCK_IDX]);
    if(CLOCK_SUCCESS != res)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "top clock_ON failed for QUP_M_AHB_CLOCK, id : %d, error: 0x%x",config->qup_data->common_clk_id[QUP_M_AHB_CLOCK_IDX], res);
        goto exit_error;
    }
    config->resources.state++;// = QUP_M_AHB_CLOCK_ON;

    res = Clock_Enable(qup_clock_handle, config->se_clk_id);
    if(CLOCK_SUCCESS != res)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "top clock_ON failed for QUP_SE_CLOCK, id : %d, error: 0x%x",config->se_clk_id,res);
        goto exit_error;
    }
    
    config->resources.state++;
    
    return TRUE;
    
exit_error:
    
    //To figure out which clock on failed in future : sticky variable
    config->resources.failure_state = config->resources.state;
    QUP_FLAG_SET(config->resources.state,QUP_CLOCK_ON_FAILURE);
    
    //To switch of the clocks already turned on
    qup_clock_disable(config);
    
    return FALSE;
}

static boolean   top_qup_gpio_enable_io (se_dev_cfg *config, uint8 io_idx)
{
    GPIOResult  res;
    GPIOConfigType gpio_config = {0};

    res = GPIO_ConfigPin(qup_gpio_handle[TOP_QUP], config->se_gpio_enable_key, gpio_config);
    if (GPIO_SUCCESS != res)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "top_qup_gpio_enable_io : GPIO_ConfigPin failed res: %d", res);
        return FALSE; 
    }
    
    return TRUE;
   
}

static boolean top_qup_gpio_enable (se_dev_cfg *config)
{
    uint8 *se_base = NULL;
    if((GET_PROTOCOL_MAJOR(config->protocol_in_use) == SE_PROTOCOL_I3C) && config->force_default_reg_write)
    {
        se_base = config->se_base_addr;
        HWIO_OUTX(GENI_CFG_BASE(se_base), GENI_FORCE_DEFAULT_REG, 0x1);   //writing 1 to SE_GENI_FORCE_DEFAULT_REG
        config->force_default_reg_write =  FALSE;
        qurt_mem_barrier();
    }
    if(!qup_gpio_enable_io(config,0))
    {
        return FALSE;
    }
    /*if(GET_PROTOCOL_MAJOR(config->protocol_in_use) == SE_PROTOCOL_I3C)
    {
		// TODO: Add SLEW RATE CTRL for TOP QUP
        //QUP_SSC_I3C_SLEW_RATE_CTRL(config->qup_data->se_wrapper_base_addr,config->se_id,0x3F);
    }*/

   return TRUE;
}

static boolean top_qup_gpio_disable (se_dev_cfg *config)
{

   GPIOResult  res = 0;

    res = GPIO_ConfigPinInactive(qup_gpio_handle[TOP_QUP], config->se_gpio_disable_key);
    if (GPIO_SUCCESS != res)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "top_qup_gpio_disable : GPIO_ConfigPin failed res: %d", res);
        return FALSE; 
    }

   return TRUE;
}



static boolean top_qup_timer_set(se_dev_cfg *dcfg, void *handle, uint32 time_out_msec)
{
    timer_type *timer_handle= (timer_type *)handle;
    timer_error_type t_error;

    t_error = timer_set_64(timer_handle, time_out_msec, 0, T_MSEC);
    if(TE_SUCCESS == t_error)
    {
        return TRUE;
    }
    else
    {
        QUP_LOG(LEVEL_ERROR, "top qup_timer_set : timer set fail 0x%x 0x%x", handle,t_error);
        return FALSE;
    }
}

static boolean top_qup_timer_clear(se_dev_cfg *dcfg, void *handle)
{
    timer_type *timer_handle= (timer_type *)handle;
    timer_error_type t_error;

    t_error = timer_stop(timer_handle, T_MSEC, NULL);
    if(TE_SUCCESS == t_error)
    {
        return TRUE;
    }
    else
    {
        QUP_LOG(LEVEL_ERROR, "top qup_timer_clear : timer clr fail 0x%x %d", handle,t_error);
        return FALSE;
    }
}

static void *top_qup_timer_init(se_dev_cfg *dcfg,void (*timer_cb) (void *), void *data)
{
    timer_error_type         t_error;
    timer_ptr_type           timer_handle = NULL;
    
    timer_handle =  (timer_type *)qup_mem_alloc (dcfg, sizeof(timer_type), OS_TIMER_TYPE);

    if(timer_handle == NULL)
    {
        return NULL;
    }
    
    t_error = timer_def_osal(timer_handle, NULL, TIMER_FUNC1_CB_TYPE, timer_cb, (unsigned long)data);
    
    if(TE_SUCCESS == t_error)
    {
        return (void *) timer_handle;
    }
    else
    {
        qup_mem_free(timer_handle, sizeof(timer_type),dcfg ,OS_TIMER_TYPE);
        return NULL;
    }
}

static boolean  top_qup_timer_deinit(se_dev_cfg *dcfg, void *handle)
{
    timer_type *timer_handle= (timer_type *)handle;
    timer_error_type t_error;
    
    t_error = timer_undef(timer_handle);
    if(TE_SUCCESS == t_error)
    {
        qup_mem_free(timer_handle, sizeof(timer_type),dcfg ,OS_TIMER_TYPE);
        return TRUE;
    }
    else
    {
        QUP_LOG(LEVEL_ERROR, "top qup_timer_deinit : timer undef fail 0x%x 0x%x", timer_handle,t_error);
        return FALSE;
    }
}

static void   top_qup_gpio_dump_stat (se_dev_cfg *config)
{
    int i;
    GPIOResult   res;
    GPIOConfigType      curr_cfg;
    GPIOValueType       curr_val;
    GPIOKeyType         gpio_key;
    
    for(i = (MAX_IO_PADS - 1); i >= 0; i--)
    {
        if(config->io[i].valid)
        {
            res = GPIO_RegisterPinExplicit(qup_gpio_handle[TOP_QUP], config->io[i].pin_no, GPIO_ACCESS_SHARED, &gpio_key);
            if (GPIO_SUCCESS == res) 
            {
                res = GPIO_GetPinConfig(qup_gpio_handle[TOP_QUP], gpio_key, &curr_cfg);
                if (GPIO_SUCCESS == res) 
                {
                    QUP_SE_LOG(config,LEVEL_ERROR, "top_qup_gpio_dump_stat : GPIO_GetPinConfig  GPIO:%d CFG:0x%x ", config->io[i].pin_no, curr_cfg);
                    QUP_SE_LOG(config,LEVEL_ERROR, "top_qup_gpio_dump_stat : func: %d, dir : %d, pull : %d, drive : %d, strongpull : %d",
                               curr_cfg.func, curr_cfg.dir, curr_cfg.pull, curr_cfg.drive, curr_cfg.strongpull); 
                }
                else
                {
                    QUP_SE_LOG(config,LEVEL_ERROR, "top_qup_gpio_dump_stat : GPIO_GetPinConfig failed with res : %d", res);
                    return;
                }

                res = GPIO_ReadPin(qup_gpio_handle[TOP_QUP], gpio_key, &curr_val);
                if (GPIO_SUCCESS == res)
                {
                    QUP_SE_LOG(config,LEVEL_ERROR, "top_qup_gpio_dump_stat : GPIO_ReadPin  GPIO:%d VAL:0x%x ", config->io[i].pin_no, curr_val);
                }
                else
                {
                    QUP_SE_LOG(config,LEVEL_ERROR, "top_qup_gpio_dump_stat : GPIO_ReadPin failed with res : %d", res);
                    return;
                }
            }
            else
            {
                QUP_SE_LOG(config,LEVEL_ERROR, "top_qup_gpio_dump_stat : GPIO_RegisterPinExplicit failed with res : %d", res);
                return;
            }
        }
    }
}
