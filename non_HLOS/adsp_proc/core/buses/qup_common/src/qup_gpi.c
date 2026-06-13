/**
    @file  qup_gpi.c
    @brief Qup interface implementation
 */
/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
10/10/25   GST     Channel deinit update if PDR and timeout happens simultaneously
08/14/25   GKR     Added changes to supoort SFE
10/07/24   GKR     Changes to Support DT based log level selection
07/06/24   GKR     converted gpi_data from ptr to array for better memory handling
*/
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/


/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/
#include "qup_common_internal.h"
#include "qup_os.h"
#include "qup_log.h"
#include "qup_error.h"
#include "qup_gpi.h"

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/


/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/


/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/
static qup_gpi_ring_mem qup_gpi_mem[] = { {TX_TRANSFER_ELEM_TYPE, MAX_TX_TRE_LIST_SIZE_PER_CORE},
                                          {RX_TRANSFER_ELEM_TYPE, MAX_RX_TRE_LIST_SIZE_PER_CORE},
                                          {EVR_ELEM_TYPE, MAX_EVR_TRE_LIST_SIZE_PER_CORE},       };

/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/
void *qup_gpi_get_io_ctxt (se_dev_cfg *dcfg)
{
    void *ptr = NULL;

    ptr =  qup_mem_alloc (dcfg,
                          sizeof(qup_gpi_iface_ctxt),
                          IO_CTXT_TYPE);
    return ptr;
}

void qup_gpi_release_io_ctxt (se_dev_cfg *dcfg, void *ptr)
{
    qup_transfer_ctxt *t_ctxt = (qup_transfer_ctxt *)ptr;
    qup_gpi_iface_ctxt *io_ctxt = t_ctxt->io_ctxt;

    if(t_ctxt->buffer_list[GPI_OUTBOUND_CHAN]) // will be NULL for legacy transfer API
    {
        qup_mem_free(t_ctxt->buffer_list[GPI_OUTBOUND_CHAN],  io_ctxt->tx_tre_req.num_tre * sizeof(gpi_ring_elem), dcfg, QUP_TRE_LIST_ISLAND);
    }
    if(t_ctxt->buffer_list[GPI_INBOUND_CHAN])
    {
        qup_mem_free(t_ctxt->buffer_list[GPI_INBOUND_CHAN],  io_ctxt->rx_tre_req.num_tre * sizeof(gpi_ring_elem), dcfg, QUP_TRE_LIST_ISLAND);
    }
    qup_mem_free(io_ctxt, sizeof(qup_gpi_iface_ctxt), dcfg, IO_CTXT_TYPE);
}

/**
 * @brief Allocates and initializes a GPI core context.
 *
 * This function allocates memory for a `qup_gpi_core_ctxt` structure and
 * initializes its members, including allocating memory for GPI ring elements
 * (TREs) for outbound, inbound, and event channels. It handles memory allocation
 * and initializes relevant fields. If memory allocation fails at any point,
 * it cleans up already allocated resources.
 *
 * @param dcfg Pointer to the SE device configuration structure.
 * @return A pointer to the newly allocated and initialized `qup_gpi_core_ctxt`
 *         on success, or `NULL` if memory allocation fails or an error occurs.
 */
void *qup_gpi_get_gpi_ctxt (se_dev_cfg *dcfg)
{
    qup_gpi_core_ctxt *g_ctxt = NULL;
    qup_status q_status = QUP_SUCCESS;
    GPI_CHAN_TYPE chan = GPI_OUTBOUND_CHAN;

    // Allocate memory for the GPI core context
    g_ctxt = (qup_gpi_core_ctxt *) qup_mem_alloc (dcfg,
                                                  sizeof(qup_gpi_core_ctxt),
                                                  CORE_IFACE_TYPE);

    // Check if context allocation failed
    if (g_ctxt == NULL) { q_status =  QUP_ERROR_NO_MEM; goto error;}

    // Initialize the allocated context to zero
    qup_memset((void *)g_ctxt, 0, sizeof(qup_gpi_core_ctxt));

    // Iterate through GPI channels to allocate ring elements
    for ( chan = GPI_OUTBOUND_CHAN; chan < GPI_CHAN_MAX; chan++)
    {
        // Special handling for SSC QUP event ring, allocated by GPI driver
        if (GET_QUP_MAJOR(dcfg->qup_data->qup_id) == SSC_QUP && chan == GPI_EVT_RING) continue;

        // Initialize ring_info fields for the current channel
        g_ctxt->ring_info[chan].tre_ring_size = qup_gpi_mem[chan].num_tre_elem * dcfg->gpi_data.ring_size_multiplier;
        g_ctxt->ring_info[chan].ring_base_va = (U_LONG)qup_mem_alloc(dcfg, sizeof(gpi_ring_elem) * g_ctxt->ring_info[chan].tre_ring_size, qup_gpi_mem[chan].mem_type);
        // Check if TRE allocation failed
        if (g_ctxt->ring_info[chan].ring_base_va == 0) { q_status = QUP_ERROR_NO_MEM; goto error; }
        g_ctxt->ring_info[chan].ring_base_pa = (U_LONG) qup_mem_virt_to_phys (NULL, (void *) g_ctxt->ring_info[chan].ring_base_va,
                                                                            ATTRIB_TRE, REGION_NONE);
        if (g_ctxt->ring_info[chan].ring_base_pa == 0){ q_status = QUP_ERROR_DMA_TX_CHAN_ADDRESS_MAP_FAIL + chan; goto error; }
    }

error:

    // If any error occurred during allocation, release the allocated context
    if (QUP_ERROR(q_status))
    {
        qup_gpi_release_gpi_ctxt(dcfg, g_ctxt);
        g_ctxt = NULL;
    }
    return (void *) g_ctxt;
}

/**
 * @brief Releases the resources associated with a GPI core context.
 *
 * This function deallocates memory associated with the GPI core context,
 * specifically the GPI ring elements (TREs) for all channels and then
 * the core context structure itself. It handles cases where the context
 * pointer is NULL safely.
 *
 * @param dcfg Pointer to the SE device configuration structure.
 * @param ptr A pointer to the `qup_gpi_core_ctxt` structure to be released.
 */
void qup_gpi_release_gpi_ctxt (se_dev_cfg *dcfg, void *ptr)
{
    qup_gpi_core_ctxt *g_ctxt = (qup_gpi_core_ctxt *)ptr;
    GPI_CHAN_TYPE chan = GPI_OUTBOUND_CHAN;

    // Return if the context pointer is NULL
    if (g_ctxt == NULL) { return; }

    // Iterate through GPI channels to free ring elements
    for ( chan = GPI_OUTBOUND_CHAN; chan < GPI_CHAN_MAX; chan++)
    {
        // Free allocated TRE base if it's not NULL
        if (g_ctxt->ring_info[chan].ring_base_va != NULL)
        {
            qup_mem_free ((void *) g_ctxt->ring_info[chan].ring_base_va,
                           sizeof(gpi_ring_elem) * g_ctxt->ring_info[chan].tre_ring_size,
                           dcfg,
                           qup_gpi_mem[chan].mem_type);
            g_ctxt->ring_info[chan].ring_base_va = NULL; // Clear the pointer after freeing
            g_ctxt->ring_info[chan].ring_base_pa = 0; // Clear physical address as well
        }
    }
    // Free the GPI core context itself
    qup_mem_free(g_ctxt, sizeof(qup_gpi_core_ctxt), dcfg, CORE_IFACE_TYPE);
}

/**
 * @brief Initializes the GPI interface parameters structure.
 *
 * This static helper function populates the `gpi_iface_params` structure
 * with configuration details required for GPI interface registration.
 * It sets up parameters such as GPI ID, island specification, SE ID, protocol,
 * QUP type, ring size multiplier, interrupt modulation values, log level,
 * IRQ mode, and callback functions. It also calculates and assigns the
 * virtual and physical addresses and sizes for the TRE rings of
 * outbound, inbound, and event channels.
 *
 * @param dcfg Pointer to the SE device configuration structure.
 * @param g_ctxt Pointer to the GPI core context structure.
 * @param params Pointer to the `gpi_iface_params` structure to be initialized.
 * @param evr_cb Client callback function for event ring events.
 * @param cmd_cb Client callback function for command events.
 * @param gpi_idx The GPI instance index.
 * @return `QUP_SUCCESS` if parameters are successfully initialized,
 *         or an error code if virtual-to-physical address mapping fails.
 */
static qup_status qup_init_gpi_iface_params (se_dev_cfg *dcfg, qup_gpi_core_ctxt *g_ctxt, gpi_iface_params* params, client_cb evr_cb, client_cb cmd_cb, uint8 gpi_idx)
{
    qup_status q_status = QUP_SUCCESS;

    // Initialize the params structure to zero
    qup_memset((void *)params, 0, sizeof(gpi_iface_params));

    // Initialize GPI interface parameters
    params->gpi_handle = NULL; // GPI handle will be set by gpi_iface_reg
    params->gpii_id = gpi_idx; // GPI interface ID
    params->island_spec = dcfg->island_spec_in_use; // Island mode specification
    params->se = dcfg->se_id; // SE instance ID
    params->protocol = (GPI_PROTOCOL)dcfg->protocol_in_use; // Protocol in use (I2C, SPI, UART)
    params->qup_type = dcfg->qup_data->qup_id; // QUP type (e.g., SSC QUP)
    params->ring_size_multiplier = dcfg->gpi_data.ring_size_multiplier; // Multiplier for ring size
    params->gpi_log_level = dcfg->log_level; // Log level for GPI
    // Determine IRQ mode based on polled mode flag
    params->irq_mode = ( dcfg->flags & POLLED_MODE)? FALSE:TRUE;     // polled mode is bus_iface implementation
    params->event_cb   = evr_cb; // Callback for event ring events
    params->command_cb = cmd_cb; // Callback for command completion events

    qup_mem_copy (&params->chan[GPI_OUTBOUND_CHAN], sizeof(gpi_tre_ring)*GPI_CHAN_MAX,
                  &g_ctxt->ring_info[GPI_OUTBOUND_CHAN],sizeof(gpi_tre_ring)*GPI_CHAN_MAX);

    params->user_data = g_ctxt; // User data for callbacks (pointer to GPI core context)

    return q_status; // Return initialization status
}

qup_status qup_gpi_iface_init (qup_hw_ctxt *h_ctxt)
{
    se_dev_cfg *dcfg = (se_dev_cfg *)h_ctxt->core_config;
    qup_status q_status = QUP_SUCCESS;
    qup_gpi_core_ctxt *g_ctxt = NULL;
    qup_gpi_core_ctxt *g_ctxt_prev = NULL;
    uint8 i = 0;

    for (i = 0; i < dcfg->gpi_data.num_gpi_idx; i++)
    {
        g_ctxt = qup_gpi_get_gpi_ctxt(dcfg);
        if (g_ctxt == NULL)
        {
           if (i == 0) {q_status = QUP_ERROR_NO_MEM; goto error; } // Must catch error on first time
           else break;
        }

        q_status =  qup_gpi_iface_init_ex(dcfg, g_ctxt, qup_gpi_evr_callback,
                                          qup_gpi_cmd_callback, h_ctxt, dcfg->gpi_data.gpi_idx[i]);

        if(QUP_ERROR(q_status))
        {
            qup_gpi_release_gpi_ctxt(dcfg, g_ctxt);
            if (i == 0) {q_status = QUP_ERROR_NO_MEM; goto error; } // Must catch error on first time
            else break;
        }
        else
        {
            if (i == 0) h_ctxt->core_iface = g_ctxt;
            else g_ctxt_prev->next = g_ctxt;
            g_ctxt_prev = g_ctxt;
        }
    }

error:

    if (QUP_SUCCESS(q_status))
    {
        h_ctxt->iface_functions.iface_enable = qup_gpi_enable;
        h_ctxt->iface_functions.cancel_transfer = qup_gpi_cancel;
    }

    return q_status;
}

/**
 * @brief Initializes a specific GPI interface instance.
 *
 * This function initializes a single GPI interface based on the provided
 * device configuration and GPI core context. It first prepares the GPI
 * interface parameters using `qup_init_gpi_iface_params`, then registers
 * the GPI interface with the GPI driver. After successful registration,
 * it enables interrupts, allocates and starts the GPI channels (outbound,
 * inbound, and event ring). It handles error conditions and performs
 * de-initialization if any step fails.
 *
 * @param dcfg Pointer to the SE device configuration structure.
 * @param g_ctxt Pointer to the GPI core context structure for this instance.
 * @param evr_cb Client callback function for event ring events.
 * @param cmd_cb Client callback function for command events.
 * @param h_ctxt Handle to the hardware context.
 * @param gpi_idx The GPI instance index to initialize.
 * @return `QUP_SUCCESS` if the GPI interface is successfully initialized,
 *         or an error code indicating the failure.
 */
qup_status qup_gpi_iface_init_ex (se_dev_cfg *dcfg, qup_gpi_core_ctxt *g_ctxt, client_cb evr_cb, client_cb cmd_cb, void *h_ctxt, uint8 gpi_idx)
{
    qup_status q_status = QUP_SUCCESS;
    gpi_iface_params params;
    GPI_CHAN_TYPE chan;

    // Initialize GPI interface parameters
    q_status = qup_init_gpi_iface_params(dcfg, g_ctxt, &params, evr_cb, cmd_cb, gpi_idx);
    if (QUP_ERROR(q_status))
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR, "qup_init_gpi_iface_params failed with status : %x", q_status);
        return q_status;
    }

    // Reset GPI context flags
    g_ctxt->flags        = GPI_FLAGS_RESET;

    // Initialize command wait signal for synchronization
    g_ctxt->command_wait_signal = qup_signal_init (dcfg,CORE_CMD_SIGNAL);

    // Register GPI interface with the GPI driver
    if (GPI_STATUS_SUCCESS != gpi_iface_reg(&params))
    {
        q_status = QUP_ERROR_DMA_REG_FAIL;
        goto error;
    }
    // Set flag indicating GPI interface is registered
    QUP_FLAG_SET(g_ctxt->flags,GPI_IFACE_REGISTERED);

    // Store GPI handle and initialize channel states
    g_ctxt->gpi_handle   = params.gpi_handle;
    g_ctxt->chan_state[GPI_OUTBOUND_CHAN] = GPI_CHAN_ERROR;
    g_ctxt->chan_state[GPI_INBOUND_CHAN]  = GPI_CHAN_ERROR;
    g_ctxt->chan_state[GPI_EVT_RING]      = GPI_CHAN_ERROR;
    g_ctxt->h_ctxt = h_ctxt; // Store hardware context

    // Enable GPI interrupts
    gpi_iface_active(g_ctxt->gpi_handle, TRUE);

    // Allocate GPI channels
    for (chan = GPI_OUTBOUND_CHAN; chan < GPI_CHAN_MAX; chan++)
    {
        // Issue allocate command for each channel
        if (!qup_gpi_issue_cmd_ex(dcfg, g_ctxt, GPI_CHAN_CMD_ALLOCATE, 0, GPI_CHAN_ALLOCATED, chan, FALSE, g_ctxt))
        {
            QUP_SE_LOG(dcfg, LEVEL_ERROR, "qup_gpi_iface_init_ex: failed to allocate channel %d", chan);
            q_status = QUP_ERROR_DMA_TX_CHAN_ALLOC_FAIL+chan;
            goto error;
        }
    }

    // Start GPI channels (Event ring not explicitly started)
    for (chan = GPI_OUTBOUND_CHAN; chan < GPI_EVT_RING; chan++)
    {
        // Issue start command for each channel
        if (!qup_gpi_issue_cmd_ex(dcfg, g_ctxt, GPI_CHAN_CMD_START, 0, GPI_CHAN_RUNNING, chan, FALSE, g_ctxt))
        {
            QUP_SE_LOG(dcfg, LEVEL_ERROR, "qup_gpi_iface_init_ex: failed to start channel %d", chan);
            q_status = QUP_ERROR_DMA_TX_CHAN_START_FAIL + chan; // Reusing error code
            goto error;
        }
    }

error:

    // Disable GPI interrupts in case of error or completion
    gpi_iface_active(g_ctxt->gpi_handle, FALSE);
    g_ctxt->in_use = FALSE; // Mark context as not in use

    // If an error occurred, log and de-initialize the interface
    if (QUP_ERROR(q_status))
    {
        QUP_SE_LOG(dcfg,LEVEL_ERROR, "qup_gpi_iface_init_ex failed with status : %x", q_status);
        qup_gpi_iface_de_init_ex (dcfg, g_ctxt, g_ctxt);
    }

    return q_status;
}

qup_status qup_gpi_iface_de_init (qup_hw_ctxt *h_ctxt)
{
    qup_status q_status =  QUP_SUCCESS;
    se_dev_cfg *dcfg = (se_dev_cfg *) h_ctxt->core_config;
    qup_gpi_core_ctxt *g_ctxt = (qup_gpi_core_ctxt *) h_ctxt->core_iface;

    while(g_ctxt != NULL)
    {
        qup_gpi_core_ctxt *next_g_ctxt = g_ctxt->next;
        // Call de_init_ex to perform de-initialization and free tre_base members
        q_status =  qup_gpi_iface_de_init_ex(dcfg, g_ctxt, (void *)g_ctxt);

        // Free g_ctxt after its tre_base members have been freed by qup_gpi_iface_de_init_ex
        qup_gpi_release_gpi_ctxt(dcfg, g_ctxt);
        g_ctxt = next_g_ctxt;
    }

    return q_status;
}

qup_status qup_gpi_iface_de_init_ex (se_dev_cfg *dcfg, qup_gpi_core_ctxt *g_ctxt, void *cb_data)
{
    qup_status q_status = QUP_SUCCESS;
    GPI_CHAN_TYPE chan;

    if (g_ctxt == NULL) { return QUP_ERROR_INVALID_PARAMETER; }

    if(QUP_IS_SET(g_ctxt->flags,GPI_IFACE_REGISTERED))
    {
		// there is a consequence where timer callback hit after close and this lead to 
		// mixup of command callbacks of close and in timercallback.
		//Adding GPI_DEINIT_INPROGRESS flag to stop recovery and prioritize close funciton.
        QUP_FLAG_SET(g_ctxt->flags,GPI_DEINIT_INPROGRESS);

        if (QUP_SUCCESS != qup_wait_for_gpii_clean(g_ctxt))
        {
            QUP_SE_LOG(dcfg, LEVEL_ERROR, "ERROR! gpii is not recovered for next transfers ");
            return QUP_ERROR_DMA_INSUFFICIENT_RESOURCES;
        }
		
		
        // enable interrupts
        gpi_iface_active(g_ctxt->gpi_handle, TRUE);

        // stop channels
        for (chan = GPI_OUTBOUND_CHAN; chan < GPI_EVT_RING; chan++) // Stop TX and RX channels
        {
            if((g_ctxt->chan_state[chan] == GPI_CHAN_RUNNING) || (g_ctxt->chan_state[chan] == GPI_CHAN_ERROR))
            {
                if (!qup_gpi_issue_cmd_ex(dcfg, g_ctxt, GPI_CHAN_CMD_STOP, 0, GPI_CHAN_STOPPED, chan, FALSE, cb_data))
                {
                    QUP_SE_LOG(dcfg,LEVEL_ERROR, "qup_gpi_iface_de_init : CHAN_%d_STOP_FAIL trying reset", chan);
                    if (!qup_gpi_issue_cmd_ex(dcfg, g_ctxt, GPI_CHAN_CMD_RESET, 0, GPI_CHAN_ALLOCATED, chan, FALSE, cb_data))
                    {
                        QUP_SE_LOG(dcfg, LEVEL_ERROR, "qup_gpi_iface_de_init_ex: failed to reset channel %d after stop failure", chan);
                        q_status = QUP_ERROR_DMA_TX_CHAN_RESET_FAIL + chan; // Update status if reset also fails
                        goto error;
                    }
                }
            }
        }

        // reset channels
        for (chan = GPI_OUTBOUND_CHAN; chan < GPI_EVT_RING; chan++) // Reset TX and RX channels
        {
            if(g_ctxt->chan_state[chan] > GPI_CHAN_ALLOCATED)
            {
                if (!qup_gpi_issue_cmd_ex(dcfg, g_ctxt, GPI_CHAN_CMD_RESET, 0, GPI_CHAN_ALLOCATED, chan, FALSE, cb_data))
                {
                    QUP_SE_LOG(dcfg, LEVEL_ERROR, "qup_gpi_iface_de_init_ex: failed to reset channel %d", chan);
                    q_status = QUP_ERROR_DMA_TX_CHAN_RESET_FAIL + chan; // Using base error + chan for specificity
                    goto error;
                }
            }
        }

        // de-allocate channels
        for (chan = GPI_OUTBOUND_CHAN; chan < GPI_CHAN_MAX; chan++) // De-allocate all channels
        {
            if(g_ctxt->chan_state[chan] == GPI_CHAN_ALLOCATED)
            {
                if (!qup_gpi_issue_cmd_ex(dcfg, g_ctxt, GPI_CHAN_CMD_DE_ALLOC, 0, GPI_CHAN_DISABLED, chan, FALSE, cb_data))
                {
                    QUP_SE_LOG(dcfg, LEVEL_ERROR, "qup_gpi_iface_de_init_ex: failed to de-allocate channel %d", chan);
                    q_status = QUP_ERROR_DMA_TX_CHAN_DE_ALLOC_FAIL + chan; // Using base error + chan for specificity
                    goto error;
                }
            }
        }

error:

        // disable interrupts
        gpi_iface_active(g_ctxt->gpi_handle, FALSE);
    }

    if(QUP_SUCCESS(q_status))
    {
        if(QUP_IS_SET(g_ctxt->flags,GPI_IFACE_REGISTERED))
        {
            if (GPI_STATUS_SUCCESS != gpi_iface_dereg(g_ctxt->gpi_handle))
            {
                q_status = QUP_ERROR_DMA_DE_REG_FAIL;
            }//Maybe handle Error
            QUP_FLAG_UNSET(g_ctxt->flags,GPI_IFACE_REGISTERED);
        }

        if (g_ctxt->command_wait_signal != NULL)
        {
            qup_signal_de_init (dcfg, g_ctxt->command_wait_signal);
            g_ctxt->command_wait_signal = NULL;
        }
    }
    QUP_FLAG_UNSET(g_ctxt->flags,GPI_DEINIT_INPROGRESS);
    return q_status;
}

