/*==================================================================================================

FILE: qup_common.c

DESCRIPTION: This module provides the common features for the QUPv3 protocols ( I2C/I3C/SPI/UART )

Copyright (c) 2017, 2020, 2022 Qualcomm Technologies, Inc.
        All Rights Reserved.
Qualcomm Technologies, Inc. Confidential and Proprietary.

==================================================================================================*/
/*==================================================================================================
                                            DESCRIPTION
====================================================================================================

GLOBAL FUNCTIONS:
    qup_common_get_hw_version   

==================================================================================================*/
/*==================================================================================================
Edit History

$Header: //components/rel/core.qdsp6/8.2.3/buses/qup_common/src/qup_common_island.c#1 $

when       who     what, where, why
--------   ---     --------------------------------------------------------
08/26/22   MP      Fixed compilation errors caused due to updating the Hexagon toolchain (8.7-alpha3)
08/23/17   UNR     Fixed island mode access to qup hw version
08/16/17   UNR     Added qup version API
06/01/17   VV      Initial version
==================================================================================================*/
#include "qup_common.h"

/*==================================================================================================
                                         EXTERN VARIABLES
==================================================================================================*/
uint32  hw_version = 0;
/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/

uint32 qup_common_get_hw_version (void)
{
   return hw_version;
}
