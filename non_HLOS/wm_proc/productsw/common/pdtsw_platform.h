/*================================================================================================
* @file pdtsw_platform.h
*
* This file contains the declarations for all the abstract APIs to interact with platform layer.
*
* Copyright (c) 2025 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
===============================================================================================*/

#ifndef __PDTSW_PLATFORM_H__
#define __PDTSW_PLATFORM_H__

#include <stdint.h>
#include "npa_resource.h"
/*-------------------------------------------------------------------------------------------------
 * Datatypes & Macro
 *-----------------------------------------------------------------------------------------------*/
#ifndef ISLAND_SUCCESS
#define ISLAND_SUCCESS 0
#endif

#ifndef ISLAND_ERROR
#define ISLAND_ERROR   -1
#endif

/* Enum for NPA resource types */
typedef enum
{
    PDTSW_NPA_RESOURCE_CORE_CPU = 0,        /* Core CPU clock resource */
    PDTSW_NPA_RESOURCE_PSRAM,               /* PSRAM resource */
    PDTSW_NPA_RESOURCE_DDR,                 /* DDR resource */
    PDTSW_NPA_NUM_RESOURCES                 /* Total number of NPA resources */
} pdtsw_npa_resource_t;

/* Enum for power modes */
typedef enum
{
    PDTSW_POWER_MODE_LOWSVS_D1_XO,          /**< XO clock */
    PDTSW_POWER_MODE_LOWSVS_D1,             /**< LOW SVS D1 */
    PDTSW_POWER_MODE_LOWSVS,                /**< LOW SVS */
    PDTSW_POWER_MODE_SVS,                   /**< SVS */
    PDTSW_POWER_MODE_SVS_L1,                /**< SVS L1 */
    PDTSW_POWER_MODE_NOM,                   /**< NOM */
    PDTSW_POWER_MODE_TURBO,                 /**< TURBO */
    PDTSW_NUM_POWER_MODES
} pdtsw_power_mode_t;

/* Enum for NPA client types */
typedef enum
{
    PDTSW_NPA_CLIENT_TYPE_NON_SUPPRESSIBLE = 0, /* Non-suppressible client */
    PDTSW_NPA_CLIENT_TYPE_SUPPRESSIBLE,         /* Suppressible client */
    PDTSW_NPA_NUM_CLIENT_TYPES                  /* Total number of client types */
} pdtsw_npa_client_type_t;

/*-----------------------------------------------------------------------------------------------*/

/**
 * @brief Issues a vote for a specific NPA resource and power mode
 *
 * Creates an NPA client and issues a scalar or vector 
 * request based on the client type.
 *
 * @param[in] resource The NPA resource to vote for
 * @param[in] power_mode The desired power mode
 * @param[in] client_type The type of NPA client
 * 
 * @return Pointer to the created NPA client handle, or NULL on failure.
 */
void* pdtsw_npa_resource_vote(
    pdtsw_npa_resource_t resource, 
    pdtsw_power_mode_t power_mode, 
    pdtsw_npa_client_type_t client_type);

/**
 * @brief Unvotes a previously voted NPA resource
 *
 * Destroys the NPA client associated with the given handle
 *
 * @param[in] p_client_handle Pointer to the NPA client handle
 * 
 * @return 0 on success, ENODEV on failure.
 */
int32_t pdtsw_npa_resource_unvote(void* p_client_handle);

/**
 * @brief Creates a client handle for interacting with the Island Util.
 *
 * This function must be called by any module that wishes to vote for or against
 * entering/exiting the island state. Each client should create its own unique handle.
 *
 * @param[in] client_name A NULL-terminated string identifier for the client.
 *
 * @return An `npa_client_handle` on success, or `NULL` if client creation failed.
 */
npa_client_handle pdtsw_island_create_client(const char * /* restrict */ client_name);

/**
 * @brief Votes to prevent the system from entering the island state.
 *
 * When this function is called, the client expresses a requirement for the system
 * to stay out of the island state (i.e., remain in a full-power or higher-power mode).
 * This vote *must* be balanced by a corresponding call to `pdtsw_island_vote_island_entry()`
 * when the requirement no longer exists. it is blocking call till getting cb
 * for island exit.
 *
 * @param[in] client The client handle obtained from `island_util_create_client()`.
 *                   Must be a valid, non-NULL handle.
 *
 * @return `ISLAND_UTIL_SUCCESS` (0) on successful vote, or `ISLAND_UTIL_ERROR` (-1)
 */
int pdtsw_island_vote_island_exit(npa_client_handle client);

/**
 * @brief Votes to allow the system to enter the island state.
 *
 * This function revokes a previous `pdtsw_island_vote_island_exit()` call.
 * When all clients that previously voted for island exit have called this function,
 * the system is free to enter the island state 
 *
 * @param[in] client The client handle obtained from `pdtsw_island_create_client()`.
 *                   Must be a valid, non-NULL handle.
 *
 * @return `ISLAND_SUCCESS` (0) on successful vote, or `ISLAND_ERROR` (-1)
 */
int pdtsw_island_vote_island_entry(npa_client_handle client);

/**
 * @brief Destroys a client handle and releases associated resources.
 *
 * This function *must* be called when a client no longer needs to interact with
 * the Island Service. It releases any resources held by the client handle within NPA.
 * After calling this, the `client` handle becomes invalid and should not be used again.
 * Any outstanding votes are implicitly removed.
 *
 * @param[in] client The client handle to destroy, obtained from `pdtsw_island_create_client()`.
 *                   Passing a NULL or already destroyed handle might lead to undefined
 *                   behavior or be silently ignored depending on the NPA implementation.
 */
void pdtsw_island_destroy_client(npa_client_handle client);

#endif //__PDTSW_PLATFORM_H__