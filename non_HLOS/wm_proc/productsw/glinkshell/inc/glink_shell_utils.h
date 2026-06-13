/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
#ifndef __GLINK_UTILS_H__
#define __GLINK_UTILS_H__

#include <stdint.h>
#include "glink.h"

#define GLINK_SHELL_CONSTANT_LINEFEED			'\n'
#define GLINK_SHELL_CONSTANT_CARRIAGE_RETURN		'\r'
#define GLINK_SHELL_CONSTANT_NULLCHAR			'\0'

#define	GLINK_SHELL_XPORT_NAME				"SMEM"

#define GLINK_SHELL_WITH_Q6_REMOTESS			"lpass"
#define GLINK_SHELL_WITH_Q6_CTRL_CHANNEL_NAME		"q6-qcli"
#define GLINK_SHELL_WITH_Q6_GMI_CONTROL_STOP_IDX	0xFFFFFFFF
#define GLINK_SHELL_WITH_Q6_GMI_CONTROL_START_IDX	0x0

#define GLINK_SHELL_WITH_SWM_REMOTESS			"swm"
#define GLINK_SHELL_WITH_SWM_CTRL_CHANNEL_NAME		"wm-glink-shell"
#define GLINK_SHELL_WITH_SWM_DATA_SZ_OFFSET		1
#define GLINK_SHELL_WITH_SWM_DATA_PTR_OFFSET		2

#define GLINK_SHELL_WITH_AWM_REMOTESS			"awm"
#define GLINK_SHELL_WITH_AWM_CTRL_CHANNEL_NAME		"am-passthrough"

/**
 * Type defintion of possible msgq items.
 */
typedef enum
{
    WM_SHELL_LINK_STATE,             /* Link state */
    WM_SHELL_NOTIFY_STATE,           /* GLINK State */
    WM_SHELL_NOTIFY_RX_INTENT_REQ,   /* Rx Intent */
    WM_SHELL_NOTIFY_RX,              /* Rx Callback */
    WM_SHELL_NOTIFY_TX_DONE,         /* Tx done callback */
    WM_SHELL_FREE_MEM,               /* free AHB memory */
} wm_shell_msgq_opcode;

/**
 * To identify of Error Types.
 */
typedef enum
{
    WM_SHELL_SUCCESS,			  /* SUCCESS STATE */
    WM_SHELL_LINK_REG_FAILED,             /* Glink Link registration failed */
    WM_SHELL_RX_INTENT_FAILED,            /* RX Intent failed  */
    WM_SHELL_GLINK_OPEN_FAILED,           /* Glink open failed */
    WM_SHELL_GLINK_CLOSE_FAILED,          /* Glink close failed */
    WM_SHELL_GLINK_TX_FAILED,             /* Glink Tx Failed */
    WM_SHELL_GLINK_RX_FAILED,             /* Glink Rx Failed */
} wm_shell_glink_err_type;

/**Data item struct for wm_shell to handle queue*/
typedef struct wm_shell_msgq_data_item {
    uint32_t msgq_opcode;                     	   /* To identify event */
    const void *data_ptr;                          /* Pointer to data sent over glink*/
    size_t size;                                   /* Size of payload */
} wm_shell_msgq_data_item_t;

/**
 * Opcodes to be received on slate-qcli channel.
 */
typedef enum {
    QCLI_ADB_GMI_QCLI_DATA,
    QCLI_ADB_GMI_QCLI_START_QCLI_OVER_ADB,
    QCLI_ADB_GMI_QCLI_STARTED_QCLI_OVER_ADB,
    QCLI_ADB_GMI_QCLI_STOP_QCLI_OVER_ADB,
    QCLI_ADB_GMI_QCLI_STOPPED_QCLI_OVER_ADB
} QcliAdbGmiSlateChnlOpcode;

/**
 @brief Glink Link registration for wm_shell/q6_qcli glink backend.
       Once succesful the "wm_shell_glink_link_state_Cb"
       callback is called from Glink core.
 @param[in] none
 @return wm_shell_glink_err_type
*/
wm_shell_glink_err_type wm_shell_glink_start(void);
wm_shell_glink_err_type q6_shell_glink_start(void);

/**
@brief Glink Link state callback for wm_shell/q6_qcli glink backend.
	  Callback is called from Glink core.
@param[in] glink_link_info_type Get the link info from remote
@return none
*/
void wm_shell_glink_link_state_cb(glink_link_info_type *link_info,void *priv);

/**
 @brief Opens GLINK Channel
 @param[in] char *: holds the q6_channel name
 @return wm_shell_glink_err_type
*/
wm_shell_glink_err_type wm_shell_glink_open_channel(const char *q6_channel);

/**
 @brief Closes GLINK Channel
 @param[in] glink_handle_type: Can accept wm_shell or q6_shell_glink_hdl to address transactions
				between AWM/SWM or AWM/Q6 respectively
 @return wm_shell_glink_err_type
*/
wm_shell_glink_err_type wm_shell_glink_close_channel(glink_handle_type handle);

/**
 @brief Glink Link State is received for STATE_UP and STATE_DOWN
 @param[in] wm_shell_msgq_data_item_t *: receive msgq data item with link info from state machine
 @return wm_shell_glink_err_type
*/
wm_shell_glink_err_type wm_shell_glink_get_link_state(wm_shell_msgq_data_item_t *msgq_event);

/**
 @brief Process post channel opened with remote
 @param[in] wm_shell_msgq_data_item_t *: receive channel state info from state machine
 @return wm_shell_glink_err_type
*/
wm_shell_glink_err_type wm_shell_glink_channel_state(wm_shell_msgq_data_item_t *msgq_event);

/**
 @brief Queue Rx Intent to help queue the intent for a client.
 @param[in] wm_shell_msgq_data_item_t *: receive intent requirement info from state machine
 @return wm_shell_glink_err_type
*/
wm_shell_glink_err_type wm_shell_glink_rx_intent(wm_shell_msgq_data_item_t *msgq_event);

/**
 @brief Tx done for wm_shell channel free dynamicaly allocated memory.
 @param[in] wm_shell_msgq_data_item_t *msgq_event
 @return none
*/
//wm_shell_glink_err_type wm_shell_glink_tx_done(wm_shell_msgq_data_item_t *msgq_event);

/**
 @brief Write back to open channel
 @param[in] glink_handle_type:  Can accept wm_shell or q6_shell_glink_hdl to address transactions
				between AWM/SWM or AWM/Q6 respectively
 @param[in] data_len is the length of the data to be written
 @param[in] data_buf is a pointer to the buffer to be written
 @return wm_shell_glink_err_type
*/
wm_shell_glink_err_type wm_shell_glink_write(glink_handle_type handle, uint32_t data_len, const char *data_buf);

/**
 @brief Write to read any received buffers
 @param[in] msgq_data: message queue data item
 @return wm_shell_glink_err_type
*/
wm_shell_glink_err_type wm_shell_print_recv_str(wm_shell_msgq_data_item_t *msgq_data);

/**
 @brief Rx done for qcli_adb channel to signal to GLink layer that it is done
 * with the received data buffer.
 @param[in] wm_shell_msgq_data_item_t *msgq_data_item
 @return wm_shell_glink_err_type
*/
wm_shell_glink_err_type wm_shell_glink_rx_done(wm_shell_msgq_data_item_t *msgq_event);

#endif
