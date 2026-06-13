/**
    @file  qup_mem_island.c
    @brief Memory Operation implementation
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/


/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/

#include "qup_alloc.h"
#include "qup_log.h"
#include "qup_os.h"
#include "qurt.h"
#include <stringl/stringl.h>
#include "qurt_memory.h"
#include "qup_ssc_mem.h"

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/


/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/


/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/
qurt_mutex_t qup_cfg_desc_lock;
/*==================================================================================================
                                         EXTERNAL VARIABLES
==================================================================================================*/


/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/
void*  qup_alloc_cfg_desc_pair (const se_dev_cfg *cfg, uint32 size)
{
    void* ptr = NULL;
    uint8 iter = 0;

    qurt_mutex_lock(&qup_cfg_desc_lock);

    if(cfg->island_spec_in_use == ISLAND_CONFIG_INVALID)
    {
        ptr = qup_mem_alloc (cfg, size, QUP_CONFIG_DESC_PAIRS);
    }
    else if(GET_PROTOCOL_MAJOR(cfg->protocol_in_use) == SE_PROTOCOL_SPI || GET_PROTOCOL_MAJOR(cfg->protocol_in_use) == SE_PROTOCOL_SPI_3W)
    {
        while (iter < SPI_MAX_DESCS_PAIRS)
        {
            if(spi_cfg_desc[iter].num_desc == 0)
            {
                ptr = &(spi_cfg_desc[iter]);
                spi_cfg_desc[iter].num_desc = 0xFFF;
                break;
            }
            iter++;
        }
    }
    else if(GET_PROTOCOL_MAJOR(cfg->protocol_in_use) == SE_PROTOCOL_I2C || GET_PROTOCOL_MAJOR(cfg->protocol_in_use) == SE_PROTOCOL_I3C)
    {
        while (iter < I2C_MAX_DESCS_PAIRS)
        {
            if(i2c_cfg_desc[iter].num_desc == 0)
            {
                ptr = &(i2c_cfg_desc[iter]);
                i2c_cfg_desc[iter].num_desc = 0xFFF;
                break;
            }
            iter++;
        }
    }

    qurt_mutex_unlock(&qup_cfg_desc_lock);

    QUP_VERIFY(ptr)
    return ptr;
}

void  qup_dealloc_cfg_desc_pair (const se_dev_cfg *dcfg, void* ptr, uint32 size)
{
    uint8 *mem = (uint8 *) ptr;
    uint32 i;

    qurt_mutex_lock(&qup_cfg_desc_lock);
    if(dcfg->island_spec_in_use == ISLAND_CONFIG_INVALID)
    {
        qurt_free(ptr);
    }
    else
    {
        for (i = 0; i < size ; i++)
        {
            mem[i] = 0;
        }
    }

   qurt_mutex_unlock(&qup_cfg_desc_lock);

    ptr = NULL;

    return;
}

int32 qup_mem_cmp(const void *buf1, const void *buf2, uint32 count)
{
    if (buf1 == NULL || buf2 == NULL)
    {
        QUP_LOG(LEVEL_ERROR, "qup_mem_cmp: Error invalid buffers !!");
        return -1;
    }
    return memcmp(buf1, buf2, count);
}

boolean qup_mem_copy(void *dest, uint32 dest_size,const void *src, uint32 src_size)
{
    uint32 copied;
    copied = memscpy(dest, dest_size, src, src_size);
    if (copied != src_size) 
    { 
        QUP_LOG(LEVEL_ERROR, "qup_mem_copy : Copied fewer bytes : %d", copied);
    }
    return TRUE;

}

void qup_memset(void *ptr, uint8 val,uint32 size)
{
   memset(ptr, val, size);
}

boolean qup_mem_flush_cache (void *addr, uint32 size, qup_mem_attr_type attr, qup_mem_region_type region)
{
    if (region == REGION_DDR)
    {
        if (QURT_EOK != qurt_mem_cache_clean((qurt_addr_t) addr,
                                             (qurt_size_t) size,
                                             QURT_MEM_CACHE_FLUSH,
                                             QURT_MEM_DCACHE))
        {
            return FALSE;
        }
    }

    return TRUE;

}

boolean qup_mem_invalidate_cache (void *addr, uint32 size, qup_mem_attr_type attr, qup_mem_region_type region)
{
    if (region == REGION_DDR)
    {
        if (QURT_EOK != qurt_mem_cache_clean((qurt_addr_t) addr,
                                            (qurt_size_t) size,
                                            QURT_MEM_CACHE_INVALIDATE,
                                            QURT_MEM_DCACHE))
        {
            return FALSE;
        }
    }
    return TRUE;
}

void   *qup_mem_virt_to_phys (int pid, void *ptr, qup_mem_attr_type attr, qup_mem_region_type region)
{
    if(attr == ATTRIB_TRE)
    {
        return (void *) qurt_lookup_physaddr((qurt_addr_t) ptr);
    }
    return (void *) qurt_lookup_physaddr2 ((qurt_addr_t) ptr,pid);
}
