/** 
    @file  qup_plat_ssc_internal.h
    @brief ssc platform interface
 */
/*=============================================================================
            Copyright (c) 2020 Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __QUP_SSC_PLAT_INTERNAL_H__
#define __QUP_SSC_PLAT_INTERNAL_H__

/* *************************************************************** */
/*                          INCLUDE FILES                          */
/* *************************************************************** */

#include "qup_plat.h"

/* *************************************************************** */
/*                         DATA STRUCTURES                         */
/* *************************************************************** */


/* *************************************************************** */
/*                            FUNCTIONS                            */
/* *************************************************************** */

//SSC plat functions used accross island and non-island
boolean ssc_qup_clock_disable (se_dev_cfg *config);
boolean ssc_qup_clock_enable (se_dev_cfg *config);
boolean ssc_qup_gpio_enable_io (se_dev_cfg *config, uint8 io_idx);
boolean ssc_qup_gpio_enable (se_dev_cfg *config);
boolean ssc_qup_gpio_disable (se_dev_cfg *config);
boolean  ssc_qup_timer_clear(se_dev_cfg *dcfg, void *handle);
boolean  ssc_qup_timer_set  (se_dev_cfg *dcfg, void *handle, uint32 time_out_msec);
void *ssc_qup_timer_init(se_dev_cfg *dcfg, void (*timer_cb) (void *), void *data);
boolean  ssc_qup_timer_deinit(se_dev_cfg *dcfg, void *handle);
void   ssc_qup_gpio_dump_stat (se_dev_cfg *config);

#endif
