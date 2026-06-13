/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "pdtsw_glink.h"

LOG_MODULE_REGISTER(pdtsw_glink_stub, LOG_LEVEL_DBG);

glink_err_type pdtsw_glink_register_link_state_cb(glink_link_id_type *link_id, void* priv) {
    LOG_DBG("pdtsw_glink_register_link_state_cb called\n");
    return 0;
}

glink_err_type pdtsw_glink_deregister_link_state_cb(glink_link_handle_type handle) {
    LOG_DBG("pdtsw_glink_deregister_link_state_cb called\n");
    return 0;
}

glink_err_type pdtsw_glink_open(glink_open_config_type *cfg_ptr, glink_handle_type *handle) {
    LOG_DBG("pdtsw_glink_open called\n");
    return 0;
}

glink_err_type pdtsw_glink_close(glink_handle_type handle) {
    LOG_DBG("pdtsw_glink_close called\n");
    return 0;
}

glink_err_type pdtsw_glink_tx(glink_handle_type handle, const void *pkt_priv, const void *data, size_t size, uint32_t options) {
    LOG_DBG("pdtsw_glink_tx called\n");
    return 0;
}

glink_err_type pdtsw_glink_txv(glink_handle_type handle, const void *pkt_priv, void *iovec, size_t size, glink_buffer_provider_fn vprovider, glink_buffer_provider_fn pprovider, uint32_t options) {
    LOG_DBG("pdtsw_glink_txv called\n");
    return 0;
}

glink_err_type pdtsw_glink_queue_rx_intent(glink_handle_type handle, const void *pkt_priv, size_t size) {
    LOG_DBG("pdtsw_glink_queue_rx_intent called\n");
    return 0;
}

glink_err_type pdtsw_glink_rx_done(glink_handle_type handle, const void *ptr, bool reuse) {
    LOG_DBG("pdtsw_glink_rx_done called\n");
    return 0;
}

glink_err_type pdtsw_glink_sigs_set(glink_handle_type handle, uint32_t sig_value) {
    LOG_DBG("pdtsw_glink_sigs_set called\n");
    return 0;
}

glink_err_type pdtsw_glink_sigs_local_get(glink_handle_type handle, uint32_t *sig_value) {
    LOG_DBG("pdtsw_glink_sigs_local_get called\n");
    return 0;
}

glink_err_type pdtsw_glink_sigs_remote_get(glink_handle_type handle, uint32_t *sig_value) {
    LOG_DBG("pdtsw_glink_sigs_remote_get called\n");
    return 0;
}