/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
#include <stringl.h>
#include "glink_shell_utils.h"
#include "glink_shell.h"
#include "glink.h"
#include <zephyr/init.h>
#include "sys_m_messages.h"
#include "rcecb.h"

/**
 * Registering a callback in teardown sequence
 * This is to prevent glinkshell servicing any writes
 * back to AM during teardown
 */
void shutdown_req_p2c_cb( void )
{
	const struct shell *sh = (const struct shell *)shell_backend_glink_get_ptr();
	sh->iface->api->uninit(sh->iface);

	/**
	 * shell_stop internally calls z_shell_log_backend_disabled
	 * This is an effort to de-initialize the backend
	 */
	shell_stop(sh);
}

/* Create another backend for shell via GLINK */
SHELL_GLINK_DEFINE(shell_transport_glink);
SHELL_DEFINE(shell_glink, CONFIG_GLINK_SHELL_PROMPT, &shell_transport_glink, 256,
	     0, SHELL_FLAG_OLF_CRLF);

/* Reference to GLINK handle */
extern glink_handle_type wm_shell_glink_hdl;

/* Callback invoked when on shell start */
static int init(const struct shell_transport *transport,
		const void *config,
		shell_transport_handler_t evt_handler,
		void *context)
{
	struct shell_glink *sh_glink = (struct shell_glink *)transport->ctx;

	if (sh_glink == NULL || sh_glink->initialized) {
		return -EINVAL;
	}

	/**
	 * Registering the callback with sysmon
	 */
	rcecb_register_name(SYS_M_SHUTDOWN, shutdown_req_p2c_cb);

	sh_glink->len = 0;
	sh_glink->handler = evt_handler;

	/**
	 * When SWM, our channel state callback does not invoke channel_state
	 * function from the state machine. It invokes enable_shell_glink,
	 * which inevitably lands here. Hence we need to invoke channel state
	 * explicitly
	 */
	wm_shell_glink_channel_state(NULL);
	sh_glink->initialized = true;

	return 0;
}

/* Callback invoked when shell wants to clean up */
static int uninit(const struct shell_transport *transport)
{
	struct shell_glink *sh_glink = (struct shell_glink *)transport->ctx;

	if (sh_glink == NULL || !sh_glink->initialized) {
		return -ENODEV;
	}

	wm_shell_glink_close_channel(wm_shell_glink_hdl);

	sh_glink->initialized = false;

	return 0;
}

/* Since glinkshell relies on glink_shell_thread for instantiating, nothing here */
static int enable(const struct shell_transport *transport, bool blocking)
{
	struct shell_glink *sh_glink = (struct shell_glink *)transport->ctx;

	/**
	 *	Prevent enabling of shell if blocking: will not support log_backend state
	 *		- Enablement in blocking mode post PANIC
	 *		- Enablement in IMMEDIATE logging mode
	 */
	if (sh_glink == NULL || !sh_glink->initialized || blocking) {
		return -ENODEV;
	}

	return 0;
}

/* Any shell_(f)printfs will find themselves here. Write back to AWM */
static int write(const struct shell_transport *transport,
		 const void *data, size_t length, size_t *cnt)
{
	struct shell_glink *sh_glink = (struct shell_glink *)transport->ctx;

	*cnt = length;
	if (!data || !length)
	{
		return -EINVAL;
	}

	if (sh_glink == NULL) {
		return -ENODEV;
	}

	/**
	 * Ideally, we want to just return failure, in case the shell is not inited
	 * However, ZephyrShell's insistence to retry post failure means we need to
	 * not process writes, as opposed to returning failure
	 */
	if (sh_glink->initialized)
	{
		wm_shell_glink_write(wm_shell_glink_hdl, (uint32_t)length, (const char *)data);
	}

	return 0;
}

/* Callback invoked when we receive data on the shell's RX buffer */
static int read(const struct shell_transport *transport,
		void *data, size_t length, size_t *cnt)
{
	struct shell_glink *sh_glink = (struct shell_glink *)transport->ctx;

	if (sh_glink == NULL || !sh_glink->initialized || sh_glink->len == 0) {
		*cnt = 0;
		return -ENODEV;
	}

	if (length > sh_glink->len) {
		length = sh_glink->len;
	}

	memscpy(data, length, sh_glink->buf + *cnt, length);
	sh_glink->len -= length;
	*cnt += length;

	return 0;
}

/* glink_shell_thread takes care of every event. Nothing here */
static void update(const struct shell_transport *transport)
{
	struct shell_glink *sh_glink = (struct shell_glink *)transport->ctx;

	if (sh_glink == NULL || !sh_glink->initialized) {
		return;
	}
}

/* Register all shell backend callbacks */
const struct shell_transport_api shell_glink_transport_api = {
	.init = init,
	.uninit = uninit,
	.enable = enable,
	.write = write,
	.read = read,
	.update = update
};

/* Invoked once glink successfully opens channel locally and remotely */
wm_shell_glink_err_type enable_shell_glink(wm_shell_msgq_data_item_t *msgq_item)
{
	bool log_backend = (CONFIG_GLINK_SHELL_LOG_LEVEL > 0);
	uint32_t level = (CONFIG_GLINK_SHELL_LOG_LEVEL > LOG_LEVEL_DBG) ?
		      CONFIG_LOG_MAX_LEVEL : CONFIG_GLINK_SHELL_LOG_LEVEL;

	/* Disable all shell features: this shell is passive */
	static const struct shell_backend_config_flags cfg_flags = {0,0,0,0,0,1};

	shell_init(&shell_glink, NULL, cfg_flags, log_backend, level);

	return 0;
}

const struct shell *shell_backend_glink_get_ptr(void)
{
	return &shell_glink;
}
