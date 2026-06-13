#ifndef QUP_COMMON_H
#define QUP_COMMON_H
/*==================================================================================================

FILE: qup_common.h

DESCRIPTION: This module provides the common features for the QUPv3 protocols ( I2C/I3C/SPI/UART )

Copyright (c) Qualcomm Technologies, Inc.
        All Rights Reserved.
Qualcomm Technologies, Inc. Confidential and Proprietary.

==================================================================================================*/
/*==================================================================================================
                                            DESCRIPTION
====================================================================================================

GLOBAL FUNCTIONS:
   qup_common_pram_tre_malloc
   qup_common_pram_tre_free

==================================================================================================*/
/*==================================================================================================
Edit History

@version 2 
$Header: //components/rel/core.qdsp6/8.2.3/api/common/buses/qup_common.h#1 $

when       who  Version   what, where, why
--------   ---            --------------------------------------------------------
10/01/24   KSN     2      Updated changes for SPI_3W support
05/29/24   GKR     1      Added Support for New TOP & SSC QUP Wrappers, Added QUP_COMMON_H_VERSION
10/03/23   pcr     0      Added power levels
03/02/22   GKR     0      Added UFCS Protocols
08/30/21   BN      0      Added I3C IBI in SE protocols
03/09/21   JC      0      Added QUP Type Enumeration
08/16/17   UNR     0      Added qup version API
06/01/17   VV      0      Initial version
==================================================================================================*/
#define QUP_COMMON_H_VERSION 2

/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/
#include "com_dtypes.h"

/*==================================================================================================
                                           PRE-PROCESSOR DEFINES
==================================================================================================*/

#define SET_QUP_TYPE(qup_major,instance_number)      (((qup_major & 0xF) << 4) | (instance_number & 0xF))
#define GET_QUP_MAJOR(qup_type)                      ((qup_type & 0xF0) >> 4)
#define GET_QUP_MINOR(qup_type)                      (qup_type & 0xF)

/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/

typedef enum
{
   QUP_V1_0 = 0x10000000,
   QUP_V1_1 = 0x10010000,
   QUP_V1_2 = 0x10020000,
   QUP_V3_8 = 0x30080000,
}QUP_VERSION;

typedef enum
{
   SE_PROTOCOL_SPI      = 1,
   SE_PROTOCOL_UART     = 2,
   SE_PROTOCOL_I2C      = 3,
   SE_PROTOCOL_I3C      = 4,
   SE_PROTOCOL_UFCS     = 0xD,
   SE_PROTOCOL_I3C_IBI  = 0x104,
   SE_PROTOCOL_SPI_3W   = 0x10,
   SE_PROTOCOL_SPI_4W   = 0x110,
}SE_PROTOCOL;

typedef enum
{
   TOP_QUP       = 0,
   SSC_QUP       = 1,
   I2C_HUB       = 2,
   QUP_MAJOR_MAX
}QUP_MAJOR_TYPE;

typedef enum
{
   QUP_0         = SET_QUP_TYPE(TOP_QUP,0),
   QUP_1         = SET_QUP_TYPE(TOP_QUP,1),
   QUP_2         = SET_QUP_TYPE(TOP_QUP,2),
   QUP_3         = SET_QUP_TYPE(TOP_QUP,3),
   QUP_4         = SET_QUP_TYPE(TOP_QUP,4),
   QUP_N,
   TOP_QUP_MAX   = GET_QUP_MINOR(QUP_N),
   
   QUP_SSC       = SET_QUP_TYPE(SSC_QUP,0),
   QUP_SSC_0     = QUP_SSC,
   QUP_SSC_1     = SET_QUP_TYPE(SSC_QUP,1),
   QUP_SSC_N,
   SSC_QUP_MAX   = GET_QUP_MINOR(QUP_SSC_N),
   
   QUP_I2C_HUB   = SET_QUP_TYPE(I2C_HUB,0),
   QUP_I2C_HUB_0 = QUP_I2C_HUB,
   QUP_I2C_HUB_N,
   I2C_HUB_MAX   = GET_QUP_MINOR(QUP_I2C_HUB_N),
   
   QUP_TYPE_MAX,
}QUP_TYPE;

/**
  Enum to indicate the type of power profile, which is passed in API i2c_set_power_level.

  Client can pass on the profile based on need as explained below.
  QUP_POWER_OFF      - when client wishes to disable i2c instance post use case completetion
  QUP_POWER_LOW      - when client wishes to run the use case in low power/svs
  QUP_POWER_ON       - when client wishes enable the use case and run in nominal mode
  QUP_POWER_NOMINAL  - same as QUP_POWER_ON
  QUP_POWER_TURBO    - when client wishes to run the use case with better KPIs
  
*/
typedef enum qup_power_profile
{
    QUP_POWER_OFF       = 1,
    QUP_POWER_LOW       = 5,
    QUP_POWER_ON        = QUP_POWER_LOW,
    QUP_POWER_NOMINAL   = 10,
    QUP_POWER_TURBO     = 15,
}qup_power_profile;

/*==================================================================================================
                                        FUNCTION PROTOTYPES
==================================================================================================*/

#endif
