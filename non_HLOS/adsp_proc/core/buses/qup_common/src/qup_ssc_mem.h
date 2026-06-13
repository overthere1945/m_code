/** 
    @file  qup_ssc_mem.h
    @brief ssc island memory internal
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
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __QUP_SSC_MEM_H__
#define __QUP_SSC_MEM_H__

/* *************************************************************** */
/*                          INCLUDE FILES                          */
/* *************************************************************** */
#include "qurt.h"
#include "utimer.h"
#include "qup_plat.h"
#include "qup_drv.h"
#include "qup_gpi.h"
#include "qup_fifo.h"
#include "qup_devcfg.h"

#include "i3c_drv.h"
#include "i3c_ibi_drv.h"

//#define   MAX_SSC_I2C_CLK_ENTRIES  6
#define   MAX_FREQ_PLAN_TABLE_SIZE 9
#define   MAX_TRE_LIST_SIZE        50

/* *************************************************************** */
/*                         DATA STRUCTURES                         */
/* *************************************************************** */

typedef struct iface_core
{
    union core_mem
    {
        qup_gpi_core_ctxt   gpi;
        qup_fifo_core_ctxt  fifo;
    }mem;
}iface_core;


typedef struct iface_io
{
    union io_mem
    {
        qup_gpi_iface_ctxt   gpi;
        qup_fifo_iface_ctxt  fifo;
    }mem;
    boolean used;
}iface_io;


extern qup_hw_ctxt                    hw_ctxt_mem[];
extern qup_client_ctxt                clnt_ctxt_mem[];
extern iface_core                     iface_ctxt_mem[];
extern iface_io                       iface_io_ctxt_mem[];

extern qurt_mutex_t                   core_mutex_mem[];
extern qurt_mutex_t                   queue_mutex_mem[];

extern qurt_signal_t                  transfer_signal_mem[];
extern qurt_signal_t                  cmd_signal_mem[];

extern qup_timer_callback_data        timer_data_mem[];
extern utimer_type                    timer_handle_mem[];
 
extern i3c_core_ctxt                  i3c_ctxt_mem[];
extern ibi_core_ctxt                  ibi_ctxt_mem[];
extern qurt_mutex_t                   aux_core_mutex_mem[];
extern qurt_mutex_t                   irq_mutex_mem[];

extern qup_dev_cfg                    ssc_qup_config[];
extern se_dev_cfg                     ssc_se_config[];
extern ibi_dev_cfg                    ssc_ibi_config[];
extern se_clk_dfs_config              clk_dfs_plan[];
extern i2c_config_desc_pairs          i2c_cfg_desc[];
extern spi_config_desc_pairs          spi_cfg_desc[];
extern gpi_interface                  qup_gpi_iface_data[];

#ifdef ENABLE_XFER_OPTIMIZE_IN_ISLAND
extern qup_gpi_ring_elem              gpi_ring_elements_list[];
extern i2c_descriptor                 i2c_desc_array[];
extern spi_descriptor_t               spi_desc_array[];
extern i2c_slave_config               i2c_config_array[];
extern spi_config_t                   spi_config_array[];
#endif

/* *************************************************************** */
/*                            FUNCTIONS                            */
/* *************************************************************** */

#endif
