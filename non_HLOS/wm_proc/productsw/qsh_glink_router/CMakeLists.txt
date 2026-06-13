if(CONFIG_PDTSW_QSH_GLINK_ROUTER_ENABLE STREQUAL "y")
  find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

  list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
  include(nanopb)

  zephyr_library_named(qsh_glink_router_lib)

  set(PROTO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../sensinghub)
  set(NANOPB_IMPORT_DIRS
    ${PROTO_ROOT}
    ${PROTO_ROOT}/api/pb
  )

  set(QSH_ROUTER_PROTOS
    ${PROTO_ROOT}/api/pb/sns_client_glink.proto
    ${PROTO_ROOT}/api/pb/sns_client_sensor.proto
    ${PROTO_ROOT}/api/pb/sns_client.proto
    ${PROTO_ROOT}/api/pb/sns_physical_sensor_test.proto
    ${PROTO_ROOT}/api/pb/sns_std_event_gated_sensor.proto
    ${PROTO_ROOT}/api/pb/sns_std_sensor.proto
    ${PROTO_ROOT}/api/pb/sns_std_type.proto
    ${PROTO_ROOT}/api/pb/sns_suid.proto
    ${PROTO_ROOT}/api/pb/sns_std.proto
  )
  list(REMOVE_DUPLICATES QSH_ROUTER_PROTOS)

  nanopb_generate_cpp(NPB_ROUTER_SRCS NPB_ROUTER_HDRS
    ${QSH_ROUTER_PROTOS}
    RELPATH ${PROTO_ROOT}
  )
  set_source_files_properties(${NPB_ROUTER_SRCS} ${NPB_ROUTER_HDRS} PROPERTIES GENERATED TRUE)

  # drop nanopb core sources so they don't collide with modules__nanopb ---
  # Some FindNanopb.cmake add these into the returned list; filter them out.
  set(_filtered_router_srcs "")
  foreach(_f IN LISTS NPB_ROUTER_SRCS)
    get_filename_component(_name "${_f}" NAME)
    if(NOT _name MATCHES "^pb_(encode|decode|common)\\.c$")
      list(APPEND _filtered_router_srcs "${_f}")
    endif()
  endforeach()
  if(NOT _filtered_router_srcs STREQUAL "${NPB_ROUTER_SRCS}")
    message(STATUS "qsh_glink_router: filtered nanopb core sources from NPB_ROUTER_SRCS")
  endif()
  set(NPB_ROUTER_SRCS "${_filtered_router_srcs}")

  target_sources(qsh_glink_router_lib PRIVATE ${NPB_ROUTER_SRCS})

  target_include_directories(qsh_glink_router_lib PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/api/pb
  )

  target_include_directories(qsh_glink_router_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${PROD_SW_ROOT}/qapi/inc
    ${CORE_MPROC_API}
    ${PDTSW_COMMON_API}
  )

  target_sources(qsh_glink_router_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qsh_glink_router.c
  )
endif()