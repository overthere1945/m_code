/**
 * @file sns_steng1ax_dae_if.c
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

#include "sns_types.h"
#include "sns_steng1ax_sensor.h"
#include "sns_steng1ax_sensor_instance.h"
#if STENG1AX_DAE_ENABLED

#include "sns_mem_util.h"
#include "sns_rc.h"
#include "sns_request.h"
#include "sns_sensor_event.h"
#include "sns_service_manager.h"
#include "sns_sensor_util.h"
#include "sns_stream_service.h"
#include "sns_time.h"

#include "sns_steng1ax_hal.h"
#include "sns_steng1ax_dae_if.h"

#include "sns_dae.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "sns_pb_util.h"
#include "sns_diag_service.h"
#include "sns_printf.h"

/* ------------------------------------------------------------------------------------ */
void steng1ax_dae_if_build_static_config_request(
  steng1ax_instance_config const *inst_cfg,
  sns_dae_set_static_config     *config_req,
  uint8_t                       hw_idx,
  uint8_t                       rigid_body_type,
  bool                          for_ag)
{
  sns_com_port_config const *com_config  = &inst_cfg->com_port_info.com_config;
  sns_async_com_port_config *ascp_config = &config_req->ascp_config;

  ascp_config->bus_type             = (sns_async_com_port_bus_type)com_config->bus_type;
  ascp_config->slave_control        = com_config->slave_control;
  ascp_config->reg_addr_type        = SNS_ASYNC_COM_PORT_REG_ADDR_TYPE_8_BIT;
  ascp_config->min_bus_speed_kHz    = com_config->min_bus_speed_KHz;
  ascp_config->max_bus_speed_kHz    = com_config->max_bus_speed_KHz;
  ascp_config->bus_instance         = com_config->bus_instance;

  if(for_ag)
  {
    sns_strlcpy(
      config_req->func_table_name,
      (hw_idx == 0) ? "steng1ax_fifo_hal_table" : "steng1ax_fifo_hal_table2",
      sizeof(config_req->func_table_name));
    if( inst_cfg->irq_config.is_ibi )
    {
      //config_req->interrupt              = SNS_DAE_INT_OP_MODE_IBI;
      config_req->interrupt              = 2;
      config_req->has_ibi_config         = true;
      config_req->ibi_config             = inst_cfg->irq_config.ibi_config;
    }
    else
    {
      //config_req->interrupt              = SNS_DAE_INT_OP_MODE_IRQ;
      config_req->interrupt              = 1;
      config_req->has_irq_config         = true;
      config_req->irq_config             = inst_cfg->irq_config.irq_config;
    }
    config_req->has_eng_info         = true;
    config_req->eng_info.eng_range =
      STENG1AX_ENG_RANGE_MAX;

    config_req->eng_info.axis_map_count = ARR_SIZE(config_req->eng_info.axis_map);
    for(uint32_t i = 0; i < config_req->eng_info.axis_map_count; i++)
    {
      dest_axis_map[i].ipaxis = 0;
      dest_axis_map[i].opaxis = 0;
      dest_axis_map[i].invert = 0;
    }

    // Populate Additional Attributes
    config_req->eng_info.eng_attr[0].value.has_sint = true;
    config_req->eng_info.eng_attr[0].value.sint = rigid_body_type;
    config_req->eng_info.eng_attr[0].attr_id = SNS_STD_SENSOR_ATTRID_RIGID_BODY;
    config_req->eng_info.eng_attr_count = 1;
  }
}

/* ------------------------------------------------------------------------------------ */
sns_rc steng1ax_dae_if_send_static_config_request(
  sns_data_stream           *stream,
  sns_dae_set_static_config *config_req)
{
  sns_rc rc = SNS_RC_FAILED;
  uint8_t encoded_msg[sns_dae_set_static_config_size];
  sns_request req = {
    .message_id  = SNS_DAE_MSGID_SNS_DAE_SET_STATIC_CONFIG,
    .request     = encoded_msg,
    .request_len = 0
  };

  req.request_len = pb_encode_request(encoded_msg, sizeof(encoded_msg), config_req,
                                      sns_dae_set_static_config_fields, NULL);
  if(0 < req.request_len)
  {
    rc = stream->api->send_request(stream, &req);
  }
  return rc;
}

/* ------------------------------------------------------------------------------------ */
sns_rc steng1ax_dae_if_init(
  sns_sensor_instance           *const this,
  sns_stream_service            *stream_mgr,
  steng1ax_instance_config const *inst_cfg)
{
  sns_rc rc = SNS_RC_NOT_AVAILABLE;
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  steng1ax_dae_if_info *dae_if = &state->dae_if;

  dae_if->ag.state = inst_cfg->dae_ag_state;
  dae_if->temp.state = inst_cfg->dae_temper_state;

  if(IDLE == dae_if->ag.state && IDLE == dae_if->temp.state)
  {
    bool dae_avail = false;
    stream_mgr->api->create_sensor_instance_stream(
                       stream_mgr, this, inst_cfg->dae_suid, &dae_if->ag.stream);
    stream_mgr->api->create_sensor_instance_stream(
                       stream_mgr, this, inst_cfg->dae_suid, &dae_if->temp.stream);
    if(NULL != dae_if->ag.stream)
    {
      sns_dae_set_static_config config_req = sns_dae_set_static_config_init_default;
      dae_if->ag.stream_usable = dae_if->temp.stream_usable = true;
      steng1ax_dae_if_build_static_config_request(inst_cfg, &config_req, state->hw_idx, state->rigid_body_type, true);
      if(SNS_RC_SUCCESS == steng1ax_dae_if_send_static_config_request(dae_if->ag.stream, &config_req))
      {
          dae_avail = true;
      }
    }
    if(!dae_avail)
    {
      steng1ax_dae_if_deinit(this);
    }
  }
  DBG_INST_PRINTF_EX(HIGH, this, "dae_if_init: state(ag/t)=%u/%u usable(ag/t)=%u/%u",
                  dae_if->ag.state, dae_if->temp.state,
                  dae_if->ag.stream_usable, dae_if->temp.stream_usable);
  return rc;
}

bool steng1ax_dae_if_support_known(sns_sensor *this)
{
  bool known = false;
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
  if(shared_state != NULL)
  {
    if (shared_state->inst_cfg.dae_temper_state == UNAVAILABLE ||
      shared_state->inst_cfg.dae_temper_state == IDLE)
    {
      known = true;
    }
  }
  return known;
}

void steng1ax_dae_if_check_support(sns_sensor *this)
{
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
  DBG_PRINTF(LOW, this, "[%u] check_support: states=%X/%X", 
             shared_state->hw_idx, shared_state->inst_cfg.dae_ag_state, 
             shared_state->inst_cfg.dae_temper_state);
  if(NULL == shared_state->dae_stream)
  {
    sns_service_manager *service_mgr = this->cb->get_service_manager(this);
    sns_stream_service *stream_svc = (sns_stream_service*)
      service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);

    stream_svc->api->create_sensor_stream(stream_svc, this,
                                          shared_state->inst_cfg.dae_suid,
                                          &shared_state->dae_stream);
  }
  if(NULL != shared_state->dae_stream)
  {
    sns_dae_set_static_config config_req = sns_dae_set_static_config_init_default;
    steng1ax_exit_island(this);
    if(shared_state->inst_cfg.dae_ag_state == PRE_INIT)
    {
      shared_state->inst_cfg.dae_ag_state = INIT_PENDING;
      steng1ax_dae_if_build_static_config_request(&shared_state->inst_cfg, &config_req,
                                  shared_state->hw_idx, shared_state->rigid_body_type, true);
    }
    else if(shared_state->inst_cfg.dae_ag_state == IDLE &&
            shared_state->inst_cfg.dae_temper_state == PRE_INIT)
    {
      shared_state->inst_cfg.dae_temper_state = INIT_PENDING;
    }

    if(strlen(config_req.func_table_name) > 0)
    {
      if(SNS_RC_SUCCESS != steng1ax_dae_if_send_static_config_request(shared_state->dae_stream, &config_req))
      {
        shared_state->inst_cfg.dae_ag_state     = UNAVAILABLE;
        shared_state->inst_cfg.dae_temper_state = UNAVAILABLE;
        sns_sensor_util_remove_sensor_stream(this, &shared_state->dae_stream);
      }
    }
  }
  else
  {
    shared_state->inst_cfg.dae_ag_state     = UNAVAILABLE;
    shared_state->inst_cfg.dae_temper_state = UNAVAILABLE;
  }
}

/* ------------------------------------------------------------------------------------ */
void steng1ax_dae_if_deinit(sns_sensor_instance *const this)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  sns_sensor_util_remove_sensor_instance_stream(this, &state->dae_if.ag.stream);
  sns_sensor_util_remove_sensor_instance_stream(this, &state->dae_if.temp.stream);
  state->dae_if.ag.flushing_hw = false;
  state->dae_if.ag.flushing_data = false;
  state->dae_if.ag.state = PRE_INIT;
  state->dae_if.temp.flushing_hw = false;
  state->dae_if.temp.flushing_data = false;
  state->dae_if.temp.state = PRE_INIT;
}

#if STENG1AX_DAE_ERROR_HANDLING_ENABLED
/* ------------------------------------------------------------------------------------ */
void steng1ax_set_status_dae_error_event(sns_sensor_instance *this, uint8_t error_count)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  SNS_INST_PRINTF(HIGH, this, "set_status_dae_error_event: error_count[%d]",error_count);
  if (error_count >= MAX_DAE_ERROR_COUNT)
  {
      //If error count of any error type is  >= 6(max), return a SNS_RC_INVALID_STATE - fatal
      state->dae_error_status = SNS_RC_INVALID_STATE;
  }
  else if (error_count >= MAX_DAE_ERROR_COUNT/2)
  {
    //If error count of any error type is  == 3(max), reset hardware chip
    steng1ax_inst_exit_island(this);
    SNS_INST_PRINTF(ERROR, this, "set_status_dae_error_event: Received errors 3 in a row. Reseting Device!");
    steng1ax_recover_device(this);
    state->dae_error_status = SNS_RC_NOT_AVAILABLE;
  }
  else
  {
    //Flush fifo everytime an error is registered
    SNS_INST_PRINTF(ERROR, this, "set_status_dae_error_event: flushing FIFO");
    steng1ax_flush_fifo(this);
  }
}

/* ------------------------------------------------------------------------------------ */
bool util_last_error_over_60secs(sns_time last_error_ts, sns_time now)
{
  float time_since_last = (sns_get_time_tick_resolution() *
                          (now - last_error_ts)) * 1E-9;

  return (time_since_last > 60)? 1 : 0;
}

/* ------------------------------------------------------------------------------------ */
void steng1ax_parse_dae_error_event(sns_sensor_instance *this,
                                 sns_dae_error_event *error_event)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  sns_time now = sns_get_system_time();
  uint8_t idx;
  
  SNS_INST_PRINTF(ERROR, this, "parse_dae_error_event: Err_src=0x%X",error_event->error_source_type);

  switch(error_event->error_source_type)
  {
    case SNS_DAE_ERROR_SOURCE_TYPE_DRIVER:
    {
      idx = MAX_FRAMEWORK_ERROR_EVENTS + error_event->sns_dae_error_data.driver.error_code.bytes[0] -1;
 
      //Check if the time difference between two consecutive error events of same type is over 1 min, 
      //If yes, reset all error counters.
      if ( (state->dae_error[idx].last_error_ts != 0 ) &&
           (util_last_error_over_60secs(state->dae_error[idx].last_error_ts, now)))
      {
        steng1ax_reset_dae_error_event_count(this);
        return;		  
      }
	  
      state->dae_error[idx].error_event_cnt++;
      state->dae_error[idx].last_error_ts = now;
      steng1ax_set_status_dae_error_event(this,state->dae_error[idx].error_event_cnt);

      SNS_INST_PRINTF(ERROR, this, "DRIVER:error code=0x%X, error data:0x%08X%08X%08X",
        error_event->sns_dae_error_data.driver.error_code.bytes[0],
        error_event->sns_dae_error_data.driver.error_data.bytes[0]<<24|
        error_event->sns_dae_error_data.driver.error_data.bytes[1]<<16|
        error_event->sns_dae_error_data.driver.error_data.bytes[2]<<8|
        error_event->sns_dae_error_data.driver.error_data.bytes[3],
        error_event->sns_dae_error_data.driver.error_data.bytes[4]<<24|
        error_event->sns_dae_error_data.driver.error_data.bytes[5]<<16|
        error_event->sns_dae_error_data.driver.error_data.bytes[6]<<8|
        error_event->sns_dae_error_data.driver.error_data.bytes[7],
        error_event->sns_dae_error_data.driver.error_data.bytes[8]<<24|
        error_event->sns_dae_error_data.driver.error_data.bytes[9]<<16|
        error_event->sns_dae_error_data.driver.error_data.bytes[10]<<8|
        error_event->sns_dae_error_data.driver.error_data.bytes[11]    );

      break;
    }

    case SNS_DAE_ERROR_SOURCE_TYPE_FRAMEWORK:
    {
      if(error_event->sns_dae_error_data.framework.framework_error_type <= _sns_dae_framework_error_type_MAX)
      {
        idx = error_event->sns_dae_error_data.framework.framework_error_type - 1;

        //Check if the last error event of same type occurred over 1 minute ago, 
        //If yes reset all error counters.
        if( (state->dae_error[idx].last_error_ts != 0 ) &&
            (util_last_error_over_60secs(state->dae_error[idx].last_error_ts, now)))
        {
          steng1ax_reset_dae_error_event_count(this);
          return;
        }

        state->dae_error[idx].error_event_cnt++;
        state->dae_error[idx].last_error_ts = now;
        steng1ax_set_status_dae_error_event(this,state->dae_error[idx].error_event_cnt);

        if(error_event->sns_dae_error_data.framework.framework_error_type == SNS_DAE_FRAMEWORK_ERROR_DATA_TYPE_MEMORY_ALLOC_ERROR)
        {
          SNS_INST_PRINTF(ERROR, this, "DAE:MEMORY_ALLOC ERROR");
          SNS_INST_PRINTF(ERROR, this, "DAE:pram alloc error: bytes req %d, threshold %d",error_event->sns_dae_error_data.framework.sns_dae_framework_error_data.mem_alloc_error.bytes_requested, 
                          error_event->sns_dae_error_data.framework.sns_dae_framework_error_data.mem_alloc_error.threshold);
        }
        
        else if(error_event->sns_dae_error_data.framework.framework_error_type == SNS_DAE_FRAMEWORK_ERROR_DATA_TYPE_SPURIOUS_INTERRUPTS_ERROR)
        {
          SNS_INST_PRINTF(ERROR, this, "DAE:SPURIOUS_INT ERROR");
          SNS_INST_PRINTF(ERROR, this, "DAE:sprs interrupts:data 0x%X",error_event->sns_dae_error_data.framework.sns_dae_framework_error_data.sprs_interrupts_error.sprs_int_count);
        }
        else
        {
        	SNS_INST_PRINTF(ERROR, this,"Unknown framework error type=0x%X", error_event->sns_dae_error_data.framework.framework_error_type);
        }
      }
      else
      {
        SNS_INST_PRINTF(ERROR, this, "wrong DAE error type=0x%X",error_event->sns_dae_error_data.framework.framework_error_type);
      }
      break;
    }
    
    default:
      SNS_INST_PRINTF(ERROR, this, "wrong error type=0x%X",error_event->error_source_type);
      break;
  }
}

/* ------------------------------------------------------------------------------------ */
void process_dae_error_event(
  sns_sensor_instance *this,
  steng1ax_dae_stream  *dae_stream,
  pb_istream_t        *pbstream)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  DBG_INST_PRINTF(LOW, this, "process_dae_error_event");
  if(dae_stream == &state->dae_if.ag)
  {
    sns_dae_error_event error_event = sns_dae_error_event_init_default;

    if(pb_decode(pbstream, sns_dae_error_event_fields, &error_event))
    {
      steng1ax_parse_dae_error_event(this,&error_event);
    }
    else
    {
      SNS_INST_PRINTF(ERROR, this, "error_event: decode fail");
    }
  }
  else
  {
    SNS_INST_PRINTF(ERROR, this, "error_event: Invalid DAE stream");
  }
}
#endif

#else
/* ------------------------------------------------------------------------------------ */
void steng1ax_dae_if_check_support(sns_sensor *this)
{
  UNUSED_VAR(this);
}

bool steng1ax_dae_if_support_known(sns_sensor *this)
{
  UNUSED_VAR(this);
  return true;
}

/* ------------------------------------------------------------------------------------ */
sns_rc steng1ax_dae_if_init(
  sns_sensor_instance           *const this,
  sns_stream_service            *stream_mgr,
  steng1ax_instance_config const *inst_cfg)
{
  UNUSED_VAR(this);
  UNUSED_VAR(stream_mgr);
  UNUSED_VAR(inst_cfg);
  return false;
}

/* ------------------------------------------------------------------------------------ */
void steng1ax_dae_if_deinit(sns_sensor_instance *const this)
{
  UNUSED_VAR(this);
}

#endif
