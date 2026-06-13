# ------------------------------------------------------------------------------
#  Copyright (c) Qualcomm Technologies, Inc.
#  All rights reserved.
#  Confidential and Proprietary - Qualcomm Technologies, Inc.
# ------------------------------------------------------------------------------
# Start with an empty list
set(OFFLOAD_MGR_SOURCES)

# Always include the core source
list(APPEND OFFLOAD_MGR_SOURCES src/offload_mgr.c)

# Conditionally add sources based on configuration flags
if(CONFIG_PDTSW_OFFLOAD_MGR_AUDIO_ENABLE OR CONFIG_PDTSW_OFFLOAD_MGR_HAPTICS_ENABLE OR CONFIG_PDTSW_COMMON_UTILS_ENABLE)
  list(APPEND OFFLOAD_MGR_SOURCES src/offload_utility_hal.c)
endif()

if(CONFIG_PDTSW_OFFLOAD_MGR_RSB_ENABLE)
  list(APPEND OFFLOAD_MGR_SOURCES src/offload_rsb.c)
endif()

# Register the sources with the Zephyr library
zephyr_library_sources_ifdef(CONFIG_PDTSW_OFFLOAD_MGR_ENABLE ${OFFLOAD_MGR_SOURCES})

# Start with a base list of include directories
set(OFFLOAD_MGR_INCLUDES
  inc/
  api/
  ${PROD_SW_ROOT}/common
  ${CORE_MPROC_API}
)

# Conditionally add include directories
if(CONFIG_PDTSW_OFFLOAD_MGR_AUDIO_ENABLE)
  list(APPEND OFFLOAD_MGR_INCLUDES ${PROD_SW_ROOT}/audio_service/inc)
endif()

if(CONFIG_PDTSW_OFFLOAD_MGR_HAPTICS_ENABLE)
  list(APPEND OFFLOAD_MGR_INCLUDES ${PROD_SW_ROOT}/haptics_service/inc)
endif()

if(CONFIG_PDTSW_OFFLOAD_MGR_RSB_ENABLE)
  list(APPEND OFFLOAD_MGR_INCLUDES ${PROD_SW_ROOT}/qapi/inc)
endif()

if(CONFIG_PDTSW_COMMON_UTILS_ENABLE)
  list(APPEND OFFLOAD_MGR_INCLUDES ${PROD_SW_ROOT}/common)
endif()

# Apply the include directories
zephyr_include_directories(${OFFLOAD_MGR_INCLUDES})