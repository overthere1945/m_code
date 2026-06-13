/**
    @file  qup_alloc.h
    @brief Platform Memory Interface
 */

/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
03/17/25   GKR     Added qup wrapper for memcmp api
06/26/24   GKR     added new apis to alloc & dealloc memory to store configs fetched from DT
*/
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __QUP_ALLOC_H__
#define __QUP_ALLOC_H__

/* *************************************************************** */
/*                          INCLUDE FILES                          */
/* *************************************************************** */

#include "qup_devcfg.h"

/* *************************************************************** */
/*                         DATA STRUCTURES                         */
/* *************************************************************** */

/* Memory Attribute */
typedef enum qup_mem_attr_type
{
    ATTRIB_NONE = 0,
    ATTRIB_TRE,
    ATTRIB_BUFFER
} qup_mem_attr_type;

/* Memory Type*/
typedef enum qup_mem_region_type
{
    REGION_NONE = 0,
    REGION_PRAM,
    REGION_DDR,
    REGION_TCM
} qup_mem_region_type;

/* Memory Allocation Type*/
typedef enum qup_mem_alloc_type
{
    HW_CTXT_TYPE           = 1,
    CORE_IFACE_TYPE        = 2,
    CLIENT_CTXT_TYPE       = 3,
    TX_TRANSFER_ELEM_TYPE  = 4,
    RX_TRANSFER_ELEM_TYPE  = 5,
    EVR_ELEM_TYPE          = 6,
    IO_CTXT_TYPE           = 7,
    CORE_MUTEX_TYPE        = 8,
    Q_MUTEX_TYPE           = 9,
    TRANSFER_SIGNAL_TYPE   = 10,
    CMD_SIGNAL_TYPE        = 11,
    OS_TIMER_TYPE          = 12,
    OS_TIMER_CB_DATA_TYPE  = 13,
    BW_REQUEST_TYPE        = 14,
    I3C_CORE_CTXT_TYPE     = 15,
    I3C_IBI_CTXT_TYPE      = 16,
    I3C_BUFFER_TYPE        = 17,
    UART_SGL_BUFFER_TYPE   = I3C_BUFFER_TYPE,

    QUP_CONFIG_ISLAND      = 18,
    QUP_SE_CONFIG_ISLAND   = 19,
    QUP_I2C_CONFIG_ISLAND  = 20,
    QUP_IBI_CONFIG_ISLAND     = 21,
    QUP_IBI_CLK_CONFIG_ISLAND = 22,
    QUP_SE_DFS_CONFIG_ISLAND  = 23,
    QUP_TRE_LIST_ISLAND       = 24,
    QUP_CONFIG_DESC_PAIRS     = 25,
    I2C_DESC_TYPE             = 26,
    SPI_DESC_TYPE             = 27,
    I2C_CONFIG_TYPE           = 28,
    SPI_CONFIG_TYPE           = 29,
    QUP_GPI_CONFIG_ISLAND     = 30,
} qup_mem_alloc_type;

/* *************************************************************** */
/*                            FUNCTIONS                            */
/* *************************************************************** */

/*se_dev_cfg mem allocation*/
void   *qup_se_dev_cfg_mem_alloc (QUP_TYPE qup_type, uint32 size, qup_mem_alloc_type type);

/*se_dev_cfg mem free*/
void    qup_se_dev_cfg_mem_free (void* ptr, QUP_TYPE qup_type, uint32 size, qup_mem_alloc_type type);

/* Memory allocation */
void    *qup_mem_alloc (const se_dev_cfg *cfg, uint32 size, qup_mem_alloc_type type);

/* Memory free */
void     qup_mem_free (void *ptr, uint32 size, const se_dev_cfg *cfg, qup_mem_alloc_type type);

/* Memory Maniputlation */
boolean  qup_mem_copy(void *dest, uint32 dest_size, const void *src, uint32 src_size);
void     qup_memset(void *ptr, uint8 val,uint32 size);
int32    qup_mem_cmp(const void *buf1, const void *buf2, uint32 count);

/* Cache Manipualtion*/
boolean  qup_mem_flush_cache (void *addr, uint32 size, qup_mem_attr_type attr, qup_mem_region_type region);
boolean  qup_mem_invalidate_cache (void *addr, uint32 size, qup_mem_attr_type attr, qup_mem_region_type region);

/* Fetch Physical Address */
void    *qup_mem_virt_to_phys (int pid, void *ptr, qup_mem_attr_type attr, qup_mem_region_type region);

/* Allocate Buffer for driver related transfers*/
void    *qup_driver_buffer_alloc (uint32 index, uint32 size, qup_mem_alloc_type type);

/* Allocate cfg desc pairs */
void  *qup_alloc_cfg_desc_pair (const se_dev_cfg *dcfg, uint32 size);

/* De Allocate cfg desc pairs */
void  qup_dealloc_cfg_desc_pair (const se_dev_cfg *dcfg, void* ptr, uint32 size);

#endif
