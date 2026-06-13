/**
 * @file sns_steng1ax_sensor_instance.c
 *
 * STENG1AX Eng virtual Sensor Instance implementation.
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

#include <math.h>
#include <float.h>

#include "sns_mem_util.h"
#include "sns_sensor_instance.h"
#include "sns_event_service.h"
#include "sns_island_service.h"
#include "sns_service_manager.h"
#include "sns_stream_service.h"
#include "sns_rc.h"
#include "sns_request.h"
#include "sns_time.h"
#include "sns_sensor_event.h"
#include "sns_types.h"

#include "sns_steng1ax_hal.h"
#include "sns_steng1ax_sensor.h"
#include "sns_steng1ax_sensor_instance.h"

#include "sns_interrupt.pb.h"
#include "sns_async_com_port.pb.h"
#include "sns_sensor_util.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "sns_pb_util.h"
#include "sns_async_com_port_pb_utils.h"
#include "sns_diag_service.h"
#include "sns_diag.pb.h"
#include "sns_printf.h"
#include "sns_service_manager.h"

#define IBI_CLOCK_FREQ_RES (0.5f) //ibi clock freq resolution in MHz

#define MAX_HEART_ATTACKS 6
#define MAX_FLUSH_WAIT_NS (20*1000*1000ULL)

const odr_reg_map steng1ax_odr_map[] =
{
  {
    .odr = STENG1AX_ODR_0,
    .eng_odr_reg_value = STENG1AX_ENG_ODR_OFF,
    .odr_coeff = 0,
    .eng_group_delay = 0,
    .eng_discard_samples = 0,
    .exp_lpf0_en = 0,
  },
  {
    .odr = STENG1AX_ODR_800,
    .eng_odr_reg_value = STENG1AX_ENG_ODR800,
    .odr_coeff = 0,
    .eng_group_delay = 0.0f,
    .eng_discard_samples = 0,
    .exp_lpf0_en = 1,
  },
  {
    .odr = STENG1AX_ODR_3200,
    .eng_odr_reg_value = STENG1AX_ENG_ODR3200,
    .odr_coeff = 0,
    .eng_group_delay = 0.0f,
    .eng_discard_samples = 0,
    .exp_lpf0_en = 0,
  },
};

const uint32_t steng1ax_odr_map_len = ARR_SIZE(steng1ax_odr_map);

void steng1ax_inst_exit_island(sns_sensor_instance *this)
{
#if !STENG1AX_ISLAND_DISABLED
  sns_service_manager *smgr = this->cb->get_service_manager(this);
  sns_island_service  *island_svc  =
    (sns_island_service *)smgr->get_service(smgr, SNS_ISLAND_SERVICE);
  island_svc->api->sensor_instance_island_exit(island_svc, this);
#else
  UNUSED_VAR(this);
#endif
}
void steng1ax_inst_create_timer(sns_sensor_instance *this,
    sns_data_stream** timer_data_stream,
    sns_timer_sensor_config* req_payload)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;

  if(!timer_data_stream || !req_payload) {
    SNS_INST_PRINTF(ERROR, this, "Invalid timer data stream or req_payload pointer passed");
    return;
  }
  size_t req_len;
  uint8_t buffer[50];
  req_len = pb_encode_request(buffer, sizeof(buffer), req_payload,
      sns_timer_sensor_config_fields, NULL);
  if(req_len > 0)
  {
    if(*timer_data_stream == NULL)
    {
      sns_service_manager *service_mgr = this->cb->get_service_manager(this);
      sns_stream_service *stream_mgr = (sns_stream_service*)
        service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);
      stream_mgr->api->create_sensor_instance_stream(stream_mgr,
          this,
          state->timer_suid,
          timer_data_stream);
      DBG_INST_PRINTF_EX(MED, this, "creating timer stream!");
    }
    if(*timer_data_stream == NULL)
    {
      SNS_INST_PRINTF(HIGH, this,
          "Error creating timer stream!");
      return;
    }
    sns_request timer_req =
    {  .message_id = SNS_TIMER_MSGID_SNS_TIMER_SENSOR_CONFIG,
      .request = buffer, .request_len = req_len  };
    (*timer_data_stream)->api->send_request(*timer_data_stream, &timer_req);
    STENG1AX_INST_DEBUG_TS(
      MED, this, "set timer start_time=%u timeout_period=%u",
      (uint32_t)req_payload->start_time, (uint32_t)req_payload->timeout_period);
  }
  else
  {
    SNS_INST_PRINTF(ERROR, this, "LSM timer req encode error");
  }
}

uint8_t steng1ax_get_odr_rate_idx(float desired_sample_rate)
{
  uint8_t rate_idx = steng1ax_odr_map_len;
  if(STENG1AX_HW_MAX_ODR >= desired_sample_rate)
  {
    for(uint8_t i = 0; i < steng1ax_odr_map_len; i++)
    {
      if(desired_sample_rate <= steng1ax_odr_map[i].odr)
      {
        rate_idx = i;
        break;
      }
    }
  }
  return rate_idx;
}


void steng1ax_restart_hb_timer(sns_sensor_instance *const this, bool reset)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  steng1ax_health *health = &state->health;
  sns_timer_sensor_config req_payload = sns_timer_sensor_config_init_default;

  req_payload.is_periodic = true;
  req_payload.start_time = sns_get_system_time();
  req_payload.timeout_period = health->heart_beat_timeout;
  steng1ax_inst_create_timer(this, &state->timer_heart_beat_data_stream, &req_payload);

  health->expected_expiration = req_payload.start_time + health->heart_beat_timeout;
  if(reset)
  {
    health->heart_attack     = false;
    health->heart_attack_cnt = 0;
  }
  DBG_INST_PRINTF_EX(HIGH, this, "restart_hb_timer: exp_exp=%X%08X to=%X%08X",
    (uint32_t)(state->health.expected_expiration >> 32), (uint32_t)(state->health.expected_expiration),
    (uint32_t)(health->heart_beat_timeout >> 32), (uint32_t)health->heart_beat_timeout);

    DBG_INST_PRINTF_EX(HIGH, this, "config: %u, 0x%x",
      (uint32_t)steng1ax_odr_map[state->eng_info.curr_odr_idx].odr,
      state->enabled_sensors);
}

#if !STENG1AX_DRDY_OUT_ENABLED
static void steng1ax_handle_fifo_interrupt(sns_sensor_instance *const this, sns_time irq_ts)
{
  steng1ax_instance_state *state =
    (steng1ax_instance_state*)this->state->state;
  //set this once entered in irq handle
  state->fifo_info.th_info.recheck_int = false;
  if(state->fifo_info.cur_wmk > 0
      && state->fifo_info.fifo_rate > STENG1AX_ENG_ODR_OFF)
  {
    STENG1AX_INST_DEBUG_TS(MED, this,
        "[INT] last_ts = %u irq_time = %u",(uint32_t)(state->fifo_info.last_timestamp), (uint32_t)(irq_ts));

    //check if this irq is valid, may belong to the recent flush request
    if(irq_ts > state->fifo_info.last_timestamp) {
      state->fifo_info.th_info.interrupt_fired = true;
      state->fifo_info.th_info.interrupt_ts = irq_ts;
      for (int hw_id = 0; hw_id < state->multi_eng_cfg.num_sensors_enable; hw_id++)
      {
        steng1ax_read_fifo_data(this, irq_ts, false, hw_id);
      }
    } else {
      STENG1AX_INST_DEBUG_TS(MED, this, "[INT]: irq_time less than last_ts");
    }
  }
}
#endif

static sns_rc steng1ax_handle_int1_event(sns_sensor_instance *const this,
    sns_time cur_time,
    uint16_t event_msg,
    sns_interrupt_event* irq_event)
{
  steng1ax_instance_state *state =
    (steng1ax_instance_state*)this->state->state;
  UNUSED_VAR(cur_time);
  sns_time irq_ts = irq_event->timestamp;
  // Handle interrupts
  if(SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REG_EVENT == event_msg)
  {
    DBG_INST_PRINTF_EX(MED, this, "INT reg soft_pd %u", state->soft_pd);
    int int_idx = STENG1AX_INTR_HW_IDX;

    state->irq_info[int_idx].irq_ready = true;
    state->irq_ready = true;

    /* This is to make sure to check if client req reaches to instance(state->FW->instance) or not
    instance config_sensors variable would be set only when client req reaches to instance
    usecase:
    1.state receives req and sents to fw to call instance client_set_req function
    2. Meanwhile receives interrupt registered event
    3. At this stage, driver should wait for client req to reach instance, instead of configuring

    steng1ax specific logic, when creat new instance and fw call to register interrupt,
    soft_pd may not been set while reaching here */

    if(state->soft_pd)
    {
      if(STENG1AX_IS_ESP_DESIRED(state))
        steng1ax_reconfig_esp(this);
    }

    //steng1ax_dump_reg(this, STENG1AX_ENG);
  }
  else if(SNS_INTERRUPT_MSGID_SNS_INTERRUPT_EVENT == event_msg)
  {
    //set this once entered in irq handle

    DBG_INST_PRINTF_EX(HIGH, this, "[%d]HW INT: handle interrupt: /%d/%d/%d ts=%u",
        state->hw_idx,
        state->self_test_info.test_alive,
        STENG1AX_IS_ESP_ENABLED(state),
        irq_event->has_ibi_data,
        (uint32_t)irq_ts);

    uint8_t rw_buffer[1];

    if(STENG1AX_IS_ESP_ENABLED(state))
    {
      int i =0;
      for (i=0; i < state->multi_eng_cfg.num_sensors_enable; i++)
      {
        uint8_t wake_src = STENG1AX_REG_FSM_STATUS_MAINPAGE;
        steng1ax_read_regs_scp(this, STENG1AX_REG_FSM_STATUS_MAINPAGE, 1, rw_buffer, i);

        if(STENG1AX_IS_ESP_ENABLED(state))
        {
          steng1ax_handle_esp_interrupt(this, irq_ts, &wake_src, &rw_buffer[0], i);
        }
      }
    }
    state->irq_ts = irq_ts;
#if STENG1AX_DRDY_OUT_ENABLED
    steng1ax_handle_sensor_sample(this, irq_ts);
#else
    steng1ax_handle_fifo_interrupt(this, irq_ts);
#endif
    sns_time cur_time = sns_get_system_time();
    STENG1AX_INST_DEBUG_TS(HIGH, this, "cur_time=%X%08X exp_exp=%X%08X",
      (uint32_t)(cur_time >> 32), (uint32_t)cur_time,
      (uint32_t)(state->health.expected_expiration >> 32), (uint32_t)(state->health.expected_expiration));
      if(state->health.expected_expiration < cur_time + (state->health.heart_beat_timeout / 2))
      {
        steng1ax_restart_hb_timer(this, true);
      }
      else
      {
        state->health.heart_attack = false;
        state->health.heart_attack_cnt = 0;
      }
  }
  return SNS_RC_SUCCESS;
}


#define STENG1AX_IS_INT_LEVEL_TRIG(x) (((x) == SNS_INTERRUPT_TRIGGER_TYPE_HIGH) || \
                                      ((x) == SNS_INTERRUPT_TRIGGER_TYPE_LOW) ? true : false)

void steng1ax_send_interrupt_is_cleared_msg(sns_sensor_instance *const this, sns_data_stream* data_stream, uint8_t hw_id)
{
  steng1ax_instance_state *state =
    (steng1ax_instance_state*)this->state->state;
  // clear msg is not needed for edge triggered and dae
  if((state->irq_info[hw_id].is_ibi)||
    !STENG1AX_IS_INT_LEVEL_TRIG(state->irq_info[hw_id].irq_config.interrupt_trigger_type) ||
      steng1ax_dae_if_available(this) || !data_stream)
    return;

  sns_request irq_msg =
  {
    .message_id = SNS_INTERRUPT_MSGID_SNS_INTERRUPT_IS_CLEARED,
    .request    = NULL
  };

  data_stream->api->send_request(data_stream, &irq_msg);
  DBG_INST_PRINTF_EX(HIGH, this, "interrupt clear msg sent");
}

void steng1ax_clear_interrupt_q(sns_sensor_instance *const instance,
    sns_data_stream* interrupt_data_stream, uint8_t hw_id)
{
  UNUSED_VAR(instance);
  if((interrupt_data_stream == NULL) || steng1ax_dae_if_available(instance))
    return;
  sns_sensor_event *event;
  uint16_t num_events = 0;
  event = interrupt_data_stream->api->peek_input(interrupt_data_stream);
  while((NULL != event) && ((SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REG_EVENT != event->message_id)))
  {
    event = interrupt_data_stream->api->get_next_input(interrupt_data_stream);
    num_events++;
  }
  DBG_INST_PRINTF(HIGH, instance, "clean interrupt q events = %d", num_events);
  if(num_events)
    steng1ax_send_interrupt_is_cleared_msg(instance, interrupt_data_stream, hw_id);
}


uint16_t steng1ax_handle_interrupt(sns_sensor_instance *const instance,
    sns_data_stream* interrupt_data_stream,
    sns_rc (*interrupt_handler)(sns_sensor_instance *const , sns_time cur_time, uint16_t event_msg, sns_interrupt_event* latest_interrupt_event))
{
  sns_sensor_event *event;
  uint16_t num_events = 0;
  sns_interrupt_event irq_event = sns_interrupt_event_init_zero;
  pb_istream_t stream;
  sns_rc rv = SNS_RC_SUCCESS;
  UNUSED_VAR(rv);
  if(interrupt_data_stream == NULL)
    return num_events;
  event = interrupt_data_stream->api->peek_input(interrupt_data_stream);

  while(NULL != event)
  {
    if (SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REG_EVENT == event->message_id) {
      interrupt_handler(instance, sns_get_system_time(), SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REG_EVENT, &irq_event);

    }
    else if(SNS_INTERRUPT_MSGID_SNS_INTERRUPT_EVENT == event->message_id) {
      stream = pb_istream_from_buffer((pb_byte_t*)event->event, event->event_len);
      num_events++;
    }
    else
    {
    }
    event = interrupt_data_stream->api->get_next_input(interrupt_data_stream);
  }
  if(num_events) {
    if(pb_decode(&stream, sns_interrupt_event_fields, &irq_event))
      rv = interrupt_handler(instance, sns_get_system_time(), SNS_INTERRUPT_MSGID_SNS_INTERRUPT_EVENT, &irq_event);
  }
  return num_events;
}

static void steng1ax_handle_hw_interrupts(sns_sensor_instance *const this)
{
  steng1ax_instance_state *state =
    (steng1ax_instance_state*)this->state->state;
  uint16_t event_cnt = 0;
  event_cnt = steng1ax_handle_interrupt(this, state->interrupt_data_stream,
      &steng1ax_handle_int1_event);
  DBG_INST_PRINTF_EX(HIGH, this, "event_cnt=%d", event_cnt);
  if (event_cnt)
  {
    int idx = STENG1AX_INTR_HW_IDX;
    steng1ax_send_interrupt_is_cleared_msg(this, state->interrupt_data_stream, idx);
  }
}

/** this function executes for handling fifo read data only
 * driver make sure to have only one ascp com port req at a time*/
static void steng1ax_handle_ascp_events(sns_sensor_instance *const this)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;

  // return if no ASCP req
  if (!state->ascp_req_count[0] && !state->ascp_req_count[1] && !state->ascp_req_count[2] && !state->ascp_req_count[3])
    return;

  for (int hw_id = 0; hw_id < state->multi_eng_cfg.num_sensors_enable; hw_id++)
  {

    DBG_INST_PRINTF_EX(HIGH, this, "[%d] ASCP req_cnt=%d %d %d %d", hw_id,
      state->ascp_req_count[0],
      state->ascp_req_count[1],
      state->ascp_req_count[2],
      state->ascp_req_count[3]);

    // iterate through all ascp_req_count to check if need to address
    if (state->ascp_req_count[hw_id] == 0)
      continue;

    sns_data_stream *data_stream = state->async_com_port_data_stream[hw_id];
    sns_sensor_event *event = NULL;
    uint32_t port_rw_events_rcvd = 0;

    if(NULL == data_stream || NULL == (event = data_stream->api->peek_input(data_stream))) {
      return;
    }


    // Handle Async Com Port events
    while(NULL != event)
    {
      if(SNS_ASYNC_COM_PORT_MSGID_SNS_ASYNC_COM_PORT_ERROR == event->message_id)
      {
        SNS_INST_PRINTF(ERROR, this, "ASCP error=%d", event->message_id);
        for (int hw_id = 0; hw_id < state->multi_eng_cfg.num_sensors_enable; hw_id++)
        {
          state->ascp_req_count[hw_id] = 0;
        }
      }
      else if(SNS_ASYNC_COM_PORT_MSGID_SNS_ASYNC_COM_PORT_VECTOR_RW == event->message_id &&
              (state->ascp_req_count[0] || state->ascp_req_count[1] || state->ascp_req_count[2] || state->ascp_req_count[3]))
      {
        if(state->fifo_info.reconfig_req)
          DBG_INST_PRINTF(HIGH, this, "ASCP VEC_RW req_cnt=%d %d %d %d", state->ascp_req_count[0], state->ascp_req_count[1], state->ascp_req_count[2], state->ascp_req_count[3]);
        state->ascp_req_count[hw_id]--;

        //is this timestamp accurate to use?
        state->fifo_info.ascp_event_timestamp = event->timestamp;
        pb_istream_t stream = pb_istream_from_buffer((uint8_t *)event->event, event->event_len);
        state->ascp_hw_id = hw_id;
        sns_ascp_for_each_vector_do(&stream, steng1ax_process_com_port_vector, (void *)this);

        if(state->flushing_sensors != 0)
        {
          if (is_data_ready_to_process(this, state->multi_eng_cfg.num_sensors_enable))
          {
            steng1ax_send_fifo_flush_done(this, state->flushing_sensors, FLUSH_DONE_AFTER_DATA);
            state->flushing_sensors = 0;
            steng1ax_restart_hb_timer(this, false);
          }
        }
        if (hw_id == STENG1AX_INTR_HW_IDX)
          steng1ax_send_interrupt_is_cleared_msg(this, state->interrupt_data_stream, hw_id);
        port_rw_events_rcvd++;
      }
      event = data_stream->api->get_next_input(data_stream);
    }

    if(port_rw_events_rcvd > 0)
    {
      if(port_rw_events_rcvd > 1)
      {
        STENG1AX_INST_DEBUG_TS(MED, this, "#rw_evs=%u", port_rw_events_rcvd);
      }
      //if heart beat had happened, just reset the flag
      //if events are coming through, no need to worry about heart_attack
      state->health.heart_attack = false;
      //TODO::how about the counter
      if(state->health.heart_attack_cnt < MAX_HEART_ATTACKS/2)
      {
        state->health.heart_attack_cnt = 0;
        // how does framework reacts receiving data after heartattack
      }
    }
  }
}

static sns_rc steng1ax_handle_heart_attack_timer_events(
  sns_sensor_instance *const this,
  sns_timer_sensor_event *timer_event)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  steng1ax_health *health = &state->health;
  sns_rc rv = SNS_RC_SUCCESS;

  if(((state->desired_sensors & STENG1AX_ENG)) &&
     !state->self_test_info.test_alive)
  {
    DBG_INST_PRINTF(MED, this,
      "heart_attack: count=%u to=%u exp=%u",
      state->health.heart_attack_cnt,
      (uint32_t)timer_event->timeout_time,
      (uint32_t)health->expected_expiration);
  }
  else
  {
    health->expected_expiration = UINT64_MAX;
    health->heart_beat_timeout = UINT64_MAX/2;
    steng1ax_restart_hb_timer(this, true);
    health->heart_attack = false;
    return SNS_RC_SUCCESS;
  }
  //check this event is closer to last timestamp
  //case: interrupt and timer would have fired at the same time
  //no need to handle this, check current time with previous interrupt time
  // QC: It's probably sufficient to look at the timer event timestamp.
  sns_time now = sns_get_system_time();
  int64_t diff = timer_event->timeout_time - state->irq_ts;

  if(diff < (health->heart_beat_timeout >> 1)) {
    //checkng most recent event, if the diff is less than one report interval
    DBG_INST_PRINTF(LOW, this, "heart_attack: diff=%u ", (int32_t)diff);
    health->expected_expiration = sns_get_system_time()  + health->heart_beat_timeout;
    return SNS_RC_SUCCESS;
  }

  health->heart_attack = true;
  health->heart_attack_cnt++;
  rv = SNS_RC_SUCCESS;
  if(health->heart_attack_cnt >= MAX_HEART_ATTACKS) {
    steng1ax_inst_exit_island(this);
    SNS_INST_PRINTF(ERROR, this, "heart_attack: hw_idx:[%u] Max reset tried.", state->hw_idx);
    health->expected_expiration = UINT64_MAX;
    health->heart_beat_timeout = UINT64_MAX/2;
    health->heart_attack = false;
    steng1ax_restart_hb_timer(this, true);
    steng1ax_send_interrupt_is_cleared_msg(this, state->interrupt_data_stream, STENG1AX_INTR_HW_IDX);
    health->heart_attack_cnt = 0;
    rv = SNS_RC_INVALID_STATE;

  } else if(health->heart_attack_cnt >= MAX_HEART_ATTACKS/2) {
    // reset sensor Something is wrong. Reset device.
    // Reset Sensor
    steng1ax_inst_exit_island(this);
    SNS_INST_PRINTF(ERROR, this,
        "heart_attack: hw_idx:[%u] Something wrong. Reseting Device!", state->hw_idx);
    steng1ax_send_interrupt_is_cleared_msg(this, state->interrupt_data_stream, STENG1AX_INTR_HW_IDX);
    for (int i=0; i < state->multi_eng_cfg.num_sensors_enable; i++)
    {
      rv = steng1ax_recover_device(this, i);
    }
    //restart timer here even if this is periodic
    steng1ax_restart_hb_timer(this, false);
    // increment till it reaches MAX_HEART_ATTACKS
    health->heart_attack_cnt++;
    health->heart_attack = false;
    rv = SNS_RC_NOT_AVAILABLE;
  }
  if(rv == SNS_RC_SUCCESS) {
    int64_t ts_diff;
#if STENG1AX_DRDY_OUT_ENABLED
    steng1ax_handle_sensor_sample(this, sns_get_system_time());
#else
    DBG_INST_PRINTF(LOW, this,
        "heart_attack: hw_idx:[%u] flushing FIFO", state->hw_idx);
    for (int i=0; i < state->multi_eng_cfg.num_sensors_enable; i++)
    {
      steng1ax_flush_fifo(this, i);
    }
#endif
    steng1ax_send_interrupt_is_cleared_msg(this, state->interrupt_data_stream, STENG1AX_INTR_HW_IDX);

    ts_diff = now - state->eng_info.last_ts;
    if(ts_diff < health->heart_beat_timeout) {
      DBG_INST_PRINTF(LOW, this, "heart_attck: ts_diff=%d cur=%u",ts_diff, (uint32_t)now);
      state->health.heart_attack_cnt = 0;
      state->health.heart_attack = false;
      steng1ax_send_interrupt_is_cleared_msg(this, state->interrupt_data_stream, STENG1AX_INTR_HW_IDX);
    }  
    health->expected_expiration += health->heart_beat_timeout;
  }
  return rv;
}

sns_rc steng1ax_handle_timer(
    sns_sensor_instance *const instance, sns_data_stream* timer_data_stream,
    sns_rc (*timer_handler)(sns_sensor_instance *const , sns_time, sns_timer_sensor_event* latest_timer_event))
{
  sns_sensor_event *event;
  uint16_t num_events = 0;
  sns_timer_sensor_event timer_event;
  sns_rc rv = SNS_RC_SUCCESS;
  if(timer_data_stream == NULL)
    return rv;
  event = timer_data_stream->api->peek_input(timer_data_stream);
  while(NULL != event)
  {
    pb_istream_t stream = pb_istream_from_buffer((pb_byte_t*)event->event,
        event->event_len);
    if(pb_decode(&stream, sns_timer_sensor_event_fields, &timer_event))
    {
      if(event->message_id == SNS_TIMER_MSGID_SNS_TIMER_SENSOR_EVENT)
      {
        num_events++;
      }
    }
    else
    {
    }
    event = timer_data_stream->api->get_next_input(timer_data_stream);
  }
  if(num_events) {
    rv = timer_handler(instance,sns_get_system_time(), &timer_event);
  }
  return rv;
}

sns_rc steng1ax_handle_config_timer_events(sns_sensor_instance *const instance, sns_timer_sensor_event *timer_event)
{
  steng1ax_instance_state *state =
    (steng1ax_instance_state*)instance->state->state;
  UNUSED_VAR(timer_event);
  switch (state->eng_info.config_stage)
  {
    case CONFIG_FIFO:
      DBG_INST_PRINTF_EX(LOW, instance, "CONFIG_FIFO");
      for (int hw_id = 0 ; hw_id < state->multi_eng_cfg.num_sensors_enable; hw_id++)
      {
        if(state->fifo_info.reconfig_req) {
          //flush before configuring in non-dae mode
          steng1ax_reconfig_fifo(instance, steng1ax_dae_if_available(instance) ? false : true, hw_id);
        }
      }

    if(state->fifo_info.fifo_enabled != 0 ||
       state->desired_sensors != 0 ||
       STENG1AX_IS_ESP_ENABLED(state) ||
       STENG1AX_IS_XSENSOR_TIMER_ON(state)
      ) {
#if STENG1AX_DAE_ENABLED
      if(steng1ax_dae_if_available(instance)) {
        steng1ax_dae_if_start_streaming(instance);
      }
      // else
#endif
    }
    state->eng_info.config_stage++;
    case CONFIG_LPF:
    {
      DBG_INST_PRINTF_EX(LOW, instance, "CONFIG_LPF");
      uint8_t buffer[2] = {0};
      bool lpf0_en_set = false;
      if (STENG1AX_ODR_800 == steng1ax_odr_map[state->eng_info.desired_odr_idx].odr)
      {
        buffer[0] = 0x04;
        lpf0_en_set = true;
      }
      for (int hw_id = 0 ; hw_id < state->multi_eng_cfg.num_sensors_enable; hw_id++)
      {
        // LPF0_EN bit- 0 = 3200Hz , 1 = 800Hz
        steng1ax_write_regs_scp(instance, STM_STENG1AX_REG_CTRL3, 1, &buffer[0], hw_id);
        state->eng_info.lpf0_en_set = lpf0_en_set;

        // reset to zero
        buffer[1] = 0x00;
        steng1ax_write_regs_scp(instance, STM_STENG1AX_REG_AH_ENG_CFG3, 1, &buffer[1], hw_id);

      }
      DBG_INST_PRINTF(LOW, instance, "eng_config: odr=0x%d, CTRL3=0x%x, CFG3=0x%x",
                (int)steng1ax_odr_map[state->eng_info.desired_odr_idx].odr, buffer[0], buffer[1]);
      state->eng_info.config_stage++;
      steng1ax_inst_exit_island(instance);
      steng1ax_start_sensor_config_timer(instance);
      break;
    }
    case CONFIG_ODR:
    {
      DBG_INST_PRINTF_EX(LOW, instance, "CONFIG_ODR");
      steng1ax_eng_odr eng_odr = state->eng_info.desired_odr_reg_val;
      UNUSED_VAR(eng_odr);
      //Set minimum ODR
      for (int hw_id = 0 ; hw_id < state->multi_eng_cfg.num_sensors_enable; hw_id++)
      {
        if(state->eng_info.desired_odr_reg_val == STENG1AX_ENG_ODR_OFF)
        {
          if(STENG1AX_IS_ESP_ENABLED(state))
          {
            eng_odr = (steng1ax_eng_odr)steng1ax_get_esp_rate_idx(instance);
          }
          else
          {
            eng_odr = steng1ax_odr_map[state->min_odr_idx].eng_odr_reg_value;
          }
        }

#if !STENG1AX_DRDY_OUT_ENABLED
        if(state->fifo_info.wmk_postpone)
          steng1ax_set_fifo_wmk(instance, hw_id);

        steng1ax_start_fifo_streaming(instance, hw_id);
#else
        steng1ax_set_odr_config(instance,
            eng_odr,
            state->eng_info.sstvt,
            state->eng_info.bw,
            hw_id);
#endif
        if ((state->multi_eng_cfg.num_sensors_enable-1) == hw_id)
        {
          state->enabled_sensors = state->desired_sensors;
          state->eng_info.curr_odr_idx = state->eng_info.desired_odr_idx;
          state->eng_info.curr_odr = steng1ax_odr_map[state->eng_info.curr_odr_idx].odr;
          state->eng_info.num_samples_to_discard = steng1ax_odr_map[state->eng_info.curr_odr_idx].eng_discard_samples;
          state->eng_info.last_ts = sns_get_system_time();
          state->eng_info.sampling_intvl = steng1ax_get_sample_interval(instance, (float)state->eng_info.curr_odr);

          state->fifo_info.reconfig_req = false;
          state->fifo_info.full_reconf_req = false;

          if (state->eng_stream_mode == DRI)
          {
            steng1ax_set_interrupts(instance, true, STENG1AX_INTR_HW_IDX);
            steng1ax_update_heartbeat_monitor(instance);
          }
          else
          {
            steng1ax_start_sensor_polling_timer(instance);
          }
          DBG_INST_PRINTF(LOW, instance, "eng_config: stage=%u conf_event=%d curr_odr=%d num_samples_to_discard=%u sampling_interval=%u",
            state->eng_info.stage, state->eng_info.config_event,
            (int)state->eng_info.curr_odr, state->eng_info.num_samples_to_discard,
            (uint32_t)state->eng_info.sampling_intvl);
          steng1ax_send_config_event(instance, false);
        }
        steng1ax_dump_reg(instance, STENG1AX_ENG, hw_id);
      }
      state->eng_info.config_stage = CONFIG_IDLE;
      break;
    }
    default:
      break;
  }
  return SNS_RC_SUCCESS;
}


static sns_rc steng1ax_handle_polling_timer_event(sns_sensor_instance *const this,
  sns_time timestamp,
  sns_timer_sensor_event* latest_timer_event)
{
UNUSED_VAR(timestamp);
sns_rc rv = SNS_RC_SUCCESS;
steng1ax_instance_state *state =
  (steng1ax_instance_state*)this->state->state;
if(NULL != state->timer_sensor_polling_data_stream)
{
  
  steng1ax_handle_sensor_sample(this, latest_timer_event->timeout_time);
}
return rv;
}

static sns_rc steng1ax_handle_heart_beat_timer_event(sns_sensor_instance *const this,
    sns_time timestamp,
    sns_timer_sensor_event* latest_timer_event)
{
  sns_rc rv = SNS_RC_SUCCESS;
  UNUSED_VAR(timestamp);
  steng1ax_instance_state *state =
    (steng1ax_instance_state*)this->state->state;
  if(!state->self_test_info.test_alive) //Skip heart attack for  self-test case
    rv = steng1ax_handle_heart_attack_timer_events(this, latest_timer_event);
  return rv;
}

static sns_rc steng1ax_handle_config_timer_event(sns_sensor_instance *const this,
    sns_time timestamp,
    sns_timer_sensor_event* latest_timer_event)
{
  sns_rc rv = SNS_RC_SUCCESS;
  UNUSED_VAR(timestamp);
  steng1ax_instance_state *state =
    (steng1ax_instance_state*)this->state->state;
  if(NULL != state->timer_config_data_stream)
  {
    rv = steng1ax_handle_config_timer_events(this, latest_timer_event);
  }
  return rv;
}


sns_rc steng1ax_handle_timer_events(sns_sensor_instance *const this)
{
  steng1ax_instance_state *state =
    (steng1ax_instance_state*)this->state->state;
  sns_rc rv = SNS_RC_SUCCESS;

  if (state->eng_stream_mode == POLLING)
  {
    rv = steng1ax_handle_timer(this, state->timer_sensor_polling_data_stream,
      &steng1ax_handle_polling_timer_event);
  }
  else
  {
    rv = steng1ax_handle_timer(this, state->timer_heart_beat_data_stream,
        &steng1ax_handle_heart_beat_timer_event);
  }

  rv = steng1ax_handle_timer(this, state->timer_config_data_stream,
      &steng1ax_handle_config_timer_event);

  return rv;
}


static void steng1ax_report_error(sns_sensor_instance *this, sns_sensor_uid const *suid)
{
  sns_service_manager *mgr = this->cb->get_service_manager(this);
  sns_event_service *ev_svc = (sns_event_service*)mgr->get_service(mgr, SNS_EVENT_SERVICE);
  sns_sensor_event *event = ev_svc->api->alloc_event(ev_svc, this, 0);
  if(NULL != event)
  {
    event->message_id = SNS_STD_MSGID_SNS_STD_ERROR_EVENT;
    event->event_len = 0;
    event->timestamp = sns_get_system_time();
    ev_svc->api->publish_event(ev_svc, this, event, suid);
  }
}

static void steng1ax_process_flush_request(
  sns_sensor_instance *const this,
  sns_request const *client_request)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  steng1ax_sensor_type flushing_sensor = *((steng1ax_sensor_type*)client_request->request);
  sns_time now = sns_get_system_time();
  UNUSED_VAR(now);
  if(state->flushing_sensors != 0)
  {
    // Flush already started
    state->flushing_sensors |= flushing_sensor;
  }
  else if(!state->soft_pd)
  {
    steng1ax_send_fifo_flush_done(this, flushing_sensor, FLUSH_DONE_NOT_SOFT_PD);
  }
  else if((state->eng_info.last_ts + state->eng_info.sampling_intvl) > now)
  {
    // there're no samples to flush
    steng1ax_send_fifo_flush_done(this, flushing_sensor, FLUSH_DONE_FIFO_EMPTY);
  }
  else if(state->fifo_info.cur_wmk == 1 &&
          (!steng1ax_dae_if_available(this) || state->fifo_info.max_requested_wmk <= 1))
  {
    // ignore flush req if wmk=1
    steng1ax_send_fifo_flush_done(this, flushing_sensor, FLUSH_DONE_FIFO_EMPTY);
  }
  /* DAE usecase: flush DAE data only if hw_wmk=1 and dae_wmk > 1 */
  else if(state->fifo_info.cur_wmk == 1 &&
      state->fifo_info.max_requested_wmk > state->fifo_info.cur_wmk && 
      steng1ax_dae_if_available(this)) {
      state->flushing_sensors |= flushing_sensor;
      DBG_INST_PRINTF_EX(MED, this, "flush samples: s=0x%x wmk:%d dae_wmk=%d",
          flushing_sensor, state->fifo_info.cur_wmk, state->fifo_info.max_requested_wmk);
      steng1ax_dae_if_flush_samples(this);
  }
  else
  {
    sns_time next_int;
    state->flushing_sensors |= flushing_sensor;
    next_int = (state->fifo_info.bh_info.interrupt_ts + 
                state->fifo_info.avg_interrupt_intvl);
    if(state->fifo_info.max_requested_wmk > state->fifo_info.cur_wmk  || // DAE WM bigger than FIFO WM.
       next_int < now || // flush-only request or missed interrupt
       next_int > now + (SNS_MAX(state->fifo_info.avg_sampling_intvl, sns_convert_ns_to_ticks(MAX_FLUSH_WAIT_NS) /*20ms*/)))
    {
      for(int hw_id = 0 ; hw_id < state->multi_eng_cfg.num_sensors_enable; hw_id++)
        steng1ax_flush_fifo(this, hw_id);
    }
    // else be patient, interrupt will fire soon
  }
}

static sns_rc steng1ax_parse_config_info(
  sns_sensor_instance     *const this,
  steng1ax_instance_config *inst_cfg)
{
  sns_rc rc = SNS_RC_SUCCESS;
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  uint8_t desired_rate_idx = 0;
  uint32_t curr_max_requested_wmk = state->fifo_info.max_requested_wmk;


  state->desired_sensors = inst_cfg->client_present;
  state->config_sensors |= inst_cfg->config_sensors;

  desired_rate_idx = steng1ax_get_odr_rate_idx(inst_cfg->sample_rate);

  if(desired_rate_idx < steng1ax_odr_map_len)
  {
    //uint16_t odr_coeff = steng1ax_odr_map[desired_rate_idx].odr_coeff;
    //float odr_drift_max = state->clock_trim_factor * STENG1AX_HW_MAX_ODR;
    //float actual_odr = odr_drift_max/odr_coeff;
    float actual_odr = steng1ax_odr_map[desired_rate_idx].odr;
    float desired_report_rate = inst_cfg->report_rate;
    float dae_report_rate     = inst_cfg->dae_report_rate;
    state->fifo_info.fifo_enabled       = inst_cfg->fifo_enable;

    state->eng_info.desired_odr_idx         = desired_rate_idx;
    state->eng_info.desired_odr_reg_val     = steng1ax_odr_map[desired_rate_idx].eng_odr_reg_value;
    state->eng_info.desired_odr             = steng1ax_odr_map[desired_rate_idx].odr;

    desired_report_rate = SNS_MIN(desired_report_rate, state->eng_info.desired_odr);
    dae_report_rate     = SNS_MIN(dae_report_rate, state->eng_info.desired_odr);


    // calculate FIFO WM
    if(STENG1AX_ODR_0 < state->eng_info.desired_odr && 0.0f < desired_report_rate)
    {
      uint16_t max_fifo = STENG1AX_MAX_FIFO;

      if(((float)state->eng_info.desired_odr / UINT16_MAX) >= desired_report_rate)
      {
        state->eng_info.desired_wmk = max_fifo;
      }
      else
      {
        state->eng_info.desired_wmk =
          (uint16_t)(actual_odr/desired_report_rate);
        if( state->eng_info.desired_wmk > max_fifo )
        {
          uint32_t divider = (max_fifo - 1 + state->eng_info.desired_wmk) / max_fifo;
          state->eng_info.desired_wmk = state->eng_info.desired_wmk / SNS_MAX(divider,1);
        }
        else if(state->eng_info.desired_wmk * desired_report_rate * 0.9f > actual_odr)
        {
          // must adjust WM or the actual report rate would be too low
          state->eng_info.desired_wmk--;
        }
        state->eng_info.desired_wmk = SNS_MAX(state->eng_info.desired_wmk, 1);
      }
      DBG_INST_PRINTF(MED, this, "wmk desired wmk%d rr %d act %d", (int)state->eng_info.desired_wmk, (int)desired_report_rate, (int)actual_odr);
    }
    else if(STENG1AX_ODR_0 == state->eng_info.desired_odr)
    {
      state->eng_info.desired_wmk = 0;
    }
    else
    {
      state->eng_info.desired_wmk = 1;
    }

    // calculate DAE WM
    // if(FLT_MIN < dae_report_rate &&
    //    (actual_odr / (float)UINT32_MAX) < dae_report_rate)
    // {
    //   state->fifo_info.max_requested_wmk =
    //    (uint32_t)(actual_odr / dae_report_rate);
    //   if(state->fifo_info.max_requested_wmk > state->eng_info.desired_wmk)
    //   {
    //     state->fifo_info.max_requested_wmk =
    //       (state->fifo_info.max_requested_wmk / state->eng_info.desired_wmk) *
    //       state->eng_info.desired_wmk;
    //   }
    //   if(state->fifo_info.max_requested_wmk * dae_report_rate * 0.9f > actual_odr)
    //   {
    //     // must adjust WM or the actual report rate would be too low
    //     state->fifo_info.max_requested_wmk--;
    //   }
    //   state->fifo_info.max_requested_wmk = SNS_MAX(state->fifo_info.max_requested_wmk, 1);
    // }
    // else
    // {
    //   state->fifo_info.max_requested_wmk = UINT32_MAX;
    // }

    // Flush old DAE data before reconfiguration as SEE driver does not need the old data.
    // if(steng1ax_dae_if_available(this) &&
    //   state->desired_conf.publish_sensors & STENG1AX_ENG &&
    //    !state->current_conf.max_requested_flush_ticks &&
    //    state->current_conf.odr > STENG1AX_ODR_0)
    // {
    //   state->fifo_info.last_timestamp = sns_get_system_time();
    //   DBG_INST_PRINTF(LOW, this, "config: resetting last_timestamp last_ts=%u flush_ticks(c:d)= %u:%u",
    //       (uint32_t)state->fifo_info.last_timestamp,
    //       (uint32_t)state->current_conf.max_requested_flush_ticks,
    //       (uint32_t)state->desired_conf.max_requested_flush_ticks);
    //   steng1ax_dae_if_flush_hw(this);
    // }

    state->eng_info.desired_max_requested_flush_ticks = inst_cfg->flush_period_ticks * 1.1f;
    // DBG_INST_PRINTF(LOW, this, "config: publish=0x%x dae_wm=%u flush_per=%u",
    //                 state->desired_conf.publish_sensors, state->fifo_info.max_requested_wmk,
    //                 (uint32_t)state->desired_conf.max_requested_flush_ticks);

    state->fifo_info.reconfig_req = false;
    state->fifo_info.full_reconf_req = false;

    if((state->eng_info.desired_wmk != state->eng_info.curr_wmk) ||
       (state->eng_info.desired_odr != state->eng_info.curr_odr) ||
       (state->desired_sensors != state->enabled_sensors))
    {
      state->fifo_info.reconfig_req = true;
    }

    if(state->desired_sensors == 0 && state->enabled_sensors == 0)
    {
      state->fifo_info.reconfig_req = false;
    }

    if(!state->fifo_info.reconfig_req &&
       curr_max_requested_wmk != state->fifo_info.max_requested_wmk)
    {
      steng1ax_update_heartbeat_monitor(this);
    }

    //If odr, wmk, sensors all same as before (reconfig_req = false)
    //but flush_period is changed
    //case: if first request is with flush_period = 0 (DAE discards data instead of pushing to SEE)
    //and new request has non-zero flush_period
    bool will_stream = ((state->desired_sensors & STENG1AX_ENG) &&
        (!steng1ax_dae_if_available(this) || inst_cfg->flush_period_ticks));

    if(!state->fifo_info.reconfig_req &&
        !state->fifo_info.is_streaming &&
        will_stream) {
      state->fifo_info.last_ts_valid = false; //set last ts as inaccurate
      state->fifo_info.is_streaming = will_stream;
      //move last ts, useful when flush comes right after this
      state->fifo_info.last_timestamp = sns_get_system_time();
    }

    DBG_INST_PRINTF(MED, this, "[%u] config: have(i,w,r,es) %u,%u,%u,%x",
                    state->hw_idx, state->eng_info.curr_odr_idx,
                    state->eng_info.curr_wmk,
                    (uint32_t)state->eng_info.curr_odr,
                    state->enabled_sensors);
    DBG_INST_PRINTF(MED, this, "[%u] config: want(i,w,r,es) %u,%u,%u,%x",
                    state->hw_idx, state->eng_info.desired_odr_idx,
                    state->eng_info.desired_wmk,
                    (uint32_t)state->eng_info.desired_odr,
                    state->desired_sensors);
  }
  else
  {
    rc = SNS_RC_FAILED;
    SNS_INST_PRINTF(ERROR, this, "Sending error report for ENG");
    steng1ax_report_error(this, &state->eng_info.suid);
  }

  return rc;
}

static void steng1ax_process_sensor_config_request(
  sns_sensor_instance *const this,
  sns_request const *client_request)
{
  // 1. Extract sample, report rates from client_request.
  // 2. Configure sensor HW.
  // 3. sendRequest() for Timer to start/stop in case of polling using timer_data_stream.
  // 4. sendRequest() for Intrerupt register/de-register in case of DRI using interrupt_data_stream.
  // 5. Save the current config information like type, sample_rate, report_rate, etc.
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  steng1ax_instance_config *payload = (steng1ax_instance_config*)client_request->request;

  if(state->self_test_info.reconfig_postpone) {
    payload->config_sensors |= state->config_sensors;
  }

  DBG_INST_PRINTF_EX(HIGH, this, "config_sensors = 0x%x", payload->config_sensors);

  sns_rc rc = steng1ax_parse_config_info(this, payload);
  if(SNS_RC_SUCCESS == rc)
  {
    const odr_reg_map *map_entry = &steng1ax_odr_map[state->eng_info.desired_odr_idx];

    DBG_INST_PRINTF(MED, this, "[%u] desired=0x%x enabled=0x%x self_testing=%d reconfig=%u",
      state->hw_idx, state->desired_sensors, state->enabled_sensors, state->self_test_info.test_alive,
      state->fifo_info.reconfig_req);

    // Reset ENG bits of config sensors, If there is no change in configuration
    if(!state->fifo_info.reconfig_req && !steng1ax_dae_if_available(this))
    {
      state->config_sensors &= ~STENG1AX_ENG;
    }

    if(state->eng_info.curr_odr > 0.0f)
    {
      // new clients need to know ODR/WM for samples received between now and
      // when the next config takes effect
      steng1ax_send_config_event(this, true);
    }
    else
    {
      // sns_memzero(&state->fifo_info.last_sent_config, sizeof(state->fifo_info.last_sent_config));
    }

    // Update variables that may have been set to different value when client config changes
    steng1ax_set_fifo_config(this,
        state->eng_info.desired_wmk,
        map_entry->eng_odr_reg_value,
        state->fifo_info.fifo_enabled);

    if(!state->self_test_info.test_alive)
    {
#if STENG1AX_DAE_ENABLED
      if(state->config_step == STENG1AX_CONFIG_IDLE)
      {
        bool stopping = false;
        if(state->fifo_info.reconfig_req)
        {
          steng1ax_disable_fifo_intr(this, hw_id);
          steng1ax_sensor_type sensors = 
            (state->config_sensors | state->current_conf.enabled_sensors);
          stopping |= steng1ax_dae_if_stop_streaming(this, sensors);
        }
        if(stopping)
        {
          state->config_step = STENG1AX_CONFIG_STOPPING_STREAM;
        }
      }
      if(state->config_step == STENG1AX_CONFIG_IDLE)
#endif
      {
        steng1ax_reconfig_hw(this, STENG1AX_INTR_HW_IDX);
        state->self_test_info.reconfig_postpone = false;
      }
    }
    else
    {
      //Postpone the reconfig till Self-test is over
      state->fifo_info.reconfig_req = false;
      state->self_test_info.reconfig_postpone = true;
    }

  }
}

/** See sns_sensor_instance_api::notify_event */
static sns_rc steng1ax_inst_notify_event(sns_sensor_instance *const this)
{
  sns_rc rv = SNS_RC_SUCCESS;
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  steng1ax_dae_if_process_events(this);
#if STENG1AX_DAE_ERROR_HANDLING_ENABLED
  //If any DAE error event has return status is SNS_RC_INVALID_STATE then return that error immediately
  if (state->dae_error_status == SNS_RC_INVALID_STATE)
  {
    return SNS_RC_INVALID_STATE;
  }
#endif
  if (state->eng_stream_mode == DRI)
  {
    steng1ax_handle_ascp_events(this);
    steng1ax_handle_hw_interrupts(this);
  }
  rv = steng1ax_handle_esp_timer_events(this);
  rv = steng1ax_handle_timer_events(this);
  int i =0;
  for (i=0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    steng1ax_turn_on_bus_power(state, false, i);
  }
    return rv;
}

/** See sns_sensor_instance_api::set_client_config */
static sns_rc steng1ax_inst_set_client_config(sns_sensor_instance *const this,
                                             sns_request const *client_request)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;

  if(client_request->message_id != SNS_STD_MSGID_SNS_STD_FLUSH_REQ)
  {
    SNS_INST_PRINTF(MED, this, "[%u] client_config: msg=%u #samples=%u",
                    state->hw_idx, client_request->message_id, state->eng_sample_counter);
  }

  if(client_request->message_id == SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG)
  {
    steng1ax_process_sensor_config_request(this, client_request);
  }
  else if(client_request->message_id == SNS_STD_MSGID_SNS_STD_FLUSH_REQ)
  {
    steng1ax_process_flush_request(this, client_request);
  }
  else if(client_request->message_id == SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG)
  {
    /** All self-tests are handled in normal mode. */
    steng1ax_inst_exit_island(this);
    steng1ax_set_client_test_config(this, client_request);
  }

  steng1ax_turn_on_bus_power(state, false, 0);
  return SNS_RC_SUCCESS;
}

/** Public Data Definitions. */

sns_sensor_instance_api steng1ax_sensor_instance_api =
{
  .struct_len        = sizeof(sns_sensor_instance_api),
  .init              = &steng1ax_inst_init,
  .deinit            = &steng1ax_inst_deinit,
  .set_client_config = &steng1ax_inst_set_client_config,
  .notify_event      = &steng1ax_inst_notify_event
};

