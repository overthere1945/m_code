/**
    @file  spi_plat.h
    @brief platform interface
 */
/*=============================================================================
            Copyright (c) 2016, 2019-2020 Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __SPI_PLAT_H__
#define __SPI_PLAT_H__

#include "comdef.h"
#include "spi_api.h"
#include "qup_devcfg.h"

uint32 spi_plat_clock_get_frequency(se_dev_cfg *config, uint32 frequency_hz);

boolean spi_plat_get_div_dfs (se_dev_cfg *cfg, uint32 frequency_hz, uint32* dfs_index, uint32* div);

spi_timestamp_type spi_plat_get_timestamp_supported(void);

#endif /*__SPI_PLAT_H__*/
