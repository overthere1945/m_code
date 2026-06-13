/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#include <stdint.h>
#include "pdtsw_glink.h"

glink_err_type pdtsw_glink_register_link_state_cb
(
  glink_link_id_type *link_id,
  void* priv
)
{
    return glink_register_link_state_cb(link_id, priv);
}

glink_err_type pdtsw_glink_deregister_link_state_cb
(
  glink_link_handle_type handle
)
{
    return glink_deregister_link_state_cb(handle);
}

glink_err_type pdtsw_glink_open
(
  glink_open_config_type *cfg_ptr,
  glink_handle_type      *handle
)
{
    return glink_open(cfg_ptr, handle);
}

glink_err_type pdtsw_glink_close
(
  glink_handle_type handle
)
{
    return glink_close(handle);
}

glink_err_type pdtsw_glink_tx
(
  glink_handle_type handle,
  const void        *pkt_priv,
  const void        *data,
  size_t            size,
  uint32_t          options
)
{
    return glink_tx(handle, pkt_priv, data, size, options);
}

glink_err_type pdtsw_glink_txv
(
  glink_handle_type        handle,
  const void               *pkt_priv,
  void                     *iovec,
  size_t                   size,
  glink_buffer_provider_fn vprovider,
  glink_buffer_provider_fn pprovider,
  uint32_t                 options
)
{
    return glink_txv(handle, pkt_priv, iovec, size, vprovider, pprovider, options);
}

glink_err_type pdtsw_glink_queue_rx_intent
(
  glink_handle_type handle,
  const void        *pkt_priv,
  size_t            size
)
{
    return glink_queue_rx_intent(handle, pkt_priv, size);
}

glink_err_type pdtsw_glink_rx_done
(
  glink_handle_type handle,
  const void        *ptr,
  bool              reuse
)
{
    return glink_rx_done(handle, ptr, reuse);
}

glink_err_type pdtsw_glink_sigs_set
(
  glink_handle_type handle,
  uint32_t          sig_value
)
{
    return glink_sigs_set(handle, sig_value);
}

glink_err_type pdtsw_glink_sigs_local_get
(
  glink_handle_type handle,
  uint32_t          *sig_value
)
{
    return glink_sigs_local_get(handle, sig_value);
}

glink_err_type pdtsw_glink_sigs_remote_get
(
  glink_handle_type handle,
  uint32_t          *sig_value
)
{
    return glink_sigs_remote_get(handle, sig_value);
}