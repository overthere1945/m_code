/**
    @file  spi_gpi.h
    @brief GSI interface for SPI
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __SPI_GPI_H__
#define __SPI_GPI_H__

#include "qup_gpi.h"
#include "qup_drv.h"
#include "spi_gpi.h"
#include "spi_plat.h"
#include "qup_log.h"
#include "qurt.h"
#include "qurt_island.h"
#include "qup_common.h"
#include "qup_alloc.h"
#include "qup_os.h"
#include "qup_plat.h"

#define GPI_POLLING_TIMEOUT_US    1000000
#define GPI_POLLING_INTERVAL_US   20

spi_status_t spi_gpi_queue_transfer (qup_client_ctxt *c_ctxt);

spi_status_t spi_gpi_prepare_transfer (qup_client_ctxt *c_ctxt);

spi_status_t spi_gpi_submit_transfer (qup_client_ctxt *c_ctxt);

spi_status_t spi_gpi_queue_transfer (qup_client_ctxt *c_ctxt);

void spi_handle_tf_completion (qup_client_ctxt *c_ctxt);

void spi_gpi_set_tre_go (gpi_ring_elem *tre, qup_transfer_ctxt *t_ctxt, se_dev_cfg *dcfg );

void spi3w_update_go_tre_for_rx (gpi_ring_elem *tre, qup_transfer_ctxt *t_ctxt, spi_descriptor_t *dc);

void spi_gpi_set_tre_config_0 (gpi_ring_elem *tre, qup_client_ctxt *c_ctxt);

boolean spi_gpi_tre_non_data_opcode (gpi_ring_elem *tre);

void spi_gpi_set_tre_dma (qup_client_ctxt *c_ctxt, gpi_ring_elem *tre, uint8 *buf, spi_descriptor_t *dc);

void spi_gpi_dump_tres(se_dev_cfg *cfg,gpi_ring_elem *tre_list, gpi_ring_elem *base, uint32 count, boolean is_tx, qup_mem_region_type region, uint32 tre_list_size);

#endif /*__SPI_GPI_H__*/
