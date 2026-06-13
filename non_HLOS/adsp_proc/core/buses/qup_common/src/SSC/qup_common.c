/*==================================================================================================

FILE: qup_common.c

DESCRIPTION: This module provides the common features for the QUPv3 protocols ( I2C/I3C/SPI/UART )

Copyright (c) Qualcomm Technologies, Inc.
        All Rights Reserved.
Qualcomm Technologies, Inc. Confidential and Proprietary.

==================================================================================================*/
/*==================================================================================================
                                            DESCRIPTION
====================================================================================================

GLOBAL FUNCTIONS:
   qup_common_init
   qup_common_pram_tre_malloc
   qup_common_pram_tre_free
   qup_common_get_fw_loaded

==================================================================================================*/
/*==================================================================================================
Edit History

$Header: //components/rel/core.qdsp6/8.2.3/buses/qup_common/src/SSC/qup_common.c#1 $

when       who     what, where, why
--------   ---     --------------------------------------------------------
10/15/25   GKR     Added changes to support RUMI
07/02/25   SS      Added i3c_spawn_threads init support.
07/02/25   SS      KW fixes.
10/01/24   MG      Corrected FW protocol data type
06/26/24   gkr     Changes to support multiple SSC QUPS
07/30/23   bng     Added changes to support variable ring sizes and multiple GPIIs per SE
04/21/22   PS      Increased TRE Memory for UART
11/20/21   BNG     Added IBI Controller specific changes
06/08/21   JC      Move Initilations before platform detection.
02/09/21   JC      updated MAX_SSC_SE_INSTANCES to 9
08/08/20   JC      Add support for both top & ssc qup
03/17/20   jc      Added support transfer buffer allocation and  fw_load api
01/16/19   BNG     Added platform detection
11/10/18   BNG     updated MAX_SSC_SE_INSTANCES
01/15/18   PR      KW Fixes
08/23/17   UNR     Fixed island mode access to qup hw version
08/16/17   UNR     Added qup version API
06/01/17   VV      Initial version
==================================================================================================*/

/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/
#include "qup_common_internal.h"
#include "pram_mgr.h"
#include <string.h>
#include "fw_config.h"
#include "fw_devcfg.h"
#include "DALDeviceId.h"
#include "DDIPlatformInfo.h"
#include "DALSys.h"
#include "err.h"
#include "qurt_printf.h"
#include "qup_log.h"
#include "qup_plat.h"
#include "qup_os.h"
#include "qup_drv.h"
#include "qup_devcfg.h"
#include "qup_hal.h"
#include <stringl/stringl.h>
#include "DTBExtnLib.h"
#include "i3c_thread.h"

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/
#define TRE_SIZE 16 // 16 bytes

#define IS_ODD(val)  (val % 2)

#define SSC_FW_DT_NAME "/soc/ssc_qup_fw_cfg"
/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/
typedef struct
{
   uint32               addr;
   boolean              used;
   QUP_PROTOCOL_INDEX   protocol;
}TRE_MEM;

typedef struct
{
   TRE_MEM tre_mem[MAX_SSC_QUP_CORES * 2];
   uint8 tres_available[QUP_PROTOCOL_MAX];
}TRE_POOL;

typedef struct
{
   uint32        base_addr;
   uint32        start_addr;
   uint32        availble_size;
}BUFF_POOL;


/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/
static TRE_POOL       tre_pool_512_bytes_aligned; // 32 TREs
static TRE_POOL       tre_pool_256_bytes_aligned; // 16 TREs
static TRE_POOL       tre_pool_128_bytes_aligned; //  8 TRES
static TRE_POOL       tre_pool_64_bytes_aligned;  //  4 TREs
static BUFF_POOL      pram_buff_available;
static qurt_mutex_t   qup_common_mutex;
static uint32         buses_pram_base;
static uint8          num_gsi_ses[PROTOCOL_MAJOR_MAX];
static uint8          num_512_byte_ses[PROTOCOL_MAJOR_MAX];
static uint8          num_256_byte_ses[PROTOCOL_MAJOR_MAX];
static uint8          num_128_byte_ses[PROTOCOL_MAJOR_MAX];
static uint16         se_proto_present[NUM_SSC_QUP][MAX_SE_PER_SSC_QUP];
static uint16         se_island_present[NUM_SSC_QUP][MAX_SE_PER_SSC_QUP];
static boolean        buses_pram_allocated;

/*==================================================================================================
                                          EXTERN VARIABLES
==================================================================================================*/
extern uint32         hw_version;
extern uint8          qup_num_se[];
/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/
void qup_common_init(void)
{
    char   dt_handle_name[50];
    char   *platform_cfg = "cfg";
    se_cfg  *scfg = NULL;
    uint8  i = 0, j = 0;
    uint32  pram_size;
    uint32  num_tres;
    uint32  req_size;
    uint32  addr;
    se_cfg se_fw_config= {0};
    fdt_node_handle node;
    uint32   size = 0;
    int ret_value = -1;
    boolean  is_rumi = FALSE;

    qurt_mutex_init (&qup_common_mutex);

    buses_pram_base = (uint32) pram_acquire_partition("BUSES", &pram_size);

    //Execute internal Initialisations
    qup_plat_init();
    qup_os_init();
    qup_config_init();

    /* This must be assigned after qup_plat_init() only*/
    is_rumi = qup_detect_rumi_platform();

    if (is_rumi)
    {
       platform_cfg = "cfg_rumi";
    }

    if(I2C_SUCCESS != i3c_spawn_threads())
    {
       ERR_FATAL("Buses i3c_spawn_threads failed",0,0,0);
    }

    for ( i = 0; i < NUM_SSC_QUP; i++)
    {
        for (j = 0; j < qup_num_se[qup_idx_offset[SSC_QUP]+i]; j++)
        {
            ret_value = snprintf(dt_handle_name, sizeof(dt_handle_name), "%s/ssc_qup_%d/se%d_%s",SSC_FW_DT_NAME, i, j, platform_cfg);
            if (ret_value < 0 || ret_value >= sizeof(dt_handle_name)) {
                QUP_LOG(LEVEL_ERROR,"ssc qup_common_init - snprintf failed for ssc_qup_%d/se%d\n", i, j);
                goto ON_EXIT;
            }

            ret_value = fdt_get_node_handle(&node, NULL, dt_handle_name);
            if (ret_value)
            {
                /* if DT node fetch failed try to read default handle if it is RUMI */
                if (is_rumi)
                {
                    ret_value = snprintf(dt_handle_name, sizeof(dt_handle_name), "%s/ssc_qup_%d/se%d_%s",SSC_FW_DT_NAME, i, j, "cfg");
                    if (ret_value < 0 || ret_value >= sizeof(dt_handle_name)) {
                        QUP_LOG(LEVEL_ERROR,"ssc qup_common_init - snprintf failed for ssc_qup_%d/se%d (rumi fallback)\n", i, j);
                        goto ON_EXIT;
                    }
                    ret_value = fdt_get_node_handle(&node, NULL, dt_handle_name);
                }

                if (ret_value)
                {
                    QUP_LOG(LEVEL_ERROR,"ssc qup_common_init - ERR : %d-fdt_get_node_handle \n",ret_value);
                    goto ON_EXIT;
                }
            }

            ret_value = fdt_get_prop_values_size_of_node(&node, &size);
            if (ret_value)
            {
                QUP_LOG(LEVEL_ERROR,"ssc qup_common_init - ERR : %d-fdt_get_prop_values_size_of_node \n",ret_value);
                goto ON_EXIT;
            }

            ret_value = fdt_get_prop_values_of_node(&node, "wwwhbbbbb", (void *)&se_fw_config, size);
            if (ret_value)
            {
                QUP_LOG(LEVEL_ERROR,"ssc qup_common_init - ERR : %d-fdt_get_prop_values_of_node \n",ret_value);
                goto ON_EXIT;
            }

            scfg = (se_cfg *)&se_fw_config;

            se_proto_present[i][j] = scfg->protocol;
            se_island_present[i][j] = scfg->se_island_config;

            if(scfg->mode == GSI)
            {
               num_gsi_ses[GET_PROTOCOL_MAJOR(scfg->protocol)]++;
            }
            else
            {
                continue; // No need to reserve PRAM for for NON GSI SE
            }

            switch(scfg->tre_list_size)
            {
              case NUM_TRE_ELEM_32:
                      num_512_byte_ses[GET_PROTOCOL_MAJOR(scfg->protocol)]++; //TX
                      num_256_byte_ses[GET_PROTOCOL_MAJOR(scfg->protocol)]++; //RX
                      break;
              case NUM_TRE_ELEM_16:
                      num_256_byte_ses[GET_PROTOCOL_MAJOR(scfg->protocol)]++; //TX
                      num_128_byte_ses[GET_PROTOCOL_MAJOR(scfg->protocol)]++; //RX
                      break;
              case NUM_TRE_ELEM_8:
                      num_128_byte_ses[GET_PROTOCOL_MAJOR(scfg->protocol)]++; //TX
                      num_128_byte_ses[GET_PROTOCOL_MAJOR(scfg->protocol)]++; //RX
                      break;
            }
        }
    }
   // SPI/I2C/I3C uses 16 TX TRES and 8 RX TREs
   // UART uses 4 TX TREs and 4 RX TREs
   // req_size = total tres * 16 bytes;
   /*
   req_size = ((((num_gsi_ses[SE_PROTOCOL_SPI] +
                  num_gsi_ses[SE_PROTOCOL_I2C] +
                  num_gsi_ses[SE_PROTOCOL_I3C]) * (16 + 8)) +
                  (num_gsi_ses[SE_PROTOCOL_UART] * (16 + 8))) * TRE_SIZE) +
                  (num_gsi_ses[SE_PROTOCOL_I3C]  * 8); //i3c ccc Buff_SIZE*/

   req_size = ((num_512_byte_ses[SE_PROTOCOL_SPI] + num_512_byte_ses[SE_PROTOCOL_I2C] +  num_512_byte_ses[SE_PROTOCOL_I3C] + num_512_byte_ses[SE_PROTOCOL_UART] + num_512_byte_ses[SE_PROTOCOL_SPI_3W]) *  32 +
              (num_256_byte_ses[SE_PROTOCOL_SPI] + num_256_byte_ses[SE_PROTOCOL_I2C] +  num_256_byte_ses[SE_PROTOCOL_I3C] + num_256_byte_ses[SE_PROTOCOL_UART]  + num_256_byte_ses[SE_PROTOCOL_SPI_3W] ) *  16 +
              (num_128_byte_ses[SE_PROTOCOL_SPI] + num_128_byte_ses[SE_PROTOCOL_I2C] +  num_128_byte_ses[SE_PROTOCOL_I3C] + num_128_byte_ses[SE_PROTOCOL_UART]  + num_128_byte_ses[SE_PROTOCOL_SPI_3W] ) *  8 ) * TRE_SIZE +
              (num_gsi_ses[SE_PROTOCOL_I3C]  * 8); //i3c ccc Buff_SIZE


   if (req_size > pram_size)
   {
      ERR_FATAL("Not enough PRAM memory for QUP TRE allocations.",0,0,0);
   }

   // TODO: Make changes to work with different aligned base address.
   // For now it assumes we always get minimum of 256 byte aligned

   if (buses_pram_base & 0xFF)
   {
      ERR_FATAL("Buses PRAM base not aligned to 256 bytes",0,0,0);
   }

   // 32 TRE memory
   addr = buses_pram_base;
   //num_tres = (num_gsi_ses[SE_PROTOCOL_SPI] + num_gsi_ses[SE_PROTOCOL_I2C] + num_gsi_ses[SE_PROTOCOL_I3C] + num_gsi_ses[SE_PROTOCOL_UART]);

   num_tres = num_512_byte_ses[SE_PROTOCOL_SPI] + num_512_byte_ses[SE_PROTOCOL_I2C] +  num_512_byte_ses[SE_PROTOCOL_I3C] + num_512_byte_ses[SE_PROTOCOL_UART] + num_512_byte_ses[SE_PROTOCOL_SPI_3W];

   for (i = 0; i < num_tres; i++)
   {
      tre_pool_512_bytes_aligned.tre_mem[i].addr = addr;
      addr = addr + 512;
   }
   tre_pool_512_bytes_aligned.tres_available[SE_PROTOCOL_SPI] = num_512_byte_ses[SE_PROTOCOL_SPI];
   tre_pool_512_bytes_aligned.tres_available[SE_PROTOCOL_I2C] = num_512_byte_ses[SE_PROTOCOL_I2C];
   tre_pool_512_bytes_aligned.tres_available[SE_PROTOCOL_I3C] = num_512_byte_ses[SE_PROTOCOL_I3C];
   tre_pool_512_bytes_aligned.tres_available[SE_PROTOCOL_UART] = num_512_byte_ses[SE_PROTOCOL_UART];
   tre_pool_512_bytes_aligned.tres_available[SE_PROTOCOL_SPI_3W] = num_512_byte_ses[SE_PROTOCOL_SPI_3W];

   //16 TRE memory
   num_tres = num_256_byte_ses[SE_PROTOCOL_SPI] + num_256_byte_ses[SE_PROTOCOL_I2C] +  num_256_byte_ses[SE_PROTOCOL_I3C] + num_256_byte_ses[SE_PROTOCOL_UART] + num_256_byte_ses [SE_PROTOCOL_SPI_3W];
   for (i = 0; i < num_tres; i++)
   {
      tre_pool_256_bytes_aligned.tre_mem[i].addr = addr;
      addr = addr + 256;
   }
   tre_pool_256_bytes_aligned.tres_available[SE_PROTOCOL_SPI] = num_256_byte_ses[SE_PROTOCOL_SPI];
   tre_pool_256_bytes_aligned.tres_available[SE_PROTOCOL_I2C] = num_256_byte_ses[SE_PROTOCOL_I2C];
   tre_pool_256_bytes_aligned.tres_available[SE_PROTOCOL_I3C] = num_256_byte_ses[SE_PROTOCOL_I3C];
   tre_pool_256_bytes_aligned.tres_available[SE_PROTOCOL_UART] = num_256_byte_ses[SE_PROTOCOL_UART];
   tre_pool_256_bytes_aligned.tres_available[SE_PROTOCOL_SPI_3W] = num_256_byte_ses[SE_PROTOCOL_SPI_3W];

   // 8 TRE memory
   num_tres = num_128_byte_ses[SE_PROTOCOL_SPI] + num_128_byte_ses[SE_PROTOCOL_I2C] +  num_128_byte_ses[SE_PROTOCOL_I3C] + num_128_byte_ses[SE_PROTOCOL_UART] + num_128_byte_ses[SE_PROTOCOL_SPI_3W];
   for (i = 0; i < num_tres; i++)
   {
      tre_pool_128_bytes_aligned.tre_mem[i].addr = addr;
      addr = addr + 128;
   }
   tre_pool_128_bytes_aligned.tres_available[SE_PROTOCOL_SPI] = num_128_byte_ses[SE_PROTOCOL_SPI];
   tre_pool_128_bytes_aligned.tres_available[SE_PROTOCOL_I2C] = num_128_byte_ses[SE_PROTOCOL_I2C];
   tre_pool_128_bytes_aligned.tres_available[SE_PROTOCOL_I3C] = num_128_byte_ses[SE_PROTOCOL_I3C];
   tre_pool_128_bytes_aligned.tres_available[SE_PROTOCOL_UART] = num_128_byte_ses[SE_PROTOCOL_UART];
   tre_pool_128_bytes_aligned.tres_available[SE_PROTOCOL_SPI_3W] = num_128_byte_ses[SE_PROTOCOL_SPI_3W];

   // 4 TRE memory
   //num_tres = num_gsi_ses[SE_PROTOCOL_UART] * 2;
   //for (i = 0; i < num_tres; i++)
   //{
   //   tre_pool_64_bytes_aligned.tre_mem[i].addr = addr;
   //   addr = addr + 64;
   //}
   //tre_pool_64_bytes_aligned.tres_available[SE_PROTOCOL_UART] = num_gsi_ses[SE_PROTOCOL_UART] * 2;

   //store pram buff
   pram_buff_available.start_addr = addr;
   pram_buff_available.base_addr = addr;
   pram_buff_available.availble_size = (addr - buses_pram_base);

   // initialize hw_version by calling qup_fw api (non-island code)
   hw_version = qup_hw_version();

   buses_pram_allocated = TRUE;

ON_EXIT:
   if (ret_value)
   {
        qurt_mutex_destroy(&qup_common_mutex);
        QUP_LOG(LEVEL_ERROR,"ssc qup_common_init - ERR %d- failed to read DTB!\n",ret_value);
        return;
   }
}

void qup_common_pram_tre_free(void* mem_addr)
{
   uint32 i;
   TRE_POOL* tre_pool = NULL;
   uint32 addr = (uint32)mem_addr;
   boolean free_successful = FALSE;

   if (!buses_pram_allocated)
   {
      ERR_FATAL("Calling Buses TRE memory free before its initialization.", 0, 0, 0);
   }

   if (addr == 0)
   {
      ERR_FATAL("QUP TRE mem free failed. Freeing address 0x00.", 0, 0, 0);
   }
   else if(addr >= pram_buff_available.base_addr)
   {
      ERR_FATAL("QUP TRE buffer mem free not allowed. Freeing address 0x00.", 0, 0, 0);
   }

   if ((tre_pool_64_bytes_aligned.tre_mem[0].addr) && (addr >= tre_pool_64_bytes_aligned.tre_mem[0].addr))
   {
      tre_pool = &tre_pool_64_bytes_aligned;
   }
   else if ((tre_pool_128_bytes_aligned.tre_mem[0].addr) && (addr >= tre_pool_128_bytes_aligned.tre_mem[0].addr))
   {
      tre_pool = &tre_pool_128_bytes_aligned;
   }
   else if ((tre_pool_256_bytes_aligned.tre_mem[0].addr) && (addr >= tre_pool_256_bytes_aligned.tre_mem[0].addr))
   {
      tre_pool = &tre_pool_256_bytes_aligned;
   }
    else if ((tre_pool_512_bytes_aligned.tre_mem[0].addr) && (addr >= tre_pool_512_bytes_aligned.tre_mem[0].addr))
   {
      tre_pool = &tre_pool_512_bytes_aligned;
   }
   if(tre_pool == NULL)
   {
      ERR_FATAL("QUP tre_pool mem refer Null Memory pointer",0,0,0);
      return;
   }

   qurt_mutex_lock(&qup_common_mutex);

   for (i = 0; i < MAX_SSC_QUP_CORES * 2; i++)
   {
      if (tre_pool->tre_mem[i].addr == addr)
      {
         if (!tre_pool->tre_mem[i].used)
         {
            ERR_FATAL("QUP TRE mem free failed. Freeing unused QUP TRE memory. Addr: 0x%08x",
                      addr,
                      0,
                      0);
         }
         tre_pool->tre_mem[i].used = FALSE;
         tre_pool->tres_available[tre_pool->tre_mem[i].protocol]++;
         tre_pool->tre_mem[i].protocol = 0;
         free_successful = TRUE;
         break;
      }
   }

   qurt_mutex_unlock(&qup_common_mutex);

   if (!free_successful)
   {
      ERR_FATAL("QUP TRE mem free failed. Invalid TRE memory. Addr: 0x%08x", addr, 0, 0);
   }

}

void* qup_common_pram_tre_malloc(SE_PROTOCOL protocol, uint32 size)
{
   uint32     i;
   TRE_POOL  *tre_pool;
   void      *ptr;
   uint8      index=0xff;
   QUP_PROTOCOL_INDEX protocol_idx = GET_PROTOCOL_MAJOR(protocol);

   if (!buses_pram_allocated)
   {
      ERR_FATAL("Calling Buses TRE memory free before its initialization.", 0, 0, 0);
   }

   switch (size)
   {
      case 512:
         tre_pool = &tre_pool_512_bytes_aligned;
         break;

      case 256:
         tre_pool = &tre_pool_256_bytes_aligned;
         break;

      case 128:
         tre_pool = &tre_pool_128_bytes_aligned;
         break;

      case 64:
         tre_pool = &tre_pool_64_bytes_aligned;
         break;
      default:
        if(pram_buff_available.availble_size < size)
            ERR_FATAL("QUP TRE alloc failed. No BUffer Space. Protocol: %d, Size: %d",
                    protocol,
                    size,
                    0);
        qurt_mutex_lock(&qup_common_mutex);
        ptr = (void *)pram_buff_available.start_addr;
        pram_buff_available.start_addr += size;
        pram_buff_available.availble_size -= size;
        qurt_mutex_unlock(&qup_common_mutex);
        return ptr;
   }

   qurt_mutex_lock(&qup_common_mutex);

   if (tre_pool->tres_available[protocol_idx] == 0)
   {
      ERR_FATAL("QUP TRE alloc failed. Protocol used all its allocations. Protocol: %d, Size: %d",
                protocol,
                size,
                0);
   }

   for (i = 0; i < MAX_SSC_QUP_CORES * 2; i++)
   {
      if ((tre_pool->tre_mem[i].addr != 0) && (!tre_pool->tre_mem[i].used))
      {
            index = i;
            break;
      }
   }

   if (index == 0xFF)
   {
      ERR_FATAL("QUP TRE alloc failed. No available memory in pool. Protocol: %d, Size: %d",
                protocol,
                size,
                0);
   }

   tre_pool->tres_available[protocol_idx]--;
   tre_pool->tre_mem[index].used = TRUE;
   tre_pool->tre_mem[index].protocol = protocol_idx;

   qurt_mutex_unlock(&qup_common_mutex);

   return (void*)(tre_pool->tre_mem[index].addr);

}

SE_PROTOCOL qup_common_get_fw_loaded (void *se_cfg)
{
    se_dev_cfg *cfg = (se_dev_cfg *)se_cfg;
    uint8 *se_base = NULL;
    SE_PROTOCOL  se_protocol= SE_PROTOCOL_INVALID;

    if(cfg)
    {
        switch(GET_QUP_MAJOR(cfg->qup_data->qup_id))
        {
            case SSC_QUP :
                se_protocol =  se_proto_present[GET_QUP_MINOR(cfg->qup_data->qup_id)][cfg->se_id];
                break;
            default :
                qup_clock_enable(cfg);

                se_base = cfg->se_base_addr;

                se_protocol =  HWIO_INXF(se_base,GENI_FW_REVISION_RO,PROTOCOL);

                if(se_protocol == SE_PROTOCOL_I3C)
                {
                    se_protocol = SE_PROTOCOL_I3C_IBI;
                }

                if(!hw_version)
                {
                    hw_version = HWIO_INX(cfg->qup_data->common_base_addr ,QUPV3_HW_VERSION);
                }

                /* To support dual I2C Charger UEFI usecases
                Change Instance to GSI mode post UEFI access*/
                if((se_protocol == SE_PROTOCOL_I2C) && (HWIO_INXF(GENI_CFG_BASE(se_base),FIFO_IF_DISABLE_RO,FIFO_IF_DISABLE) == 0))
                {
                    HWIO_OUTXF(GENI_IMAGE_REGS_BASE(se_base),GENI_DMA_MODE_EN,GENI_DMA_MODE_EN,0x1);
                    HWIO_OUTX(GENI_SE_DMA_BASE(se_base),SE_IRQ_EN,0x0);
                    HWIO_OUTX(GENI_SE_DMA_BASE(se_base),SE_GSI_EVENT_EN,HWIO_RMSK(SE_GSI_EVENT_EN));
                }

                qup_clock_disable(cfg);
        }
    }
    return se_protocol;
}

uint16 qup_common_get_island_spec (void *se_cfg)
{
    se_dev_cfg *cfg = (se_dev_cfg *)se_cfg;
    uint8 ssc_qup_id = 0;

    if(cfg && GET_QUP_MAJOR(cfg->qup_data->qup_id) == SSC_QUP)
    {
        ssc_qup_id = GET_QUP_MINOR(cfg->qup_data->qup_id);
        return se_island_present[ssc_qup_id][cfg->se_id];
    }

    return 0;
}
