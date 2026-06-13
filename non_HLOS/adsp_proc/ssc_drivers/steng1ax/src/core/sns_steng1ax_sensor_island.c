/**
 * @file sns_steng1ax_sensor.c
 *
 * Common implementation for STENG1AX Sensors.
 *
 * Copyright (c) 2022, STMicroelectronics.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the STMicroelectronics nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **/

#include <string.h>
#include "sns_attribute_util.h"
#include "sns_event_service.h"
#include "sns_diag_service.h"
#include "sns_island_service.h"
#include "sns_steng1ax_sensor.h"
#include "sns_math_util.h"
#include "sns_mem_util.h"
#include "sns_sensor_instance.h"
#include "sns_service_manager.h"
#include "sns_sensor_util.h"
#include "sns_service.h"
#include "sns_stream_service.h"
#include "sns_types.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "sns_pb_util.h"
#include "sns_printf.h"
#include "sns_registry.pb.h"
#include "sns_std.pb.h"
#include "sns_std_sensor.pb.h"
#include "sns_std_event_gated_sensor.pb.h"
#include "sns_suid.pb.h"
#include "sns_timer.pb.h"
#include "sns_cal.pb.h"

extern const odr_reg_map steng1ax_odr_map[];

/** Forward Declaration of Eng Sensor API */
sns_sensor_api steng1ax_eng_sensor_api;

//define non-dependent sensors at the begining
//then dependent sensors ex: MA depends on eng
//so MA shoule be defined below the eng
//Not following the order fails sensor configuration
const steng1ax_sensors steng1ax_supported_sensors[ MAX_SUPPORTED_SENSORS ] = {
  { STENG1AX_ENG,                     ALIGN_8(sizeof(steng1ax_state)) + sizeof(steng1ax_shared_state),
    &(sns_sensor_uid)ENG_SUID_0,
    &steng1ax_eng_sensor_api,
    &steng1ax_sensor_instance_api}
};

static void steng1ax_handle_flush_request(
  sns_sensor           *this,
  sns_sensor_instance  *instance,
  steng1ax_shared_state *shared_state);

static void  steng1ax_send_flush_config(
  sns_sensor          *const this,
  sns_sensor_instance *instance,
  steng1ax_sensor_type sensor);


#if !STENG1AX_ISLAND_DISABLED
void steng1ax_exit_island(sns_sensor *const this)
{
  sns_service_manager *smgr = this->cb->get_service_manager(this);
  sns_island_service  *island_svc  =
    (sns_island_service *)smgr->get_service(smgr, SNS_ISLAND_SERVICE);
  island_svc->api->sensor_island_exit(island_svc, this);
}
#else
void steng1ax_exit_island(sns_sensor *const this)
{
  UNUSED_VAR(this);
}
#endif

#if !STENG1AX_POWERRAIL_DISABLED
static void steng1ax_start_power_rail_timer(
  sns_sensor                       *const this,
  sns_time                         timeout_ticks,
  steng1ax_power_rail_pending_state pwr_rail_pend_state,
  steng1ax_shared_state             *shared_state)
{
  if(NULL == shared_state->timer_stream)
  {
    sns_service_manager *service_mgr = this->cb->get_service_manager(this);
    sns_stream_service *stream_svc = (sns_stream_service*)
      service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);
    DBG_PRINTF_EX(LOW, this, "power_rail_timer: master sensor creating stream");
    stream_svc->api->create_sensor_stream(stream_svc, steng1ax_get_master_sensor(this),
                                          shared_state->inst_cfg.timer_suid,
                                          &shared_state->timer_stream);
  }

  if(NULL != shared_state->timer_stream)
  {
    sns_timer_sensor_config req_payload = sns_timer_sensor_config_init_default;
    size_t req_len;
    uint8_t buffer[20];
    sns_memset(buffer, 0, sizeof(buffer));
    req_payload.is_periodic = false;
    req_payload.start_time = sns_get_system_time();
    req_payload.timeout_period = timeout_ticks;

    req_len = pb_encode_request(buffer, sizeof(buffer), &req_payload,
                                sns_timer_sensor_config_fields, NULL);
    if(req_len > 0)
    {
      sns_request timer_req =
        {  .message_id = SNS_TIMER_MSGID_SNS_TIMER_SENSOR_CONFIG,
           .request = buffer, .request_len = req_len};
      sns_rc rc = shared_state->timer_stream->api->send_request(shared_state->timer_stream,
                                                                &timer_req);
      if(SNS_RC_SUCCESS == rc)
      {
        shared_state->power_rail_pend_state = pwr_rail_pend_state;
      }
    }
  }
}

static void steng1ax_turn_rails_off(sns_sensor *this, steng1ax_shared_state *shared_state)
{
#if !STENG1AX_POWERRAIL_DISABLED
  SNS_PRINTF(MED, this, "turn_rails_off: vote=%u pend_state=%u",
             shared_state->rail_config.rail_vote, shared_state->power_rail_pend_state);
  if((SNS_RAIL_OFF != shared_state->rail_config.rail_vote) &&
     (STENG1AX_POWER_RAIL_PENDING_NONE == shared_state->power_rail_pend_state))
  {
    sns_time timeout = sns_convert_ns_to_ticks(STENG1AX_POWER_RAIL_OFF_TIMEOUT_NS);
    steng1ax_start_power_rail_timer(this, timeout, STENG1AX_POWER_RAIL_PENDING_OFF, shared_state);
  }
#else
  UNUSED_VAR(this);
  UNUSED_VAR(shared_state);
#endif
}

static void steng1ax_check_pending_flush_requests(
  sns_sensor           *this,
  sns_sensor_instance  *instance)
{
  UNUSED_VAR(instance);
  sns_sensor *lib_sensor;
  for(lib_sensor = this->cb->get_library_sensor(this, true);
      NULL != lib_sensor;
      lib_sensor = this->cb->get_library_sensor(this, false))
  {
    steng1ax_state *state = (steng1ax_state*)lib_sensor->state->state;
    if(state->flush_req)
    {
      state->flush_req = false;
      DBG_PRINTF(MED, this, "pending_flush_requests: sensor=%u", state->sensor);
      steng1ax_send_fifo_flush_done(instance, state->sensor, FLUSH_DONE_CONFIGURING);
    }
  }
}

static void steng1ax_deregister_com(steng1ax_shared_state *shared_state)
{
    // Logic to free-up any legacy com port
    for (int i = 0; i < SENSOR_CNT; i++)
    {
      steng1ax_com_port_info *com_port_temp = &shared_state->inst_cfg.com_port_info[i];

      if (com_port_temp->port_handle_ex)
      {
        shared_state->scp_service->api->sns_scp_close(com_port_temp->port_handle_ex);
        if (com_port_temp->port_handle_ex != NULL)
        {
          shared_state->scp_service->api->sns_scp_deregister_com_port(&com_port_temp->port_handle_ex);
        }
      }
    }
}
static sns_rc steng1ax_process_timer_events(sns_sensor *const this)
{
  sns_rc rv = SNS_RC_SUCCESS;
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  UNUSED_VAR(state);
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
  sns_data_stream *stream = shared_state->timer_stream;

  if(NULL == stream || 0 == stream->api->get_input_cnt(stream))
  {
    return rv;
  }
  for(sns_sensor_event *event = stream->api->peek_input(stream);
      NULL != event;
      event = stream->api->get_next_input(stream))
  {
    sns_timer_sensor_event timer_event;
    pb_istream_t pbstream;

    if(SNS_TIMER_MSGID_SNS_TIMER_SENSOR_EVENT != event->message_id)
    {
      continue; /* not interested in other events */
    }

    pbstream = pb_istream_from_buffer((pb_byte_t*)event->event, event->event_len);
    if(!pb_decode(&pbstream, sns_timer_sensor_event_fields, &timer_event))
    {
      SNS_PRINTF(ERROR, this, "pb_dec err");
      continue;
    }

    DBG_PRINTF_EX(HIGH, this, "Timer fired: sensor=%u pend_state=%u current_power_rail_vote=%u",
               state->sensor, shared_state->power_rail_pend_state,
               shared_state->rail_config.rail_vote);

    if(shared_state->power_rail_pend_state == STENG1AX_POWER_RAIL_PENDING_SOFT_PD)
    {
      shared_state->power_rail_pend_state = STENG1AX_POWER_RAIL_PENDING_INIT;
      steng1ax_exit_island(this);
      int i = 0;
      for (i = 0; i < shared_state->inst_cfg.multi_eng_cfg.num_sensors_enable; i++)
      {
        steng1ax_set_soft_pd(this, i);
      }
      steng1ax_deregister_com(shared_state);
      steng1ax_start_power_rail_timer(this, sns_convert_ns_to_ticks(STENG1AX_PENDING_SOFT_PD * 1000 * 1000),
                                       STENG1AX_POWER_RAIL_PENDING_INIT,
                                       shared_state);
    }
    else if(shared_state->power_rail_pend_state == STENG1AX_POWER_RAIL_PENDING_INIT)
    {
      shared_state->soft_pd = true;
      shared_state->soft_pd_timestamp = shared_state->power_rail_on_timestamp;
      DBG_PRINTF_EX(HIGH, this, "soft_pd=%d power_rail_on_ts=%x%x",
        shared_state->soft_pd, (uint32_t)(shared_state->soft_pd_timestamp>>32),
        (uint32_t)(shared_state->soft_pd_timestamp));
      if(!shared_state->hw_is_present)
      {
        /** Initial HW discovery is OK to run in normal mode. */
        shared_state->power_rail_pend_state = STENG1AX_POWER_RAIL_PENDING_NONE;
        steng1ax_exit_island(this);
        int i = 0;
        for (i = 0; i < shared_state->inst_cfg.multi_eng_cfg.num_sensors_enable; i++)
        {
          rv = steng1ax_discover_hw(this, i);
        }
        /**----------------------Turn Power Rail OFF--------------------------*/
        steng1ax_update_rail_vote(this, shared_state, SNS_RAIL_OFF);
      }
      else
      {
        sns_sensor_instance *instance = sns_sensor_util_get_shared_instance(this);
        if(instance != NULL)
        {
          steng1ax_instance_state *inst_state = (steng1ax_instance_state*)instance->state->state;
          inst_state->soft_pd = shared_state->soft_pd;
          DBG_PRINTF_EX(HIGH, this, "instance state update soft_pd=%d", inst_state->soft_pd);
        }
        shared_state->power_rail_pend_state = STENG1AX_POWER_RAIL_PENDING_SET_CLIENT_REQ;
        steng1ax_start_power_rail_timer(this, sns_convert_ns_to_ticks(STENG1AX_PENDING_SOFT_PD * 1000 * 1000),
                                         STENG1AX_POWER_RAIL_PENDING_SET_CLIENT_REQ,
                                         shared_state);
      }
    }
    else if(shared_state->power_rail_pend_state == STENG1AX_POWER_RAIL_PENDING_SET_CLIENT_REQ)
    {
      sns_sensor_instance *instance = sns_sensor_util_get_shared_instance(this);
      if(NULL != instance)
      {
        steng1ax_exit_island(this);
        steng1ax_reset_device( instance, STENG1AX_ENG);
        steng1ax_set_client_config(this, instance, shared_state);
        steng1ax_check_pending_flush_requests(this, instance);
      } else {
        DBG_PRINTF(LOW, this, "instance no longer available");
        steng1ax_turn_rails_off(this, shared_state);
      }
      shared_state->power_rail_pend_state = STENG1AX_POWER_RAIL_PENDING_NONE;
    }
    else if(shared_state->power_rail_pend_state == STENG1AX_POWER_RAIL_PENDING_OFF)
    {
      DBG_PRINTF_EX(HIGH, this, "Turning off power rail");
      shared_state->power_rail_pend_state = STENG1AX_POWER_RAIL_PENDING_NONE;
      steng1ax_update_rail_vote(this, shared_state, SNS_RAIL_OFF);
    }
  }
  if(shared_state->power_rail_pend_state == STENG1AX_POWER_RAIL_PENDING_NONE)
  {
    sns_sensor_util_remove_sensor_stream(this, &shared_state->timer_stream);
  }
  return rv;
}
#endif

/* See sns_sensor::get_sensor_uid */
sns_sensor_uid const* steng1ax_get_sensor_uid(sns_sensor const *const this)
{
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  return &state->my_suid;
}

/** See sns_steng1ax_sensor.h*/
sns_rc steng1ax_sensor_notify_event(sns_sensor *const this)
{
  sns_rc rv = SNS_RC_SUCCESS;
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);

  if((NULL != shared_state->suid_stream &&
      0 != shared_state->suid_stream->api->get_input_cnt(shared_state->suid_stream)) ||
     (NULL != state->reg_data_stream &&
      0 != state->reg_data_stream->api->get_input_cnt(state->reg_data_stream))
#if STENG1AX_DAE_ENABLED
     || (NULL != shared_state->dae_stream &&
      0 != shared_state->dae_stream->api->get_input_cnt(shared_state->dae_stream))
#endif
    )
  {
    steng1ax_exit_island(this);
    steng1ax_process_suid_events(this);
    steng1ax_process_registry_events(this);
    steng1ax_dae_if_process_sensor_events(this);
  }
#if !STENG1AX_POWERRAIL_DISABLED
  rv = steng1ax_process_timer_events(this);
#endif

  if(rv == SNS_RC_SUCCESS && STENG1AX_ENG == state->sensor)
  {
#if STENG1AX_POWERRAIL_DISABLED
    if(!shared_state->hw_is_present)
    {
      int i = 0;
      for (i=0; i < shared_state.inst_cfg.multi_eng_cfg.num_sensors_enable; i++)
      {
        rv = steng1ax_discover_hw(this, i);
      }
    }
#else
    if(!shared_state->soft_pd &&
       !shared_state->hw_is_present &&
       NULL != shared_state->pwr_rail_service &&
       NULL != shared_state->timer_stream &&
       shared_state->power_rail_pend_state == STENG1AX_POWER_RAIL_PENDING_NONE)
    {
      sns_time delta;
      steng1ax_update_rail_vote(this, shared_state, SNS_RAIL_ON_NPM);
      
      delta = sns_get_system_time() - shared_state->power_rail_on_timestamp;
      
      if(delta < sns_convert_ns_to_ticks(STENG1AX_OFF_TO_IDLE_MS * 1000 * 1000))
      {
        shared_state->power_rail_pend_state = STENG1AX_POWER_RAIL_PENDING_SOFT_PD;
        steng1ax_start_power_rail_timer(this, sns_convert_ns_to_ticks(STENG1AX_OFF_TO_IDLE_MS * 1000 * 1000) - delta,
                                      STENG1AX_POWER_RAIL_PENDING_SOFT_PD,
                                      shared_state);
      }
      else
      {
        steng1ax_exit_island(this);
        int i =0;
        for (i=0; i < shared_state->inst_cfg.multi_eng_cfg.num_sensors_enable; i++)
        {
          steng1ax_set_soft_pd(this, i);
        }
        steng1ax_deregister_com(shared_state);
        shared_state->power_rail_pend_state = STENG1AX_POWER_RAIL_PENDING_INIT;
        steng1ax_start_power_rail_timer(this, sns_convert_ns_to_ticks(STENG1AX_PENDING_SOFT_PD * 1000 * 1000),
                                      STENG1AX_POWER_RAIL_PENDING_INIT,
                                      shared_state);
      }
    }
#endif
  }
  if(!state->available && shared_state->hw_is_present && (shared_state->outstanding_reg_requests == 0) && (shared_state->outstanding_reg_platform_requests == 0))
  {
    steng1ax_exit_island(this);
    if(steng1ax_dae_if_support_known(this))
    {
      shared_state->hw_idx = 0;
      steng1ax_update_siblings(this, shared_state);
    }
    else
    {
      steng1ax_dae_if_check_support(this);
    }
  }

  return rv;
}

static void  steng1ax_send_flush_config(
  sns_sensor          *const this,
  sns_sensor_instance *instance,
  steng1ax_sensor_type sensor)
{
  sns_request config;

  config.message_id = SNS_STD_MSGID_SNS_STD_FLUSH_REQ;
  config.request_len = sizeof(sensor);
  config.request = &sensor;

  this->instance_api->set_client_config(instance, &config);
}

/**
 * Returns decoded request message for type
 * sns_std_sensor_config.
 *
 * @param[in] in_request   Request as sotred in client_requests
 *                         list.
 * @param decoded_request  Standard decoded message.
 * @param decoded_payload  Decoded stream request payload.
 *
 * @return bool true if decode is successful else false
 */
static bool steng1ax_get_decoded_imu_request(
  sns_sensor const      *this,
  sns_request const     *in_request,
  sns_std_request       *decoded_request,
  sns_std_sensor_config *decoded_payload)
{

  pb_istream_t stream;
  pb_simple_cb_arg arg =
      { .decoded_struct = decoded_payload,
        .fields = sns_std_sensor_config_fields };
  decoded_request->payload = (struct pb_callback_s)
      { .funcs.decode = &pb_decode_simple_cb, .arg = &arg };
  stream = pb_istream_from_buffer(in_request->request,
                                  in_request->request_len);
  if(!pb_decode(&stream, sns_std_request_fields, decoded_request))
  {
    SNS_PRINTF(ERROR, this, "decode err");
    return false;
  }
  return true;
}

/**
 * Returns decoded request message for type
 * sns_physical_sensor_test_config_fields.
 *
 * @param[in] in_request   Request as stored in client_requests
 *                         list.
 * @param decoded_request  Standard decoded message.
 * @param decoded_payload  Decoded stream request payload.
 *
 * @return bool true if decode is successful else false
 */
bool steng1ax_get_decoded_self_test_request(
  sns_sensor const                *this,
  sns_request const               *in_request,
  sns_std_request                 *decoded_request,
  sns_physical_sensor_test_config *decoded_payload)
{
  pb_simple_cb_arg arg =
      { .decoded_struct = decoded_payload,
        .fields = sns_physical_sensor_test_config_fields };
  decoded_request->payload = (struct pb_callback_s)
      { .funcs.decode = &pb_decode_simple_cb, .arg = &arg };

  pb_istream_t stream = pb_istream_from_buffer(in_request->request, in_request->request_len);
  if(!pb_decode(&stream, sns_std_request_fields, decoded_request))
  {
    SNS_PRINTF(ERROR, this, "decode err - self test");
    return false;
  }
  return true;
}

void steng1ax_set_client_config(
  sns_sensor           *this,
  sns_sensor_instance  *instance,
  steng1ax_shared_state *shared_state)
{
  sns_request req_config;
  steng1ax_instance_state *inst_state = (steng1ax_instance_state*)instance->state->state;
  if(shared_state->inst_cfg.selftest.requested && !inst_state->self_test_info.test_alive)
  {
    req_config.message_id = SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG;
  }
  else
  {
    req_config.message_id = SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG;
  }

  DBG_PRINTF(MED, this, "set_client_config: msg=%u config=0x%x",
             req_config.message_id, shared_state->inst_cfg.config_sensors);

  STENG1AX_AUTO_DEBUG_PRINTF(HIGH, this, "set_client_config: msg=%u", req_config.message_id);
  req_config.request_len = sizeof(steng1ax_instance_config);
  req_config.request = &shared_state->inst_cfg;
  this->instance_api->set_client_config(instance, &req_config);

  if(SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG ==
     req_config.message_id)
  {
    shared_state->inst_cfg.selftest.requested = false;
  }
  else
  {
    shared_state->inst_cfg.config_sensors = 0; // consumed by instance, can be cleared
  }
}

static void steng1ax_process_eng_request(
  sns_sensor          *this, 
  sns_request const   *request)
{
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  steng1ax_combined_request *cimu = &state->combined_imu;
  sns_std_request decoded_request = sns_std_request_init_default;
  sns_std_sensor_config decoded_payload = sns_std_sensor_config_event_init_default;

  if(steng1ax_get_decoded_imu_request(this, request, &decoded_request, &decoded_payload))
  {
    bool max_batch  = false;
    bool flush_only = false;
    bool is_passive = false;
    UNUSED_VAR(is_passive);
    float dae_report_rate = decoded_payload.sample_rate;
    uint64_t flush_period_ticks = UINT64_MAX;
    uint32_t flush_period = UINT32_MAX;
    uint32_t report_period_us = (uint32_t)(1000000.0f / decoded_payload.sample_rate);
    uint32_t dae_report_period_us = report_period_us;

    if(decoded_request.has_batching)
    {
      if(decoded_request.batching.has_flush_period)
      {
        flush_period = decoded_request.batching.flush_period;
      }
      else if(decoded_request.batching.batch_period > 0)
      {
        flush_period = decoded_request.batching.batch_period;
      }

      if(decoded_request.batching.batch_period > 0)
      {
        dae_report_period_us = report_period_us = decoded_request.batching.batch_period;
      }

      flush_only = (decoded_request.batching.has_flush_only && decoded_request.batching.flush_only);
      if(!flush_only)
      {
        max_batch  = (decoded_request.batching.has_max_batch && decoded_request.batching.max_batch);
      }
      if(flush_only || flush_period == 0)
      {
        dae_report_period_us = UINT32_MAX;
        dae_report_rate = 0.0f;
      }
      else
      {
        dae_report_rate = (1000000.0f / (float)dae_report_period_us);
      }

      flush_period_ticks = sns_convert_ns_to_ticks((uint64_t)flush_period*1000);
    }

    cimu->max_batch         &= max_batch;
    cimu->flush_only        &= flush_only;
    if(!max_batch)
    {
      if(cimu->report_per_us > 0 && report_period_us > 0)
        cimu->report_per_us = SNS_MIN(cimu->report_per_us, report_period_us);
      else if(cimu->report_per_us == 0)
        cimu->report_per_us = report_period_us;

      cimu->report_rate      = (1000000.0f / (float)cimu->report_per_us);
      cimu->dae_report_rate  = SNS_MAX(cimu->dae_report_rate, dae_report_rate);
    }
    cimu->flush_period_ticks = SNS_MAX(cimu->flush_period_ticks, flush_period_ticks);
    cimu->sample_rate        = SNS_MAX(cimu->sample_rate, decoded_payload.sample_rate);

    if(cimu->sample_rate)
    {
      if(request->message_id == SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG)
      {
        cimu->client_present = true;
      }
    }

    DBG_PRINTF(
      HIGH, this, "eng: msg=%u SR*1k=%d RR*1k=%d/%d BP/FP(us)=%d/%d MB/FO/Psv=%03x",
      request->message_id, (int)(decoded_payload.sample_rate*1000), 
      (int)(1000000000UL/report_period_us), (int)(dae_report_rate*1000),
      decoded_request.has_batching ? decoded_request.batching.batch_period : -1,
      decoded_request.batching.has_flush_period ? decoded_request.batching.flush_period : -1,
      ((uint16_t)max_batch << 8) | ((uint16_t)flush_only << 4) | (uint16_t)is_passive);

    STENG1AX_AUTO_DEBUG_PRINTF(
      HIGH, this,  "sr*1000=%d rr*1000=%d",
      (int)decoded_payload.sample_rate,(int)(dae_report_rate*1000));
  }
}

static void steng1ax_process_self_test_request(
  sns_sensor           *this,
  sns_request const    *request,
  steng1ax_shared_state *shared_state)
{
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  sns_std_request decoded_request;
  sns_physical_sensor_test_config decoded_payload =
    sns_physical_sensor_test_config_init_default;

  if (steng1ax_get_decoded_self_test_request(this, request, &decoded_request, &decoded_payload))
  {
    shared_state->inst_cfg.selftest.requested  = true;
    shared_state->inst_cfg.selftest.test_type  = decoded_payload.test_type;
    shared_state->inst_cfg.selftest.sensor     = state->sensor;
    shared_state->inst_cfg.selftest_client_present |= state->sensor;
  }
}

static void steng1ax_combine_requests(
  sns_sensor           *this,
  sns_sensor_instance  *instance,
  steng1ax_shared_state *shared_state)
{
  UNUSED_VAR(shared_state);
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  steng1ax_combined_request *cimu = &state->combined_imu;
  sns_request const *request;

  sns_memzero(cimu, sizeof(*cimu));
  cimu->max_batch = cimu->flush_only = true;

  for(request = instance->cb->get_client_request(instance, &state->my_suid, true);
      NULL != request;
      request = instance->cb->get_client_request(instance, &state->my_suid, false))
  {
    if(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG             == request->message_id ||
       SNS_STD_EVENT_GATED_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG == request->message_id)
    {
      steng1ax_process_eng_request(this, request);
    }
  }
  if(0.0f == cimu->sample_rate)
  {
    cimu->max_batch = cimu->flush_only = false;
  }
  if(cimu->max_batch)
  {
    cimu->report_rate = cimu->dae_report_rate = (1.0f / (float)UINT32_MAX);
    cimu->flush_period_ticks = UINT64_MAX;
  }

  DBG_PRINTF(
    MED, this, "combine: sens=0x%x SR=%u RR*1k=%d MB=%u FO=%u fl_per=%u",
    state->sensor, (int)cimu->sample_rate, (int)(cimu->report_rate*1000),
    cimu->max_batch, cimu->flush_only, (uint32_t)cimu->flush_period_ticks);
}

static void steng1ax_update_shared_imu(sns_sensor *this, steng1ax_shared_state *shared_state)
{
  sns_sensor *lib_sensor;
  steng1ax_instance_config *inst_cfg = &shared_state->inst_cfg;

  inst_cfg->sample_rate           = 0.0f;
  inst_cfg->report_rate           = 0.0f;
  inst_cfg->dae_report_rate       = 0.0f;
  inst_cfg->flush_period_ticks    = 0;
  inst_cfg->client_present        = 0;
  inst_cfg->fifo_enable           = 0;

  for(lib_sensor = this->cb->get_library_sensor(this, true);
      NULL != lib_sensor;
      lib_sensor = this->cb->get_library_sensor(this, false))
  {
    steng1ax_state *lib_state = (steng1ax_state*)lib_sensor->state->state;
    steng1ax_combined_request *cimu = &lib_state->combined_imu;
    if(cimu->client_present)
    {
      inst_cfg->client_present |= lib_state->sensor;
      inst_cfg->sample_rate = SNS_MAX(inst_cfg->sample_rate, cimu->sample_rate);
      inst_cfg->report_rate = SNS_MAX(inst_cfg->report_rate, cimu->report_rate);
      inst_cfg->dae_report_rate = SNS_MAX(inst_cfg->dae_report_rate, cimu->dae_report_rate);
      inst_cfg->flush_period_ticks = SNS_MAX(inst_cfg->flush_period_ticks,
                                             cimu->flush_period_ticks);
      if(lib_state->sensor == STENG1AX_ENG)
      {
        inst_cfg->fifo_enable |= STENG1AX_ENG;
      }
    }

    STENG1AX_AUTO_DEBUG_PRINTF(
      HIGH, this, "config: %d %d %d",
      lib_state->sensor, (int)(cimu->sample_rate*1000), (int)(cimu->report_rate*1000));
  }

  DBG_PRINTF(HIGH, this, "shared_imu: SR*1k=%d RR*1k=%d/%d fl_per(ticks)=%u",
            (int)(inst_cfg->sample_rate*1000), (int)(inst_cfg->report_rate*1000),
            (int)(inst_cfg->dae_report_rate*1000), (uint32_t)inst_cfg->flush_period_ticks);
  DBG_PRINTF(HIGH, this, "shared_imu: clients(ng/st)=0x%x/%x fifo: %d",
             inst_cfg->client_present, inst_cfg->selftest_client_present, inst_cfg->fifo_enable);
}

static void steng1ax_reval(
  sns_sensor          *const this,
  sns_sensor_instance *instance,
  sns_request const   *request)
{
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
  steng1ax_combine_requests(this, instance, shared_state);

  if(SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG !=
     request->message_id)
  {
    steng1ax_update_shared_imu(this, shared_state);
  }
  else
  {
    steng1ax_process_self_test_request(this, request, shared_state);
  }

#if STENG1AX_POWERRAIL_DISABLED
  steng1ax_set_client_config(this, instance, shared_state);
#else
  if(SNS_RAIL_OFF != shared_state->rail_config.rail_vote &&
     STENG1AX_POWER_RAIL_PENDING_NONE == shared_state->power_rail_pend_state)
  {
    steng1ax_set_client_config(this, instance, shared_state);
  }
  else
  {
    DBG_PRINTF_EX(MED, this, "reval: rail=%u pending=%u",
               shared_state->rail_config.rail_vote, shared_state->power_rail_pend_state);

  }
#endif
}

static void steng1ax_remove_request(
  sns_sensor               *const this,
  sns_sensor_instance      *instance,
  steng1ax_shared_state     *shared_state,
  struct sns_request const *exist_request)
{
  if(NULL != instance)
  {
    steng1ax_state *state = (steng1ax_state*)this->state->state;

    /* Assumption: The FW will call deinit() on the instance before destroying it.
       Putting all HW resources (sensor HW, COM port, power rail)in
       low power state happens in Instance deinit().*/
    if(exist_request->message_id != SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG)
    {
      shared_state->inst_cfg.config_sensors |= state->sensor;
      steng1ax_reval(this, instance, exist_request);
    }
    else
    {
      shared_state->inst_cfg.selftest_client_present &= ~state->sensor;
      steng1ax_exit_island(this);
      steng1ax_handle_selftest_request_removal(this, instance, shared_state);
    }
  }
}

static sns_rc steng1ax_add_new_request(
  sns_sensor               *const this,
  sns_sensor_instance      *instance,
  steng1ax_shared_state     *shared_state,
  struct sns_request const *new_request)
{
  sns_rc rc = SNS_RC_SUCCESS;
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  uint8_t hw_id = shared_state->hw_idx;
  float max_sample_rate = steng1ax_odr_map[shared_state->inst_cfg.max_odr_idx[hw_id]].odr;

  if(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG             == new_request->message_id ||
     SNS_STD_EVENT_GATED_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG == new_request->message_id)
  {
    sns_std_request decoded_request;
    sns_std_sensor_config decoded_payload = {0};
    if(steng1ax_get_decoded_imu_request(this, new_request, &decoded_request,
                                       &decoded_payload) &&
       0.0f < decoded_payload.sample_rate &&
       ((!STENG1AX_IS_ESP_SENSOR(state->sensor)&& 
         max_sample_rate >= decoded_payload.sample_rate)))
    {
      shared_state->inst_cfg.config_sensors |= state->sensor;
    }
    else if((!STENG1AX_IS_ESP_SENSOR(state->sensor)&& 
         (0.0f >= decoded_payload.sample_rate || max_sample_rate < decoded_payload.sample_rate)))
    {
      rc = SNS_RC_INVALID_VALUE;

      SNS_PRINTF(ERROR, this, "new_request: Invalid ODR sensor=%u SR=%d",
                 state->sensor, (int)decoded_payload.sample_rate);
    }
    else if (STENG1AX_IS_ESP_SENSOR(state->sensor))
    {
      rc = SNS_RC_INVALID_TYPE;
      SNS_PRINTF(ERROR, this, "Invalid stream type request: sensor=%u", state->sensor);
    }
  }
  else if(SNS_STD_SENSOR_MSGID_SNS_STD_ON_CHANGE_CONFIG == new_request->message_id)
  {
    if(!STENG1AX_IS_ESP_SENSOR(state->sensor))
    {
      rc = SNS_RC_INVALID_TYPE;
    }
    if(STENG1AX_IS_ESP_SENSOR(state->sensor))
    {
      shared_state->inst_cfg.config_sensors |= state->sensor;
    }

  }
  else if(SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG ==
          new_request->message_id)
  {
    sns_std_request decoded_request;
    sns_physical_sensor_test_config decoded_payload =
      sns_physical_sensor_test_config_init_default;
    if (!steng1ax_get_decoded_self_test_request(this, new_request, &decoded_request,
                                               &decoded_payload) ||
        (state->sensor & STENG1AX_ENG &&
         decoded_payload.test_type == SNS_PHYSICAL_SENSOR_TEST_TYPE_SW) ||
        (decoded_payload.test_type != SNS_PHYSICAL_SENSOR_TEST_TYPE_COM))
    {
      rc = SNS_RC_INVALID_TYPE;
    }
    else if(STENG1AX_IS_ESP_SENSOR(state->sensor))
    {
      shared_state->inst_cfg.config_sensors |= state->sensor;
    }
  }
  else
  {
    rc = SNS_RC_INVALID_TYPE;
  }

  if(rc == SNS_RC_SUCCESS)
  {
    steng1ax_reval(this, instance, new_request);
  }
  else
  {
    sns_sensor_uid *suid = &state->my_suid;
    sns_std_error_event error_event;

    if(rc == SNS_RC_NOT_SUPPORTED)
      error_event.error = SNS_STD_ERROR_NOT_SUPPORTED;
    else if(rc == SNS_RC_FAILED)
      error_event.error = SNS_STD_ERROR_FAILED;
    else if(rc == SNS_RC_INVALID_TYPE)
      error_event.error = SNS_STD_ERROR_INVALID_TYPE;
    else
      error_event.error = SNS_STD_ERROR_INVALID_VALUE;
    pb_send_event(instance, 
                 sns_std_error_event_fields, 
                 &error_event, 
                 sns_get_system_time(), 
                 SNS_STD_MSGID_SNS_STD_ERROR_EVENT, 
                 suid);

    instance->cb->remove_client_request(instance, new_request);
    SNS_PRINTF(ERROR, this, "req rejec, rc=%d", rc);
  }
  return rc;
}

sns_sensor_instance *steng1ax_create_new_instance(sns_sensor *const this)
{
  sns_sensor_instance *instance = NULL;
  sns_sensor *master_sensor = steng1ax_get_master_sensor(this);
  if(NULL != master_sensor)
  {
#if !STENG1AX_POWERRAIL_DISABLED
    steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
    sns_rc rc;
    steng1ax_update_rail_vote(
      master_sensor, shared_state,
      SNS_RAIL_ON_NPM);

    DBG_PRINTF_EX(MED, this, "new_inst: soft_pd=%d", shared_state->soft_pd);
    if(shared_state->soft_pd)
    {
      sns_time to = sns_convert_ns_to_ticks(STENG1AX_OFF_TO_IDLE_MS*1000*1000);
      steng1ax_start_power_rail_timer(
        this,
        to,
        STENG1AX_POWER_RAIL_PENDING_SET_CLIENT_REQ,
        shared_state);
    }
    else
    {
      sns_service_manager *service_mgr = this->cb->get_service_manager(this);
      shared_state->scp_service =  (sns_sync_com_port_service *)
        service_mgr->get_service(service_mgr, SNS_SYNC_COM_PORT_SERVICE);
      uint8_t hw_id = shared_state->hw_idx;
      steng1ax_com_port_info *com_port = &shared_state->inst_cfg.com_port_info[hw_id];

      if(com_port->com_config.bus_type == SNS_BUS_I3C ||
         com_port->com_config.bus_type == SNS_BUS_I3C_SDR)
      {
        rc = shared_state->scp_service->api->
          sns_scp_register_com_port(&com_port->com_config_ex, &com_port->port_handle_ex);
        if(rc == SNS_RC_SUCCESS)
        {
          rc = shared_state->scp_service->api->sns_scp_open(com_port->port_handle_ex);
        }
        else
        {
          SNS_PRINTF(ERROR, this, "register_com_port fail rc:%u",rc);
        }
      }

      steng1ax_start_power_rail_timer(
        this,
        sns_convert_ns_to_ticks(STENG1AX_OFF_TO_IDLE_MS * 1000 * 1000),
        STENG1AX_POWER_RAIL_PENDING_SOFT_PD,
        shared_state);
    }
#endif

    // must create instance from master sensor whose state includes the shared state
    DBG_PRINTF_EX(MED, this, "creating inst");
    instance = master_sensor->cb->create_instance(master_sensor,
                                                  sizeof(steng1ax_instance_state));
  }
  return instance;
}

static void steng1ax_handle_flush_request(
  sns_sensor           *this,
  sns_sensor_instance  *instance,
  steng1ax_shared_state *shared_state)
{
  UNUSED_VAR(shared_state);
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  steng1ax_instance_state *inst_state = (steng1ax_instance_state*)instance->state->state;
  steng1ax_flush_done_reason reason = FLUSH_TO_BE_DONE;
#if !STENG1AX_POWERRAIL_DISABLED
  if(SNS_RAIL_OFF == shared_state->rail_config.rail_vote ||
     STENG1AX_POWER_RAIL_PENDING_SET_CLIENT_REQ == shared_state->power_rail_pend_state)
  {
    DBG_PRINTF(MED, this, "handle_flush_request: sensor=%u", state->sensor);
    state->flush_req = true;
    return;
  }
  else
#endif

  if(state->sensor & !STENG1AX_ENG)
  {
    reason = FLUSH_DONE_NOT_ENG;
  }

  if(reason != FLUSH_TO_BE_DONE || inst_state->self_test_info.test_alive)
  {
    steng1ax_send_fifo_flush_done(instance, state->sensor, reason);
  }
  else
  {
    steng1ax_send_flush_config(this, instance, state->sensor);
  }
}

bool steng1ax_is_request_present(sns_sensor_instance *instance, uint8_t hw_idx )
{
  UNUSED_VAR(hw_idx);
  sns_sensor_uid* eng_suid = &((sns_sensor_uid)ENG_SUID_0);

  {
    if(NULL != instance->cb->get_client_request(instance,
      eng_suid, true)
      )
      return true;
  }
  //No request for primary sensors

  if(steng1ax_is_esp_request_present(instance))
    return true;

  return false;
}

sns_sensor_instance* steng1ax_update_request_q(sns_sensor *const this,
                              struct sns_request const *exist_request,
                              struct sns_request const *new_request,
                              bool remove)
{
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  UNUSED_VAR(state);
  sns_sensor_instance *instance = sns_sensor_util_get_shared_instance(this);
  if(remove && (NULL != exist_request) && (NULL != instance))
  {
    instance->cb->remove_client_request(instance, exist_request);
  } else if(!remove && NULL != new_request) {
    if (NULL == instance &&
        // first request cannot be a Flush request or Calibration reset request
        SNS_STD_MSGID_SNS_STD_FLUSH_REQ != new_request->message_id)
    {
      instance = steng1ax_create_new_instance(this);
    }
    if(instance) {
      steng1ax_instance_state *inst_state = (steng1ax_instance_state*)instance->state->state;

      if(!inst_state->self_test_info.test_alive && 
          SNS_STD_MSGID_SNS_STD_FLUSH_REQ != new_request->message_id &&
          SNS_CAL_MSGID_SNS_CAL_RESET != new_request->message_id) {
        if (NULL != exist_request)
        {
          DBG_PRINTF(LOW, this, "replace req");
          instance->cb->remove_client_request(instance, exist_request);
        }
        instance->cb->add_client_request(instance, new_request);
        DBG_PRINTF(LOW, this, "adding req to Q sensor=%d req=%p msg=%d", state->sensor, new_request, new_request->message_id);
      }
    }
  }
  return instance;
}

sns_sensor_instance* steng1ax_handle_client_request(sns_sensor *const this,
                                   struct sns_request const *exist_request,
                                   struct sns_request const *new_request,
                                   bool remove)
{
  sns_sensor_instance *instance = sns_sensor_util_get_shared_instance(this);
  sns_sensor_instance *return_instance = instance;
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
  
  if(remove && (NULL != exist_request))
  {
    steng1ax_remove_request(this, instance, shared_state, exist_request);
  }
  else if(NULL != new_request)
  {
    // 1. If new request then:
    //     a. Power ON rails.
    //     b. Power ON COM port - Instance must handle COM port power.
    //     c. Create new instance.
    //     d. Re-evaluate existing requests and choose appropriate instance config.
    //     e. set_client_config for this instance.
    //     f. Add new_request to list of requests handled by the Instance.
    //     g. Power OFF COM port if not needed- Instance must handle COM port power.
    //     h. Return the Instance.
    // 2. If there is an Instance already present:
    //     a. Add new_request to list of requests handled by the Instance.
    //     b. Remove exist_request from list of requests handled by the Instance.
    //     c. Re-evaluate existing requests and choose appropriate Instance config.
    //     d. set_client_config for the Instance if not the same as current config.
    //     e. publish the updated config.
    //     f. Return the Instance.
    // 3.  If "flush" request:
    //     a. Perform flush on the instance.
    //     b. Return NULL.

    if (NULL != instance)
    {
      steng1ax_instance_state *inst_state = (steng1ax_instance_state*)instance->state->state;
      if(SNS_STD_MSGID_SNS_STD_FLUSH_REQ == new_request->message_id) // most frequent request
      {
        if(NULL == exist_request)
        {
          SNS_PRINTF(HIGH, this, "orphan Flush req!");
          return_instance = NULL;
        }
        else
        {
          steng1ax_handle_flush_request(this, instance, shared_state);
#if STENG1AX_FLUSH_SPECIAL_HANDLING
          return_instance = &sns_instance_no_error;
#endif
        }
      }
      else
      {
        if(!inst_state->self_test_info.test_alive)
        {
          if(steng1ax_is_valid_oem_request(new_request->message_id))
          {
            steng1ax_handle_oem_request(this, instance, new_request);
            return instance;
          }
          else if(SNS_CAL_MSGID_SNS_CAL_RESET != new_request->message_id)
          {
            if(SNS_RC_SUCCESS != steng1ax_add_new_request(this, instance, shared_state, new_request))
            {
              if(NULL != exist_request)
              {
                DBG_PRINTF(HIGH, this, "restoring existing req");
                instance->cb->add_client_request(instance, exist_request);
              }
              return_instance = NULL; // no instance is handling this invalid request
            }
          }
          else // CAL_RESET is the least frequent request
          {
            if(NULL == exist_request)
            {
              instance->cb->add_client_request(instance, new_request);
              instance->cb->remove_client_request(instance, new_request);
            }
          }
        }
        else
        {
          DBG_PRINTF(HIGH, this, "Selftest running. Reject");
          return_instance = NULL; // no instance is handling this request
        }
      }
#if STENG1AX_POWERRAIL_DISABLED
      if(NULL != instance)
      {
        steng1ax_set_client_config(this, instance, shared_state);
      }
#endif
    }
  }
  else // bad request
  {
    return_instance = NULL; // no instance is handling this invalid request
  }

  if(NULL != instance && !steng1ax_is_request_present(instance, shared_state->hw_idx))
  {
    // steng1ax_instance_state *inst_state = (steng1ax_instance_state*)instance->state->state;
    // must call remove_instance() when clientless

    SNS_PRINTF(MED, this, "client_req: remove instance");

    this->cb->remove_instance(instance);
    return_instance = NULL;
#if !STENG1AX_POWERRAIL_DISABLED
    steng1ax_turn_rails_off(this, shared_state);
#endif
  }
  if(NULL != new_request &&
     SNS_CAL_MSGID_SNS_CAL_RESET == new_request->message_id)
  {
    return_instance = &sns_instance_no_error;
  }
  return return_instance;
}


/** See sns_steng1ax_sensor.h */
sns_sensor_instance* steng1ax_set_client_request(sns_sensor *const this,
                                                struct sns_request const *exist_request,
                                                struct sns_request const *new_request,
                                                bool remove)
{
  steng1ax_state *state = (steng1ax_state*)this->state->state;

  UNUSED_VAR(state);
  if(new_request == NULL || new_request->message_id != SNS_STD_MSGID_SNS_STD_FLUSH_REQ)
  {
    SNS_PRINTF(HIGH, this, "client_req: sensor=%u req=%d/%d remove=%u hw_id=[%u]",
               state->sensor, exist_request != NULL ? exist_request->message_id : -1,
               new_request != NULL ? new_request->message_id : -1, remove,
               state->hardware_id);
  }
  steng1ax_update_request_q(this, exist_request, new_request, remove);
  return steng1ax_handle_client_request(this, exist_request, new_request, remove);
}

sns_sensor* steng1ax_get_sensor_by_type(sns_sensor *const this, steng1ax_sensor_type sensor)
{
  sns_sensor *lib_sensor;

  for(lib_sensor = this->cb->get_library_sensor(this, true);
      NULL != lib_sensor;
      lib_sensor = this->cb->get_library_sensor(this, false))
  {
    steng1ax_state *lib_state = (steng1ax_state*)lib_sensor->state->state;
    if(lib_state->sensor == sensor)
    {
      break;
    }
  }
  return lib_sensor;
}

sns_sensor* steng1ax_get_master_sensor(sns_sensor *const this)
{
  return (steng1ax_get_sensor_by_type(this, STENG1AX_ENG)); // Eng is the master sensor
}

steng1ax_shared_state* steng1ax_get_shared_state_from_state(sns_sensor_state const *state)
{
  return (steng1ax_shared_state*)((intptr_t)(state->state) + ALIGN_8(sizeof(steng1ax_state)));
}

steng1ax_shared_state* steng1ax_get_shared_state(sns_sensor *const this)
{
  steng1ax_shared_state *shared_state = NULL;
  sns_sensor *master = steng1ax_get_master_sensor(this);
  if(NULL != master)
  {
    shared_state = steng1ax_get_shared_state_from_state(master->state);
  }
  return shared_state;
}
#if 0
steng1ax_instance_config const* steng1ax_get_instance_config(sns_sensor_state const *state)
{
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state_from_state(state);
  return &shared_state->inst_cfg;
}
#endif
void steng1ax_update_rail_vote(
  sns_sensor            *this,
  steng1ax_shared_state  *shared_state,
  sns_power_rail_state  vote)
{
#if !STENG1AX_POWERRAIL_DISABLED
  sns_power_rail_state pre_vote = shared_state->rail_config.rail_vote;
  shared_state->rail_config.rail_vote = vote;
  shared_state->pwr_rail_service->api->
    sns_vote_power_rail_update(shared_state->pwr_rail_service,
                               this,
                               &shared_state->rail_config,
                               &shared_state->power_rail_on_timestamp);
  DBG_PRINTF(MED, this, "update_rail_vote: %u->%u on_time change: %x%x : %x%x",
      pre_vote, shared_state->rail_config.rail_vote,
      (uint32_t)(shared_state->soft_pd_timestamp>>32),
      (uint32_t)(shared_state->soft_pd_timestamp),
      (uint32_t)(shared_state->power_rail_on_timestamp>>32),
      (uint32_t)(shared_state->power_rail_on_timestamp));
  if(shared_state->soft_pd_timestamp < shared_state->power_rail_on_timestamp)
  {
    shared_state->soft_pd = false;
    DBG_PRINTF(MED, this, "power rail updated, soft_pd=%d", shared_state->soft_pd);
  }
#else
  UNUSED_VAR(this);
  UNUSED_VAR(shared_state);
  UNUSED_VAR(vote);
#endif
}

/*===========================================================================
  Public Data Definitions
  ===========================================================================*/
sns_sensor_api steng1ax_eng_sensor_api =
{
  .struct_len         = sizeof(sns_sensor_api),
  .init               = &steng1ax_eng_init,
  .deinit             = &steng1ax_eng_deinit,
  .get_sensor_uid     = &steng1ax_get_sensor_uid,
  .set_client_request = &steng1ax_set_client_request,
  .notify_event       = &steng1ax_sensor_notify_event,
};
