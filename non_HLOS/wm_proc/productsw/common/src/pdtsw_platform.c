/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include "icbarb.h"
#include "npa.h"
#include "pdtsw_platform.h"
#include "wdss_lpi_infra.h"
#include "ee_modechange.h"
#include "npa.h"

// Define constants for NPA resources and client naming
#define ISLAND_RESOURCE "/soc/ee_modechange"

// Zephyr application initialization priority
#define ISLAND_APP_INIT_PRIORITY 89

// Define the context structure for island utility.
typedef struct island_ctx
{
  struct k_spinlock island_exit_cs; /* Spin lock to implement critical section */
  // Semaphore for blocking functionality.
  // It's used to signal that the system has transitioned to non-island mode.
  struct k_sem pending_island_exit_sem;

  // Count of clients currently waiting for island util to exit (transition to non-island).
  // This helps determine how many k_sem_give calls are needed.
  int waiting_clients_cnt;

} island_ctx_t;

// Global instance of the util context.
static island_ctx_t island_ctx;
LOG_MODULE_REGISTER(pdtsw_platform, LOG_LEVEL_ERR);
extern void diag_change_mode_in_island( bool status );
/**
 * @brief Converts from Hz to KHz
 */
#define FREQ_HZ_TO_KHZ(hz)                  ((hz)/1000)

/**
 * @brief Bus width in bytes for DDR bandwidth calculation.
 */
#define WDSS_LPI_CX_NOC_HS_BUS_WIDTH_BYTES  (128/8)

/**
 * @brief Structure to hold NPA resource information.
 */
typedef struct
{
    const char          *resource_name;         /**< Resource name */
    const char          *client_name;           /**< Client name */
    npa_client_type     client_type[PDTSW_NPA_NUM_CLIENT_TYPES];         /**< Client type - one of the two is selected */
    unsigned int        client_value;           /**< NPA client value */
    void                *client_ref;            /**< Client reference */
} pdtsw_npa_resource_info_t;

/**
 * @brief DDR route configuration for ICBArb.
 */
static ICBArb_MasterSlaveType am_ddr_route[] =
{
    {ICBID_MASTER_LPASS_AWM, ICBID_SLAVE_DDR},
};

/**
 * @brief NPA resource information for CPU, PSRAM, and DDR
 */
static const pdtsw_npa_resource_info_t pdtsw_npa_resource_info[PDTSW_NPA_NUM_RESOURCES] =
{
    [PDTSW_NPA_RESOURCE_CORE_CPU] = 
        {
            .resource_name = "/clk/cpu",
            .client_name = "pdtsw_core_cpu_client",
            .client_type = {NPA_CLIENT_REQUIRED, NPA_CLIENT_SUPPRESSIBLE},
            .client_value = 0,
            .client_ref = NULL
        },
    [PDTSW_NPA_RESOURCE_PSRAM] = 
        {
            .resource_name = "/clk/psram",
            .client_name = "pdtsw_psram_client",
            .client_type = {NPA_CLIENT_REQUIRED, NPA_CLIENT_SUPPRESSIBLE},
            .client_value = 0,
            .client_ref = NULL
        },
    [PDTSW_NPA_RESOURCE_DDR] = 
        {
            .resource_name = "/icb/arbiter",
            .client_name = "pdtsw_ddr_client",
            .client_type = {NPA_CLIENT_VECTOR, NPA_CLIENT_SUPPRESSIBLE_VECTOR},
            .client_value = sizeof(am_ddr_route),
            .client_ref = am_ddr_route
        },
};

/**
 * @brief Frequency corner corresponding to power modes for each NPA resource
 */
static const uint32_t pdtsw_npa_resource_power_mode_info[PDTSW_NPA_NUM_RESOURCES][PDTSW_NUM_POWER_MODES] =
{
    [PDTSW_NPA_RESOURCE_CORE_CPU] =
        { /* as per IPCAT for swm_cc_proc_clk */
            [PDTSW_POWER_MODE_LOWSVS_D1_XO] = 172800000, /* LOWSVS_D1 */
            [PDTSW_POWER_MODE_LOWSVS_D1]    = 230400000, /* LOWSVS */
            [PDTSW_POWER_MODE_LOWSVS]       = 288000000, /* SVS */
            [PDTSW_POWER_MODE_SVS]          = 364800000, /* SVS_L1 */
            [PDTSW_POWER_MODE_SVS_L1]       = 422400000, /* NOM */
            [PDTSW_POWER_MODE_NOM]          = 480000000, /* NOM_L1 */
            [PDTSW_POWER_MODE_TURBO]        = 499200000, /* TURBO */
        },
    [PDTSW_NPA_RESOURCE_PSRAM] =
        { /* as per qmspi_power_corners_t */
            [PDTSW_POWER_MODE_LOWSVS_D1_XO] = PDTSW_POWER_MODE_LOWSVS_D1_XO,
            [PDTSW_POWER_MODE_LOWSVS_D1]    = PDTSW_POWER_MODE_LOWSVS_D1,
            [PDTSW_POWER_MODE_LOWSVS]       = PDTSW_POWER_MODE_LOWSVS,
            [PDTSW_POWER_MODE_SVS]          = PDTSW_POWER_MODE_SVS,
            [PDTSW_POWER_MODE_SVS_L1]       = PDTSW_POWER_MODE_SVS_L1,
            [PDTSW_POWER_MODE_NOM]          = PDTSW_POWER_MODE_NOM,
            [PDTSW_POWER_MODE_TURBO]        = PDTSW_POWER_MODE_TURBO,
        },
    [PDTSW_NPA_RESOURCE_DDR] =
        {
            [PDTSW_POWER_MODE_LOWSVS_D1_XO] = 0,
            [PDTSW_POWER_MODE_LOWSVS_D1]    = 122880000,
            [PDTSW_POWER_MODE_LOWSVS]       = 204800000,
            [PDTSW_POWER_MODE_SVS]          = 245760000,
            [PDTSW_POWER_MODE_SVS_L1]       = 307200000,
            [PDTSW_POWER_MODE_NOM]          = 409600000,
            [PDTSW_POWER_MODE_TURBO]        = 614400000,
        }
};

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
    pdtsw_npa_client_type_t client_type)
{
    /* validate input arguments */
    if((resource >= PDTSW_NPA_NUM_RESOURCES) 
        || (power_mode >= PDTSW_NUM_POWER_MODES)
        || (client_type >= PDTSW_NPA_NUM_CLIENT_TYPES))
    {
        LOG_ERR("Invalid args: %d, %d, %d\n", 
            resource, power_mode, client_type);
        return NULL;
    }

    /* Create a synchronous NPA client */
    npa_client_handle client_handle = npa_create_sync_client_ex(
        pdtsw_npa_resource_info[resource].resource_name,
        pdtsw_npa_resource_info[resource].client_name,
        pdtsw_npa_resource_info[resource].client_type[client_type],
        pdtsw_npa_resource_info[resource].client_value,
        pdtsw_npa_resource_info[resource].client_ref);
    if(client_handle == NULL)
    {
        LOG_ERR("npa client create failed: %d\n", resource);
        return NULL;
    }

    ClockResult ret = CLOCK_SUCCESS; 
    ret = wdss_lpi_set_vote_ahb_config_path();
    if(CLOCK_SUCCESS != ret)
    {
        LOG_ERR("vote_ahb_cfg_path failed:%d, %d, %d, %d\n", 
            ret, resource, power_mode, client_type);
    }

    /* Issue appropriate NPA request based on the resource type */
    switch(resource)
    {
        case PDTSW_NPA_RESOURCE_CORE_CPU:
        {
            /* Issue scalar vote request for CPU clock */
            npa_issue_scalar_request(client_handle,
                FREQ_HZ_TO_KHZ(pdtsw_npa_resource_power_mode_info[resource][power_mode]));
            break;
        }
        case PDTSW_NPA_RESOURCE_PSRAM:
        {
            /* Issue scalar vote request for PSRAM resource */
            npa_issue_scalar_request(client_handle, 
                pdtsw_npa_resource_power_mode_info[resource][power_mode]);
            break;
        }
        case PDTSW_NPA_RESOURCE_DDR:
        {
            /* Get the bandwidth in bytes per second */
            uint64_t bw_bytes_per_sec = (pdtsw_npa_resource_power_mode_info[resource][power_mode] *
                            WDSS_LPI_CX_NOC_HS_BUS_WIDTH_BYTES);

            /* Prepare ICBArb vector request with bandwidth and latency parameters */
            ICBArb_RequestType requests =
            {
                .arbType        =   ICBARB_REQUEST_TYPE_3,
                .arbData.type3  =
                {
                    .uIb        =   bw_bytes_per_sec,
                    .uAb        =   bw_bytes_per_sec,
                    .uLatencyNs =   0,
                }
            };

            /* Issue vector vote request for DDR */
            npa_issue_vector_request(client_handle,
                sizeof(requests)/sizeof(npa_resource_state),
                (npa_resource_state *)&requests);
            break;
        }
        default:
            break;
    }

    ret = wdss_lpi_cancel_vote_ahb_config_path();
    if(CLOCK_SUCCESS != ret)
    {
        LOG_ERR("unvote_ahb_cfg_path failed: %d, %d, %d, %d\n", 
            ret, resource, power_mode, client_type);
    }

    /* Return the client handle */
    return (void *)client_handle;
}

/**
 * @brief Unvotes a previously voted NPA resource
 *
 * Destroys the NPA client associated with the given handle
 *
 * @param[in] p_client_handle Pointer to the NPA client handle
 * 
 * @return 0 on success, ENODEV on failure.
 */
int32_t pdtsw_npa_resource_unvote(void* p_client_handle)
{
    /* Cast the generic pointer to an NPA client handle */
    npa_client_handle client_handle = (npa_client_handle)p_client_handle;

    /* Ensure the client handle and its associated resource are valid */
    if((NULL != client_handle) && (NULL != client_handle->resource))
    {
        /* Destroy the NPA client to release the vote */
        npa_destroy_client(client_handle);
    }
    else
    {
        /* Return error if supplied client handle is invalid */
        LOG_ERR("Unvote failed: %p\n", client_handle);
        return ENODEV;
    }

    return 0;
}


/*----------------------------------------------------------------------------*/

/**
 * @brief Callback function invoked when the island mode changes.
 *        Signals waiting clients if the system transitions to non-island mode.
 *
 * @param status True if the system is in non-island mode, false otherwise (i.e., island mode).
 */
static void pdtsw_island_change_cb(bool status)
{
  
  // Only act if the system transitions to non-island mode (status == true).
  if (status)
  {
    k_spinlock_key_t key = k_spin_lock(&island_ctx.island_exit_cs);

    // Unblock all clients that were waiting for the non-island state.
    // Loop 'waiting_clients_cnt' times to give semaphore for each waiting client.
    // A single k_sem_give would increment the count, allowing multiple k_sem_take to pass
    // if the count was accumulated. This approach correctly gives a signal for each waiter.
    for (int i = 0; i < island_ctx.waiting_clients_cnt; i++)
    {
      k_sem_give(&island_ctx.pending_island_exit_sem);
    }
    LOG_INF("Signaled %d waiting clients for non-island state.",
            island_ctx.waiting_clients_cnt);

    // Reset the count of waiting clients after signaling them all.
    island_ctx.waiting_clients_cnt = 0;

    k_spin_unlock(&island_ctx.island_exit_cs, key);
  }
  #if defined(CONFIG_QC_DIAG_ENABLED)
    diag_change_mode_in_island(status);
  #endif
}

/**
 * @brief Votes to allow island mode entry (or releases a previous vote for island exit).
 *
 * This function is used when a client wants to allow the system to enter island mode
 * or release its vote that was preventing island mode entry.
 *
 * @param handle The NPA client handle associated with the caller.
 * @return 0 on success, negative error code on failure.
 */
int pdtsw_island_vote_island_entry(npa_client_handle handle)
{
  if (NULL == handle)
  {
    LOG_ERR("Invalid NPA client handle for island entry vote (NULL).");
    return ISLAND_ERROR;
  }

  // Allow island mode re-entry
  if (enter_island_mode(&handle))
  {
    LOG_ERR("Failed to allow island mode entry for client handle %p.", (void *)handle);
    return ISLAND_ERROR;
  }

  LOG_INF("Voted to allow island mode entry for client handle %p.", (void *)handle);
  return ISLAND_SUCCESS;
}

/**
 * @brief Votes to prevent island mode entry (request non-island mode) and waits if necessary.
 *
 * This function is used when a client needs to perform operations that require
 * the system to be in non-island mode. It votes to prevent island mode entry
 * and, if the system is currently in island mode, it blocks until the system
 * transitions to non-island mode.
 *
 * @param handle The NPA client handle associated with the caller.
 * @return 0 on success, negative error code on failure.
 */
int pdtsw_island_vote_island_exit(npa_client_handle handle)
{
  if (handle == NULL)
  {
    LOG_ERR("Invalid NPA client handle for island exit vote (NULL).");
    return ISLAND_ERROR;
  }

  // Exit island mode call is mandatory here. This ensures that ISLAND re-entry
  // is prevented until this client completes its resource usage.
  if (exit_island_mode(&handle))
  {
    LOG_INF("Failed to exit island mode for client %p", (void *)handle);
    return ISLAND_ERROR;
  }

  // Critical section to protect `waiting_clients_cnt` and `pending_island_exit_sem`
  // related operations from race conditions with `pdtsw_island_change_cb`.
  k_spinlock_key_t key = k_spin_lock(&island_ctx.island_exit_cs);
  int current_state = get_current_state(); // 0 for ISLAND, 1 for NON_ISLAND
  LOG_INF("%s: current state = %d (0=ISLAND, 1=NON_ISLAND)", __func__, current_state);

  if (!current_state) // If currently in ISLAND mode (state == 0)
  {
    island_ctx.waiting_clients_cnt += 1;
  }
  k_spin_unlock(&island_ctx.island_exit_cs, key);

  // If we were in ISLAND mode, block until the `pdtsw_island_change_cb` signals the semaphore.
  // This `k_sem_take` must happen *after* releasing the spinlock to avoid deadlocks
  // if `pdtsw_island_change_cb` needs to acquire the spinlock before signaling.
  if (!current_state)
  {
    LOG_INF("Client %p waiting for semaphore", (void *)handle);
    // This blocks until `pdtsw_island_change_cb` signals the semaphore.
    // K_FOREVER implies an indefinite wait, ensure this is the desired behavior.
    int ret = k_sem_take(&island_ctx.pending_island_exit_sem, K_FOREVER);
    if(ret!=0)
    {
      LOG_ERR("Client %p not received semaphore", (void *)handle);
      return ISLAND_ERROR;
    }
  }

  LOG_INF("Island exit vote successful %p", (void *)handle);

  return ISLAND_SUCCESS;
}

/**
 * @brief Creates an NPA client handle for island mode management.
 *
 * This function should be called by any module that needs to vote for/against
 * island mode entry. Each module should create its own unique client handle.
 *
 * @param client_name A unique string name for the client (e.g., "island_client").
 * @return An NPA client handle on success, or NULL on failure.
 */
npa_client_handle pdtsw_island_create_client(const char *client_name)
{
  if ((client_name == NULL) || (strlen(client_name) == 0))
  {
    LOG_ERR("Invalid client name provided: %p (NULL or empty string).", client_name);
    return NULL;
  }
  npa_client_handle handle = npa_create_sync_client(
      ISLAND_RESOURCE, client_name, NPA_CLIENT_REQUIRED);
  if (NULL == handle)
  {
    LOG_ERR("Failed to create NPA client for island mode resource '%s' with name '%s'.",
            ISLAND_RESOURCE, client_name);
    return NULL;
  }

  LOG_INF("NPA client '%s' created successfully.", client_name);
  return handle;
}

/**
 * @brief Destroys an NPA client handle.
 *
 * This function should be called when a module no longer needs to manage island mode,
 * to release its resources.
 *
 * @param handle The NPA client handle to destroy.
 */
void pdtsw_island_destroy_client(npa_client_handle handle)
{
  if (handle == NULL)
  {
    LOG_ERR("Attempted to destroy a NULL NPA client handle. Ignoring.");
    return;
  }

  LOG_INF("Destroying NPA client handle %p", (void *)handle);
  npa_destroy_client(handle);
  LOG_INF("Destroyed NPA client handle");
}

/**
 * @brief Initializes the island utility module.
 *
 * This function sets up internal data structures, initializes semaphores,
 * and registers the callback for island mode changes.
 * It should be called once during system initialization.
 *
 * @return 0 on success, negative error code on failure.
 */
int pdtsw_island_init(void)
{
  // Zero-initialize the context structure to ensure all members are in a known state.
  memset(&island_ctx, 0, sizeof(island_ctx_t));

  // Initialize the semaphore.
  // Initial count is 0, meaning `k_sem_take` will block immediately until `k_sem_give` is called.
  // Max count K_SEM_MAX_LIMIT allows for multiple `k_sem_give` calls to accumulate.
  if (k_sem_init(&island_ctx.pending_island_exit_sem, 0, K_SEM_MAX_LIMIT) != 0)
  {
    LOG_ERR("Failed to initialize `pending_island_exit_sem` semaphore.");
    return -EAGAIN; // Or other appropriate error code.
  }

  // Register the callback for island mode changes
  (void)ee_modechange_registercb(pdtsw_island_change_cb);

  LOG_INF("island_util_init successful");

  return 0;
}

/*----------------------------------------------------------------------------*/

// Zephyr system initialization macro to call island_util_init.
// APPLICATION priority ensures it runs after KERNEL and before most user threads.
SYS_INIT(pdtsw_island_init, POST_KERNEL, ISLAND_APP_INIT_PRIORITY);