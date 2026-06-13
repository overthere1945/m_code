/** 
    @file  qup_common_internal.h
    @brief Qup common internal implemention
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __QUP_COMMON_INTERNAL_H__
#define __QUP_COMMON_INTERNAL_H__

/* *************************************************************** */
/*                          INCLUDE FILES                          */
/* *************************************************************** */

#include "qup_common.h"
#include "qup_hal.h"

/* *************************************************************** */
/*                         MACRO DEFINITION                        */
/* *************************************************************** */

#define    SE_PROTOCOL_INVALID  0XFF

#define    MAX_FREQ_PLAN_TABLE_SIZE 9

#define    QUP_MAX    TOP_QUP_MAX + SSC_QUP_MAX + I2C_HUB_MAX

#define GET_PROTOCOL_MAJOR(protocol)    (protocol & 0xFF)
#define GET_PROTOCOL_MINOR(protocol)    ((protocol & 0xF00) >> 8)
#define PROTOCOL_MAJOR_MAX              0x11
#define PROTOCOL_MINOR_MAX              1

#define ISLAND_CONFIG_INVALID 0

typedef enum
{
   QUP_SPI_INDEX       = 1,
   QUP_UART_INDEX      = 2,
   QUP_I2C_INDEX       = 3,
   QUP_I3C_INDEX       = 4,
   QUP_I3C_IBI_INDEX   = 5,
   QUP_UFCS_INDEX      = 0xD,
   QUP_SPI_INDEX_3W    = 0x10,
   QUP_PROTOCOL_MAX    = 0x11
}QUP_PROTOCOL_INDEX;

typedef struct
{
    boolean user_pd_island_enabled ;
}user_pd_features;


static const char *QUP_name[QUP_MAJOR_MAX] = {"TOP","SSC","HUB"};

static const uint8  qup_minor_max[QUP_MAJOR_MAX] = {TOP_QUP_MAX, SSC_QUP_MAX, I2C_HUB_MAX};

static const uint8  qup_idx_offset[QUP_MAJOR_MAX ] = { 0,
                                                       TOP_QUP_MAX, 
                                                       TOP_QUP_MAX + SSC_QUP_MAX, 
                                                       // TOP_QUP_MAX + SSC_QUP_MAX + I2C_HUB_MAX,
                                                     };

/* *************************************************************** */
/*                            FUNCTIONS                            */
/* *************************************************************** */

/* Get Protocol loaded on particular SE*/
SE_PROTOCOL qup_common_get_fw_loaded (void *se_cfg);
uint16 qup_common_get_island_spec (void *se_cfg);

/*!
 * \brief Allocates memory that is aligned to the size from the buses segment of PRAM
 *
 * \param in SE_PROTOCOL protocol
 * \param in uint32 size
 * \return allocated memory in case of success and NULL in case of failure
 */

void* qup_common_pram_tre_malloc(SE_PROTOCOL protocol, uint32 size);

/*!
 * \brief Allocates memory that is aligned to the size from the buses segment of PRAM
 *
 * \param in void* addr - memory to be freed
 */
void qup_common_pram_tre_free(void* addr);

/*!
 * \brief Determines QUP core version
 *
 * \return QUP_VERSION
 */

uint32 qup_common_get_hw_version (void);

/*!
 * \brief Checks user features enabled from user device tree
 *
 * \return qup_user_pd_features structure pointer
 */
user_pd_features* qup_get_user_pd_features(void);
#endif