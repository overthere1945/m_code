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

#include "sns_mem_util.h"
#include "sns_sensor_instance.h"
#include "sns_service_manager.h"
#include "sns_stream_service.h"
#include "sns_rc.h"
#include "sns_request.h"
#include "sns_types.h"
#include "sns_sensor_util.h"

#include "sns_steng1ax_hal.h"
#include "sns_steng1ax_sensor.h"
#include "sns_steng1ax_sensor_instance.h"
#include "sns_steng1ax_log_pckts.h"

#include "sns_interrupt.pb.h"
#include "sns_async_com_port.pb.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "sns_cal.pb.h"
#include "sns_pb_util.h"
#include "sns_diag_service.h"
#include "sns_diag.pb.h"
#include "sns_sync_com_port_service.h"
#include "sns_printf.h"
#include "float.h"

extern const odr_reg_map steng1ax_odr_map[];
extern const uint32_t steng1ax_odr_map_len;


static void send_com_self_test_result(sns_sensor_instance *const instance, bool test_passed)
{

  steng1ax_instance_state *state =
     (steng1ax_instance_state*)instance->state->state;

  DBG_INST_PRINTF_EX(MED, instance, "Self Test(com) Result=%d", test_passed);

  sns_physical_sensor_test_event physical_sensor_test_event;
  uint8_t data[1] = {0};
  pb_buffer_arg buff_arg = (pb_buffer_arg)
      { .buf = &data, .buf_len = sizeof(data) };
  sns_sensor_uid *suid_current;

  //update suid
  {
    suid_current = &state->eng_info.suid;
  }

  DBG_INST_PRINTF_EX(HIGH, instance, "Sending Self Test event");

  physical_sensor_test_event.test_passed = test_passed;
  physical_sensor_test_event.test_type = SNS_PHYSICAL_SENSOR_TEST_TYPE_COM;
  physical_sensor_test_event.test_data.funcs.encode = &pb_encode_string_cb;
  physical_sensor_test_event.test_data.arg = &buff_arg;

  pb_send_event(instance,
                sns_physical_sensor_test_event_fields,
                &physical_sensor_test_event,
                sns_get_system_time(),
                SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_EVENT,
                suid_current);

  state->self_test_info.test_alive = false;
  state->health.heart_attack = false;
}

void steng1ax_inst_com_self_test(sns_sensor_instance *const this)
{
  steng1ax_instance_state *state =
    (steng1ax_instance_state*)this->state->state;
  sns_rc rv = SNS_RC_SUCCESS;
  uint8_t buffer[SENSOR_CNT] = {0};
  uint32_t rw_bytes = 0;
  bool who_am_i_success[SENSOR_CNT] = {false};
  bool success = true;
  int i =0;
  for (i =0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    rv = steng1ax_instance_com_read_wrapper(state,
        STM_STENG1AX_REG_WHO_AM_I,
        &buffer[i],
        1,
        &rw_bytes,
        i);

    if(rv == SNS_RC_SUCCESS &&
       (buffer[i] == STENG1AX_WHOAMI_VALUE))
    {
      who_am_i_success[i] = true;
    }
  }
  for (i=0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    if (who_am_i_success[i] != true)
    {
      success = false;
    }
  }
  //Send result
  send_com_self_test_result(this, success);
  //Disable all self test flags
  state->self_test_info.polling_count = 0;
  state->self_test_info.curr_odr = STENG1AX_ENG_ODR800;
}

static void inst_cleanup(sns_sensor_instance *const this,
                         steng1ax_instance_state *state)
{
  SNS_INST_PRINTF(HIGH, this, "[%u] inst_cleanup: #samples=%u",
                  state->hw_idx, state->eng_sample_counter);
  int i =0;
  for (i = 0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    steng1ax_turn_on_bus_power(state, true, i);
    steng1ax_reconfig_hw(this, i);
    steng1ax_dump_reg(this, STENG1AX_ENG, i);
    if(state->scp_service != NULL){
      steng1ax_exit_i3c_mode(&state->com_port_info[i], state->scp_service);
    }
    steng1ax_turn_on_bus_power(state, false, i);

    if(NULL != state->scp_service)
    {
      state->scp_service->api->sns_scp_close(state->com_port_info[i].port_handle);
      state->scp_service->api->sns_scp_deregister_com_port(&state->com_port_info[i].port_handle);
      if ((state->multi_eng_cfg.num_sensors_enable-1) == i)
      {
        state->scp_service = NULL;
      }
    }

    sns_sensor_util_remove_sensor_instance_stream(this, &state->interrupt_data_stream);
    sns_sensor_util_remove_sensor_instance_stream(this, &state->async_com_port_data_stream[i]);
    sns_sensor_util_remove_sensor_instance_stream(this, &state->timer_self_test_data_stream);
    sns_sensor_util_remove_sensor_instance_stream(this, &state->timer_heart_beat_data_stream);
    sns_sensor_util_remove_sensor_instance_stream(this, &state->timer_sensor_polling_data_stream);
    sns_sensor_util_remove_sensor_instance_stream(this, &state->timer_config_data_stream);

    steng1ax_dae_if_deinit(this);
    steng1ax_esp_deinit(this);
  }
}
/** See sns_sensor_instance_api::init */
sns_rc steng1ax_inst_init(sns_sensor_instance *const this, sns_sensor_state const *sstate)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state_from_state(sstate);
  steng1ax_instance_config const *inst_cfg = &shared_state->inst_cfg;
  float data[SENSOR_CNT] = {0.0f};
  int i = 0;
  sns_service_manager *service_mgr = this->cb->get_service_manager(this);
  sns_stream_service *stream_mgr = (sns_stream_service*)
              service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);
  //memset instance state
  sns_memset(state, 0, sizeof(steng1ax_instance_state));
  uint8_t hw_id = shared_state->hw_idx;
  //this would be stored when the state is initialized
  state->soft_pd = shared_state->soft_pd;
  state->hw_idx = shared_state->hw_idx;
  state->rigid_body_type = shared_state->rigid_body_type;
  state->eng_info.config_stage = CONFIG_LPF;

  state->multi_eng_cfg.use_multi_eng = inst_cfg->multi_eng_cfg.use_multi_eng;
  state->multi_eng_cfg.num_sensors_enable = inst_cfg->multi_eng_cfg.num_sensors_enable;

  for (i=0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    state->min_odr_idx = inst_cfg->min_odr_idx[hw_id];
    state->bus_pwr_on[i] = false;
  }

  /**---------Setup stream connections with dependent Sensors---------*/
  stream_mgr->api->create_sensor_instance_stream(stream_mgr,
                                                 this,
                                                 inst_cfg->irq_suid,
                                                 &state->interrupt_data_stream);

  state->eng_stream_mode = inst_cfg->eng_stream_mode;

  SNS_INST_PRINTF(HIGH, this, "[%u] inst_init:: eng_stream_mode:%d",
                  state->hw_idx, state->eng_stream_mode);
  for (i=0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    /** Initialize COM port to be used by the Instance */
    sns_memscpy(&state->com_port_info[i],
                sizeof(state->com_port_info[i]),
                &inst_cfg->com_port_info[i],
                sizeof(inst_cfg->com_port_info[i]));

    state->scp_service = (sns_sync_com_port_service*)
        service_mgr->get_service(service_mgr, SNS_SYNC_COM_PORT_SERVICE);
    state->scp_service->api->sns_scp_register_com_port(&state->com_port_info[i].com_config,
                                                &state->com_port_info[i].port_handle);
    SNS_INST_PRINTF(HIGH, this, "inst_init: open port %x", state->com_port_info[i].port_handle);
    state->scp_service->api->sns_scp_open(state->com_port_info[i].port_handle);

    stream_mgr->api->create_sensor_instance_stream(stream_mgr,
                                               this,
                                               inst_cfg->acp_suid,
                                               &state->async_com_port_data_stream[i]);
  }

  for (i=0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    if (NULL == state->async_com_port_data_stream[i])
    {
      SNS_INST_PRINTF(ERROR, this, "inst_init: ASYNC STREAM[%d] failed", i);
      inst_cleanup(this, state);
      return SNS_RC_FAILED;
    }
  }

  if(NULL == state->interrupt_data_stream)
  {
    SNS_INST_PRINTF(ERROR, this, "inst_init: INT STREAM failed");
    inst_cleanup(this, state);
    return SNS_RC_FAILED;
  }

  /**----------- Copy all Sensor UIDs in instance state -------------*/
  sns_sensor_uid* eng_suid = &((sns_sensor_uid)ENG_SUID_0);

  sns_memscpy(&state->eng_info.suid,
      sizeof(state->eng_info.suid),
      eng_suid,
      sizeof(state->eng_info.suid));

  sns_memscpy(&state->timer_suid,
      sizeof(state->timer_suid),
      &(inst_cfg->timer_suid),
      sizeof(inst_cfg->timer_suid));

  stream_mgr->api->create_sensor_instance_stream(stream_mgr,
      this,
      state->timer_suid,
      &state->timer_heart_beat_data_stream);


  /**-------------------------Init Eng State-------------------------*/
  state->eng_info.sstvt = STENG1AX_ENG_RESOLUTION;
  state->eng_info.bw = STENG1AX_ODR_BW_HALF;
  state->eng_info.sample.opdata_status = SNS_STD_SENSOR_SAMPLE_STATUS_UNRELIABLE;

  /**-------------------------Init ENG config State-------------------------*/

  for (int i = 0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    state->eng_registry_cfg[i].zin_eng2_disable = inst_cfg->eng_reg_cfg[i].zin_eng2_disable;
    state->eng_registry_cfg[i].zin_eng1_disable = inst_cfg->eng_reg_cfg[i].zin_eng1_disable;
    state->eng_registry_cfg[i].eng_mode = inst_cfg->eng_reg_cfg[i].eng_mode & 0x3;
    state->eng_registry_cfg[i].eng_impedance = inst_cfg->eng_reg_cfg[i].eng_impedance;
    state->eng_registry_cfg[i].eng_gain = inst_cfg->eng_reg_cfg[i].eng_gain;
    state->eng_impedance_idx[i] = inst_cfg->eng_impedance_idx[i];
    state->eng_gain_idx[i] = inst_cfg->eng_gain_idx[i];
  }

  SNS_INST_PRINTF(HIGH, this, "inst_init: sstvt %d %d %d", (int32_t)state->eng_info.sstvt, state->multi_eng_cfg.use_multi_eng, state->multi_eng_cfg.num_sensors_enable);

  /**-------------------------Init Self Test State-------------------------*/
  state->self_test_info.self_test_stage = STENG1AX_SELF_TEST_STAGE_1;
  state->self_test_info.test_type = SNS_PHYSICAL_SENSOR_TEST_TYPE_COM;
  state->self_test_info.test_alive = false;
  state->self_test_info.reconfig_postpone = false;

  state->health.heart_attack = false;
  state->encoded_eng_event_len = pb_get_encoded_size_sensor_stream_event(data, SENSOR_CNT);

  state->diag_service =  (sns_diag_service*)
      service_mgr->get_service(service_mgr, SNS_DIAG_SERVICE);

  for (int i = 0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    /** Initialize IRQ info to be used by the Instance */
    state->irq_info[i]                    = inst_cfg->irq_config[i];
    state->irq_info[i].irq_registered     = false;
    state->irq_info[i].irq_ready          = false;

    /** Initialize com config to be used by the Instance */
    sns_com_port_config const *com_config = &inst_cfg->com_port_info[i].com_config;
    state->ascp_config[i].bus_type          = (sns_async_com_port_bus_type)com_config->bus_type;
    state->ascp_config[i].slave_control     = com_config->slave_control;
    state->ascp_config[i].reg_addr_type     = SNS_ASYNC_COM_PORT_REG_ADDR_TYPE_8_BIT;
    state->ascp_config[i].min_bus_speed_kHz = com_config->min_bus_speed_KHz;
    state->ascp_config[i].max_bus_speed_kHz = com_config->max_bus_speed_KHz;
    state->ascp_config[i].bus_instance      = com_config->bus_instance;

    /** Configure the Async Com Port */
    {
      sns_data_stream* data_stream = state->async_com_port_data_stream[i];
      uint8_t pb_encode_buffer[100];
      sns_request async_com_port_request =
      {
        .message_id  = SNS_ASYNC_COM_PORT_MSGID_SNS_ASYNC_COM_PORT_CONFIG,
        .request     = &pb_encode_buffer
      };

      async_com_port_request.request_len =
        pb_encode_request(pb_encode_buffer,
                          sizeof(pb_encode_buffer),
                          &state->ascp_config[i],
                          sns_async_com_port_config_fields,
                          NULL);
      data_stream->api->send_request(data_stream, &async_com_port_request);
    }
  }
  state->ascp_hw_id = 0;

#if !STENG1AX_POWERRAIL_DISABLED
  if(shared_state->power_rail_pend_state == STENG1AX_POWER_RAIL_PENDING_NONE)
#endif
  {
    sns_busy_wait(sns_convert_ns_to_ticks(1000*1000));
    steng1ax_reset_device( this, STENG1AX_ENG);
  }



  steng1ax_init_esp_instance(this, sstate);
  for (int i = 0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    steng1ax_turn_on_bus_power(state, false, i);
  }
  steng1ax_dae_if_init(this, stream_mgr, inst_cfg);

  if (state->eng_stream_mode == DRI)
  {
    int idx = STENG1AX_INTR_HW_IDX;
    steng1ax_register_interrupt(this, &state->irq_info[idx], state->interrupt_data_stream);
  }

  steng1ax_init_raw_log_info(this);

  return SNS_RC_SUCCESS;
}

sns_rc steng1ax_inst_deinit(sns_sensor_instance *const this)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  DBG_INST_PRINTF_EX(HIGH, this, "inst_deinit");
  UNUSED_VAR(state);

  inst_cleanup(this, state);


  return SNS_RC_SUCCESS;
}

void steng1ax_set_client_test_config(
  sns_sensor_instance *this,
  sns_request const *client_request)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  steng1ax_instance_config const *inst_cfg =
    (steng1ax_instance_config const*)client_request->request;

  // 1. Extract test type from client_request.
  // 2. Configure sensor HW for test type.
  // 3. send_request() for Timer Sensor in case test needs polling/waits.
  // 4. Factory test is TBD.
  if(inst_cfg->selftest.test_type != SNS_PHYSICAL_SENSOR_TEST_TYPE_SW)
  {
    state->self_test_info.sensor = inst_cfg->selftest.sensor;
    state->self_test_info.test_type = inst_cfg->selftest.test_type;
    state->self_test_info.test_alive = true;
    DBG_INST_PRINTF_EX(HIGH, this, "Self test start, type=%u sensor=%u",
                    state->self_test_info.test_type, state->self_test_info.sensor);

    if(state->self_test_info.test_type == SNS_PHYSICAL_SENSOR_TEST_TYPE_COM)
    {
      steng1ax_inst_com_self_test(this);
    }
  }
  else // this should never happen as the parents would not have forwarded bad request
  {
    DBG_INST_PRINTF_EX(HIGH, this, "Unsupported test type = %d", state->self_test_info.test_type);
    state->self_test_info.test_alive = false;
  }
}
