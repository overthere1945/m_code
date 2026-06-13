/** 
    @file  qup_sdc_handler.h
    @brief Qup SDC Handler
 */

/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
06/26/24   GKR     Added Data type to store GPIIs mapped to SDC per SSC QUP
*/
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __QUP_SDC_HANDLER_H__
#define __QUP_SDC_HANDLER_H__

/* *************************************************************** */
/*                          INCLUDE FILES                          */
/* *************************************************************** */

#include "comdef.h"
#include "qurt.h"
#include "qurt_error.h"
#include "qurt_qdi_driver.h"

#define   QUP_SDC_DRIVER_NAME  "qup_sdc_handler"

#define   MAX_SDC_GPII_PER_QUP  10
/* *************************************************************** */
/*                         DATA STRUCTURES                         */
/* *************************************************************** */

typedef struct 
{
    qurt_qdi_obj_t  obj;
    boolean         in_use;
}qup_sdc_object;

typedef struct qup_sdc_cfg_
{
    uint8     sdc_gpii_list[MAX_SDC_GPII_PER_QUP];
    uint8     num_gpii;
}qup_sdc_cfg_t;


/* *************************************************************** */
/*                            FUNCTIONS                            */
/* *************************************************************** */



#endif
