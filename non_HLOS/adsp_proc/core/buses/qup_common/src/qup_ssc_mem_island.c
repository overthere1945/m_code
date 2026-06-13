/**
    @file  qup_ssc_mem_island.c
    @brief Memory allocation for SSC island usecases
 */

/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
02/03/25   GKR     Added changes to support I2C HS mode
06/26/24   GKR     Enabled support for multiple SSC QUPs 
                   OPtimized island memory required to store I2C, I3C and IBI counters
*/
/*=============================================================================
            Copyright (c)  Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/


/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/

#include "qup_ssc_mem.h"
#include "qup_hal.h"
/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/


/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/


/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/
qup_hw_ctxt                    hw_ctxt_mem[MAX_SSC_QUP_CORES];
qup_client_ctxt                clnt_ctxt_mem[MAX_SSC_QUP_CLIENTS];

iface_core                     iface_ctxt_mem[MAX_SSC_QUP_CORES];
iface_io                       iface_io_ctxt_mem[MAX_SSC_QUP_CLIENTS];
          
qurt_mutex_t                   core_mutex_mem[MAX_SSC_QUP_CORES];
qurt_mutex_t                   queue_mutex_mem[MAX_SSC_QUP_CORES];

qurt_signal_t                  transfer_signal_mem[MAX_SSC_QUP_CORES];
qurt_signal_t                  cmd_signal_mem[MAX_SSC_QUP_CORES + MAX_IBI_INSTANCE];

qup_timer_callback_data        timer_data_mem[MAX_SSC_QUP_CORES];
utimer_type                    timer_handle_mem[MAX_SSC_QUP_CORES];

i3c_core_ctxt                  i3c_ctxt_mem[MAX_I3C_INSTANCE];
ibi_core_ctxt                  ibi_ctxt_mem[MAX_IBI_INSTANCE];
qurt_mutex_t                   aux_core_mutex_mem[MAX_I3C_INSTANCE];
qurt_mutex_t                   irq_mutex_mem[MAX_IBI_INSTANCE];

qup_dev_cfg                    ssc_qup_config[NUM_SSC_QUP] = {0};
se_dev_cfg                     ssc_se_config[MAX_SSC_QUP_CORES];
ibi_dev_cfg                    ssc_ibi_config[MAX_IBI_INSTANCE];
se_clk_dfs_config              clk_dfs_plan[MAX_SSC_QUP_CORES];
i2c_config_desc_pairs          i2c_cfg_desc[I2C_MAX_DESCS_PAIRS] = {0};
spi_config_desc_pairs          spi_cfg_desc[SPI_MAX_DESCS_PAIRS] = {0};

#ifdef ENABLE_XFER_OPTIMIZE_IN_ISLAND
qup_gpi_ring_elem              gpi_ring_elements_list[MAX_TRE_LIST_SIZE];
i2c_descriptor                 i2c_desc_array[I2C_MAX_DESCS];
spi_descriptor_t               spi_desc_array[SPI_MAX_DESCS];
i2c_slave_config               i2c_config_array[I2C_MAX_CONFIG];
spi_config_t                   spi_config_array[SPI_MAX_CONFIG];
#endif

/*==================================================================================================
                                         EXTERNAL VARIABLES
==================================================================================================*/

/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/
