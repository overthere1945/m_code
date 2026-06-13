# ------------------------------------------------------------------------------
#  Copyright (c) Qualcomm Technologies, Inc.
#  All rights reserved.
#  Confidential and Proprietary - Qualcomm Technologies, Inc.
# ------------------------------------------------------------------------------
zephyr_include_directories(
  inc/
  ${CORE_MPROC_API}
)

if(CONFIG_GLINK_SHELL)
	zephyr_sources(
	  src/glink_shell_passthrough.c
	  src/glink_shell_utils.c
	)

	zephyr_sources_ifndef(
	  CONFIG_GLINK_SHELL_PASSTHROUGH
	  src/glink_shell.c
	)
endif()
