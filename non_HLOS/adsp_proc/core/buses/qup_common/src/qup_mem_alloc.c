/**
    @file  qup_alloc.c
    @brief Memory allocation implementation
 */

/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
08/14/25   GKR     Added changes to support SFE
03/17/25   SS      SWA for Kaanapali QUP AXI HW Bug
02/03/25   GKR     Added changes to support I2C HS mode
06/26/24   GKR     Added New APi qup_island_mem_free(), qup_se_dev_cfg_mem_free()
                   Moved from if else if ladder to switch case for optimized execution
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
#include "qup_plat.h"
#include "qup_os.h"
#include "qup_drv.h"
#include "qup_gpi.h"
#include "qup_hal.h"
#include "qurt_memory.h"
#include "qup_ssc_mem.h"

#ifdef QUP_AXI_HW_BUG
#include "smem.h"
#endif

#define I3C_BUFFER_SIZE 64
#define MAX_CONCURRENT_SSC_CLIENTS 50

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/

#define ADDRESS_IN_ARRAY(ptr,type,array,size)  (((uint8 *)ptr >= (uint8 *)array) && ( (uint8 *)ptr <= ((uint8 *)array + (sizeof(type) * size))))
#define INDEX_IN_ARRAY(ptr,type,array)         (((uint8 *)ptr - (uint8 *)array) / sizeof(type))
#ifdef QUP_AXI_HW_BUG
#define MAX_RING_BUFFERS 4
#define MAX_BUFFER_ALIGN_TYPES 3

#define QUP_NS_SMEM_ID 673
#define QUP_NS_SMEM_SIZE 58*1024  // 58kB

#define QUP_SMEM_ADSP_CH_OFFSET 51*1024 //Aligned base(1k) + HLOS(24k) + TZ (4k)+ SOCCP (8k) +MODEM(4k) +DCP(4k) + BOOT(6k)
#define QUP_SMEM_ADSP_CH_SIZE   3*1024

#define NUM_512_BYTE_RINGS 2
#define NUM_256_BYTE_RINGS 2
#define NUM_128_BYTE_RINGS 2
/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/

typedef struct
{
    uint8*        base_addr;
    uint8*        curr_avail_addr;
    int32         availble_size;
}qup_smem_buffer;

typedef struct
{
    uint8* ring_addr;
    boolean in_use;
}qup_smem_ring;

typedef struct
{
    qup_smem_ring tre_ring_buffers[MAX_RING_BUFFERS];
} qup_smem_pool;


qup_smem_buffer qup_smem;
static qup_smem_pool smem_tre_pool_512_bytes_aligned;
static qup_smem_pool smem_tre_pool_256_bytes_aligned;
static qup_smem_pool smem_tre_pool_128_bytes_aligned;

#endif
/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/

/*==================================================================================================
                                         EXTERNAL VARIABLES
==================================================================================================*/

/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/
#ifdef QUP_AXI_HW_BUG
uintptr_t qup_align_to_1024_bytes (uintptr_t address) {
    return (address + 0x3FF) & ~0x3FF;
}

void qup_smem_init(void)
{
    uint8 i = 0;

    if (qup_smem.base_addr == NULL)
    {
        qup_smem.base_addr = (uint8 *)smem_alloc(QUP_NS_SMEM_ID, QUP_NS_SMEM_SIZE);
        if (qup_smem.base_addr == NULL)
        {
            QUP_ASSERT(NULL, QUP_FATAL_TYPE);
        }

        qup_smem.base_addr = (uint8 *)qup_align_to_1024_bytes((uintptr_t)qup_smem.base_addr);
        qup_smem.curr_avail_addr = qup_smem.base_addr + QUP_SMEM_ADSP_CH_OFFSET;
        qup_smem.availble_size = QUP_SMEM_ADSP_CH_SIZE;
    }
    for (i = 0; i < NUM_512_BYTE_RINGS; i++)
    {
        if(qup_smem.availble_size < 512)
        {
            QUP_LOG(LEVEL_ERROR, "QUP SMEM not Enough memory for 512 bytes ring !!!");
        }
        smem_tre_pool_512_bytes_aligned.tre_ring_buffers[i].ring_addr = qup_smem.curr_avail_addr;
        qup_smem.curr_avail_addr += 512;
        qup_smem.availble_size -= 512;
    }

    for (i = 0; i < NUM_256_BYTE_RINGS; i++)
    {
        if(qup_smem.availble_size < 256)
        {
            QUP_LOG(LEVEL_ERROR, "QUP SMEM not Enough memory for 256 bytes ring !!!");
        }
        smem_tre_pool_256_bytes_aligned.tre_ring_buffers[i].ring_addr = qup_smem.curr_avail_addr;
        qup_smem.curr_avail_addr += 256;
        qup_smem.availble_size -= 256;
    }

    for (i = 0; i < NUM_128_BYTE_RINGS; i++)
    {
        if(qup_smem.availble_size < 128)
        {
            QUP_LOG(LEVEL_ERROR, "QUP SMEM not Enough memory for 256 bytes ring !!!");
        }
        smem_tre_pool_128_bytes_aligned.tre_ring_buffers[i].ring_addr = qup_smem.curr_avail_addr;
        qup_smem.curr_avail_addr += 128;
        qup_smem.availble_size -= 128;
    }
}

static void* qup_smem_alloc(uint32 size)
{
    qup_smem_pool* tre_pool = NULL;
    uint8* ptr = NULL;
    uint8 i = 0;


    switch (size)
    {
      case 512:
         tre_pool = &smem_tre_pool_512_bytes_aligned;
         break;
      case 256:
         tre_pool = &smem_tre_pool_256_bytes_aligned;
         break;
      case 128:
         tre_pool = &smem_tre_pool_128_bytes_aligned;
         break;
      default:
        return NULL;
    }

    for (i = 0; i < MAX_RING_BUFFERS; i++)
    {
        if (tre_pool->tre_ring_buffers[i].in_use == FALSE)
        {
            if (tre_pool->tre_ring_buffers[i].ring_addr != NULL)
            {
                ptr = tre_pool->tre_ring_buffers[i].ring_addr;
                tre_pool->tre_ring_buffers[i].in_use = TRUE;
            }
            else
            {
                ptr = NULL;
            }
            break;
        }
    }

    return ptr;

}

static void qup_smem_free(void* mem_addr)
{
    qup_smem_pool* tre_pool[MAX_BUFFER_ALIGN_TYPES] = {NULL};
    uint8 i = 0, j = 0;

    tre_pool[0] = &smem_tre_pool_128_bytes_aligned;
    tre_pool[1] = &smem_tre_pool_256_bytes_aligned;
    tre_pool[2] = &smem_tre_pool_512_bytes_aligned;

    for ( i = 0; i < MAX_BUFFER_ALIGN_TYPES; i++)
    {
        for ( j = 0; j < MAX_RING_BUFFERS; j++)
        {
            if (tre_pool[i]->tre_ring_buffers[j].ring_addr == mem_addr)
            {
                tre_pool[i]->tre_ring_buffers[j].in_use = FALSE;
                return;
            }
        }
    }

}
#endif
static void qup_island_mem_free (void* ptr, uint32 size)
{
    uint32 i = 0;
    uint8* mem = NULL;

    mem = (uint8*)ptr;

    for (i = 0; i < size; i++)
    {
        mem[i] = 0;
    }

    return;
}

/* This function should be called holding qup_mutex_global_lock() from upper layer */
void* qup_se_dev_cfg_mem_alloc (QUP_TYPE qup_type, uint32 size, qup_mem_alloc_type type)
{
    void *ptr = NULL;
    uint8 iter = 0;

    if(GET_QUP_MAJOR(qup_type) != SSC_QUP)
    {

        ptr = qurt_malloc(size);

        if(ptr)
        {
            qup_memset(ptr,0x0,size);
        }
    }
    else
    {
        switch (type)
        {
            case QUP_CONFIG_ISLAND:
            {
                while (iter < NUM_SSC_QUP)
                {
                    if (ssc_qup_config[iter].core_base_addr == NULL)
                    {
                        ptr = &ssc_qup_config[iter];
                        ssc_qup_config[iter].core_base_addr = (uint8*) 0xFFFFFFFF;
                        break;
                    }
                  iter++;
                }
                break;
            }

            case QUP_SE_CONFIG_ISLAND:
            {
                while (iter < MAX_SSC_QUP_CORES)
                {
                    if(ssc_se_config[iter].qup_data == NULL)
                    {
                        ptr = &(ssc_se_config[iter]);
                        ssc_se_config[iter].qup_data = (void*) 0xFFFFFFFF;
                        break;
                    }
                    iter++;
                }
                break;
            }

            case QUP_IBI_CONFIG_ISLAND:
            {
                while (iter < MAX_IBI_INSTANCE)
                {
                    if(ssc_ibi_config[iter].core_base_addr == NULL)
                    {
                        ptr = &(ssc_ibi_config[iter]);
                        ssc_ibi_config[iter].core_base_addr = (void*) 0xFFFFFFFF;
                        break;
                    }
                    iter++;
                }
                break;
            }

            default:
                ptr = NULL;
        }
    }

    QUP_VERIFY(ptr)
    return ptr;
}

/* This function should be called holding qup_mutex_global_lock() from upper layer */
void qup_se_dev_cfg_mem_free (void* ptr,  QUP_TYPE qup_type, uint32 size,qup_mem_alloc_type type)
{

    if(GET_QUP_MAJOR(qup_type) != SSC_QUP)
    {
        qurt_free(ptr);
    }
    else
    {
        qup_island_mem_free(ptr, size);
    }
}
/* This function should be called holding qup_mutex_global_lock() from upper layer */
void   *qup_mem_alloc (const se_dev_cfg *cfg, uint32 size, qup_mem_alloc_type type)
{
    void *ptr = NULL;
    uint8 iter = 0;

    if(cfg == NULL)
    {
        QUP_LOG(LEVEL_ERROR,"qup_mem_alloc NULL cfg passed");
        return NULL;
    }

    if(GET_QUP_MAJOR(cfg->qup_data->qup_id) != SSC_QUP)
    {
        switch (type)
        {
            case TX_TRANSFER_ELEM_TYPE:
            case RX_TRANSFER_ELEM_TYPE:
            case EVR_ELEM_TYPE:
            {
#ifdef QUP_AXI_HW_BUG
                if (cfg->qup_data->qup_id == QUP_1 && qup_get_chip_version() == CHIP_VERSION_R1)
                {
                    ptr = qup_smem_alloc(size);
                }
                else
#endif
                {
#ifdef QUP_DEVICE_HEAP_MGR_ENABLE
                    ptr = device_heap_mgr_heap_aligned_alloc(QUP_GPII_RING_BUFFER_ALIGN, size);
#else
                    ptr = qurt_memalign(QUP_GPII_RING_BUFFER_ALIGN, size);
#endif
                    if(ptr && type == EVR_ELEM_TYPE)
                    {
                        qup_memset(ptr, 0, size);
                        qup_mem_flush_cache(ptr, size, ATTRIB_TRE, REGION_DDR);
                    }
                }
                break;
            }
            
#ifdef QUP_DEVICE_HEAP_MGR_ENABLE
            case I3C_BUFFER_TYPE:
            {
                ptr = device_heap_mgr_heap_alloc(size);
                break;
            }
#endif

            case I3C_IBI_CTXT_TYPE:
            {
                ptr = qurt_malloc(size);
                if(ptr)
                {
                    qup_memset(ptr,0x0,size);
                    ((ibi_core_ctxt *)ptr)->irq_lock = qurt_malloc(sizeof(qurt_mutex_t));
                    QUP_VERIFY(((ibi_core_ctxt *)ptr)->irq_lock);
                    if(((ibi_core_ctxt *)ptr)->irq_lock)
                    {
                        qup_memset(((ibi_core_ctxt *)ptr)->irq_lock,0x0,sizeof(qurt_mutex_t));
                    }
                    qurt_pimutex_init(((ibi_core_ctxt *)ptr)->irq_lock);
                }
                goto ON_EXIT;
            }
            default :
            {
                ptr = qurt_malloc(size);
                break;
            }
        }
        if(ptr)
        {
            qup_memset(ptr,0x0,size);
        }

        goto ON_EXIT;

    }
    else
    {
        if (cfg->se_id >= MAX_SE_PER_QUP)
        {
            QUP_ASSERT(NULL,QUP_FATAL_TYPE);
            return NULL;
        }

        if(cfg->island_spec_in_use == ISLAND_CONFIG_INVALID)// for NON ISLAND usecase SE on SSC QUP for OIS APIs
        {
            switch (type)
            {
                case I2C_DESC_TYPE:
                case I2C_CONFIG_TYPE:
                case QUP_TRE_LIST_ISLAND:
                case SPI_DESC_TYPE:
                case SPI_CONFIG_TYPE:
                case QUP_CONFIG_DESC_PAIRS:
                case CLIENT_CTXT_TYPE:
                case IO_CTXT_TYPE:
                {
                    ptr = qurt_malloc(size);
                    if(ptr) qup_memset(ptr,0x0,size);
                    goto ON_EXIT;
                }
                    break;

                default:
                    ptr = NULL;
                    // if none of the above cases goto next switch for island memory
            }
        }

        switch (type)
        {
            case CLIENT_CTXT_TYPE:
            {
                while (iter < MAX_SSC_QUP_CLIENTS)
                {
                    if(clnt_ctxt_mem[iter].h_ctxt == NULL)
                    {
                        ptr = &clnt_ctxt_mem [iter];
                        clnt_ctxt_mem[iter].h_ctxt = (void*) 0xFFFFFFFF;
                        break;
                    }
                    iter++;
                }

                break;
            }
            case IO_CTXT_TYPE:
            {
                while (iter < (MAX_SSC_QUP_CLIENTS))
                {
                    if(iface_io_ctxt_mem[iter].used == FALSE)
                    {
                        ptr = &(iface_io_ctxt_mem[iter].mem);
                        iface_io_ctxt_mem[iter].used = TRUE;
                        break;
                    }
                    iter++;
                }
                break;
            }

            case TX_TRANSFER_ELEM_TYPE:
            case RX_TRANSFER_ELEM_TYPE:
            case I3C_BUFFER_TYPE:
            {
                if(GET_PROTOCOL_MAJOR(cfg->protocol_in_use) < PROTOCOL_MAJOR_MAX)
                {
                    ptr = qup_common_pram_tre_malloc(cfg->protocol_in_use, size);
                }
                break;
            }

            case I3C_CORE_CTXT_TYPE:
            {
                while (iter < MAX_I3C_INSTANCE)
                {
                    if(i3c_ctxt_mem[iter].core_lock == NULL)
                    {
                        ptr = &i3c_ctxt_mem[iter];

                        /* Init Mutex : Maybe move out later*/
                        i3c_ctxt_mem[iter].core_lock = &(aux_core_mutex_mem[iter]);
                        qurt_mutex_init(i3c_ctxt_mem[iter].core_lock);
                        break;
                    }
                    iter++;
                }
                break;
            }

            case I3C_IBI_CTXT_TYPE:
            {
                while (iter < MAX_IBI_INSTANCE)
                {
                    if(ibi_ctxt_mem[iter].irq_lock == NULL)
                    {
                        ptr = &ibi_ctxt_mem[iter];

                        /* Init Mutex : Maybe move out later*/
                        ibi_ctxt_mem[iter].irq_lock = &(irq_mutex_mem[iter]);
                        qurt_pimutex_init(ibi_ctxt_mem[iter].irq_lock);

                        break;
                    }
                    iter++;
                }
                break;
            }

            case QUP_SE_DFS_CONFIG_ISLAND:
            {
                while (iter < MAX_SSC_QUP_CORES)
                {
                    if(clk_dfs_plan[iter].num_entries == 0)
                    {
                        ptr = &clk_dfs_plan[iter];
                        break;
                    }
                    iter++;
                }
                break;
            }

#ifdef ENABLE_XFER_OPTIMIZE_IN_ISLAND
            case QUP_TRE_LIST_ISLAND:
            {
                while (iter < (MAX_TRE_LIST_SIZE))
                {
                    if(gpi_ring_elements_list[iter].in_use == FALSE)
                    {
                        ptr = &(gpi_ring_elements_list[iter].tre_list);
                        gpi_ring_elements_list[iter].in_use =  TRUE;
                        break;
                    }
                    iter++;
                }
                break;
            }
            case I2C_DESC_TYPE:
            {
                while (iter < (I2C_MAX_DESCS - size))
                {
                    if(i2c_desc_array[iter].buffer == NULL)
                    {
                        ptr = &(i2c_desc_array[iter]);
                        break;
                    }
                    iter++;
                }
                break;
            }
            case SPI_DESC_TYPE:
            {
                while (iter < (SPI_MAX_DESCS - size))
                {
                    if(spi_desc_array[iter].len == 0)
                    {
                        ptr = &(spi_desc_array[iter]);
                        break;
                    }
                    iter++;
                }
                break;
            }
            case I2C_CONFIG_TYPE:
            {
                while (iter < I2C_MAX_CONFIG)
                {
                    if(i2c_config_array[iter].bus_frequency_khz == 0)
                    {
                        ptr = &(i2c_config_array[iter]);
                        break;
                    }
                    iter++;
                }
                break;
            }
            case SPI_CONFIG_TYPE:
            {
                while (iter < SPI_MAX_CONFIG)
                {
                    if(spi_config_array[iter].clk_freq_hz == 0)
                    {
                        ptr = &(spi_config_array[iter]);
                        break;
                    }
                    iter++;
                }
                break;
            }
#else
            case I2C_DESC_TYPE:
            case I2C_CONFIG_TYPE:
            case QUP_TRE_LIST_ISLAND:
            case SPI_DESC_TYPE:
            case SPI_CONFIG_TYPE:
            case QUP_CONFIG_DESC_PAIRS:
            {
                ptr = NULL;  //OIS APIs currently not supported in island
                QUP_LOG(LEVEL_ERROR, "QUP OIS APIs Currently not supported in island");
                break;
            }
#endif
            default :
            {
                if(ADDRESS_IN_ARRAY(cfg, se_dev_cfg, ssc_se_config, MAX_SSC_QUP_CORES))
                {
                    iter = INDEX_IN_ARRAY(cfg, se_dev_cfg, ssc_se_config);
                    switch (type)
                    {
                        case HW_CTXT_TYPE:
                        {
                            ptr = &(hw_ctxt_mem [iter]);
                            break;
                        }
                        case CORE_IFACE_TYPE:
                        {
                            ptr = &(iface_ctxt_mem[iter].mem);
                            break;
                        }
                        case CORE_MUTEX_TYPE:
                        {
                            ptr = &(core_mutex_mem[iter]);
                            break;
                        }
                        case Q_MUTEX_TYPE:
                        {
                            ptr = &(queue_mutex_mem[iter]);
                            break;
                        }
                        case TRANSFER_SIGNAL_TYPE:
                        {
                            ptr = &(transfer_signal_mem[iter]);
                            break;
                        }
                        case CMD_SIGNAL_TYPE:
                        {
                            ptr = &(cmd_signal_mem[iter]);
                            break;
                        }
                        case OS_TIMER_TYPE:
                        {
                            ptr = &timer_handle_mem[iter];
                            break;
                        }
                        case OS_TIMER_CB_DATA_TYPE:
                        {
                            ptr = &timer_data_mem[iter];
                            break;
                        }
                        default:
                        {
                            ptr = NULL;
                        }
                    }
                }
                else
                {
                    ptr = NULL;
                }
            }
        }
    }

ON_EXIT:
   if( ptr == NULL)
   {
	  QUP_LOG(LEVEL_ERROR, "QUP mem alloc fail size:%d type:%d",size,type);

   }
    return ptr;
}

/* This function should be called holding qup_mutex_global_lock() from upper layer */
void qup_mem_free (void *ptr, uint32 size, const se_dev_cfg *cfg, qup_mem_alloc_type type)
{

    if(!ptr)
    {
        QUP_LOG(LEVEL_ERROR,"qup_mem_free NULL pointer passed");
        return;
    }

    if(GET_QUP_MAJOR(cfg->qup_data->qup_id) != SSC_QUP)
    {
#ifdef QUP_AXI_HW_BUG
        if(cfg->qup_data->qup_id == QUP_1 && qup_get_chip_version() == CHIP_VERSION_R1 &&
           ((type == TX_TRANSFER_ELEM_TYPE) || (type == RX_TRANSFER_ELEM_TYPE) || (type == EVR_ELEM_TYPE)))
        {
            qup_smem_free(ptr);
            return;
        }

#endif
#ifdef QUP_DEVICE_HEAP_MGR_ENABLE
        switch(type)
        {
            case TX_TRANSFER_ELEM_TYPE:
            case RX_TRANSFER_ELEM_TYPE:
            case EVR_ELEM_TYPE:
            case I3C_BUFFER_TYPE:
            {
                ptr = device_heap_mgr_heap_free(size);
                break;
            }

            case I3C_IBI_CTXT_TYPE:
            {
                qurt_free(((ibi_core_ctxt *)ptr)->irq_lock);
                qurt_free(ptr);
                break;
            }

            default:
            {
                qurt_free(ptr);
                break;
            }
        }
#else
        if(type == I3C_IBI_CTXT_TYPE)
        {
            qurt_free(((ibi_core_ctxt *)ptr)->irq_lock);
            qurt_free(ptr);
        }
        else
        {
            qurt_free(ptr);
        }
#endif
    }
    else
    {
        switch(type)
        {
            case TX_TRANSFER_ELEM_TYPE:
            case RX_TRANSFER_ELEM_TYPE:
            case I3C_BUFFER_TYPE:
                qup_common_pram_tre_free(ptr);
                break;

            case IO_CTXT_TYPE:
                if(cfg->island_spec_in_use == ISLAND_CONFIG_INVALID)
                {
                    qurt_free(ptr);
                }
                else
                {
                    ((iface_io *)ptr)->used = FALSE;
                }
                break;

            case I2C_DESC_TYPE:
            case I2C_CONFIG_TYPE:
            case QUP_TRE_LIST_ISLAND:
            case SPI_DESC_TYPE:
            case SPI_CONFIG_TYPE:
            case QUP_CONFIG_DESC_PAIRS:
            case CLIENT_CTXT_TYPE:

                if(cfg->island_spec_in_use == ISLAND_CONFIG_INVALID)
                {
                    qurt_free(ptr);
                }
                else
                {
                    qup_island_mem_free(ptr, size);
                }
                break;

            default:

                    qup_island_mem_free(ptr, size);

        }
    }

    QUP_LOG(LEVEL_VERBOSE, "free ptr 0x%08x size %d", ptr, size);
    return;
}
