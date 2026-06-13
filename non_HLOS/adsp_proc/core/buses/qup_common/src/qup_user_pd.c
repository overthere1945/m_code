/**
    @file  qup_user_pd.c
    @brief qup_user_pd_feature_enabled implementation
 */
/*=============================================================================
            Copyright (c)  Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/


/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/


#include <stdio.h>
#include <stdint.h>
#include "DTBExtnLib.h"
#include "qup_common_internal.h"
// to get blob handle
#include "qurt_devtree.h" 

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/

/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/


/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/


user_pd_features qup_user_pd_feature = {.user_pd_island_enabled = 0};

boolean user_dtb_read  = FALSE;

user_pd_features* qup_get_user_pd_features(void)
{
    int ret_value = -1;
    int blob_id = -1;
    
    const void* blob = NULL ;
    fdt_node_handle hNode;

    
    /* Returns the Blob ID for the Blob type passed.
    The value returned from this API can be passed as Blob ID parameter to DTBExtnLib APIs */
    if (!user_dtb_read)
    {
        blob_id = qurt_dt_blob_id_get(QURT_DT_BLOB_TYPE_LOCAL);
        if (blob_id !=  INVALID_BLOB_ID)
        {
          /* grab blob handle */
            ret_value = fdt_get_blob_handle(&blob, blob_id);
            if( FDT_ERR_QC_NOERROR == ret_value)
            {
                ret_value = fdt_get_node_handle(&hNode, blob, "/sw/qup_user_pd_feature");
                if(ret_value ==  FDT_ERR_QC_NOERROR)
                {
                    ret_value = fdt_get_uint8_prop(&hNode, "user_pd_island_enabled", (uint8 *)&(qup_user_pd_feature.user_pd_island_enabled));
                    if(ret_value ==  FDT_ERR_QC_NOERROR)
                    {
                        user_dtb_read = TRUE;
                    }
                }
            }
        }
    }    
    return (&qup_user_pd_feature);
}