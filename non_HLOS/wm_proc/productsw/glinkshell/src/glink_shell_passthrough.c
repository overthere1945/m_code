/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
#include <stdlib.h>
#include <string.h>
#include <stringl.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <zephyr/init.h>

#include "glink_shell.h"
#include "glink_shell_utils.h"

/* Zephyr MSGQ initialized for GLINK state machine */
K_MSGQ_DEFINE(glink_shell_msgq,
				  sizeof(wm_shell_msgq_data_item_t),
				  CONFIG_GLINK_SHELL_MSGQ_ELEM_COUNT, 1);

#if CONFIG_GLINK_SHELL_PASSTHROUGH
K_MSGQ_DEFINE(q6_shell_msgq,
			  sizeof(wm_shell_msgq_data_item_t),
			  CONFIG_GLINK_SHELL_MSGQ_ELEM_COUNT, 1);
#endif

/* Keeps track of q6 channel currently open with AWM */
char *q6_shell_open_channel_name = NULL;
/* Glink handle for interactions with Q6 */
extern glink_handle_type q6_shell_glink_hdl;
/* Glink handle for interactions with SWM */
extern glink_handle_type wm_shell_glink_hdl;
/* Check if Q6 is open remotely as well */
extern bool is_q6_remote_open;
/**
 * State Machine:
 * STATE 0: After receiving link state, try opening channel
 * STATE 1: After opening channel, create handler code for packets TODO!!!
 * STATE 2: Queue intents for RX to host
 * STATE 3: Handler post GLINK RX to host complete
 * STATE 4: Clean-up crew post GLINK TX from host
 * STATE 5: Clean-up crew post GLINK RX to host
 **/
wm_shell_glink_err_type (*glink_shell_state_machine[])(wm_shell_msgq_data_item_t *) = \
{
	wm_shell_glink_get_link_state,
#if CONFIG_GLINK_SHELL_PASSTHROUGH
	wm_shell_glink_channel_state,
#else
	enable_shell_glink,
#endif
	wm_shell_glink_rx_intent,
	wm_shell_print_recv_str,
	NULL,
	wm_shell_glink_rx_done
};

/* Main thread for Thread on Host (AWM/SWM or Q6) */
void glink_shell_thread(void *msgq_ctx, void *arg2, void *arg3)
{
	wm_shell_msgq_data_item_t shell_glink_msgq_item;

	/* Enquire for state of LINK between SWM<->AWM */
#if CONFIG_GLINK_SHELL_PASSTHROUGH
	if (msgq_ctx == (void *)&q6_shell_msgq)
	{
		q6_shell_glink_start();
	}
	else
#endif
	{
		wm_shell_glink_start();
	}

	/* Begin State Machine */
	while (1)
	{
		/* Check for any triggered callbacks from GLINK */
		k_msgq_get(msgq_ctx,
			   &shell_glink_msgq_item,
			   K_FOREVER);

		/* Process triggered callback */
		if (glink_shell_state_machine[shell_glink_msgq_item.msgq_opcode] != NULL)
			glink_shell_state_machine[shell_glink_msgq_item.msgq_opcode](&shell_glink_msgq_item);
	}
}

/**
 * Start thread for interaction between (AWM/SWM).
 * Will initiate on boot as per priority
 */
K_THREAD_DEFINE(glink_thread_ctx,
		CONFIG_GLINK_SHELL_THREAD_STACK_SZ,
                glink_shell_thread, &glink_shell_msgq, NULL, NULL,
		CONFIG_GLINK_SHELL_THREAD_PRIORITY, 0, 0);

#if CONFIG_GLINK_SHELL_PASSTHROUGH
K_THREAD_STACK_DEFINE(q6_qcli_thread_stack, CONFIG_GLINK_SHELL_THREAD_STACK_SZ);
struct k_thread q6_qcli_thread_ctx;
k_tid_t q6_qcli_thread_tid = NULL;
#endif

/* AWM passthrough menus: Don't require this in SWM */
#if CONFIG_GLINK_SHELL_PASSTHROUGH
static int q6_passthrough_open_channel(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t ch_name_len = strlen(argv[1]);

	/* Track the channel that shall be opened, if successful */
	q6_shell_open_channel_name = (char *)malloc(sizeof(char) * (ch_name_len + 1));
	if (q6_shell_open_channel_name == NULL)
	{
		shell_fprintf(sh, SHELL_ERROR, "q6_pass: open_ch: Failed to allocate %d bytes\n",
					 (ch_name_len + sizeof(GLINK_SHELL_CONSTANT_NULLCHAR)));
		return WM_SHELL_GLINK_OPEN_FAILED;
	}

	memscpy(q6_shell_open_channel_name, ch_name_len, argv[1], ch_name_len);
	q6_shell_open_channel_name[ch_name_len] = GLINK_SHELL_CONSTANT_NULLCHAR;

	if (q6_qcli_thread_tid == NULL)
	{
		q6_qcli_thread_tid = k_thread_create(&q6_qcli_thread_ctx, q6_qcli_thread_stack,
                    	K_THREAD_STACK_SIZEOF(q6_qcli_thread_stack),
                    	glink_shell_thread,
                    	&q6_shell_msgq, NULL, NULL,
                    	CONFIG_GLINK_SHELL_THREAD_PRIORITY, 0, K_NO_WAIT);
		shell_fprintf(sh, SHELL_INFO, "q6_pass: open_ch: Q6-QCLI thread created\n");
	}
	else
	{
		q6_shell_glink_start();
	}

	return WM_SHELL_SUCCESS;
}

static int q6_passthrough_close_channel(const struct shell *sh, size_t argc, char **argv)
{
	glink_err_type glink_result;
	wm_shell_glink_err_type err;

	if (q6_shell_glink_hdl == NULL)
	{
		shell_error(sh, "q6_pass: No channel open\n");
		is_q6_remote_open = false;
		if (q6_shell_open_channel_name != NULL)
		{
			free(q6_shell_open_channel_name);
			q6_shell_open_channel_name = NULL;
		}

		return WM_SHELL_GLINK_CLOSE_FAILED;
	}

	if (is_q6_remote_open == true)
	{
		/* Send packet with QCLI_ADB_GMI_QCLI_STOP_QCLI_OVER_ADB */
		err = wm_shell_glink_write(q6_shell_glink_hdl, (uint32_t)(GLINK_SHELL_WITH_Q6_GMI_CONTROL_STOP_IDX), NULL);
		if (err != WM_SHELL_SUCCESS)
		{
			return WM_SHELL_GLINK_TX_FAILED;
		}
	}

	/* Close the channel and clean up pointers */
	glink_result = glink_close(q6_shell_glink_hdl);
	if(glink_result != GLINK_STATUS_SUCCESS)
	{
		return WM_SHELL_GLINK_CLOSE_FAILED;
	}

	q6_shell_glink_hdl = NULL;
	is_q6_remote_open = false;

	if (q6_shell_open_channel_name != NULL)
	{
		free(q6_shell_open_channel_name);
		q6_shell_open_channel_name = NULL;
	}

	return WM_SHELL_SUCCESS;
}

/* Display currently open channel with Q6 */
static int q6_passthrough_get_channel_name(const struct shell *sh, size_t argc, char **argv)
{
	if (q6_shell_open_channel_name != NULL)
		shell_print(sh, "q6_pass: Channel %s open\n", q6_shell_open_channel_name);
	else
		shell_error(sh, "q6_pass: No channel open\n");
	return WM_SHELL_SUCCESS;
}

/* Send QCLI command to q6 */
static int q6_passthrough_send_cmd(const struct shell *sh, size_t argc, char **argv)
{
	char *full_cmd = NULL;
	uint32_t user_cmd_sz, full_cmd_sz;
	wm_shell_glink_err_type err;

	if (is_q6_remote_open == false)
	{
		shell_error(sh, "q6_pass: No channel open\n");
		return WM_SHELL_GLINK_TX_FAILED;
	}

	/**
	 * QCLI expects a carriage return at the end of the command
	 * Effort to append carriage return char to command string
	 */
	user_cmd_sz = strlen(argv[1]);
	full_cmd_sz = user_cmd_sz + sizeof((char)GLINK_SHELL_CONSTANT_CARRIAGE_RETURN);
	full_cmd = (char *)malloc(sizeof(char) * (full_cmd_sz + sizeof((char)GLINK_SHELL_CONSTANT_NULLCHAR)));
	if (full_cmd == NULL)
	{
		shell_fprintf(sh, SHELL_ERROR, "q6_pass: failed to allocate %d bytes\n", \
				full_cmd_sz + sizeof((char)GLINK_SHELL_CONSTANT_NULLCHAR));
		free(full_cmd);
		return WM_SHELL_GLINK_TX_FAILED;
	}

	memscpy(full_cmd, user_cmd_sz, argv[1], user_cmd_sz);
	full_cmd[user_cmd_sz] = (char)GLINK_SHELL_CONSTANT_CARRIAGE_RETURN;
	full_cmd[full_cmd_sz] = (char)GLINK_SHELL_CONSTANT_NULLCHAR;

	/* Push command over to Q6 post process */
	err = wm_shell_glink_write(q6_shell_glink_hdl, full_cmd_sz, full_cmd);
	free(full_cmd);
	if (err != WM_SHELL_SUCCESS)
	{
		return WM_SHELL_GLINK_TX_FAILED;
	}

	return WM_SHELL_SUCCESS;
}

/* Send ZephyrShell command to WM */
static int wm_passthrough_send_cmd(const struct shell *sh, size_t argc, char **argv)
{
	char *full_cmd = NULL;
	uint32_t user_cmd_sz, full_cmd_sz;
	wm_shell_glink_err_type err;

	/**
	 * ZephyrShell expects carriage return and linefeed at the end of a cmd
	 * Effort to append both to the command string
	 */
	user_cmd_sz = strlen(argv[1]);
	full_cmd_sz = user_cmd_sz + sizeof((char)GLINK_SHELL_CONSTANT_CARRIAGE_RETURN) + \
								sizeof((char)GLINK_SHELL_CONSTANT_LINEFEED);
	full_cmd = (char *)malloc(sizeof(char) * (full_cmd_sz + sizeof((char)GLINK_SHELL_CONSTANT_NULLCHAR)));
	if (full_cmd == NULL)
	{
		shell_fprintf(sh, SHELL_ERROR, "wm_pass: failed to allocate %d bytes\n", \
				full_cmd_sz + sizeof((char)GLINK_SHELL_CONSTANT_NULLCHAR));
		free(full_cmd);
		return WM_SHELL_GLINK_TX_FAILED;
	}

	memscpy(full_cmd, user_cmd_sz, argv[1], user_cmd_sz);
	full_cmd[user_cmd_sz] = (char)GLINK_SHELL_CONSTANT_CARRIAGE_RETURN;
	full_cmd[user_cmd_sz+sizeof((char)GLINK_SHELL_CONSTANT_CARRIAGE_RETURN)] = (char)GLINK_SHELL_CONSTANT_LINEFEED;
	full_cmd[full_cmd_sz] = (char)GLINK_SHELL_CONSTANT_NULLCHAR;

	/* Push command over to WM post process */
	err = wm_shell_glink_write(wm_shell_glink_hdl, full_cmd_sz, full_cmd);
	free(full_cmd);
	if (err != WM_SHELL_SUCCESS)
	{
		return WM_SHELL_GLINK_TX_FAILED;
	}

	return WM_SHELL_SUCCESS;
}

/* Register commands with ZephyrShell on AWM */
SHELL_STATIC_SUBCMD_SET_CREATE(wm_passthrough_cmds,
		SHELL_CMD_ARG(send_cmd, NULL,
		      "Send CMD over GLINK to WM_SHELL\n"
		      "Usage: wm_pass send_cmd [cmd_str]",
		      wm_passthrough_send_cmd, 2, 2),
		SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_STATIC_SUBCMD_SET_CREATE(q6_passthrough_cmds,
		SHELL_CMD_ARG(open_ch, NULL,
		      "Open specified channel with Q6\n"
		      "Usage: q6_pass open_ch [channel_name]\n",
		      q6_passthrough_open_channel, 2, 1),
		SHELL_CMD_ARG(close_ch, NULL,
		      "Close opened channel with Q6\n"
		      "Usage: q6_pass close_ch\n",
		      q6_passthrough_close_channel, 1, 0),
		SHELL_CMD_ARG(curr_ch_name, NULL,
		      "Get currently open GLINK channel with Q6\n"
		      "Usage: q6_pass curr_ch_name\n",
		      q6_passthrough_get_channel_name, 1, 0),
		SHELL_CMD_ARG(send_cmd, NULL,
		      "Send CMD over GLINK to Q6-QCLI\n"
		      "Usage: q6_pass send_cmd [cmd_str]\n",
		      q6_passthrough_send_cmd, 2, 1),
		SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(wm_pass, &wm_passthrough_cmds, "WM_SHELL Commands Relay\n", NULL);
SHELL_CMD_REGISTER(q6_pass, &q6_passthrough_cmds, "Q6-QCLI Commands Relay\n", NULL);
#endif
