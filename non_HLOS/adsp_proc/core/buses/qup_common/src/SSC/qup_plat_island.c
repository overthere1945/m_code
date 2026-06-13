/**
    @file  qup_plat_island.c
    @brief platform implementation
 */
/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
05/13/25   RK      Added SWA for CESTA bug on aspen
09/11/24   MG 	   Added proper HWIO MACRO to avoid hard codeing 
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
#include "qup_plat_ssc_internal.h"
#include "qup_os.h"
#include "Clock.h"
#include "GPIO.h"
#include "utimer.h"
#include "qup_hal.h"
#include "qurt_memory.h"
#include "DTBExtnLib.h"
#include "GPIOImage.h"
/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/


/*==================================================================================================
                                          EXTERN VARIBLES
==================================================================================================*/
extern ClockHandle qup_clock_handle ;
extern GPIOClientHandleType qup_gpio_handle[];

/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/
const QupPlatFcnTable ssc_qup_functions = 
{
    ssc_qup_clock_enable    ,
    ssc_qup_clock_disable   ,
    ssc_qup_gpio_enable_io  ,
    ssc_qup_gpio_enable     ,
    ssc_qup_gpio_disable    ,
    ssc_qup_gpio_dump_stat  ,
    ssc_qup_timer_init      ,
    ssc_qup_timer_set       ,
    ssc_qup_timer_clear     ,
    ssc_qup_timer_deinit    ,
};

/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/

boolean ssc_qup_clock_disable (se_dev_cfg *config)
{
    ClockResult res = CLOCK_SUCCESS;

    QUP_SE_LOG(config,LEVEL_VERBOSE, "clock_OFF ALL");
    
    if(QUP_IS_SET(config->resources.state,QUP_CLOCK_ON_FAILURE))
    {
        QUP_FLAG_UNSET(config->resources.state,QUP_CLOCK_ON_FAILURE);
    }
    else
    {
        config->resources.state = QUP_ALL_CLOCKS_ON;
    }

    switch(config->resources.state)
    {
        case QUP_SE_CLOCK_ON:
        res = Clock_Disable(qup_clock_handle, config->se_clk_id);
        if(CLOCK_SUCCESS != res)
        {
            QUP_SE_LOG(config,LEVEL_ERROR, "clock_OFF failed for QUP_SE_CLOCK, id : %d, error: 0x%x ",config->se_clk_id,res);
            goto exit_error;
        }
        config->resources.state = QUP_M_AHB_CLOCK_ON;

        case QUP_M_AHB_CLOCK_ON:
        res = Clock_Disable(qup_clock_handle, config->qup_data->common_clk_id[QUP_M_AHB_CLOCK_IDX]);
        if(CLOCK_SUCCESS != res)
        {
            QUP_SE_LOG(config,LEVEL_ERROR, "clock_OFF failed for QUP_M_AHB_CLOCK, id : %d, error: 0x%x ",config->qup_data->common_clk_id[QUP_M_AHB_CLOCK_IDX],res);
            goto exit_error;
        }
        config->resources.state = QUP_S_AHB_CLOCK_ON;

        case QUP_S_AHB_CLOCK_ON:
        res = Clock_Disable(qup_clock_handle, config->qup_data->common_clk_id[QUP_S_AHB_CLOCK_IDX]);
        if(CLOCK_SUCCESS != res)
        {
            QUP_SE_LOG(config,LEVEL_ERROR, "clock_OFF failed for QUP_S_AHB_CLOCK, id : %d, error: 0x%x",config->qup_data->common_clk_id[QUP_S_AHB_CLOCK_IDX],res);
            goto exit_error;
        }
        config->resources.state = QUP_CORE_CLOCK_ON;

        case QUP_CORE_CLOCK_ON:
        res = Clock_Disable(qup_clock_handle, config->qup_data->common_clk_id[QUP_CORE_CLOCK_IDX]);
        if(CLOCK_SUCCESS != res)
        {
            QUP_SE_LOG(config,LEVEL_ERROR, "clock_OFF failed for QUP_CORE_CLOCK, id : %d,error: 0x%x",config->qup_data->common_clk_id[QUP_CORE_CLOCK_IDX],res);
            goto exit_error;
        }
        config->resources.state = QUP_CORE_2X_CLOCK_ON;

        case QUP_CORE_2X_CLOCK_ON:
        res = Clock_Disable(qup_clock_handle, config->qup_data->common_clk_id[QUP_CORE_2X_CLOCK_IDX]);
        if(CLOCK_SUCCESS != res)
        {
            QUP_SE_LOG(config,LEVEL_ERROR, "clock_OFF failed for QUP_CORE2X_CLOCK, id : %d, error: 0x%x",config->qup_data->common_clk_id[QUP_CORE_2X_CLOCK_IDX],res);
            goto exit_error;
        }
        config->resources.state = QUP_NO_CLOCK_ON;

        break;
        default : 
            goto exit_error;
    }
    
    return TRUE;
    
exit_error:
    
    //ERR FATAL::Probable SW race condition
    QUP_ASSERT(config,QUP_FATAL_TYPE);
    return FALSE;
}

boolean ssc_qup_clock_enable (se_dev_cfg *config)
{
    ClockResult res = CLOCK_SUCCESS;

    QUP_SE_LOG(config,LEVEL_VERBOSE, "clock_ON ALL");
    
    res = Clock_Enable(qup_clock_handle, config->qup_data->common_clk_id[QUP_CORE_2X_CLOCK_IDX]);
    if(CLOCK_SUCCESS != res)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "clock_ON failed for QUP_CORE_2X_CLOCK, id : %d, error: 0x%x",config->qup_data->common_clk_id[QUP_CORE_2X_CLOCK_IDX],res);
        goto exit_error;
    }
    config->resources.state = QUP_CORE_2X_CLOCK_ON;
    
    res = Clock_Enable(qup_clock_handle, config->qup_data->common_clk_id[QUP_CORE_CLOCK_IDX]);
    if(CLOCK_SUCCESS != res)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "clock_ON failed for QUP_CORE_CLOCK, id  : %d, error: 0x%x",config->qup_data->common_clk_id[QUP_CORE_CLOCK_IDX],res);
        goto exit_error;
    }
    config->resources.state = QUP_CORE_CLOCK_ON;	

    res = Clock_Enable(qup_clock_handle, config->qup_data->common_clk_id[QUP_S_AHB_CLOCK_IDX]);
    if(CLOCK_SUCCESS != res)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "clock_ON failed for QUP_S_AHB_CLOCK, id : %d, error: 0x%x",config->qup_data->common_clk_id[QUP_S_AHB_CLOCK_IDX],res);
        goto exit_error;
    }
    config->resources.state = QUP_S_AHB_CLOCK_ON;	
    
    res = Clock_Enable(qup_clock_handle, config->qup_data->common_clk_id[QUP_M_AHB_CLOCK_IDX]);
    if(CLOCK_SUCCESS != res)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "clock_ON failed for QUP_M_AHB_CLOCK, id : %d, error: 0x%x",config->qup_data->common_clk_id[QUP_M_AHB_CLOCK_IDX],res);
        goto exit_error;
    }
    config->resources.state = QUP_M_AHB_CLOCK_ON;

    res = Clock_Enable(qup_clock_handle, config->se_clk_id);
    if(CLOCK_SUCCESS != res)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "clock_ON failed for QUP_SE_CLOCK, id : %d,error: 0x%x",config->se_clk_id,res);
        goto exit_error;
    }
    config->resources.state = QUP_SE_CLOCK_ON;
    
    return TRUE;
    
exit_error:
    
    //To figure out which clock on failed in future : sticky variable
    config->resources.failure_state = config->resources.state;
    QUP_FLAG_SET(config->resources.state,QUP_CLOCK_ON_FAILURE);
    
    //To switch of the clocks already turned on
    qup_clock_disable(config);
    
    return FALSE;


}

boolean ssc_qup_gpio_enable_io (se_dev_cfg *config, uint8 io_idx)
{
    GPIOConfigType gpio_config = {0};
    GPIOResult     res =0;

    res = GPIO_ConfigPin(qup_gpio_handle[SSC_QUP], config->se_gpio_enable_key, gpio_config);
    if (GPIO_SUCCESS != res)
    {
      QUP_SE_LOG(config,LEVEL_ERROR, "ssc_qup_gpio_enable_io : GPIO_ConfigPin failed res:  0x%x", res);
      return FALSE;
    }


    return TRUE;
}

boolean ssc_qup_gpio_enable (se_dev_cfg *config)
{
   int i=0;
   uint8 *se_base = NULL;

    if((GET_PROTOCOL_MAJOR(config->protocol_in_use) == SE_PROTOCOL_I3C) && config->force_default_reg_write)
    {
        se_base = config->se_base_addr;
		HWIO_OUTX(GENI_CFG_BASE(se_base), GENI_FORCE_DEFAULT_REG, 0x1);   //writing 1 to SE_GENI_FORCE_DEFAULT_REG
        config->force_default_reg_write =  FALSE;
        qurt_mem_barrier();
    }

    if(!qup_gpio_enable_io(config,i))
    {
        return FALSE;
    }


   if(GET_PROTOCOL_MAJOR(config->protocol_in_use) == SE_PROTOCOL_I3C)
   {
       QUP_SSC_I3C_SLEW_RATE_CTRL(config->qup_data->se_wrapper_base_addr,config->se_id,0x3F);
   }
   return TRUE;
}

boolean ssc_qup_gpio_disable (se_dev_cfg *config)
{
    GPIOResult     res =0;

    res = GPIO_ConfigPinInactive(qup_gpio_handle[SSC_QUP], config->se_gpio_disable_key);
    if (GPIO_SUCCESS != res)
    {
      QUP_SE_LOG(config,LEVEL_ERROR, "ssc_qup_gpio_disable : GPIO_ConfigPin failed res: :  0x%x", res);
      return FALSE;
    }

    return TRUE;
}

boolean  ssc_qup_timer_clear(se_dev_cfg *dcfg, void *handle)
{
    utimer_type *timer_handle= (utimer_type *)handle;
    utimer_error_type t_error;

    t_error = utimer_stop(timer_handle,  UT_MSEC,NULL);
    if(UTE_SUCCESS == t_error)
    {
        return TRUE;
    }
    else
    {
        QUP_LOG(LEVEL_ERROR, "qup_timer_clear : timer clr fail 0x%x %d", handle,t_error);
        return FALSE;
    }
}
boolean  ssc_qup_timer_set  (se_dev_cfg *dcfg, void *handle, uint32 time_out_msec)
{
    utimer_type *timer_handle= (utimer_type *)handle;
    utimer_error_type t_error;

    t_error = utimer_set_64(timer_handle, time_out_msec, 0, UT_MSEC);
    if(UTE_SUCCESS == t_error)
    {
        return TRUE;
    }
    else
    {
        QUP_LOG(LEVEL_ERROR, "qup_timer_set : timer set fail 0x%x with error :%d", handle, t_error);
        return FALSE;
    }

}

