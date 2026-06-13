/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
#ifndef __SHELL_GLINK_H__
#define __SHELL_GLINK_H__

#include "glink_shell_utils.h"
#include <zephyr/shell/shell.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const struct shell_transport_api shell_glink_transport_api;

struct shell_glink {
	bool initialized;

	/** current number of bytes in buffer (0 if no output) */
	size_t len;

	/** output buffer to collect shell output */
	char buf[CONFIG_GLINK_SHELL_TRANSPORT_BUF_SZ];

	/** Caller for signals */
	shell_transport_handler_t handler;
};

#define SHELL_GLINK_DEFINE(_name)					\
	static struct shell_glink _name##_shell_glink;			\
	struct shell_transport _name = {				\
		.api = &shell_glink_transport_api,			\
		.ctx = (struct shell_glink *)&_name##_shell_glink	\
	}

/**
 * @brief This function shall not be used directly. It provides pointer to shell
 *	  glink backend instance.
 *
 * Function returns pointer to the shell glink instance. This instance can be
 * next used with shell_execute_cmd function in order to test commands behavior.
 *
 * @returns Pointer to the shell instance.
 */
const struct shell *shell_backend_glink_get_ptr(void);

wm_shell_glink_err_type enable_shell_glink(wm_shell_msgq_data_item_t *msgq_item);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_GLINK_H__ */
