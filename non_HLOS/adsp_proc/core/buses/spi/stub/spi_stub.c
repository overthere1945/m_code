/**
    @file  spi_stub.c
    @brief
 */
/*=============================================================================
            Copyright (c) 2023 Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

/*==================================================================================================
Edit History

$Header:

when       who          what, where, why
--------   --------     --------------------------------------------------------
09/08/23   GST          Added stubs 

==================================================================================================*/

#include "spi_api.h"
#include "qup_common.h"

spi_status_t spi_open (spi_instance_t instance, void **spi_handle)
{
	return SPI_ERROR;
}

spi_status_t spi_open_ex (QUP_TYPE qup, uint32 se_index, void **spi_handle)
{
	return SPI_ERROR;
}

spi_status_t spi_power_on (void *spi_handle)
{
	return SPI_ERROR;
}

spi_status_t spi_power_off (void *spi_handle){
	return SPI_ERROR;
}

spi_status_t spi_full_duplex (void *spi_handle, spi_config_t *config, spi_descriptor_t *desc, uint32 num_descriptors, callback_fn c_fn, void *c_ctxt, uint8 timestamp)
{
	return SPI_ERROR;
}

spi_status_t spi_close (void *spi_handle)
{
	return SPI_ERROR;
}

spi_status_t spi_get_timestamp (void *spi_handle, uint64 *start_time, uint64 *completed_time)
{
	return SPI_ERROR;
}

spi_status_t spi_setup_island_resource (spi_instance_t instance, uint32 frequency_hz)
{
	return SPI_ERROR;
}

spi_status_t spi_setup_island_resource_ex (QUP_TYPE qup, uint32 se_index, uint32 frequency_hz)
{
	return SPI_ERROR;
}

spi_status_t spi_reset_island_resource (spi_instance_t instance)
{
	return SPI_ERROR;
}

spi_status_t spi_reset_island_resource_ex (QUP_TYPE qup, uint32 se_index, uint32 frequency_hz)
{
	return SPI_ERROR;
}

spi_status_t spi_get_timestamp_64 (void *spi_handle, spi_timestamp_type *ts_type, uint64 *timestamp_val)
{
	return SPI_ERROR;
}

spi_status_t spi_enable_slave_index (void *spi_handle, spi_cs_index cs_index)
{
	return SPI_ERROR;
}

void spi_qdi_root_init (void)
{
	return;
}

void spi_qdi_user_init (void)
{
	return;
}

spi_status_t
spi_prepare_transfer
(
    void *spi_handle,
    spi_config_desc_pairs *spi_cfg_desc_pairs,
    uint16 num_desc_pairs,
    callback_fn c_fn,
    void *ctxt,
    uint8 timestamp,
    void **multi_transfer_handle
)
{
	return SPI_ERROR;
}

spi_status_t
spi_submit_transfer
(
    void *spi_handle,
    void *multi_transfer_handle
)
{
	return SPI_ERROR;
}