/**
 * @file sns_steng1ax_hal_island.c
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
#include "sns_cal_util.h"
#include "sns_com_port_types.h"
#include "sns_diag_service.h"
#include "sns_event_service.h"
#include "sns_math_util.h"
#include "sns_mem_util.h"
#include "sns_rc.h"
#include "sns_sensor_event.h"
#include "sns_service_manager.h"
#include "sns_sync_com_port_service.h"
#include "sns_time.h"
#include "sns_types.h"
#include "sns_sensor_util.h"
#include "sns_steng1ax_hal.h"
#include "sns_steng1ax_sensor.h"
#include "sns_steng1ax_sensor_instance.h"
#include "sns_steng1ax_log_pckts.h"

#include "pb_decode.h"
#include "pb_encode.h"
#include "sns_printf.h"
#include "sns_async_com_port.pb.h"
#include "sns_async_com_port_pb_utils.h"
#include "sns_diag.pb.h"
#include "sns_pb_util.h"
#include "sns_std.pb.h"
#include "sns_std_sensor.pb.h"
#include "sns_timer.pb.h"

#define ASYNC_MIN_SAMPLES (5) //use ascp only if wmk > 5
#define STENG1AX_MIN_HEART_BEAT_TIMEOUT_NS (20*1000*1000ULL)
#define STENG1AX_HEART_BEAT_ODR_COUNT 5
// QC - please put usage of t and b in parenthesis
#define IS_ODR_CHANGE_TIME_EXPIRED(t, b) ((t > 0) && ((t) > (b))) ? (true) : (false)

/** Need to use ODR table. */
extern const odr_reg_map steng1ax_odr_map[];
extern const uint32_t steng1ax_odr_map_len;

#define STENG1AX_ODR_TOLERANCE (2) //% tolerance
#define STENG1AX_IS_INBOUNDS(data, ref, var) \
  (((data) > ((ref) * (100+(var)))/100) ||  \
   ((data) < ((ref) * (100-(var)))/100)) ? (false) : (true)
#define WINDOW_SIZE (20)
#define MAX_MISSING_SAMPLES (32) //max_odr / min_odr

/**
 * Turn on/off Com Port Service if not yet.
 *
 * @param[i] state            sensor instance state
 * @param[i] turn_on          true to turn on, false to turn off
 * @return sns_rc
 */
sns_rc steng1ax_turn_on_bus_power( steng1ax_instance_state * state, bool turn_on, uint8_t hw_id)
{
   sns_rc com_rv = SNS_RC_SUCCESS;
   if (turn_on !=state->bus_pwr_on[hw_id])
   {
     com_rv = state->scp_service->api->sns_scp_update_bus_power(state->com_port_info[hw_id].port_handle, turn_on);
     if (SNS_RC_SUCCESS == com_rv)
     {
       state->bus_pwr_on[hw_id] = turn_on;
     }
   }
   return com_rv;
}

 /**
 * Read wrapper for Sensor instance Synch Com Port Service.
 *
 * @param[i] state            sensor instance state
 * @param[i] reg_addr         register address
 * @param[i] buffer           read buffer
 * @param[i] bytes            bytes to read
 * @param[o] xfer_bytes       bytes read
 *
 * @return sns_rc
 */
sns_rc steng1ax_instance_com_read_wrapper(
   steng1ax_instance_state * state,
   uint32_t reg_addr,
   uint8_t *buffer,
   uint32_t bytes,
   uint32_t *xfer_bytes,
   uint8_t hw_id)
{
  sns_rc com_rv = SNS_RC_SUCCESS;

  com_rv = steng1ax_turn_on_bus_power(state, true, hw_id);
  if( SNS_RC_SUCCESS == com_rv )
  {
     com_rv = steng1ax_com_read_wrapper(state->scp_service,
                            state->com_port_info[hw_id].port_handle,
                            reg_addr,
                            buffer,
                            bytes,
                            xfer_bytes);
  }
  return com_rv;
}

sns_rc steng1ax_com_write_wrapper_scp(
  sns_sync_com_port_service* scp_service,
  sns_sync_com_port_handle*  port_handle,
   uint32_t reg_addr,
   uint8_t *buffer,
   uint32_t bytes,
   uint32_t *xfer_bytes)
{
  sns_port_vector port_vec;
  port_vec.buffer = buffer;
  port_vec.bytes = bytes;
  port_vec.is_write = true;
  port_vec.reg_addr = reg_addr;

    return scp_service->api->sns_scp_register_rw(port_handle,
                                                        &port_vec,
                                                        1,
                                                        false,
                                                        xfer_bytes);
}

/**
 * Read wrapper for Synch Com Port Service.
 *
 * @param[i] port_handle      port handle
 * @param[i] reg_addr         register address
 * @param[i] buffer           read buffer
 * @param[i] bytes            bytes to read
 * @param[o] xfer_bytes       bytes read
 *
 * @return sns_rc
 */
sns_rc steng1ax_com_read_wrapper(
  sns_sync_com_port_service* scp_service,
  sns_sync_com_port_handle*  port_handle,
   uint32_t reg_addr,
   uint8_t *buffer,
   uint32_t bytes,
   uint32_t *xfer_bytes)
{
  sns_port_vector port_vec;
  port_vec.buffer = buffer;
  port_vec.bytes = bytes;
  port_vec.is_write = false;
  port_vec.reg_addr = reg_addr;

    return scp_service->api->sns_scp_register_rw(port_handle,
                                                        &port_vec,
                                                        1,
                                                        false,
                                                        xfer_bytes);
}

/**
 * Write wrapper for Synch Com Port Service.
 *
 * @param[i] port_handle      port handle
 * @param[i] reg_addr         register address
 * @param[i] buffer           write buffer
 * @param[i] bytes            bytes to write
 * @param[o] xfer_bytes       bytes written
 * @param[i] save_write_time  true to save write transfer time.
 *
 * @return sns_rc
 */
sns_rc steng1ax_com_write_wrapper(
   sns_sensor_instance * instance,
   uint32_t reg_addr,
   uint8_t *buffer,
   uint32_t bytes,
   uint32_t *xfer_bytes,
   bool save_write_time,
   uint8_t hw_id)
{
  sns_port_vector port_vec;
  port_vec.buffer = buffer;
  port_vec.bytes = bytes;
  port_vec.is_write = true;
  port_vec.reg_addr = reg_addr;
  sns_rc rc;
  steng1ax_instance_state *inst_state =
    (steng1ax_instance_state*)instance->state->state;

  rc = steng1ax_turn_on_bus_power(inst_state, true, hw_id);
  if (SNS_RC_SUCCESS != rc)
  {
    SNS_INST_PRINTF(ERROR, instance, "steng1ax_com_write_wrapper: update_bus_power ON failed %d", rc);
  }
  else
  {
    rc = inst_state->scp_service->api->sns_scp_register_rw(inst_state->com_port_info[hw_id].port_handle,
                                                        &port_vec,
                                                        1,
                                                        save_write_time,
                                                        xfer_bytes);
  }
  if(rc != SNS_RC_SUCCESS || *xfer_bytes != bytes)
  {
    SNS_INST_PRINTF(ERROR, instance, "write_wrapper: reg=0x%x rc=%d #bytes=%u",
                    reg_addr, rc, *xfer_bytes);
  }
#if STENG1AX_DUMP_REG
  DBG_INST_PRINTF(HIGH, instance, "[%d] reg write 0x%x=%x", hw_id, reg_addr, buffer[0]);
#endif
  return rc;
}

/**
 * If mask = 0x0 or 0xFF, or if size > 1, write reg_value
 * directly to reg_addr. Else, read value at reg_addr and only
 * modify bits defined by mask.
 *
 * @param[i] port_handle      handle to synch COM port
 * @param[i] reg_addr         reg addr to modify
 * @param[i] reg_value        value to write to register
 * @param[i] size             number of bytes to write
 * @param[o]  xfer_bytes      number of bytes transfered
 * @param[i] save_write_time  save write time input
 * @param[i] mask             bit mask to update
 *
 * @return sns_rc
 * SNS_RC_FAILED - COM port failure
 * SNS_RC_SUCCESS
 */
sns_rc steng1ax_read_modify_write(
    sns_sensor_instance * instance,
    uint32_t reg_addr,
    uint8_t *reg_value,
    uint32_t size,
    uint32_t *xfer_bytes,
    bool save_write_time,
    uint8_t mask,
    uint8_t hw_id)
{
  uint8_t rw_buffer = 0;
  uint32_t rw_bytes = 0;

  sns_rc rc = SNS_RC_FAILED;
  steng1ax_instance_state *state =
    (steng1ax_instance_state*)instance->state->state;
  
  if((size > 1) || (mask == 0xFF) || (mask == 0x00))
  {
    rc = steng1ax_com_write_wrapper(instance,
                          reg_addr,
                          &reg_value[0],
                          size,
                          xfer_bytes,
                          save_write_time,
                          hw_id);

  }
  else
  {
    // read current value from this register
    rc = steng1ax_instance_com_read_wrapper (state,
                                            reg_addr,
                                            &rw_buffer,
                                            1,
                                            &rw_bytes,
                                            hw_id);
    if (SNS_RC_SUCCESS == rc)
    {
      // generate new value
      rw_buffer = (rw_buffer & (~mask)) | (*reg_value & mask);

      // write new value to this register
      rc = steng1ax_com_write_wrapper(instance,
                            reg_addr,
                            &rw_buffer,
                            1,
                            xfer_bytes,
                            save_write_time,
                            hw_id);
    }

  }

  if(rc != SNS_RC_SUCCESS)
  {
    SNS_INST_PRINTF(ERROR, instance, "read_modify_write %d", rc);
  }

#if STENG1AX_DUMP_REG
  DBG_INST_PRINTF(HIGH, instance, "reg write 0x%x=0x%x mask=0x%x",
                  reg_addr, *reg_value, mask);
#endif
  return rc;
}

/**
 * see sns_steng1ax_hal.h
 */
/**
 * Read regs using sync com port
 *
 * @param[i] state              Instance state
 * @param[i] addr              address to read
 * @param[i] num_of_bytes       num of bytes to read
 * @param[o] buffer             status registers
 *
 * @return SNS_RC_SUCCESS if successful else SNS_RC_FAILED
 */
sns_rc steng1ax_read_regs_scp(sns_sensor_instance * instance,
                             uint8_t addr, uint16_t num_of_bytes, uint8_t *buffer, uint8_t hw_id)
{
  sns_rc rc = SNS_RC_SUCCESS;
  uint32_t xfer_bytes;
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;

  rc = steng1ax_instance_com_read_wrapper(state,
                                         addr,
                                         buffer,
                                         num_of_bytes,
                                         &xfer_bytes,
                                         hw_id);

  if(rc != SNS_RC_SUCCESS)
  {
    SNS_INST_PRINTF(ERROR, instance, "read_regs_scp FAILED reg=0x%x", addr);
  }

  return rc;
}

sns_rc steng1ax_write_regs_scp(sns_sensor_instance *const instance,
                              uint8_t addr, uint16_t num_of_bytes, uint8_t *buffer, uint8_t hw_id)
{
  sns_rc rc = SNS_RC_SUCCESS;
  uint32_t xfer_bytes;

  rc = steng1ax_com_write_wrapper(instance,
                                 addr,
                                 buffer,
                                 num_of_bytes,
                                 &xfer_bytes,
                                 false,
                                 hw_id);

  if(rc != SNS_RC_SUCCESS)
  {
    DBG_INST_PRINTF(ERROR, instance, "write_regs_scp FAILED");
  }

  return rc;
}


/**
 * see sns_steng1ax_hal.h
 */
sns_rc steng1ax_set_eng_config(
    sns_sensor_instance *const instance, uint8_t hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  sns_rc rv = SNS_RC_SUCCESS;
  uint8_t buffer[3] = {0};
  uint32_t xfer_bytes;

  if (state->eng_registry_cfg[hw_id].zin_eng2_disable)
  {
    buffer[0] |= 0x10;
  }
  if (state->eng_registry_cfg[hw_id].zin_eng1_disable)
  {
    buffer[0] |= 0x08;
  }

  buffer[1] = 0x1; // AH_ENG_EN bit 0, enable AH/ENG sensor

  // AH_ENG_MODE[6:5]
  buffer[1] |= (state->eng_registry_cfg[hw_id].eng_mode << 5);

  // AH_ENG_ZIN[4:3]
  buffer[1] |= (state->eng_impedance_idx[hw_id] << 3);

  // AH_ENG_GAIN[2:1]
  buffer[1] |= (state->eng_gain_idx[hw_id] << 1);

  rv = steng1ax_read_modify_write(
        instance,
        STM_STENG1AX_REG_AH_ENG_CFG1,
        &buffer[0],
        1,
        &xfer_bytes,
        false,
        0x18,
        hw_id);

  rv |= steng1ax_read_modify_write(
        instance,
        STM_STENG1AX_REG_AH_ENG_CFG2,
        &buffer[1],
        1,
        &xfer_bytes,
        false,
        0x7F,
        hw_id);

  buffer[2] = 0x01; // AH_ENG_ACTIVE
  rv |= steng1ax_read_modify_write(
        instance,
        STM_STENG1AX_REG_AH_ENG_CFG3,
        &buffer[2],
        1,
        &xfer_bytes,
        false,
        0x01,
        hw_id);

  DBG_INST_PRINTF(HIGH, instance, "[%d] ENG_CFG1=0x%x ENG_CFG2=0x%x ENG_CFG3=0x%x", hw_id, buffer[0], buffer[1], buffer[2]);
  return rv;
}

/**
 * see sns_steng1ax_hal.h
 */
sns_rc steng1ax_set_odr_config(
   sns_sensor_instance *const instance,
                              steng1ax_eng_odr       curr_odr,
                              float                  sstvt,
                              steng1ax_eng_bw        bw,
                              uint8_t hw_id)
{
  sns_rc rv = SNS_RC_SUCCESS;
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  uint8_t buffer[1] = {0};
  // bool lpf0_en_set = false;
  if(STENG1AX_IS_ESP_ENABLED(state))
  {
    steng1ax_eng_odr esp_odr = (steng1ax_eng_odr)steng1ax_get_esp_rate_idx(instance);
    curr_odr = (curr_odr < esp_odr) ? esp_odr : curr_odr;
  }

  buffer[0] = (uint8_t)curr_odr | (uint8_t)bw;


  if(!state->self_test_info.test_alive)
  {
    state->eng_info.sstvt = sstvt;
  }
  state->eng_info.curr_odr_reg_val = curr_odr;

  DBG_INST_PRINTF(LOW, instance, "[%d] eng_config: odr=0x%x, CTRL5=0x%x",
                  hw_id, curr_odr, buffer[0]);


  rv |= steng1ax_write_regs_scp(instance, STM_STENG1AX_REG_CTRL5, 1, &buffer[0], hw_id);
  return rv;
}

void steng1ax_stop_fifo_streaming(sns_sensor_instance *const instance, uint8_t hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;

  if(state->fifo_info.fifo_rate > STENG1AX_ENG_ODR_OFF) {
    steng1ax_set_fifo_bypass_mode(instance, hw_id);

    if (state->multi_eng_cfg.num_sensors_enable-1 == hw_id)
      state->fifo_info.fifo_rate = STENG1AX_ENG_ODR_OFF;
    // state->current_conf.fifo_odr = 0;
  }

  if(state->eng_info.curr_odr_reg_val > STENG1AX_ENG_ODR_OFF)
  {
    steng1ax_set_odr_config(instance,
        STENG1AX_ENG_ODR_OFF,
        state->eng_info.sstvt,
        state->eng_info.bw,
        hw_id);
  }

  state->fifo_info.is_streaming = false;
  DBG_INST_PRINTF(MED, instance, "stop_fifo_streaming: odr a 0x%x",
                  state->eng_info.curr_odr_reg_val);
}

void steng1ax_set_fifo_config(sns_sensor_instance *const instance,
                             uint16_t desired_wmk,
                             steng1ax_eng_odr a_chosen_sample_rate,
                             steng1ax_sensor_type sensor)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;

  DBG_INST_PRINTF(MED, instance, "set_fifo_config: sensor=0x%x sr=0x%x wm=%u",
                  sensor, a_chosen_sample_rate, desired_wmk);

  state->fifo_info.desired_wmk = desired_wmk;
  state->fifo_info.desired_fifo_rate = a_chosen_sample_rate;
  // state->current_conf.fifo_odr = steng1ax_odr_map[state->eng_info.desired_odr_idx].odr;

  if(sensor & STENG1AX_ENG)
  {
    state->eng_info.desired_odr_reg_val = a_chosen_sample_rate;
    state->eng_info.bw = STENG1AX_ODR_BW_HALF;
  } else
    state->eng_info.desired_odr_reg_val = STENG1AX_ENG_ODR_OFF;

   DBG_INST_PRINTF_EX(MED, instance, "set_fifo_config ex: sensor=0x%x SR =0x%x",
                  sensor, state->eng_info.desired_odr_reg_val);
}

static void steng1ax_interrupt_interval_init_nominal(sns_sensor_instance *const instance)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  uint32_t interrupt_intvl_threshold;

  state->fifo_info.avg_interrupt_intvl =
    state->fifo_info.avg_sampling_intvl * state->eng_info.desired_wmk;

  // threshold is +- STENG1AX_ODR_TOLERANCE% of nominal sampling intvl
  interrupt_intvl_threshold =
    state->fifo_info.avg_interrupt_intvl * STENG1AX_ODR_TOLERANCE / 100;
  state->fifo_info.interrupt_intvl_upper_bound =
    state->fifo_info.avg_interrupt_intvl + interrupt_intvl_threshold;
  state->fifo_info.interrupt_intvl_lower_bound =
    state->fifo_info.avg_interrupt_intvl - interrupt_intvl_threshold;

  DBG_INST_PRINTF(
      LOW, instance, "i_intvl=%u i_bounds=%u/%u",
      state->fifo_info.avg_interrupt_intvl,
      state->fifo_info.interrupt_intvl_lower_bound,
      state->fifo_info.interrupt_intvl_upper_bound);
}

void steng1ax_set_fifo_wmk(sns_sensor_instance *const instance, uint8_t hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  uint8_t reg_fifo_wtm = 0;
  uint8_t reg_fifo_batch_dec = 0;

  DBG_INST_PRINTF(HIGH, instance, "[%d] set_fifo_wmk: cur=%u des=%u",
                  hw_id, state->fifo_info.cur_wmk, state->fifo_info.desired_wmk);

  state->fifo_info.cur_wmk = state->fifo_info.desired_wmk;
  state->eng_info.curr_wmk = state->eng_info.desired_wmk;

  if (hw_id == (state->multi_eng_cfg.num_sensors_enable-1))
    state->fifo_info.wmk_postpone = false;

  reg_fifo_wtm = state->eng_info.curr_wmk;

  steng1ax_write_regs_scp(instance, STM_STENG1AX_REG_FIFO_WTM, 1, &reg_fifo_wtm, hw_id);
  steng1ax_write_regs_scp(instance, STM_STENG1AX_REG_FIFO_BATCH_DEC, 1, &reg_fifo_batch_dec, hw_id);

  //update interrupt interval and bounds since wmk is changing
  steng1ax_interrupt_interval_init_nominal(instance);
  // steng1ax_update_heartbeat_monitor(instance);
}


/**
 * Provides sample interval based on current ODR.
 *
 * @param[i] curr_odr              Current FIFO ODR.
 *
 * @return sampling interval time in ticks
 */
sns_time steng1ax_get_sample_interval(sns_sensor_instance *const this, steng1ax_eng_odr curr_odr)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  sns_time  sample_interval = 0;
  float odr = steng1ax_get_eng_odr(curr_odr, state->eng_info.lpf0_en_set);

  if(odr > 0.0f)
  {
    sample_interval = sns_convert_ns_to_ticks(1000000000.0f / odr);
  }

  DBG_INST_PRINTF(HIGH, this, "get_sample_interval: odr=0x%x:%d s_intv=%u", curr_odr, (int)(odr*1000), (uint32_t)sample_interval);
  return sample_interval;
}


void steng1ax_update_heartbeat_monitor(sns_sensor_instance *const instance)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  state->health.sampling_interval = sns_convert_ns_to_ticks(1000000000.0f / steng1ax_odr_map[state->eng_info.desired_odr_idx].odr);
  uint32_t wm = SNS_MAX(1, state->eng_info.curr_wmk);

  if((state->desired_sensors & STENG1AX_ENG))
  {
    if(steng1ax_dae_if_available(instance))
    {
      state->fifo_info.nominal_dae_intvl =
        state->fifo_info.max_requested_wmk * state->health.sampling_interval * 1.1f;
      wm = (state->eng_info.desired_max_requested_flush_ticks == 0) ?
        UINT32_MAX : state->fifo_info.max_requested_wmk;
      DBG_INST_PRINTF(LOW, instance, "update_hb_mon:: dae_intvl=%u",
                      state->fifo_info.nominal_dae_intvl);
    }
    if(wm < STENG1AX_MAX_FIFO)
    {
      uint64_t min_hb_to = sns_convert_ns_to_ticks(STENG1AX_MIN_HEART_BEAT_TIMEOUT_NS);
      uint64_t max_hb_to = state->health.sampling_interval * STENG1AX_HW_MAX_FIFO;
      state->health.heart_beat_timeout = state->health.sampling_interval * wm * STENG1AX_HEART_BEAT_ODR_COUNT;
      // avoid sending timer request too frequently
      state->health.heart_beat_timeout = SNS_MAX(state->health.heart_beat_timeout, min_hb_to);
      // limit to one full FIFO to avoid large data gap
      state->health.heart_beat_timeout = SNS_MIN(state->health.heart_beat_timeout, max_hb_to);

      DBG_INST_PRINTF(MED, instance, "update_hb_mon:: heart_beat_to=%X%08X",
                      (uint32_t)(state->health.heart_beat_timeout >> 32),
                      (uint32_t)state->health.heart_beat_timeout);
    }
    else
    {
      state->health.heart_beat_timeout = state->health.sampling_interval * STENG1AX_HEART_BEAT_ODR_COUNT * wm;
    }
  }
  else
  {
    DBG_INST_PRINTF(MED, instance, "update_hb_mon:: removing timer");
    state->health.expected_expiration = UINT64_MAX;
    state->health.heart_beat_timeout = UINT64_MAX/2;
  }
  steng1ax_restart_hb_timer(instance, false);
}


sns_rc steng1ax_set_interrupts(sns_sensor_instance *const instance, bool enable, uint8_t hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  UNUSED_VAR(state);
  uint8_t rw_buffer = 0;
  uint8_t mask = 0x00;
  uint32_t xfer_bytes;
  sns_rc rv = SNS_RC_SUCCESS;

#if STENG1AX_DRDY_OUT_ENABLED
  mask = 0x08;
#else
  mask = STM_STENG1AX_FIFO_TH_MASK | STM_STENG1AX_FIFO_OVR_MASK;
#endif

  if (enable)
  {
#if STENG1AX_DRDY_OUT_ENABLED
    rw_buffer = 0x08;
#else
    // Configure steng1ax FIFO control registers
    rw_buffer = STM_STENG1AX_FIFO_TH_MASK | STM_STENG1AX_FIFO_OVR_MASK;

    if(state->fifo_info.fifo_enabled_intr == 0)
    {
      DBG_INST_PRINTF(HIGH, instance,
          "[%d] enable_fifo_intr:: fifo_enabled=0x%x irq_ready=%d cur_wmk=%d",
          hw_id, state->fifo_info.fifo_enabled, state->irq_ready, state->fifo_info.cur_wmk);
      if((state->fifo_info.cur_wmk > 0) &&
         (state->irq_ready)) {
        steng1ax_clear_interrupt_q(instance, state->interrupt_data_stream, hw_id);
        state->fifo_info.fifo_enabled_intr = STENG1AX_ENG;
      }
    }
#endif
  }

    rv = steng1ax_read_modify_write(instance,
        STM_STENG1AX_REG_CTRL2,
        &rw_buffer,
        1,
        &xfer_bytes,
        false,
        mask,
        hw_id);

  return rv;
}


sns_std_sensor_sample_status steng1ax_mark_sample_status(
    sns_sensor_instance *const instance,
    uint8_t min_num_samples,
    uint8_t sample_idx)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;

  sns_std_sensor_sample_status status = SNS_STD_SENSOR_SAMPLE_STATUS_ACCURACY_HIGH;

  // use-case where the max_num_sample == interrupt num_sample, we mark samples based on alignment
  if (min_num_samples != state->eng_info.eng_data_info[STENG1AX_INTR_HW_IDX].num_samples)
  {
    uint8_t diff = state->eng_info.eng_data_info[STENG1AX_INTR_HW_IDX].num_samples - min_num_samples;
    if ((state->eng_info.eng_data_info[STENG1AX_INTR_HW_IDX].num_samples - diff) <= sample_idx)
      status = SNS_STD_SENSOR_SAMPLE_STATUS_UNRELIABLE;
  }
  return status;
}

bool is_data_ready_to_process(sns_sensor_instance *const instance, uint8_t num_to_check)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  uint8_t not_ready_cnt = 0;

  if (num_to_check > state->multi_eng_cfg.num_sensors_enable || num_to_check < 0)
    num_to_check = 1;

  // check if only one sensor is to be processed
  for (int i =0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    if (!state->eng_info.eng_data_info[i].data_ready)
      not_ready_cnt++;
  }

  return (not_ready_cnt == num_to_check);
}


void steng1ax_handle_eng_sample(
    sns_sensor_instance *const instance,
    steng1ax_sensor_type sensor,
    sns_time ts,
    sns_time sample_interval_ticks,
    uint16_t sample_idx,
    sns_time use_time,
    log_sensor_state_raw_info* raw_log_ptr,
    uint8_t hw_id)
{
  UNUSED_VAR(hw_id);
  UNUSED_VAR(sensor);
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;


  sns_sensor_uid* suid;
  float sstvt = state->eng_info.sstvt;

  suid = &state->eng_info.suid;

  DBG_INST_PRINTF_EX(HIGH, instance, "[%d] sample_sets/num_samples %d/%d", hw_id, sample_idx, state->eng_info.eng_data_info[hw_id].num_samples);
  // check if sensor has read all data from FIFO
  if (state->eng_info.eng_data_info[hw_id].num_samples == sample_idx)
  {
    state->eng_info.eng_data_info[hw_id].data_ready = true;
  }

  // check if all sensor data has been read from FIFO
  bool data_ready_to_send = true;
  uint8_t min_num_sample = state->eng_info.eng_data_info[STENG1AX_INTR_HW_IDX].num_samples;

  for (int i = 0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    if (!state->eng_info.eng_data_info[i].data_ready)
    {
      DBG_INST_PRINTF_EX(HIGH, instance, "[%d] data not ready %d %d %d %d", 
        i,
        state->eng_info.eng_data_info[0].data_ready,
        state->eng_info.eng_data_info[1].data_ready,
        state->eng_info.eng_data_info[2].data_ready,
        state->eng_info.eng_data_info[3].data_ready);
      data_ready_to_send = false;
    }
    min_num_sample = SNS_MIN(min_num_sample, state->eng_info.eng_data_info[i].num_samples);
  }

  // send the data after all sensor's FIFO has been processed
  if (data_ready_to_send)
  {
    for (int i =0; i < state->eng_info.eng_data_info[STENG1AX_INTR_HW_IDX].num_samples; i++)
    {

      if(ts <= use_time)
      {
        state->fifo_info.last_timestamp = ts;
        //update for next sample
        ts = state->fifo_info.last_timestamp + sample_interval_ticks;
      

        float data[SENSOR_CNT] = {0.0f};
        float raw_data[SENSOR_CNT] = {0.0f};
#if STENG1AX_DEBUG_SENSOR_DATA
        DBG_INST_PRINTF_EX(HIGH, instance, "[%d] raw data %d %d %d %d",
          i,
          state->eng_info.eng_data_info[0].eng_raw_data[i],
          state->eng_info.eng_data_info[1].eng_raw_data[i],
          state->eng_info.eng_data_info[2].eng_raw_data[i],
          state->eng_info.eng_data_info[3].eng_raw_data[i]);
#endif
        sns_std_sensor_sample_status status = steng1ax_mark_sample_status(instance, min_num_sample, i);
        STENG1AX_INST_DEBUG_TS(LOW, instance, "[%d] [%d] s,ts,delay,status  %d, %u, %d", state->hw_idx, i, sensor, (uint32_t)ts, status);

        data[0] = state->eng_info.eng_data_info[0].eng_raw_data[i]/ sstvt;
        data[1] = state->eng_info.eng_data_info[1].eng_raw_data[i]/ sstvt;
        data[2] = state->eng_info.eng_data_info[2].eng_raw_data[i]/ sstvt;
        data[3] = state->eng_info.eng_data_info[3].eng_raw_data[i]/ sstvt;

        raw_data[0] = state->eng_info.eng_data_info[0].eng_raw_data[i];
        raw_data[1] = state->eng_info.eng_data_info[1].eng_raw_data[i];
        raw_data[2] = state->eng_info.eng_data_info[2].eng_raw_data[i];
        raw_data[3] = state->eng_info.eng_data_info[3].eng_raw_data[i];


        pb_send_sensor_stream_event(instance,
            suid,
            ts,
            SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_EVENT,
            status,
            data,
            SENSOR_CNT,
            state->encoded_eng_event_len);

        steng1ax_log_sample(instance, raw_log_ptr, suid,
            state->log_raw_encoded_size,
            raw_data, STENG1AX_NUM_DATA,
            ts, status,
            sample_idx ? SNS_DIAG_BATCH_SAMPLE_TYPE_INTERMEDIATE : SNS_DIAG_BATCH_SAMPLE_TYPE_FIRST);
      }
      else
      {
        sns_time cur_time = sns_get_system_time();
        UNUSED_VAR(cur_time);
        STENG1AX_INST_DEBUG_TS(HIGH, instance,
            "Drop: sensor=%d last_ts=%u ts=%u use_time=%u cur_tim=%u curr=%u",
            sensor, (uint32_t)state->fifo_info.last_timestamp, (uint32_t)ts,
            (uint32_t)use_time, (uint32_t)state->fifo_info.bh_info.cur_time, cur_time);
          if(state->fifo_info.last_timestamp > state->fifo_info.bh_info.cur_time)
          {
            state->fifo_info.last_timestamp = state->fifo_info.bh_info.cur_time;
          }
        //last_ts is used for PCE
        state->fifo_info.last_timestamp +=1;
      }
    }
    sns_memzero(&state->eng_info.eng_data_info, sizeof(state->eng_info.eng_data_info));
  }
}

int64_t steng1ax_timestamps_correction(sns_sensor_instance *instance,
  uint16_t sample_sets,
  sns_time irq_time,
  sns_time last_ts,
  sns_time sample_interval_ticks)
{
steng1ax_instance_state *state = (steng1ax_instance_state *)instance->state->state;
UNUSED_VAR(state);
// QC - Please remove, if first_ts is unused varibale
int64_t ts_drift = irq_time - (last_ts + sample_sets * sample_interval_ticks);
//if last timestamp is not valid do not try to correct
if(ts_drift == 0)
{
  return 0;
}
//take 2% of sample_interval_ticks for all samples ts_drift
//0.02 * sample_interval_ticks * sample_sets
sns_time ts_catchup_min = (0.02f * sample_interval_ticks * sample_sets);
int64_t ts_correction_drift;
int64_t ts_correction = ts_catchup_min;

if(ts_drift > 0) {
  ts_correction_drift = ts_drift - ts_catchup_min;
  //if > 0 use ts_catchup_min or use ts_drift as correction value
  if(ts_correction_drift < 0)
    ts_correction = ts_drift;
}
else {
  ts_correction = -ts_catchup_min;
  ts_correction_drift = ts_drift + ts_catchup_min;
  // if < 0 use ts_catchup_min or use ts_drift as correction value
  if(ts_correction_drift > 0)
    ts_correction = ts_drift;
}
//Floor function seems to have some issue in QCOM framework
// QC - please elaborate what the issue is with Floor function
// QC - Was the "issue" with the floor function fixed with "ts_correction = -ts_catchup_min;"?
 return (ts_correction < 0) ? ((ts_correction/sample_sets) - 1) : (ts_correction/sample_sets);
}

static void steng1ax_process_fifo_data_buffer(
  sns_sensor_instance *instance,
  sns_time            first_timestamp,
  sns_time            use_time,
  sns_time            sample_interval_ticks,
  const uint8_t       *fifo_start,
  size_t              num_bytes,
  uint16_t            total_sample_sets,
  uint8_t             hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;

  if(fifo_start == NULL)
  {
    SNS_INST_PRINTF(ERROR,instance,
        "ASCP buffer has NULL address!");
    state->num_ascp_null_events[hw_id]++;
    return;
  }

  //Temporary debug variable. To be removed
  state->fifo_start_address = (uint8_t *)fifo_start;
  if (is_data_ready_to_process(instance, 1))
  {
    STENG1AX_INST_DEBUG_TS(HIGH, instance,
        "[%d] self_test alive %d ascp_req_count %d first_ts %u sample_interval_ticks %u",
        hw_id, state->self_test_info.test_alive, state->ascp_req_count[hw_id],
        (uint32_t)first_timestamp, (uint32_t)sample_interval_ticks);

    //if orphan batch or interrupt fired
    if((state->fifo_info.interrupt_cnt >= MAX_INTERRUPT_CNT) &&
        (state->fifo_info.orphan_batch ||
         state->fifo_info.bh_info.interrupt_fired)) {

      int64_t sample_time_correction = steng1ax_timestamps_correction(
          instance, total_sample_sets,  use_time,
          state->fifo_info.last_timestamp, sample_interval_ticks);

      STENG1AX_INST_DEBUG_TS(HIGH, instance,
          "[%d] correction:  a/c= %u/%u correction = %d",
          hw_id,
          (uint32_t)sample_interval_ticks,
          (uint32_t)(sample_interval_ticks+sample_time_correction),
          (int32_t)sample_time_correction);

      sample_interval_ticks += sample_time_correction;
      first_timestamp += sample_time_correction;

    }
  }
  uint16_t sample_sets = 0;
  sns_time timestamp = 0;
  log_sensor_state_raw_info log_state_raw_info;

  sns_memset(&log_state_raw_info, 0, sizeof(log_sensor_state_raw_info));

 //reference timestamp
  timestamp = first_timestamp;
  steng1ax_sensor_type sensor = STENG1AX_ENG;

  for(uint32_t i = 0; i + STM_STENG1AX_FIFO_SAMPLE_SIZE - 1 < num_bytes; i += STM_STENG1AX_FIFO_SAMPLE_SIZE)
  {
        state->eng_info.eng_data_info[hw_id].eng_raw_data[sample_sets] = (fifo_start[i+2] << 8) | fifo_start[i+1];
        sample_sets++;
  }

  steng1ax_handle_eng_sample(
              instance, sensor,
              timestamp,
              sample_interval_ticks,
              sample_sets,
              use_time,
              &log_state_raw_info,
              hw_id);

  //makesure to submit the packets at the end
  steng1ax_log_sample(instance, &log_state_raw_info, NULL, 0, NULL, 0, 0, 0, SNS_DIAG_BATCH_SAMPLE_TYPE_LAST);
  if (is_data_ready_to_process(instance, state->multi_eng_cfg.num_sensors_enable))
  {
    STENG1AX_INST_DEBUG_TS(HIGH, instance,
        "[%d] [use_time_drift]last_ts=%u use_time=%u drift=%d",
        hw_id, (uint32_t)state->fifo_info.last_timestamp, (uint32_t)use_time,
        (int32_t)(use_time - state->fifo_info.last_timestamp));

    STENG1AX_INST_DEBUG_TS(HIGH, instance,
        "[%d] [cur_time_drift]last_ts=%u cur_time=%u drift=%d",
        hw_id, (uint32_t)state->fifo_info.last_timestamp, (uint32_t)sns_get_system_time(),
        (int32_t)(sns_get_system_time() - state->fifo_info.last_timestamp));
  }
}

void steng1ax_send_sensor_sample(
  sns_sensor_instance *const    instance,
  sns_time                      timestamp,
  int16_t                       *sensor_data)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;

  log_sensor_state_raw_info log_sensor_state_raw_info;
  sns_memzero(&log_sensor_state_raw_info, sizeof(log_sensor_state_raw_info));
  sns_sensor_uid* suid = NULL;
  sns_std_sensor_sample_status status = SNS_STD_SENSOR_SAMPLE_STATUS_ACCURACY_HIGH;

  float sstvt = state->eng_info.sstvt;
  suid = &state->eng_info.suid;

  float data[SENSOR_CNT] = {0.0f};

  int i = 0;
  for (i=0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    data[i] = sensor_data[i] / sstvt;
  }
  state->eng_sample_counter++;
  STENG1AX_INST_DEBUG_TS(HIGH, instance, "eng data*1000: %d %d %d %d %u",
    (int16_t)(data[0]*1000), (int16_t)(data[1]*1000), (int16_t)(data[2]*1000), (int16_t)(data[3]*1000), (uint32_t)timestamp);

  state->eng_info.last_ts = timestamp;

  pb_send_sensor_stream_event(instance,
      suid,
      timestamp,
      SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_EVENT,
      status,
      data,
      SENSOR_CNT,
      state->encoded_eng_event_len);

  steng1ax_log_sample(instance, &log_sensor_state_raw_info, suid,
      state->log_raw_encoded_size,
      data, SENSOR_CNT,
      timestamp, status,
      SNS_DIAG_BATCH_SAMPLE_TYPE_ONLY);
}


/**
 * Gets current Eng ODR.
 *
 * @param[i] curr_odr              Current FIFO ODR.
 *
 */
float steng1ax_get_eng_odr(steng1ax_eng_odr curr_odr, bool lpf0_set)
{
  float odr = 0.0;
  int8_t idx;

  if (curr_odr != STENG1AX_ENG_ODR_OFF)
  {
    for(idx = 1; idx < steng1ax_odr_map_len; idx++)
    {
      if (lpf0_set == steng1ax_odr_map[idx].exp_lpf0_en)
      {
        odr = steng1ax_odr_map[idx].odr;
      }
    }
  }

  return odr;
}

void steng1ax_start_fifo_streaming(sns_sensor_instance *const instance, uint8_t hw_id)
{
  // Enable FIFO Streaming
  // Enable Eng Streaming

  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  bool init_nominal = false;
  int32_t odr_percent_var;


  DBG_INST_PRINTF(HIGH, instance, "[%d] start_fifo_streaming: rate=0x%x last_ts=%u",
                  hw_id,
                  state->fifo_info.desired_fifo_rate,
                  (uint32_t)state->fifo_info.last_timestamp);

  DBG_INST_PRINTF(
    LOW, instance, "[%d] cur=%u des=%u fifo=%x reg_val=%x lpf0=%d",
    hw_id, (int)state->eng_info.curr_odr, (int)state->eng_info.desired_odr,
    state->fifo_info.fifo_rate, state->eng_info.curr_odr_reg_val, state->eng_info.lpf0_en_set);

  if((state->eng_info.curr_odr != state->eng_info.desired_odr ||
    state->desired_sensors != state->enabled_sensors ||
     state->fifo_info.fifo_rate == STENG1AX_ENG_ODR_OFF)) {
    //reset only if odr is changed
    //if odr is same as before, no need to recalculate sampling interval
    state->fifo_info.interrupt_cnt = 0;
    init_nominal = true;

    if((state->fifo_info.fifo_rate == STENG1AX_ENG_ODR_OFF)
        && (state->eng_info.curr_odr_reg_val != STENG1AX_ENG_ODR_OFF)) {
      DBG_INST_PRINTF(MED, instance, "[%d] start_fifo_streaming: turning off eng", hw_id);
      //eng is on but fifo is off, turn off eng
      steng1ax_set_odr_config(instance,
          STENG1AX_ENG_ODR_OFF,
          state->eng_info.sstvt,
          STENG1AX_ODR_BW_HALF,
          hw_id);
    }
  }

  //sns_time cur_odr_set_time = state->cur_odr_change_info.eng_odr_settime;
  //float curr_odr = state->current_conf.odr;
  steng1ax_set_odr_config(instance,
      state->eng_info.desired_odr_reg_val,
      state->eng_info.sstvt,
      state->eng_info.bw,
      hw_id);

  if((state->fifo_info.fifo_rate == STENG1AX_ENG_ODR_OFF) || (!state->fifo_info.is_streaming)){
    sns_time now = sns_get_system_time();
    DBG_INST_PRINTF(LOW, instance, "start_fifo_streaming: resetting last_timestamp to now %u", (uint32_t)now);
    state->fifo_info.last_timestamp = now;
    state->fifo_info.last_ts_valid = false; //set last ts as inaccurate
    //reset prev odr configuration as streaming is starting now
    // sns_memset(&state->prev_odr_change_info, 0, sizeof(steng1ax_odr_change_info));
  }

  if(init_nominal)
  {
    //reset only if odr is changed
    //if odr is same as before, no need to recalculate sampling interval
    state->fifo_info.interrupt_cnt = 0;

    //reset only if odr is changed
    //if odr is same as before with same sensor, no need to recalculate sampling interval
    sns_time nominal_sampling_intvl = (uint32_t)steng1ax_get_sample_interval(instance, steng1ax_odr_map[state->eng_info.desired_odr_idx].eng_odr_reg_value);

    odr_percent_var = state->odr_percent_var_eng;

    state->fifo_info.avg_sampling_intvl = nominal_sampling_intvl - (odr_percent_var / state->eng_info.desired_odr);
    steng1ax_interrupt_interval_init_nominal(instance);

  }
  //start streaming,stream mode
  steng1ax_set_fifo_stream_mode(instance, hw_id);

  state->fifo_info.is_streaming =
    ((state->desired_sensors & STENG1AX_ENG) &&
     state->eng_info.desired_max_requested_flush_ticks);

  state->config_sensors &= ~STENG1AX_ENG;
  state->fifo_info.fifo_rate = state->fifo_info.desired_fifo_rate;
  state->fifo_info.lpf0_en_set = state->eng_info.lpf0_en_set;


  DBG_INST_PRINTF(MED, instance,
                  "start_fifo_streaming EX: a_odr=0x%x lpf0=%d",
                  state->eng_info.curr_odr_reg_val, state->eng_info.lpf0_en_set);

}

void steng1ax_disable_fifo_intr(sns_sensor_instance *const instance, uint8_t hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  uint32_t xfer_bytes;
  uint8_t rw_buffer = 0;

  DBG_INST_PRINTF_EX(HIGH, instance, "[%d] disable_fifo_intr", hw_id);
  steng1ax_read_modify_write(instance,
                            STM_STENG1AX_REG_CTRL2,
                            &rw_buffer,
                            1,
                            &xfer_bytes,
                            false,
                            STM_STENG1AX_FIFO_TH_MASK | STM_STENG1AX_FIFO_OVR_MASK,
                            hw_id);
  state->fifo_info.fifo_enabled_intr = 0;
}

//return no of bytes in fifo
sns_rc steng1ax_get_fifo_status(
    sns_sensor_instance *const instance,
    uint16_t* bytes_in_fifo,
    uint8_t* status_reg,
    uint8_t hw_id)
{
  sns_rc rc = SNS_RC_SUCCESS;
#if !STENG1AX_DAE_ENABLED
  //read fifo regs
  rc = steng1ax_read_regs_scp(instance, STM_STENG1AX_REG_FIFO_STATUS1, 2, status_reg, hw_id);

  if(rc != SNS_RC_SUCCESS)
  {
    SNS_INST_PRINTF(ERROR, instance,
        "read_fifo_status fail %d", rc);
    return rc;
  }
  // Calculate the number of bytes to be read
  *bytes_in_fifo =  status_reg[1] * (STM_STENG1AX_FIFO_SAMPLE_SIZE);
  if((!*bytes_in_fifo) && (status_reg[0] & 0x40)) {
    *bytes_in_fifo = 128*7;
  }
  STENG1AX_INST_DEBUG_TS(LOW, instance,
    "[%d] status_reg 0x%x 0x%x",hw_id, status_reg[0], status_reg[1]);
  STENG1AX_INST_DEBUG_TS(LOW, instance,
    "[%d] count 0x%x num_of_bytes %d", hw_id, status_reg[1], *bytes_in_fifo);
#else
  UNUSED_VAR(instance);
  UNUSED_VAR(bytes_in_fifo);
  UNUSED_VAR(status_reg);
  rc = SNS_RC_NOT_SUPPORTED;
#endif
  return rc;
}

/**
 * see sns_steng1ax_hal.h
 */
void steng1ax_flush_fifo(sns_sensor_instance *const this, uint8_t hw_id)
{
  UNUSED_VAR(hw_id);
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  if(state->fifo_info.interrupt_cnt < MAX_INTERRUPT_CNT) {
    STENG1AX_INST_DEBUG_TS(HIGH, this, "resetting int_cnt");
    state->fifo_info.interrupt_cnt = 0;
  }
  state->fifo_info.th_info.flush_req = true;
  if (!steng1ax_dae_if_flush_hw(this))
  {
    DBG_INST_PRINTF_EX(HIGH, this, "[%d] flushing fifo", hw_id);
    steng1ax_read_fifo_data(this, sns_get_system_time(), true, hw_id);
  }
}

void steng1ax_handle_sensor_sample(sns_sensor_instance *const instance, sns_time timestamp)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  uint8_t buffer = 0;
  int16_t raw_data[SENSOR_CNT] = {0};
  int i =0;

  if (state->eng_stream_mode == DRI)
  {
    steng1ax_read_regs_scp(instance, STM_STENG1AX_REG_STATUS, 1, &buffer, STENG1AX_INTR_HW_IDX);
  }
  else
  {
    buffer = STENG1AX_ENG;
  }
  if (buffer & STENG1AX_ENG)
  {
    for (i =0; i < state->multi_eng_cfg.num_sensors_enable; i++)
    {
      steng1ax_get_data(instance, STENG1AX_ENG, 1, &raw_data[i], i);
    }

    sns_time last_ts = state->eng_info.last_ts;
    sns_time sampling_interval_ticks = state->eng_info.sampling_intvl;
    sns_time ts = 0;


    int64_t sample_time_correction = steng1ax_timestamps_correction(
      instance, 1, timestamp, last_ts, sampling_interval_ticks);
    STENG1AX_INST_DEBUG_TS(HIGH, instance,
      "[%d] correction:  a/c= %u/%u correction = %d",
      state->hw_idx,
      (uint32_t)sampling_interval_ticks,
      (uint32_t)(sampling_interval_ticks+sample_time_correction),
      (int32_t)sample_time_correction);

    sns_time ref_ts = state->eng_info.last_ts += sample_time_correction;

    ts = ref_ts + sampling_interval_ticks;

    STENG1AX_INST_DEBUG_TS(HIGH, instance, "use/last_ts= %u/%u", (uint32_t)timestamp, (uint32_t)ts);

    steng1ax_send_sensor_sample(instance, ts, &raw_data[0]);
  }
}

// is_data_read --> atleast one sample is read from fifo
void steng1ax_read_fifo_data_cleanup(sns_sensor_instance *const instance, bool is_fifo_read, uint8_t hw_id)
{
#if !STENG1AX_DAE_ENABLED
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;

  if(state->ascp_req_count[hw_id] <= 0) {

    if(state->flushing_sensors != 0) {
      steng1ax_send_fifo_flush_done(instance, state->flushing_sensors, FLUSH_DONE_AFTER_DATA);
      state->flushing_sensors = 0;
      if(is_fifo_read) {
        steng1ax_restart_hb_timer(instance, true);
      }
    }
  }
  if (state->multi_eng_cfg.num_sensors_enable-1 == hw_id)
  {
    //reset the top half params
    state->fifo_info.th_info.interrupt_fired = false;
    state->fifo_info.th_info.recheck_int = false;
    state->fifo_info.th_info.flush_req = false;
  }
   //if async read not in progress
  if(state->ascp_req_count[hw_id] <= 0) {
    //if cur wmk = 1, we try to reconfig at interrupt context, so ignoe if wmk=1
    if(state->fifo_info.reconfig_req && state->eng_info.config_stage == CONFIG_IDLE) {
      DBG_INST_PRINTF(
          LOW, instance, "read_fifo_data: reconfig_req wmk(old/new)=%d/%d",
          state->eng_info.curr_wmk, state->eng_info.desired_wmk);
      //if only wmk is changing, do not follow normal sequence
      if((state->eng_info.curr_odr == state->eng_info.desired_odr) &&
          (state->enabled_sensors == state->desired_sensors) &&
          (state->eng_info.curr_wmk != state->eng_info.desired_wmk) &&
          !state->fifo_info.full_reconf_req) {
        steng1ax_set_fifo_wmk(instance, hw_id);
        //reset avg interrupt interval
        state->fifo_info.avg_interrupt_intvl =
          state->fifo_info.avg_sampling_intvl * state->eng_info.curr_wmk;
        //enable interrupt
        steng1ax_set_interrupts(instance, true, hw_id);
        if (state->multi_eng_cfg.num_sensors_enable-1 == hw_id)
          state->fifo_info.reconfig_req = false;
      }
      else if((state->eng_info.curr_wmk != 1) || (state->fifo_info.bh_info.interrupt_fired)) {
        steng1ax_reconfig_fifo(instance, false, hw_id);
        if (hw_id == STENG1AX_INTR_HW_IDX)
          steng1ax_set_interrupts(instance, true, hw_id);
        steng1ax_reconfig_esp(instance);
      }
    }
  }
#else
  UNUSED_VAR(instance);
  UNUSED_VAR(is_fifo_read);
#endif
}

void steng1ax_fill_bh_info(sns_sensor_instance *const instance)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  sns_memscpy(&state->fifo_info.bh_info,
      sizeof(steng1ax_fifo_req),
      &state->fifo_info.th_info,
      sizeof(steng1ax_fifo_req));
  //reset th parameters
  //uint8_t buffer;

  state->fifo_info.th_info.interrupt_fired = false;
  state->fifo_info.th_info.flush_req = false;
  state->fifo_info.th_info.recheck_int = false;
  STENG1AX_INST_DEBUG_TS(LOW, instance,
      "bh_info int_fired=%d flush_req=%d s_intvl=%u",
      state->fifo_info.bh_info.interrupt_fired, state->fifo_info.bh_info.flush_req,
      (uint32_t)(state->fifo_info.avg_sampling_intvl));
  STENG1AX_INST_DEBUG_TS(LOW, instance,
      "irq_ts=%u cur_time=%u ",
      (uint32_t)state->fifo_info.bh_info.interrupt_ts,
      (uint32_t)state->fifo_info.bh_info.cur_time);
}

void steng1ax_send_fw_data_read_msg(
    sns_sensor_instance *const instance,
    uint16_t num_of_bytes,
    uint8_t hw_id)
{
#if !STENG1AX_DAE_ENABLED
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  sns_rc rc = SNS_RC_SUCCESS;
  uint8_t buffer[100];
  uint32_t enc_len;
  uint8_t sample_size = STM_STENG1AX_FIFO_SAMPLE_SIZE;
  sns_port_vector async_read_msg;
  // Compose the async com port message
  async_read_msg.bytes = num_of_bytes;
  async_read_msg.reg_addr = STM_STENG1AX_REG_FIFO_OUT_TAG;
  async_read_msg.is_write = false;
  async_read_msg.buffer = NULL;

  //if samples > ASYNC_MIN_SAMPLES use async com
  //else use sync com

  // QC: One set is 2 samples when ... is active, so we're OK using Sync com port for 60 bytes?
  if((num_of_bytes/sample_size) <= ASYNC_MIN_SAMPLES) {
    uint8_t fifo_data[async_read_msg.bytes];
    // QC: is memset necessary?
    sns_memset(fifo_data, 0, sizeof(fifo_data));
    rc = steng1ax_read_regs_scp(instance, async_read_msg.reg_addr, async_read_msg.bytes, fifo_data, hw_id);
    if(rc == SNS_RC_SUCCESS) {
      sns_port_vector vector;
      vector.reg_addr = async_read_msg.reg_addr;
      vector.bytes = async_read_msg.bytes;
      vector.buffer = fifo_data;
      state->ascp_hw_id = hw_id;
      steng1ax_process_com_port_vector(&vector, instance);
    }
  }
  else if(sns_ascp_create_encoded_vectors_buffer(&async_read_msg, 1, true, buffer,
                                                 sizeof(buffer), &enc_len)) {
    // Send message to Async Com Port
    sns_request async_com_port_request =
      (sns_request)
      {
        .message_id = SNS_ASYNC_COM_PORT_MSGID_SNS_ASYNC_COM_PORT_VECTOR_RW,
        .request_len = enc_len,
        .request = buffer
      };
    rc = state->async_com_port_data_stream[hw_id]->api->send_request(
        state->async_com_port_data_stream[hw_id], &async_com_port_request);
    if(rc != SNS_RC_SUCCESS) {
      SNS_INST_PRINTF(ERROR, instance, "async com port send request failed status %d",rc);
    }
    STENG1AX_INST_DEBUG_TS(HIGH, instance,
        "[%d] send ascp req ascp_req_count %d", hw_id, state->ascp_req_count[hw_id]);
    state->ascp_req_count[hw_id]++;
  } else {
    SNS_INST_PRINTF(ERROR, instance, "sns_ascp_create_encoded_vectors_buffer failed");
  }
#else
    UNUSED_VAR(instance);
    UNUSED_VAR(num_of_bytes);
#endif
}



/** read fifo data after checking fifo int and use sync com port or async com port */
void steng1ax_read_fifo_data(sns_sensor_instance *const instance, sns_time irq_timestamp, bool flush, uint8_t hw_id)
{
#if !STENG1AX_DAE_ENABLED
  uint8_t fifo_status[2] = {0, 0};
  sns_rc rc = SNS_RC_SUCCESS;
  uint16_t num_of_bytes = 0 , num_sets;
  UNUSED_VAR(num_sets);
  steng1ax_instance_state *state =
    (steng1ax_instance_state*)instance->state->state;
  UNUSED_VAR(irq_timestamp);
  uint8_t sample_size = STM_STENG1AX_FIFO_SAMPLE_SIZE;
  if(flush)
    state->fifo_info.th_info.flush_req = true;

  if (STENG1AX_INTR_HW_IDX == hw_id)
  {
    // Read the FIFO Status register
    rc = steng1ax_get_fifo_status(instance, &num_of_bytes, fifo_status, hw_id);
  }

  if(rc != SNS_RC_SUCCESS)
  {
    SNS_INST_PRINTF(ERROR, instance, "steng1ax_read_fifo_status FAILED");
    return;
  }
  if (fifo_status[1] > 0 && !state->ascp_req_count[hw_id])
    state->eng_info.eng_data_info[hw_id].num_samples = fifo_status[1];

  if (STENG1AX_INTR_HW_IDX == hw_id)
    state->eng_info.eng_data_info[hw_id].num_of_bytes = num_of_bytes;

#if !STENG1AX_EXT_CLK_ENABLE
  if (num_of_bytes < state->eng_info.eng_data_info[STENG1AX_INTR_HW_IDX].num_of_bytes)
#endif
  {
    num_of_bytes = state->eng_info.eng_data_info[STENG1AX_INTR_HW_IDX].num_of_bytes;
    state->eng_info.eng_data_info[hw_id].num_samples = state->eng_info.eng_data_info[STENG1AX_INTR_HW_IDX].num_samples;
  }

  if(flush)
  {
    num_sets = num_of_bytes/sample_size;
  }

  if(num_of_bytes < sample_size) {
    STENG1AX_INST_DEBUG_TS(LOW, instance,
        "#bytes %u < one pattern %u", num_of_bytes, sample_size);
    steng1ax_read_fifo_data_cleanup(instance, false, hw_id);
  } else {

    //Debug print
    if(state->fifo_info.reconfig_req) {
      DBG_INST_PRINTF(MED, instance,
          "recheck %d int_fired %d flush_req %d reconfig_req %d",
          state->fifo_info.th_info.recheck_int,state->fifo_info.th_info.interrupt_fired,
          state->fifo_info.th_info.flush_req, state->fifo_info.reconfig_req);
    }

    //**special case**:
    //if flush request received just before interrupt
    //and data reading from async com port
    //case 1: data not read yet, async req pending at fw
    //case 2: data read, and notification to driver pending at fw
    if(state->fifo_info.th_info.interrupt_fired && state->ascp_req_count[hw_id]) {

      //making sure this interrupt belong to the request in-progress
      if(state->fifo_info.bh_info.interrupt_ts > state->fifo_info.th_info.interrupt_ts) {
        STENG1AX_INST_DEBUG_TS(HIGH, instance,
            "current int is skipping as the same is in-progress: bh_ts=%u cur_int_ts=%u",
            (uint32_t)state->fifo_info.bh_info.interrupt_ts,
            (uint32_t)state->fifo_info.th_info.interrupt_ts);

        if (hw_id == STENG1AX_INTR_HW_IDX)
        {
          state->fifo_info.th_info.interrupt_fired = false;

          //update bottom half info
          state->fifo_info.bh_info.interrupt_fired = true;
          state->fifo_info.bh_info.interrupt_ts = state->fifo_info.th_info.interrupt_ts;
        }
      }
    }

    //At Top half
    //interrupt_fired = true --> use_time =  irq_ts
    //else
    //recheck_int or flush = true --> use_time = cur_time  last_ts + sample_sets * sample_interval

    //At Bottom half
    //update use_time based on new information
    //interrupt_fired = true --> use_time =  irq_ts
    //else
    //recheck_int or flush = true --> use_time = last_ts + sample_sets * sample_interval

    if((!state->fifo_info.th_info.recheck_int) && (!state->fifo_info.th_info.interrupt_fired) &&
        (!state->fifo_info.th_info.flush_req)) {
      //return nothing to be done here
      //useful for active_high/active_low interrupt handling
      return;
    }

    if(state->ascp_req_count[hw_id]) {
      DBG_INST_PRINTF_EX(HIGH, instance,
          "ascp req is pending .. returning without reading data req count %d",
          state->ascp_req_count[hw_id]);
      return;
    }
    if (hw_id == STENG1AX_INTR_HW_IDX)
    {
      state->fifo_info.th_info.eng_odr = state->eng_info.curr_odr_reg_val;
      state->fifo_info.th_info.lpf0_en_set = state->eng_info.lpf0_en_set;
      state->fifo_info.th_info.wmk = state->fifo_info.cur_wmk;
      state->fifo_info.th_info.cur_time = sns_get_system_time();
      if(!state->fifo_info.th_info.interrupt_fired)
        state->fifo_info.th_info.interrupt_ts = state->fifo_info.th_info.cur_time;
    }

    if (hw_id == (state->multi_eng_cfg.num_sensors_enable-1))
      steng1ax_fill_bh_info(instance);

  STENG1AX_INST_AUTO_DEBUG_PRINTF(HIGH, instance, "bh_info int_fired = %d flush_req = %d",
                  state->fifo_info.bh_info.interrupt_fired, state->fifo_info.bh_info.flush_req);

    steng1ax_send_fw_data_read_msg(instance, num_of_bytes, hw_id);
    steng1ax_read_fifo_data_cleanup(instance, true, hw_id);
  }
#else
  UNUSED_VAR(instance);
  UNUSED_VAR(irq_timestamp);
  UNUSED_VAR(flush);
#endif
}

/**
 * Extract a eng sample from a segment of the fifo buffer and generate an
 * event.
 *
 * @param[i] instance           The current steng1ax sensor instance
 * @param[i] sensors[]          Array of sensors for which data is requested
 * @param[i] num_sensors        Number of sensor for which data is requested
 * @param[i] raw_data           Uncalibrated sensor data to be logged
 */
void steng1ax_get_data(sns_sensor_instance *const instance,
                                steng1ax_sensor_type sensors,
                                uint8_t num_sensors,
                                int16_t *raw_data,
                                uint8_t hw_id)
{
  UNUSED_VAR(sensors);
// Timestap is not needed for this implementation as we are not sending anything ot framework
  uint8_t read_addr;

  read_addr = STM_STENG1AX_REG_OUT_AH_ENG_L;

  steng1ax_read_regs_scp(instance, read_addr, 2*num_sensors, (uint8_t*)raw_data, hw_id);
#if STENG1AX_DEBUG_SENSOR_DATA
  DBG_INST_PRINTF(LOW, instance, "DATA sensor=%u ts=%u [%d] [%x %x]",
                  sensors, (uint32_t)sns_get_system_time(), raw_data[0], ((raw_data[0] & 0xFF00)>>8), (raw_data[0] & 0xFF));
#endif
}

/**
 * see sns_steng1ax_hal.h
 */
void steng1ax_dump_reg(sns_sensor_instance *const instance,
                      steng1ax_sensor_type sensor,
                      uint8_t hw_id)
{
  UNUSED_VAR(sensor);
  UNUSED_VAR(hw_id);
#if STENG1AX_DUMP_REG
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  struct group_read {
    uint32_t first_reg;
    uint8_t  num_regs;
  } groups[] = { /* must fit within state->reg_status[] */
    {STM_STENG1AX_REG_EXT_CLK_CFG, 1},
    { STM_STENG1AX_REG_PIN_CTRL, 1 },
    { STM_STENG1AX_REG_WHO_AM_I, 9 },
    { STM_STENG1AX_REG_MD1_CFG, 1 },
    { STM_STENG1AX_REG_STATUS, 3},
    { STM_STENG1AX_REG_AH_ENG_CFG1, 4},
    { STM_STENG1AX_REG_EN_DEVICE_CONFIG, 1},
    { STM_STENG1AX_REG_FIFO_BATCH_DEC, 1 }
  };
  uint8_t *reg_val = state->reg_status;

  for(uint32_t i=0; i<ARR_SIZE(groups); i++)
  {
    steng1ax_read_regs_scp(instance, groups[i].first_reg, groups[i].num_regs, reg_val, hw_id);
    for(uint32_t j=0; j<groups[i].num_regs; j++)
    {
      DBG_INST_PRINTF(LOW, instance, "[%d] dump: 0x%02x=0x%02x",
                      hw_id, groups[i].first_reg+j, reg_val[j]);
    }
    reg_val += groups[i].num_regs;
  }
#else
  UNUSED_VAR(instance);
#endif
}

/** See sns_steng1ax_hal.h */
void steng1ax_send_config_event(sns_sensor_instance *const instance, bool new_client)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  char operating_mode_normal[] = {STENG1AX_NORMAL};
  char operating_mode_off[] = {STENG1AX_OFF};
  uint32_t active_current = STENG1AX_ENG_ACTIVE_CURRENT;
  sns_time event_ts = sns_get_system_time();

  sns_std_sensor_physical_config_event phy_sensor_config =
     sns_std_sensor_physical_config_event_init_default;

  pb_buffer_arg op_mode_args;

  if(!new_client && !state->eng_info.config_event)
  {
    // DBG_INST_PRINTF(MED, instance, "[%u] config_event: return with all zero!", state->hw_idx);
    return; // no clients present
  }

  DBG_INST_PRINTF(MED, instance, "[%u] config_event: desired_odr_idx: %u",
    state->hw_idx, state->eng_info.desired_odr_idx);

  if(!new_client)
  {
    state->fifo_info.last_sent_config = state->fifo_info.new_config;
  }

  //Lower Power mode
  op_mode_args.buf = &operating_mode_normal[0];
  op_mode_args.buf_len = sizeof(operating_mode_normal);
  active_current = STENG1AX_ENG_ACTIVE_CURRENT;


  phy_sensor_config.has_sample_rate                = true;
  phy_sensor_config.has_water_mark                 = true;
#if STENG1AX_DRDY_OUT_ENABLED
  phy_sensor_config.water_mark                     = 1;
#else
  phy_sensor_config.water_mark                     = (state->fifo_info.last_sent_config.fifo_watermark > 0) ? state->fifo_info.last_sent_config.fifo_watermark : state->eng_info.desired_wmk;
#endif
  phy_sensor_config.has_active_current             = true;
  phy_sensor_config.active_current                 = active_current;
  phy_sensor_config.has_resolution                 = true;
  phy_sensor_config.resolution                     = (float)(1/state->eng_info.sstvt);
  phy_sensor_config.range_count                    = 2;

  phy_sensor_config.operation_mode.funcs.encode    = &pb_encode_string_cb;
  phy_sensor_config.operation_mode.arg             = &op_mode_args;

  // Convert to mV
  phy_sensor_config.range[0] = STENG1AX_ENG_RANGE_MIN / STENG1AX_ENG_RESOLUTION;
  phy_sensor_config.range[1] = STENG1AX_ENG_RANGE_MAX / STENG1AX_ENG_RESOLUTION;
  phy_sensor_config.has_stream_is_synchronous = false;
  phy_sensor_config.stream_is_synchronous = false;

  phy_sensor_config.has_DAE_watermark = steng1ax_dae_if_available(instance);
  phy_sensor_config.has_dri_enabled = (state->eng_stream_mode == DRI);
  phy_sensor_config.dri_enabled = (state->eng_stream_mode == DRI);

  DBG_INST_PRINTF(MED, instance, "[%u] nc=%d enabled_s=0x%x desired_s=0x%x event=%d curr_odr=%d wmk=%u/%u",
    state->hw_idx, new_client, state->enabled_sensors, state->desired_sensors,
    state->eng_info.config_event, (int)state->eng_info.curr_odr,
    phy_sensor_config.water_mark, phy_sensor_config.DAE_watermark);

  if(state->desired_sensors & STENG1AX_ENG &&
    ((state->eng_info.config_event) ||
      (new_client && (state->eng_info.stage == CONFIG_REQUEST) && state->eng_info.curr_odr)))
  {

    phy_sensor_config.sample_rate = state->eng_info.curr_odr;

    if(!phy_sensor_config.sample_rate)
    {
      //OFF mode
      op_mode_args.buf = &operating_mode_off[0];
      op_mode_args.buf_len = sizeof(operating_mode_off);
      active_current = STENG1AX_ENG_SLEEP_CURRENT;
    }

#if STENG1AX_DRDY_OUT_ENABLED
    if(new_client && state->eng_info.stage == CONFIG_REQUEST)
    {
      event_ts = state->eng_info.last_sent_config_ts;
    }
    else
    {
      state->eng_info.last_sent_config_ts = event_ts;
    }
#else
    event_ts = state->fifo_info.last_sent_config.timestamp;
#endif


    DBG_INST_PRINTF(
      MED, instance, "[%u] config_event:: new=%u sensor=1 SR=%d ts=%u",
      state->hw_idx, new_client, (int)phy_sensor_config.sample_rate, (uint32_t)event_ts);

    pb_send_event(instance,
                  sns_std_sensor_physical_config_event_fields,
                  &phy_sensor_config,
                  event_ts,
                  SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT,
                  &state->eng_info.suid);

      state->eng_info.stage = CONFIG_EVENT;
      state->eng_info.config_event = false;
  }
}

void steng1ax_turn_off_fifo(sns_sensor_instance *this, uint8_t hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  DBG_INST_PRINTF(HIGH, this, "[%d] turn_off_fifo: rate=0x%x", hw_id, state->fifo_info.fifo_rate);
  if(state->fifo_info.fifo_rate > STENG1AX_ENG_ODR_OFF) {
    steng1ax_set_fifo_config(this,
        0,
        STENG1AX_ENG_ODR_OFF,
        state->fifo_info.fifo_enabled);
    steng1ax_stop_fifo_streaming(this, hw_id);
    steng1ax_set_fifo_wmk(this, hw_id);
    if (STENG1AX_INTR_HW_IDX == hw_id)
    {
      steng1ax_disable_fifo_intr(this, hw_id);
      // Disable timer
      state->health.expected_expiration = UINT64_MAX;
      state->health.heart_beat_timeout = UINT64_MAX/2;
      steng1ax_restart_hb_timer(this, true);
      state->health.heart_attack = false;
      state->fifo_info.last_sent_config.sample_rate = STENG1AX_ODR_0;
      state->fifo_info.last_sent_config.fifo_watermark = 0;
      state->fifo_info.last_sent_config.timestamp = sns_get_system_time();
    }
   }
}

void steng1ax_start_sensor_polling_timer(
  sns_sensor_instance *const instance)
{
steng1ax_instance_state *state =
   (steng1ax_instance_state*)instance->state->state;
sns_timer_sensor_config req_payload = sns_timer_sensor_config_init_default;
req_payload.is_periodic = true;
req_payload.start_time = sns_get_system_time();
#if STENG1AX_SUPPORT_NEW_DISABLE_RESPONSE
req_payload.has_disable_response = true;
req_payload.disable_response = true;
#endif
//timeout in sec, convert to ticks
req_payload.timeout_period = sns_convert_ns_to_ticks(1000000000.0f / state->eng_info.curr_odr);

DBG_INST_PRINTF_EX(LOW, instance, "create polling timer = odr=%u to=%u", 
                  (uint32_t)state->eng_info.curr_odr,
                  (uint32_t)req_payload.timeout_period);

steng1ax_inst_create_timer(instance, &state->timer_sensor_polling_data_stream, &req_payload);
}

void steng1ax_powerdown_hw(sns_sensor_instance *this, uint8_t hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  UNUSED_VAR(hw_id);
  DBG_INST_PRINTF(HIGH, this, "powerdown_hw: %u %u", state->enabled_sensors, state->desired_sensors);
  uint8_t buffer = 0;
  steng1ax_write_regs_scp(this, STM_STENG1AX_REG_CTRL3, 1, &buffer, hw_id);
  state->eng_info.lpf0_en_set = false;
  steng1ax_set_odr_config(this,
    STENG1AX_ENG_ODR_OFF,
    state->eng_info.sstvt,
    state->eng_info.bw,
    hw_id);
  steng1ax_turn_off_fifo(this, hw_id);
  steng1ax_reconfig_esp(this);

  if ((state->multi_eng_cfg.num_sensors_enable-1) == hw_id)
  {
    state->enabled_sensors = 0;
  }
}

void steng1ax_set_fifo_bypass_mode(sns_sensor_instance *this, uint8_t hw_id)
{
  uint8_t rw_buffer = 0x0;
  uint32_t xfer_bytes;
  DBG_INST_PRINTF_EX(HIGH, this, "[%d] set_fifo_bypass", hw_id);
  steng1ax_read_modify_write(this,
      STM_STENG1AX_REG_FIFO_CTRL,
      &rw_buffer,
      1,
      &xfer_bytes,
      false,
      STM_STENG1AX_FIFO_EN_ADV_MASK | STM_STENG1AX_FIFO_MODE_MASK,
      hw_id);
}

void steng1ax_set_fifo_stream_mode(sns_sensor_instance *this, uint8_t hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  uint8_t rw_buffer = STENG1AX_FIFO_STREAM_MODE;
  uint32_t xfer_bytes;

  if (STENG1AX_ODR_3200 == state->eng_info.curr_odr)
  {
    rw_buffer |= STM_STENG1AX_FIFO_EN_ADV_MASK;
  }
  steng1ax_read_modify_write(this,
      STM_STENG1AX_REG_FIFO_CTRL,
      &rw_buffer,
      1,
      &xfer_bytes,
      false,
      STM_STENG1AX_FIFO_EN_ADV_MASK | STM_STENG1AX_FIFO_MODE_MASK,
      hw_id);
}

// reconfig fifo after flush or without flush
void steng1ax_reconfig_fifo(sns_sensor_instance *this, bool flush, uint8_t hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  bool set_bypass_mode = false;
  UNUSED_VAR(set_bypass_mode);

  DBG_INST_PRINTF_EX(HIGH, this, "[%d] reconfig_fifo En: flush=%u cur0x%x des=0x%x wmk_postpone=%d",
                  hw_id, flush, state->fifo_info.fifo_rate, state->fifo_info.desired_fifo_rate, state->fifo_info.wmk_postpone);

  if (hw_id == STENG1AX_INTR_HW_IDX)
    steng1ax_disable_fifo_intr(this, hw_id);

  if(state->fifo_info.fifo_rate > STENG1AX_ENG_ODR_OFF || STENG1AX_IS_ESP_ENABLED(state)) {
    if(flush && (state->fifo_info.fifo_rate > STENG1AX_ENG_ODR_OFF))
    {
      steng1ax_flush_fifo(this, hw_id);
    }
    if((state->ascp_req_count[hw_id] <= 0) && state->fifo_info.reconfig_req && 
        (state->eng_info.desired_odr != state->eng_info.curr_odr)) {
      steng1ax_set_fifo_bypass_mode(this, hw_id);
      set_bypass_mode = true;
      //set in case of non-dae mode; for dae mode, wait when until new config data is received
      if(!steng1ax_dae_if_available(this))
        state->fifo_info.last_time_slot = -1;
    }
  }

  if((state->ascp_req_count[hw_id] <= 0) && state->fifo_info.reconfig_req) {
    // state->prev_odr_change_info.odr_change_timestamp = state->cur_odr_change_info.odr_change_timestamp;
    steng1ax_set_fifo_wmk(this, hw_id);

    // sns_time config_change_ts = state->cur_odr_change_info.eng_odr_settime;
    // state->cur_odr_change_info.odr_change_timestamp = 
    //   (config_change_ts > state->cur_odr_change_info.odr_change_timestamp) ? 
    //   config_change_ts : state->cur_odr_change_info.odr_change_timestamp;

    if(state->fifo_info.last_sent_config.fifo_watermark == 0) {
      state->fifo_info.new_config.fifo_watermark = state->fifo_info.desired_wmk;
      state->fifo_info.new_config.sample_rate =
        steng1ax_odr_map[state->eng_info.curr_odr_idx].odr;
      state->fifo_info.new_config.timestamp = sns_get_system_time();
#if STENG1AX_DAE_ENABLED
      state->fifo_info.new_config.dae_watermark  = state->fifo_info.max_requested_wmk;
#endif
      // if (hw_id == STENG1AX_INTR_HW_IDX)
      //   steng1ax_send_config_event(this, false);
    }
#if STENG1AX_DAE_ENABLED
    state->config_step = STENG1AX_CONFIG_IDLE; /* done with reconfig */
#endif
    // state->fifo_info.reconfig_req = false;
    // state->fifo_info.full_reconf_req = false;

    // if (hw_id == STENG1AX_INTR_HW_IDX)
    //   steng1ax_enable_fifo_intr(this, hw_id);
  }
  else if ((state->ascp_req_count[hw_id] > 0) && state->fifo_info.reconfig_req)
  {
    DBG_INST_PRINTF_EX(HIGH, this, "[%d] postpone set WMK cur0x%x des=0x%x",
                hw_id, state->eng_info.curr_wmk,  state->eng_info.desired_wmk);
    state->fifo_info.wmk_postpone = true;
  }


  DBG_INST_PRINTF(HIGH, this, "[%d] reconfig_fifo EX: flush=%u cur=0x%x wmk_postpone=%d",
                  hw_id, flush, state->fifo_info.fifo_rate, state->fifo_info.wmk_postpone);
}


void steng1ax_reconfig_hw(sns_sensor_instance *this, uint8_t hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;

  DBG_INST_PRINTF_EX(HIGH, this, "reconfig_hw: %u %u fifo_req %u", state->enabled_sensors, state->desired_sensors, state->fifo_info.reconfig_req);

  if(!state->desired_sensors) {
    if (state->eng_stream_mode == DRI)
    {
      if (STENG1AX_INTR_HW_IDX == hw_id)
      {
        steng1ax_set_interrupts(this, false, hw_id);
      }
    }
    steng1ax_powerdown_hw(this, hw_id);
  }
  else
  {
    //if eng is still off, turn on some one need this
    DBG_INST_PRINTF_EX(HIGH, this, "ENG: desired %u/0x%x prev %u/%d",
      state->eng_info.desired_odr_idx, state->eng_info.desired_odr_reg_val,
      state->eng_info.curr_odr_idx, (int)state->eng_info.curr_odr);

    if(state->enabled_sensors)
    {
      if(state->desired_sensors & STENG1AX_ENG)
      {
        state->eng_info.stage = CONFIG_REQUEST;
      }
      steng1ax_send_config_event(this, true);
    }
    if(state->enabled_sensors == state->desired_sensors &&
      state->eng_info.desired_odr_idx == state->eng_info.curr_odr_idx &&
      state->eng_info.desired_wmk == state->eng_info.curr_wmk)
    {
      DBG_INST_PRINTF_EX(HIGH, this, "same config - no reconfig: odr %d %d wmk %d %d", 
        state->eng_info.desired_odr_idx, state->eng_info.curr_odr_idx,
         state->eng_info.desired_wmk, state->eng_info.curr_wmk);

      state->eng_info.stage = CONFIG_INIT;
      return;
    }

    if (hw_id == STENG1AX_INTR_HW_IDX)
    {
#if !STENG1AX_DRDY_OUT_ENABLED
      state->eng_info.config_stage = CONFIG_FIFO;
#endif
      steng1ax_start_sensor_config_timer(this);
    }

    if(state->eng_info.desired_odr_idx &&
      (state->eng_info.desired_odr_idx != state->eng_info.curr_odr_idx ||
        state->eng_info.desired_wmk != state->eng_info.curr_wmk))
    {
      state->eng_info.config_event = true;
      state->eng_info.stage = CONFIG_SETTLED;
    }
    else
    {
      state->eng_info.stage = CONFIG_INIT;
    }

  }
}

void steng1ax_register_interrupt(
    sns_sensor_instance *this,
    steng1ax_irq_info* irq_info,
    sns_data_stream* data_stream)
{
  UNUSED_VAR(this);
  if(!irq_info->irq_registered)
  {
    uint8_t buffer[20];
    const pb_msgdesc_t *fields;
    sns_request irq_req;

    irq_req.request = buffer;
    if( irq_info->is_ibi )
    {
      irq_req.message_id = SNS_INTERRUPT_MSGID_SNS_IBI_REQ;
      fields = sns_ibi_req_fields;
    }
    else
    {
      irq_req.message_id = SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ;
      fields = sns_interrupt_req_fields;
    }

    irq_req.request_len = pb_encode_request(buffer,
                                            sizeof(buffer),
                                            &irq_info->irq_config,
                                            fields,
                                            NULL);
    if(irq_req.request_len > 0)
    {
      data_stream->api->send_request(data_stream, &irq_req);
      irq_info->irq_registered = true;
    }
  }
}

/*estimated end timestamp based on sampling intvl, number of sample set and interrupt ts*/
sns_time steng1ax_get_use_time(sns_sensor_instance *this,
                              uint16_t num_sample_sets,
                              sns_time sampling_intvl)
{
  steng1ax_instance_state *state = (steng1ax_instance_state *)this->state->state;
  uint16_t cur_wmk = state->fifo_info.bh_info.wmk;
  sns_time cur_time = state->fifo_info.bh_info.cur_time;
  // use_time is the boarder ts, calculated timestamps should not go beyond this
  sns_time use_time = state->fifo_info.bh_info.interrupt_ts;

  if(state->fifo_info.bh_info.interrupt_fired) {
    use_time = state->fifo_info.bh_info.interrupt_ts + (sampling_intvl * ((num_sample_sets >= cur_wmk) ? (num_sample_sets - cur_wmk) : 0 ));
  } else if(state->fifo_info.last_ts_valid) {
    //defines time when the data request sent, does not represent actual cur time
    use_time = state->fifo_info.last_timestamp + sampling_intvl * num_sample_sets;
  } else {
    //For flush only use-case, use flush time to mark use-time so that samples can be inserted to avoid drift.
    use_time = state->fifo_info.bh_info.interrupt_ts;
  }

  STENG1AX_INST_DEBUG_TS(HIGH, this,
      "use_ts: use_time=%u cur_time=%u last_ts=%u last_ts_valid=%d",
      (uint32_t)use_time, (uint32_t)cur_time,(uint32_t)state->fifo_info.last_timestamp, state->fifo_info.last_ts_valid);
  //if use time is greater than cur_time
  if(use_time > cur_time) {
    if(state->fifo_info.bh_info.interrupt_fired && (num_sample_sets == cur_wmk))
      use_time = state->fifo_info.bh_info.interrupt_ts;
    else if(!state->fifo_info.bh_info.interrupt_fired)
      use_time = cur_time;
  }

  return use_time;
}

sns_time steng1ax_get_first_ts(sns_sensor_instance *this,
                              uint16_t num_sample_sets,
                              sns_time sampling_intvl,
                              sns_time use_time)
{
  steng1ax_instance_state *state = (steng1ax_instance_state *)this->state->state;
  //one of the timestamp should be accurate, either last_ts valid or irq ts
  sns_time first_timestamp = state->fifo_info.last_timestamp + sampling_intvl;

  STENG1AX_INST_DEBUG_TS(HIGH, this,
      "get_first_ts: first_ts=%u last_ts_valid=%d ", (uint32_t)first_timestamp, state->fifo_info.last_ts_valid);
  if(!state->fifo_info.last_ts_valid) {
    if(state->fifo_info.bh_info.interrupt_fired) {
      first_timestamp = use_time - sampling_intvl * (num_sample_sets - 1);
      int64_t first_ts_gap = first_timestamp - state->fifo_info.last_timestamp;

      STENG1AX_INST_DEBUG_TS(HIGH, this,
          "get_first_ts: first_ts=%u first_ts_gap=%d s_int=%u use_time=%u",
          (uint32_t)first_timestamp, (int32_t)first_ts_gap, (uint32_t)sampling_intvl, (uint32_t)use_time);
      if(first_ts_gap < sampling_intvl || first_ts_gap < 0)
        first_timestamp = state->fifo_info.last_timestamp + sampling_intvl;
    } else {
      //flush case
      //for single sensor, if odr is changed, use irq_timestamp
    }
  }
  return first_timestamp;
}

void steng1ax_calculate_sampling_intvl(sns_sensor_instance *this,
    uint16_t num_sample_sets,
    sns_time* sampling_intvl)
{
  steng1ax_instance_state *state = (steng1ax_instance_state *)this->state->state;
  uint16_t cur_wmk = state->fifo_info.cur_wmk;
  sns_time cal_st  = 0;
  STENG1AX_INST_DEBUG_TS(HIGH, this,
      "calculate_ts: last_ts=%u irq_ts=%u num_sample_sets=%d lpf0 %d %d odr %x %x",
      (uint32_t)state->fifo_info.last_timestamp, (uint32_t)state->fifo_info.bh_info.interrupt_ts,
      num_sample_sets, state->fifo_info.bh_info.lpf0_en_set, state->fifo_info.lpf0_en_set,
      state->fifo_info.bh_info.eng_odr, state->fifo_info.fifo_rate);
  if(state->fifo_info.bh_info.eng_odr == state->fifo_info.fifo_rate && state->fifo_info.bh_info.lpf0_en_set == state->fifo_info.lpf0_en_set)
  {
    if((state->fifo_info.bh_info.interrupt_fired) && (num_sample_sets == cur_wmk)) {
      *sampling_intvl = steng1ax_estimate_avg_st(this, state->fifo_info.bh_info.interrupt_ts, num_sample_sets);
    } else if(state->fifo_info.interrupt_cnt < MAX_INTERRUPT_CNT) {
      state->fifo_info.interrupt_cnt = 0;
      cal_st = state->fifo_info.avg_sampling_intvl;
      if(state->fifo_info.bh_info.interrupt_fired) {
        cal_st = (state->fifo_info.bh_info.interrupt_ts -
            state->fifo_info.last_timestamp) / cur_wmk;
      }
      state->fifo_info.avg_sampling_intvl = cal_st;
      if(!STENG1AX_IS_INBOUNDS(cal_st, state->eng_info.sampling_intvl,
            STENG1AX_ODR_TOLERANCE )) {
        state->fifo_info.avg_sampling_intvl = state->eng_info.sampling_intvl;
      }
      *sampling_intvl = state->fifo_info.avg_sampling_intvl;
    }
  }
  else {
    //float odr = steng1ax_get_eng_odr(state->fifo_info.bh_info.eng_odr);
    *sampling_intvl = steng1ax_get_sample_interval(this, state->fifo_info.bh_info.eng_odr);
    DBG_INST_PRINTF(MED, this,
        "calculate_ts: older to orphan batch odr=0x%x sampling_intv=%u #set=%u",
        state->fifo_info.bh_info.eng_odr, (uint32_t)*sampling_intvl, num_sample_sets);
  }
}

void steng1ax_update_ag_config_event(
    sns_sensor_instance *instance,
    bool new_client,
    bool orphan_missing)
{
  UNUSED_VAR(orphan_missing);
  steng1ax_instance_state *state = (steng1ax_instance_state *)instance->state->state;
  float sample_rate = (state->fifo_info.bh_info.eng_odr == STENG1AX_ENG_ODR_OFF) ? STENG1AX_ODR_0 :
                       steng1ax_odr_map[state->eng_info.curr_odr_idx].odr;
  bool is_config_event_required = false, is_wmk_only_change = false;
  //if the received data is orphan and streaming is stopped; no need to send config event
  if(state->fifo_info.orphan_batch && 
     !(state->desired_sensors & STENG1AX_ENG))
    return;

  //If its orphan samples then send the config event with the odr reported by orphan sample odr
  if(!state->fifo_info.orphan_batch){
    if((state->fifo_info.bh_info.wmk  != state->fifo_info.last_sent_config.fifo_watermark)
      && (sample_rate == state->fifo_info.last_sent_config.sample_rate))
      is_wmk_only_change = true;
#if STENG1AX_DAE_ENABLED
    if(state->fifo_info.bh_info.wmk  != state->fifo_info.last_sent_config.fifo_watermark ||
       sample_rate != state->fifo_info.last_sent_config.sample_rate  ||
       ((state->fifo_info.max_requested_wmk != state->fifo_info.last_sent_config.dae_watermark) 
        && state->fifo_info.bh_info.interrupt_fired))
#else
    if(state->fifo_info.bh_info.wmk  != state->fifo_info.last_sent_config.fifo_watermark ||
       sample_rate != state->fifo_info.last_sent_config.sample_rate)
#endif
         is_config_event_required = true;
  }
  STENG1AX_INST_DEBUG_TS(HIGH, instance, "ag_config_event  req=%d sr=%d/%d wmk=%u/%u",
    is_config_event_required,
    (int)sample_rate, (int)state->fifo_info.last_sent_config.sample_rate,
    state->fifo_info.bh_info.wmk, state->fifo_info.last_sent_config.fifo_watermark);


  if(is_config_event_required)
  {
    state->eng_info.config_event = true;
    state->fifo_info.new_config.fifo_watermark = state->fifo_info.bh_info.wmk;
    state->fifo_info.new_config.sample_rate    = sample_rate;

    if(is_wmk_only_change)
    {
      if(state->fifo_info.new_config.timestamp == state->fifo_info.last_timestamp)
        state->fifo_info.new_config.timestamp++;
      else
        state->fifo_info.new_config.timestamp = state->fifo_info.last_timestamp;
    }

#if STENG1AX_DAE_ENABLED
    state->fifo_info.new_config.dae_watermark  = SNS_MAX(state->fifo_info.bh_info.wmk, state->fifo_info.max_requested_wmk);
#endif
    steng1ax_send_config_event(instance, new_client);
  }
}


void steng1ax_process_imu_data(
    sns_sensor_instance *instance,
    const uint8_t* fifo_head,
    uint32_t bytes,
    uint8_t hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state *)instance->state->state;
  uint16_t num_sample_sets;
  sns_time use_time = 0;
  sns_time sampling_intvl = state->fifo_info.avg_sampling_intvl;
  sns_time first_timestamp = 0;

  num_sample_sets = bytes / STM_STENG1AX_FIFO_SAMPLE_SIZE;
  const uint8_t* buffer = &fifo_head[0];

  STENG1AX_INST_DEBUG_TS(LOW, instance,
      "cur_wmk=%u bytes=%u num_sample_sets=%d int_cnt=%d",
      state->fifo_info.cur_wmk, bytes, num_sample_sets, state->fifo_info.interrupt_cnt);

  if(num_sample_sets >= 1)
  {
    if (hw_id == STENG1AX_INTR_HW_IDX)
    {
      //Send config event if there is any change in odr/wm
      steng1ax_update_ag_config_event(instance, false, false);
    }

    if (is_data_ready_to_process(instance, 1))
    {
      if(num_sample_sets)
        steng1ax_calculate_sampling_intvl(instance, num_sample_sets, &sampling_intvl);

      //use_time is the border ts, calculated timestamps should not go beyond this
      use_time = steng1ax_get_use_time(instance, num_sample_sets, sampling_intvl);
      //calculate first timestamp
      first_timestamp = steng1ax_get_first_ts(instance, num_sample_sets, sampling_intvl, use_time);

      STENG1AX_INST_DEBUG_TS(LOW, instance,
          "[%d] sampling_intv=%u last_ts=%u first_ts=%u use_time=%u",
          hw_id,
          (uint32_t)sampling_intvl, (uint32_t)state->fifo_info.last_timestamp,
          (uint32_t)first_timestamp, (uint32_t)use_time);
    }
    UNUSED_VAR(buffer);
    UNUSED_VAR(first_timestamp);

    steng1ax_process_fifo_data_buffer(instance,
        first_timestamp,
        use_time,
        sampling_intvl,
        buffer,
        bytes,
        num_sample_sets,
        hw_id);
    if (hw_id == STENG1AX_INTR_HW_IDX)
    {
      state->fifo_info.last_ts_valid = true;
    }
  }
}

void steng1ax_send_fifo_data(
    sns_sensor_instance *instance,
    const uint8_t* fifo_head,
    uint32_t bytes,
    uint8_t hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state *)instance->state->state;
  UNUSED_VAR(fifo_head);
  //update current time based on number of samples of eng
  if(state->fifo_info.bh_info.interrupt_fired) {

    uint16_t max_cnt = bytes / STM_STENG1AX_FIFO_SAMPLE_SIZE;
    if(max_cnt > state->fifo_info.bh_info.wmk) {
      sns_time cur_time = sns_get_system_time();
      if (hw_id == STENG1AX_INTR_HW_IDX)
      {
        state->fifo_info.bh_info.cur_time = state->fifo_info.bh_info.interrupt_ts + (max_cnt -  state->fifo_info.bh_info.wmk) * state->eng_info.sampling_intvl;
      }
      if(state->fifo_info.bh_info.cur_time > cur_time) {
        if (hw_id == STENG1AX_INTR_HW_IDX)
        {
          state->fifo_info.bh_info.cur_time = cur_time;
        }
      }
      STENG1AX_INST_DEBUG_TS(LOW, instance, "[%d] orphan = %d total_samples:wmk = %d:%d cur_time(bh_c:c)=%u:%u",
          state->hw_idx, state->fifo_info.orphan_batch, max_cnt, state->fifo_info.bh_info.wmk, (uint32_t)state->fifo_info.bh_info.cur_time, (uint32_t)cur_time);
    }
  }

  steng1ax_process_imu_data(instance, &fifo_head[0], bytes, hw_id);
}

void steng1ax_process_com_port_vector(sns_port_vector *vector, void *user_arg)
{
  sns_sensor_instance *instance = (sns_sensor_instance *)user_arg;
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  if(STM_STENG1AX_REG_FIFO_OUT_TAG == vector->reg_addr)
  {
    steng1ax_send_fifo_data(instance, vector->buffer, vector->bytes, state->ascp_hw_id);
  }
}

void steng1ax_send_fifo_flush_done(sns_sensor_instance *instance,
                                  steng1ax_sensor_type flushing_sensors,
                                  steng1ax_flush_done_reason reason)
{
  UNUSED_VAR(reason);

  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;

  SNS_INST_PRINTF(HIGH, instance, "[%u] FLUSH_EVENT sensor=%x (%u) %u",
                  state->hw_idx, flushing_sensors, reason, state->eng_sample_counter);

  while(flushing_sensors != 0)
  {
    sns_sensor_uid const *suid = NULL;
    steng1ax_sensor_type sensor_type = STENG1AX_ENG;
    if(flushing_sensors & STENG1AX_ENG)
    {
      suid = &state->eng_info.suid;
      sensor_type = STENG1AX_ENG;
    }
    else if(STENG1AX_IS_XSENSOR(flushing_sensors))
    {
      // steng1ax_sensor_type xsensor_sensor_type;
      // steng1ax_send_xsensor_fifo_flush_done(instance, flushing_sensors, reason, &xsensor_sensor_type);
      // flushing_sensors &= ~xsensor_sensor_type;
    }
    else
    {
      flushing_sensors = 0;
    }
    if(NULL != suid)
    {
      sns_service_manager *mgr = instance->cb->get_service_manager(instance);
      sns_event_service *e_service = (sns_event_service*)mgr->get_service(mgr, SNS_EVENT_SERVICE);
      sns_sensor_event *event = e_service->api->alloc_event(e_service, instance, 0);
      event->message_id = SNS_STD_MSGID_SNS_STD_FLUSH_EVENT;
      event->event_len = 0;
      event->timestamp = sns_get_system_time();
      flushing_sensors &= ~sensor_type;
      e_service->api->publish_event(e_service, instance, event, suid);
      SNS_INST_PRINTF(HIGH, instance, "[%u] publish event done", state->hw_idx);
    }
  }
}

//estimate average sample time
sns_time steng1ax_estimate_avg_st(
  sns_sensor_instance *const instance,
  sns_time irq_timestamp,
  uint16_t num_samples)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;

  uint32_t cur_int_delta = (uint32_t)(irq_timestamp - state->fifo_info.interrupt_timestamp);
  sns_time sampling_intvl = state->fifo_info.avg_sampling_intvl;

  if(state->fifo_info.interrupt_cnt == 0) {
    if(state->fifo_info.last_ts_valid)
      cur_int_delta = (uint32_t)(irq_timestamp - state->fifo_info.last_timestamp);
    else
      cur_int_delta = state->fifo_info.avg_interrupt_intvl;
  }
  float odr_tolerance = STENG1AX_ODR_TOLERANCE;
  if(state->fifo_info.interrupt_cnt < MAX_INTERRUPT_CNT) {
    odr_tolerance += 1;
  }

  state->fifo_info.interrupt_cnt++;
  if(STENG1AX_IS_INBOUNDS(cur_int_delta/num_samples, state->eng_info.sampling_intvl,
        odr_tolerance)) {

    if(state->fifo_info.interrupt_cnt > MAX_INTERRUPT_CNT) {

      uint32_t avg_int;
      uint16_t int_cnt;
      int32_t odr_percent_var;
      if(state->fifo_info.interrupt_cnt == UINT16_MAX)
        state->fifo_info.interrupt_cnt = WINDOW_SIZE + MAX_INTERRUPT_CNT;

      // QC - Is it guaranteed that fifo_info.interurpt_cnt + 1 is greater than MAX_INTERRUPT_CNT?
      int_cnt = state->fifo_info.interrupt_cnt - MAX_INTERRUPT_CNT + 1;
      if(state->fifo_info.interrupt_cnt >= WINDOW_SIZE+MAX_INTERRUPT_CNT)
        int_cnt = WINDOW_SIZE;

      avg_int = state->fifo_info.avg_interrupt_intvl;

      state->fifo_info.avg_interrupt_intvl += (int32_t)(cur_int_delta - avg_int)/int_cnt ;
      state->fifo_info.avg_sampling_intvl = state->fifo_info.avg_interrupt_intvl/num_samples;
      odr_percent_var = ((int)state->eng_info.sampling_intvl - 
                         (int)state->fifo_info.avg_sampling_intvl) * state->eng_info.curr_odr;
      
      state->odr_percent_var_eng = odr_percent_var;
      sampling_intvl = cur_int_delta/num_samples;


      STENG1AX_INST_DEBUG_TS(LOW, instance, "avg_st: cnt=%d int_delta=%u prev=%u cur int:samp=%u:%u:%d",
          int_cnt, (uint32_t)cur_int_delta, avg_int, (uint32_t)state->fifo_info.avg_interrupt_intvl,
          (uint32_t)state->fifo_info.avg_sampling_intvl, odr_percent_var);

    } else {

      DEBUG_TS_EST(HIGH, instance,
          "avg_st: irq_ts=%u prev_irq=%u delta=%d #s=%u #int=%u",
          (uint32_t)irq_timestamp, (uint32_t)state->fifo_info.interrupt_timestamp,
          cur_int_delta, num_samples, state->fifo_info.interrupt_cnt);

      if(state->fifo_info.interrupt_cnt == 1) {
        sampling_intvl = cur_int_delta / num_samples;
      } else {
        state->fifo_info.avg_interrupt_intvl = cur_int_delta;
        state->fifo_info.avg_sampling_intvl = state->fifo_info.avg_interrupt_intvl/num_samples;
        sampling_intvl = state->fifo_info.avg_sampling_intvl; 
      }

    }
  }
  state->fifo_info.interrupt_timestamp = irq_timestamp;
  return sampling_intvl;

}