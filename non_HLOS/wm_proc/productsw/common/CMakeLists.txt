# ------------------------------------------------------------------------------
#  Copyright (c) Qualcomm Technologies, Inc.
#  All rights reserved.
#  Confidential and Proprietary - Qualcomm Technologies, Inc.
# ------------------------------------------------------------------------------
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

if(CONFIG_PDTSW_COMMON_MODULES_ENABLE)

  zephyr_include_directories(
    ${CORE_MPROC_API}
    ${PDTSW_COMMON_API}
    ${PROD_SW_ROOT}/ect/wdss_lpi_infra
    ${PROD_SW_ROOT}/qapi/inc
  )

  set(PDTSW_COMMON_SRC)

  list(APPEND PDTSW_COMMON_SRC src/pdtsw_sys_init.c)
  list(APPEND PDTSW_COMMON_SRC src/pdtsw_platform.c)

  if(CONFIG_PDTSW_COMMON_STATE_MACHINE_ENABLE)
    list(APPEND PDTSW_COMMON_SRC src/pdtsw_sm.c)
  endif()

  if(CONFIG_PDTSW_COMMON_HEAP_MGR_ENABLE)
    list(APPEND PDTSW_COMMON_SRC src/pdtsw_heap.c)
  endif()

  if(CONFIG_PDTSW_COMMON_WORKER_THREAD_ENABLE)
    list(APPEND PDTSW_COMMON_SRC src/pdtsw_worker_thread.c)
  endif()

  if(CONFIG_PDTSW_GLINK_STUB_ENABLE)
    list(APPEND PDTSW_COMMON_SRC src/pdtsw_glink_stub.c)
  else()
    list(APPEND PDTSW_COMMON_SRC src/pdtsw_glink.c)
  endif()

  if(CONFIG_PDTSW_COMMON_UTILS_ENABLE)
    list(APPEND PDTSW_COMMON_SRC src/pdtsw_util.c)
  endif()

  zephyr_library_sources(${PDTSW_COMMON_SRC})

else()
    message(STATUS "PDTSW: Common module is disabled")
endif()