/**
 * @file sns_steng1ax_eng_sensor.c
 *
 * STENG1AX Eng virtual Sensor implementation.
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
#include "sns_mem_util.h"
#include "sns_types.h"
#include "sns_service_manager.h"
#include "sns_steng1ax_sensor.h"
#include "pb_encode.h"
#include "sns_attribute_util.h"
#include "sns_pb_util.h"

/**
 * Publish all sensor-specific attributes.
 *
 * @param[i] this    reference to this Sensor
 *
 * @return none
 */
static void steng1ax_acc_publish_attributes(sns_sensor *const this)
{
#if STENG1AX_ATTRIBUTE_DISABLED
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  const char type[] = "eng";
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg = &((pb_buffer_arg)
        { .buf = type, .buf_len = sizeof(type) });
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_TYPE, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_boolean = true;
    value.boolean = state->available;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_AVAILABLE, &value, 1, true);
  }
#else
  const char type[] = "electro_neuro_graph";
  const uint32_t active_current[2] = {STENG1AX_ENG_ACTIVE_CURRENT,
                                      STENG1AX_ENG_SLEEP_CURRENT};
  const uint32_t sleep_current = STENG1AX_ENG_SLEEP_CURRENT;

  steng1ax_publish_def_attributes(this);
  {
    sns_std_attr_value_data values[] = {SNS_ATTR/*, SNS_ATTR, SNS_ATTR, SNS_ATTR*/};
    values[0].has_flt = true;
    values[0].flt = MAX_LOW_LATENCY_RATE;
    //QC currently we are limiting to 832
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_ADDITIONAL_LOW_LATENCY_RATES,
       values, ARR_SIZE(values), false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg = &((pb_buffer_arg)
        { .buf = type, .buf_len = sizeof(type) });
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_TYPE, &value, 1, false);
  }
  {
    sns_std_attr_value_data values[] = {SNS_ATTR};

    values[0].has_flt = true;
    values[0].flt = 1/STENG1AX_ENG_RESOLUTION;
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_RESOLUTIONS,
        values, 1, false);
  }
  {
    sns_std_attr_value_data values[] = {SNS_ATTR, SNS_ATTR};
    int i;
    for(i = 0; i < ARR_SIZE(active_current); i++)
    {
      values[i].has_sint = true;
      values[i].sint = active_current[i];
    }
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_ACTIVE_CURRENT,
        values, i, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = sleep_current; //uA
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_SLEEP_CURRENT, &value, 1, false);
  }
  {
    sns_std_attr_value_data values[] = {SNS_ATTR};
    char const proto1[] = "sns_electro_neuro_graph.proto";
    // char const proto1[] = "sns_std_sensor.proto";
    values[0].str.funcs.encode = pb_encode_string_cb;
    values[0].str.arg = &((pb_buffer_arg)
        { .buf = proto1, .buf_len = sizeof(proto1) });
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_API,
        values, ARR_SIZE(values), false);
  }
  {
    sns_std_attr_value_data values[] = {SNS_ATTR};
    values[0].has_sint = true;
    values[0].sint = SNS_PHYSICAL_SENSOR_TEST_TYPE_COM;
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_PHYSICAL_SENSOR_TESTS,
        values, ARR_SIZE(values), false);
  }
  {
    sns_std_attr_value_data values[] = {SNS_ATTR};
    // Publish ranges in mV/LSB
    sns_std_attr_value_data range1[] = {SNS_ATTR, SNS_ATTR};
    range1[0].has_flt = true;
    range1[0].flt = STENG1AX_ENG_RANGE_MIN / STENG1AX_ENG_RESOLUTION;
    range1[1].has_flt = true;
    range1[1].flt = STENG1AX_ENG_RANGE_MAX / STENG1AX_ENG_RESOLUTION;
    values[0].has_subtype = true;
    values[0].subtype.values.funcs.encode = sns_pb_encode_attr_cb;
    values[0].subtype.values.arg =
      &((pb_buffer_arg){ .buf = range1, .buf_len = ARR_SIZE(range1) });

    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_RANGES,
        values, ARR_SIZE(values), true);
  }
#endif // STENG1AX_ATTRIBUTE_DISABLED
}

void steng1ax_init_inst_config(sns_sensor *const this, steng1ax_shared_state* shared_state)
{
  UNUSED_VAR(this);

  int i = 0;
  for (i=0; i < SENSOR_CNT; i++)
  {
    shared_state->inst_cfg.eng_reg_cfg[i].zin_eng2_disable = false;
    shared_state->inst_cfg.eng_reg_cfg[i].zin_eng1_disable = false;
    shared_state->inst_cfg.eng_reg_cfg[i].eng_mode = 0;
    shared_state->inst_cfg.eng_reg_cfg[i].eng_impedance = 100;
    shared_state->inst_cfg.eng_reg_cfg[i].eng_gain = 2;
    shared_state->inst_cfg.eng_impedance_idx[i] = 0;
    shared_state->inst_cfg.eng_gain_idx[i] = 0;
  }
  shared_state->inst_cfg.multi_eng_cfg.use_multi_eng = true;
  shared_state->inst_cfg.multi_eng_cfg.num_sensors_enable = 2;
}

/* See sns_sensor::init */
sns_rc steng1ax_eng_init(sns_sensor *const this)
{
  steng1ax_shared_state* shared_state = steng1ax_get_shared_state_from_state(this->state);
  sns_service_manager *smgr = this->cb->get_service_manager(this);
  shared_state->scp_service = (sns_sync_com_port_service*)
    smgr->get_service(smgr, SNS_SYNC_COM_PORT_SERVICE);
  steng1ax_instance_config *inst_cfg = &shared_state->inst_cfg;
  shared_state->hw_idx = this->cb->get_registration_index(this);


  sns_sensor_uid* suid = &((sns_sensor_uid)ENG_SUID_0);

  steng1ax_init_inst_config(this, shared_state);
  int i =0;
  for (i=0; i< SENSOR_CNT; i++)
  {
    inst_cfg->eng_reg_cfg[i].sensor_type = STENG1AX_ENG;
  }
  steng1ax_init_sensor_info(this, suid, STENG1AX_ENG);
  steng1ax_acc_publish_attributes(this);
  DBG_PRINTF_EX(LOW, this, "eng init");
  return SNS_RC_SUCCESS;
}

sns_rc steng1ax_eng_deinit(sns_sensor *const this)
{
  // Turn Sensor OFF.
  // Close COM port.
  // Turn Power Rails OFF.
  // No need to clear steng1ax_state because it will get freed anyway.

  DBG_PRINTF_EX(LOW, this, "eng deinit");
  return SNS_RC_SUCCESS;
}

