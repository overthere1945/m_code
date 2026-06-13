/** 
    @file  spi_drv.h
    @brief spi driver
 */
/*=============================================================================
            Copyright (c) 2023 Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __SPI_DRV_H__
#define __SPI_DRV_H__

#include "qup_gpi.h"
#include "qup_drv.h"
#include "spi_api.h"

boolean spi_validate_transfer_params (qup_client_ctxt *c_ctxt, 
        spi_config_desc_pairs *cfg_desc_pairs, 
        uint16 num_descr_pairs,       
        callback_fn c_fn,
        void *ctxt        
        );

spi_status_t spi_add_client_ctxt_to_queue(qup_hw_ctxt *h_ctxt, qup_client_ctxt *c_ctxt);

void spi_update_transfer_ctxt(qup_client_ctxt *c_ctxt, spi_config_desc_pairs *cfg_desc_pairs, callback_fn c_fn, void *caller_ctxt,uint32 num_descriptors, uint8 timestamp);

#endif