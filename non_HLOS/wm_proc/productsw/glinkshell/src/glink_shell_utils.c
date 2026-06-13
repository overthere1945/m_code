/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <stringl.h>
#include <stdbool.h>
#include "glink.h"
#if !CONFIG_GLINK_SHELL_PASSTHROUGH
#include "glink_shell.h"
#endif
#include "glink_shell_utils.h"

/* wm_shell msgq supported element count */
#define WM_SHELL_MSGQ_ELEM_COUNT				200
#define FALSE									false
#define TRUE									true

/* Handles for GLINK with WM and Q6 */
glink_handle_type wm_shell_glink_hdl;
glink_handle_type q6_shell_glink_hdl;

/**
	GLINK transport parameters
		@var xport_name defines the physical interface used for the link
		@var remote_ss defines the name of the channel to be used
 */
static const char *xport_name = GLINK_SHELL_XPORT_NAME;
bool is_q6_remote_open = false;

#if CONFIG_GLINK_SHELL_PASSTHROUGH
static const char *remote_ss = GLINK_SHELL_WITH_SWM_REMOTESS;
static const char *q6_remote_ss = GLINK_SHELL_WITH_Q6_REMOTESS;
#else
static const char *remote_ss = GLINK_SHELL_WITH_AWM_REMOTESS;
#endif

/* Glink Shell Utils Heap info */
struct k_heap glink_shell_heap;
static uint8_t glink_shell_heap_mem[CONFIG_GLINK_SHELL_TRANSPORT_BUF_SZ];

extern struct k_msgq glink_shell_msgq;

#if CONFIG_GLINK_SHELL_PASSTHROUGH
#if (CONFIG_SHELL_BACKEND_SERIAL || CONFIG_QC_SHELL_BACKEND_SERIAL)
extern const struct shell *shell_backend_uart_get_ptr(void);
#endif
extern struct k_msgq q6_shell_msgq;
#endif

extern char *q6_shell_open_channel_name;

/**
	Glink Callbacks registered before channel open
		tx_done_cb for freeing memory after successful tx-done
		rx_notification_cb to prepare to receive data over wm-shell-glink channel
		rx_intent_req_cb to falicitate queueing of intents upon client request
		channel_state_cb to continute flow post GLINK_CONNECTED state over channel
		link_state_cb to check for STATE_UP or STATE_DOWN of channel
 */
static void wm_shell_glink_rx_notification_cb(glink_handle_type handle, const void *priv,
                                                    const void *pkt_priv, const void *ptr,
                                                    size_t size, size_t intent_used);
static bool wm_shell_glink_rx_intent_req_cb(glink_handle_type handle,const void *priv,
										size_t req_size);
static glink_err_type wm_shell_glink_rx_intent_alloc_cb(glink_handle_type handle, const void *priv,
										const void *pkt_priv,
										size_t intent_size, void **buffer_ptr);
static void wm_shell_glink_rx_intent_dealloc_cb(glink_handle_type handle, const void *priv,
										const void *pkt_priv,
										size_t intent_size, void *buffer_ptr);
static void wm_shell_glink_channel_state_cb(glink_handle_type handle, const void *priv,
										glink_channel_event_type event);

/**
	msgq control structure
		A central API will be utilized by all the callbacks wishing to progress the glink
		process by populating the message queue with state information
		@param msgq_opcode is the state/opcode relevent to the glink process
		@param data_ptr is a pointer to the information relevent to the message
		@param size is the size of the data passed along @ data_ptr
 */
static inline void wm_shell_glink_send_to_msgq(struct k_msgq *msgq_ctx, uint32_t msgq_opcode, const void *data_ptr, size_t size);


/**
 @brief Glink Link registration for wm and q6 shell.
       Once succesful the "wm_shell_glink_link_state_cb"
       callback is called from Glink core.
 @param[in] none
 @return none
*/
wm_shell_glink_err_type wm_shell_glink_start(void)
{
	glink_err_type glink_result;
	glink_link_id_type glink_id;
	GLINK_LINK_ID_STRUCT_INIT(glink_id);

	/**
	 * For AWM/SWM interaction:
	 * xport -> SMEM
	 * remote_ss -> swm
	 * callback is the same for both interactions to save on code space
	 */
	glink_id.xport = xport_name;
	glink_id.remote_ss = remote_ss;
	glink_id.link_notifier = wm_shell_glink_link_state_cb;

	/* Registering link state callback shall begin request for LOCAL_OPEN */
	glink_result = glink_register_link_state_cb(&glink_id,NULL);

	if(glink_result != GLINK_STATUS_SUCCESS)
	{
		return WM_SHELL_LINK_REG_FAILED;
	}

	return WM_SHELL_SUCCESS;
}

#if CONFIG_GLINK_SHELL_PASSTHROUGH
wm_shell_glink_err_type q6_shell_glink_start(void)
{
	glink_err_type glink_result;
    glink_link_id_type q6_glink_id;
    GLINK_LINK_ID_STRUCT_INIT(q6_glink_id);

	/**
	 * For AWM/Q6 interaction:
	 * xport -> SMEM
	 * remote_ss -> swm
	 * callback is the same for both interactions to save on code space
	 */
	q6_glink_id.xport = xport_name;
    q6_glink_id.remote_ss = q6_remote_ss;
	q6_glink_id.link_notifier = wm_shell_glink_link_state_cb;

    glink_result = glink_register_link_state_cb(&q6_glink_id,NULL);

	if(glink_result != GLINK_STATUS_SUCCESS)
    {
        return WM_SHELL_LINK_REG_FAILED;
    }

    return WM_SHELL_SUCCESS;
}
#endif

/**
    msgq control structure
        A central API will be utilized by all the callbacks wishing to progress the glink
        process by populating the message queue with state information
        @param msgq_opcode is the state/opcode relevent to the glink process
        @param data_ptr is a pointer to the information relevent to the message
        @param size is the size of the data passed along @ data_ptr
 */
static inline void wm_shell_glink_send_to_msgq(struct k_msgq *msgq_ctx, uint32_t msgq_opcode, const void *data_ptr, size_t size)
{
	wm_shell_msgq_data_item_t _msgq_event = {msgq_opcode, data_ptr, size};
	while (k_msgq_put(msgq_ctx, &_msgq_event, K_NO_WAIT) != 0);
}

/**
 @brief Link State Callback
 @param[in] glink_link_info_type *link_info,void *priv
 @return none
*/
void wm_shell_glink_link_state_cb(glink_link_info_type *link_info,void *priv)
{
    if(link_info == NULL)
        return;

/**
 * The only way to know which interaction has triggered the link state callback
 * is via the time at which the callback is received, since the callback does not
 * provide a handle. AWM/SWM interaction is the first thread to start and
 * starts automatically; it will be the first to register link state. During this
 * time, the user would not have sent the command to open a q6 channel. Hence,
 * q6_shell_open_channel_name shall be NULL, which differentiates the interactions
 */
#if CONFIG_GLINK_SHELL_PASSTHROUGH
	wm_shell_glink_send_to_msgq((q6_shell_open_channel_name == NULL) ? \
								&glink_shell_msgq : &q6_shell_msgq,
#else
	wm_shell_glink_send_to_msgq(&glink_shell_msgq,
#endif
								WM_SHELL_LINK_STATE,
							    NULL,
								link_info->link_state);
}

/**
	@brief Glink Link State is received for STATE_UP and STATE_DOWN
	@param[in] none
	@return none
*/
wm_shell_glink_err_type wm_shell_glink_get_link_state(wm_shell_msgq_data_item_t *msgq_event)
{
	/**
	 * q6_shell_open_channel_name shall be NULL when the immutable interaction
	 * AWM/SWM is attempted. Post this interaction it shall be populated with
	 * the name of the requested channel. This saves on code space and avoids
	 * branching
	 */
	switch ((glink_link_state_type)msgq_event->size)
	{
		case GLINK_LINK_STATE_UP:
			wm_shell_glink_open_channel(q6_shell_open_channel_name);
			break;

		/**
		 * Ideally, AWM/SWM interaction shall never be closed. However, due to link
		 * down between AWM/SWM, we might need to clean-up satisfactorily, which this
		 * effort denies. @Reviewers, any WAR for this? (Link state does not return handle)
		 * We might have to register different link state callbacks for AWM/Q6 and AWM/SWM
		 */
		case GLINK_LINK_STATE_DOWN:
#if CONFIG_GLINK_SHELL_PASSTHROUGH
			wm_shell_glink_close_channel( \
			(q6_shell_open_channel_name == NULL) ? wm_shell_glink_hdl : q6_shell_glink_hdl);
#else
			wm_shell_glink_close_channel(wm_shell_glink_hdl);
#endif
			break;

		default:
			return WM_SHELL_LINK_REG_FAILED;
			break;
	}

	return WM_SHELL_SUCCESS;
}

/**
 @brief Glink Channel State Callback with State or GLINK State.

 @param[in] glink_handle_type handle,const void *priv, glink_channel_event_type event
 @return none
*/
static void wm_shell_glink_channel_state_cb(glink_handle_type handle, const void *priv,
											glink_channel_event_type event)
{
	if (event != GLINK_CONNECTED)
	{
		wm_shell_glink_close_channel(handle);
		return;
	}

	/**
 	* For this callback, and all subsequent callbacks, glink handle is returned
 	* This is useful for differentiating interactions AWM/SWM and AWM/Q6, since
 	* both use different message queues and exist in different states
 	*/
#if CONFIG_GLINK_SHELL_PASSTHROUGH
	wm_shell_glink_send_to_msgq((handle == q6_shell_glink_hdl) ? &q6_shell_msgq : &glink_shell_msgq,
#else
	wm_shell_glink_send_to_msgq(&glink_shell_msgq,
#endif
								WM_SHELL_NOTIFY_STATE, (void *)handle, 0);
}

/**
	@brief Process post channel opened with remote
	@param[in] wm_shell_msgq_data_item_t *
	@return wm_shell_glink_err_type
*/
wm_shell_glink_err_type wm_shell_glink_channel_state(wm_shell_msgq_data_item_t *msgq_event)
{

	/**
	 * By default, qcli starts in UART mode. We need to enforce ADB (GLINK) mode with the
	 * help of this GMI control packets. Sending ADB_START should enforce the GLINK mode
	 * This is only required for Q6-QCLI. Other interactions initialize the heap and close
	 * channel should remote/local request a disconnect.
	 */
#if CONFIG_GLINK_SHELL_PASSTHROUGH
	wm_shell_glink_err_type err;
	if ((glink_handle_type)(msgq_event->data_ptr) == q6_shell_glink_hdl)
	{
		is_q6_remote_open = true;
		err = wm_shell_glink_write(q6_shell_glink_hdl, GLINK_SHELL_WITH_Q6_GMI_CONTROL_START_IDX, NULL);
		if (err != WM_SHELL_SUCCESS)
		{
			return WM_SHELL_GLINK_TX_FAILED;
		}
	}
	else
#endif
	/**
 	* We need to init the heap for GLINKSHELL on AWM and SWM only once; this will happen
 	* during the AWM/SWM interaction on boot. Post this, any AWM/Q6 interaction does not
 	* require further init, and on SWM channel state shall not be reached again post the
 	* initial interaction is complete
 	*/
	{
		k_heap_init(&glink_shell_heap,
					(void *)glink_shell_heap_mem,
					CONFIG_GLINK_SHELL_TRANSPORT_BUF_SZ);
	}
	return WM_SHELL_SUCCESS;
}

/**
 @brief Queue Rx Intent to help queue the intent for a client.
 @param[in] glink_handle_type handle,const void *priv, size_t req_size
 @return bool status
*/
wm_shell_glink_err_type wm_shell_glink_rx_intent(wm_shell_msgq_data_item_t *msgq_event)
{
	glink_err_type glink_result;
	size_t glink_intent_size;

	glink_intent_size = msgq_event->size;
	glink_result = glink_queue_rx_intent((glink_handle_type)(msgq_event->data_ptr),
										NULL,
										glink_intent_size);
	if (glink_result != GLINK_STATUS_SUCCESS)
	{
		return WM_SHELL_RX_INTENT_FAILED;
	}

	return WM_SHELL_SUCCESS;
}

/**
 @brief Rx Intent Queue Callback to help queue the intent for a client.
 @param[in] glink_handle_type handle,const void *priv, size_t req_size
 @return bool status
*/
static bool wm_shell_glink_rx_intent_req_cb(glink_handle_type handle,
                                                const void *priv, size_t req_size)
{
#if CONFIG_GLINK_SHELL_PASSTHROUGH
	wm_shell_glink_send_to_msgq((handle == q6_shell_glink_hdl) ? &q6_shell_msgq : &glink_shell_msgq,
#else
	wm_shell_glink_send_to_msgq(&glink_shell_msgq,
#endif
								WM_SHELL_NOTIFY_RX_INTENT_REQ, (void *)handle, req_size);
	return true;
}

/* Alloc, Dealloc callbacks for intents will both consume GLINK_SHELL heap memory */
static glink_err_type wm_shell_glink_rx_intent_alloc_cb(glink_handle_type handle, const void *priv,
										const void *pkt_priv,
										size_t intent_size, void **buffer_ptr)
{
	*buffer_ptr = k_heap_alloc(&glink_shell_heap, intent_size, K_NO_WAIT);
	if (*buffer_ptr == NULL)
		return GLINK_STATUS_OUT_OF_RESOURCES;
	return GLINK_STATUS_SUCCESS;
}

static void wm_shell_glink_rx_intent_dealloc_cb(glink_handle_type handle, const void *priv,
										const void *pkt_priv,
										size_t intent_size, void *buffer_ptr)
{
	k_heap_free(&glink_shell_heap, buffer_ptr);
}

/**
 @brief Rx Callback to receive data over wm-shell-glink channel.

 @param[in] glink_handle_type handle,const void *priv, const void *pkt_priv,
               const void *ptr, size_t size, size_t intent_used
 @return none
*/
static void wm_shell_glink_rx_notification_cb(glink_handle_type handle, const void *priv,
                                                    const void *pkt_priv, const void *ptr,
                                                    size_t size, size_t intent_used)
{
#if CONFIG_GLINK_SHELL_PASSTHROUGH
	wm_shell_glink_send_to_msgq((handle == q6_shell_glink_hdl) ? &q6_shell_msgq : &glink_shell_msgq,
#else
	wm_shell_glink_send_to_msgq(&glink_shell_msgq,
#endif
								WM_SHELL_NOTIFY_RX, ptr, size);

#if CONFIG_GLINK_SHELL_PASSTHROUGH
	wm_shell_glink_send_to_msgq((handle == q6_shell_glink_hdl) ? &q6_shell_msgq : &glink_shell_msgq,
#else
	wm_shell_glink_send_to_msgq(&glink_shell_msgq,
#endif
								WM_SHELL_FREE_MEM, ptr, (size_t)handle);
}

/**
 @brief Tx done callback for wm-shell-glink channel
 @param[in] glink_handle_type handle,const void *priv, const void *pkt_priv,
               const void *ptr, size_t size
 @return none
*/
static void wm_shell_glink_tx_done_cb(glink_handle_type handle,const void *priv,
                                    const void *pkt_priv,const void *ptr, size_t size)
{
	k_heap_free(&glink_shell_heap, (void *)(ptr));
/**
 * SWM runs ZephyrShell with glink backend, which requires a TX_DONE event to be invoked
 * post a TX_DONE, to inform the shell of a completed TX, and ready to schedule another
 * queued TX, if any
 */
#if !CONFIG_GLINK_SHELL_PASSTHROUGH
	struct shell_glink *sh_glink = (struct shell_glink *)( \
			(const struct shell *)shell_backend_glink_get_ptr())->iface->ctx;
	sh_glink->handler(SHELL_TRANSPORT_EVT_TX_RDY, (void *)shell_backend_glink_get_ptr());
#endif

}

/**
 @brief Rx done for wm-shell-glink channel to signal to GLink layer that it is done
 * with the received data buffer.
 @param[in] Qcli_Adb_pipe_msg_t uint32 pipe_msg_opcode,const void *data_ptr, size_t size
 @return none
*/
wm_shell_glink_err_type wm_shell_glink_rx_done(wm_shell_msgq_data_item_t *msgq_event)
{
	glink_rx_done((glink_handle_type)(msgq_event->size),
				  msgq_event->data_ptr,
				  FALSE);

	return WM_SHELL_SUCCESS;
}

/**
 @brief Opens GLINK Channel
 @param[in] none
 @return none
*/
wm_shell_glink_err_type wm_shell_glink_open_channel(const char *q6_channel)
{
    glink_open_config_type open_cfg;
    glink_err_type glink_result;

    memset(&open_cfg, 0, sizeof(open_cfg));

	open_cfg.notify_state = wm_shell_glink_channel_state_cb;
    	open_cfg.notify_rx_intent_req = wm_shell_glink_rx_intent_req_cb;
    	open_cfg.notify_rx = wm_shell_glink_rx_notification_cb;
    	open_cfg.notify_tx_done = wm_shell_glink_tx_done_cb;
	open_cfg.options = GLINK_OPT_CLIENT_BUFFER_ALLOCATION;
	open_cfg.notify_allocate = wm_shell_glink_rx_intent_alloc_cb;
	open_cfg.notify_deallocate = wm_shell_glink_rx_intent_dealloc_cb;

#if CONFIG_GLINK_SHELL_PASSTHROUGH
	if (q6_channel == NULL)
	{
#endif
		open_cfg.remote_ss = remote_ss;
    	open_cfg.name = GLINK_SHELL_WITH_SWM_CTRL_CHANNEL_NAME;
    	open_cfg.priv = NULL;

		glink_result = glink_open(&open_cfg, &wm_shell_glink_hdl);

#if CONFIG_GLINK_SHELL_PASSTHROUGH
	}
	else
	{
		open_cfg.remote_ss = q6_remote_ss;
    	open_cfg.name = q6_channel;
    	open_cfg.priv = NULL;

		glink_result = glink_open(&open_cfg, &q6_shell_glink_hdl);
   	}
#endif

	if(glink_result != GLINK_STATUS_SUCCESS)
	{
		return WM_SHELL_GLINK_OPEN_FAILED;
	}
	return WM_SHELL_SUCCESS;
}

/**
 @brief Closes GLINK Channel
 @param[in] none
 @return none
*/
wm_shell_glink_err_type wm_shell_glink_close_channel(glink_handle_type handle)
{
    glink_err_type glink_result;

#if CONFIG_GLINK_SHELL_PASSTHROUGH
	/* Once close channel is called, clear the history of open channel with Q6 */
	if (handle == q6_shell_glink_hdl)
	{
		free(q6_shell_open_channel_name);
		q6_shell_open_channel_name = NULL;
		is_q6_remote_open = false;
	}
#endif

    glink_result = glink_close(handle);
    if(glink_result != GLINK_STATUS_SUCCESS)
    {
		return WM_SHELL_GLINK_CLOSE_FAILED;
    }

	return WM_SHELL_SUCCESS;
}

/**
 @brief This function is used to Tx a buffer over glink.
 @param Length is the length of the data to be written.
 @param Buffer is a pointer to the buffer to be written to the AM shell.
*/
wm_shell_glink_err_type wm_shell_glink_write(glink_handle_type handle, uint32_t Length, const char *Buffer)
{
    glink_err_type glink_result = 1;
    size_t size;
    void * buf;
	uint32_t gmi_ctrl = GLINK_SHELL_WITH_Q6_GMI_CONTROL_STOP_IDX;
    size = Length + sizeof(uint32_t) + sizeof(uint32_t);

	if (handle == NULL)
	{
		return WM_SHELL_GLINK_TX_FAILED;
	}

	/**
 	* Since SWM holds a single value of this longint (default),
	* we can reserve it for Q6
 	* Special arguments for length:
 	* If length is zero, send QCLI_ADB_GMI_QCLI_START_QCLI_OVER_ADB
 	* If length is UINT32_MAX, send QCLI_ADB_GMI_QCLI_STOP_QCLI_OVER_ADB
 	* Else, we know it is a data packet QCLI_ADB_GMI_QCLI_DATA
 	**/
#if CONFIG_GLINK_SHELL_PASSTHROUGH
	switch (Length)
	{
		case GLINK_SHELL_WITH_Q6_GMI_CONTROL_START_IDX:
			gmi_ctrl = QCLI_ADB_GMI_QCLI_START_QCLI_OVER_ADB;
			size = 8;
			Length = 0;
			break;
		case GLINK_SHELL_WITH_Q6_GMI_CONTROL_STOP_IDX:
			gmi_ctrl = QCLI_ADB_GMI_QCLI_STOP_QCLI_OVER_ADB;
			size = 8;
			Length = 0;
			break;
		default:
			gmi_ctrl = QCLI_ADB_GMI_QCLI_DATA;
			break;
	}
#endif

	buf = k_heap_alloc(&glink_shell_heap, size, K_NO_WAIT);
	if (buf == NULL)
	{
		return WM_SHELL_GLINK_TX_FAILED;
	}

	( (uint32_t*)buf)[0] = gmi_ctrl;
	( (uint32_t*)buf)[1] = Length;

	/* Account for the fact that when sending GMI control packets, Buffer will be NULL */
	if (Buffer != NULL && Length != 0)
		memscpy((char*)buf + 8, Length, Buffer, Length);

	/* Send over to Q6 or SWM shall be dictated by the handle passed to function */
	glink_result = glink_tx(handle,
							NULL,
							(uint32_t*)buf,
							size,
							GLINK_TX_REQ_INTENT);

    if(glink_result != GLINK_STATUS_SUCCESS)
    {
		k_heap_free(&glink_shell_heap, buf);
		return WM_SHELL_GLINK_TX_FAILED;
    }

	return WM_SHELL_SUCCESS;
}

wm_shell_glink_err_type wm_shell_print_recv_str(wm_shell_msgq_data_item_t *msgq_data)
{
	/* First 4 bytes possess the GMI control data (Don't care for SWM) */
	uint32_t raw_data_sz = (uint32_t)(*(((uint32_t*)(msgq_data->data_ptr)) + \
							GLINK_SHELL_WITH_SWM_DATA_SZ_OFFSET));
	
#if CONFIG_GLINK_SHELL_PASSTHROUGH && \
	(CONFIG_SHELL_BACKEND_SERIAL || CONFIG_QC_SHELL_BACKEND_SERIAL)
	/* Next 4 bytes possess the Length of the data buffer */
	uint32_t gmi_ctrl = (uint32_t)(*(((uint32_t*)(msgq_data->data_ptr))));
	char *str_container;
#endif

	/* The data buffer starts 8 bytes post received buffer */
	char *raw_data_ptr = (char *)(((uint32_t*)(msgq_data->data_ptr)) + \
							GLINK_SHELL_WITH_SWM_DATA_PTR_OFFSET);

/* Get UART shell pointer if we are AWM, else get the glink shell ptr */
#if CONFIG_GLINK_SHELL_PASSTHROUGH
#if	(CONFIG_SHELL_BACKEND_SERIAL || CONFIG_QC_SHELL_BACKEND_SERIAL)
	const struct shell *sh = (const struct shell *)(shell_backend_uart_get_ptr());
#endif
#else
	const struct shell *sh = (const struct shell *)(shell_backend_glink_get_ptr());
	struct shell_glink *sh_glink = (struct shell_glink *)sh->iface->ctx;
#endif

/**
 * If we are:
 * AWM receiving from SWM: print string to UART in MAGENTA
 * AWM receiving from Q6: print string to UART in YELLOW
 * SWM receiving from AWM: add received command data to shell RX buffer
 */
#if CONFIG_GLINK_SHELL_PASSTHROUGH
#if (CONFIG_SHELL_BACKEND_SERIAL || CONFIG_QC_SHELL_BACKEND_SERIAL)
	str_container = (char *)k_heap_alloc(&glink_shell_heap,
										(raw_data_sz + sizeof(char)),
										K_NO_WAIT);
	if (str_container == NULL)
	{
		return WM_SHELL_GLINK_RX_FAILED;
	}

	memscpy(str_container, raw_data_sz, raw_data_ptr, raw_data_sz);
	str_container[raw_data_sz] = '\0';
	if (gmi_ctrl == QCLI_ADB_GMI_QCLI_DATA)
		shell_fprintf(sh, SHELL_VT100_COLOR_YELLOW, "%s", str_container);
	else
		shell_fprintf(sh, SHELL_VT100_COLOR_MAGENTA, "%s", str_container);
	k_heap_free(&glink_shell_heap, str_container);
#endif
#else
	/* ZephyrShell on WM should handle this */
	memscpy(sh_glink->buf + sh_glink->len, raw_data_sz, raw_data_ptr, raw_data_sz);
	sh_glink->len += raw_data_sz;
	sh_glink->handler(SHELL_TRANSPORT_EVT_RX_RDY, (void*)sh);
#endif

	return WM_SHELL_SUCCESS;
}
