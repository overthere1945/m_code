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

==================================================================================================*/
/*==================================================================================================
Edit History

$Header: //components/rel/core.qdsp6/8.2.3/buses/qup_common/src/TOP_LEVEL/qup_common.c#1 $

when       who     what, where, why
--------   ---     --------------------------------------------------------
09/03/24   MG      Corrected Protocol ver type 
08/08/20   JC      Add support for both top & ssc qup 
03/17/20   jc      Added support to read FW loaded
01/16/19   BNG     Added platform detection
11/10/18   BNG     updated MAX_SES
01/15/18   PR      KW Fixes
08/23/17   UNR     Fixed island mode access to qup hw version
08/16/17   UNR     Added qup version API
06/01/17   VV      Initial version
==================================================================================================*/

/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/
#include "qup_common.h"
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
#include "qup_hal.h"


/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/
#define IS_ODD(val)  (val % 2)

/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/

/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/

extern   uint32           hw_version ;

/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/
void qup_common_init(void)
{
   qup_plat_init();
   qup_os_init();
   qup_config_init();
   
}

void qup_common_pram_tre_free(void* mem_addr)
{
}

void* qup_common_pram_tre_malloc(SE_PROTOCOL protocol, uint32 size)
{
    return NULL;
}

SE_PROTOCOL qup_common_get_fw_loaded (void *se_cfg)
{
    se_dev_cfg *cfg = (se_dev_cfg *)se_cfg;;
    uint8 *se_base = NULL;
    SE_PROTOCOL  se_proto_present = SE_PROTOCOL_INVALID;
    uint8  ver = 0;
    
    if(cfg)
    {
        qup_clock_enable(cfg);
        
        se_base = GET_SE_BASE_ADDR(cfg); 
        
        se_proto_present =  HWIO_INXF(GENI_CFG_BASE(se_base),GENI_FW_REVISION_RO,PROTOCOL);
        if(se_proto_present == SE_PROTOCOL_I3C)
        {
            se_proto_present =  SE_PROTOCOL_I3C_IBI;
        }
              
        if(!hw_version)
        {
            hw_version = HWIO_INX(cfg->qup_data->common_base_addr ,QUPV3_HW_VERSION);
        }
        
        /* To support dual I2C Charger UEFI usecases
           Change Instance to GSI mode post UEFI access*/
        if((se_proto_present == SE_PROTOCOL_I2C) && (HWIO_INXF(GENI_CFG_BASE(se_base),FIFO_IF_DISABLE_RO,FIFO_IF_DISABLE) == 0))
        {
            HWIO_OUTXF(GENI_IMAGE_REGS_BASE(se_base),GENI_DMA_MODE_EN,GENI_DMA_MODE_EN,0x1);
            HWIO_OUTX(GENI_SE_DMA_BASE(se_base),SE_IRQ_EN,0x0);
            HWIO_OUTX(GENI_SE_DMA_BASE(se_base),SE_GSI_EVENT_EN,HWIO_RMSK(SE_GSI_EVENT_EN));
        }
        
        qup_clock_disable(cfg);
    }
    
    return se_proto_present;
}

uint16 qup_common_get_island_spec (void *se_cfg)
{
 	return 0;
}
