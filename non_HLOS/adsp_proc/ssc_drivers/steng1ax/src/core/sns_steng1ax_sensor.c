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
#include "sns_mem_util.h"
#include "sns_service_manager.h"
#include "sns_stream_service.h"
#include "sns_service.h"
#include "sns_sensor_util.h"
#include "sns_types.h"
#include "sns_attribute_util.h"

#include "sns_steng1ax_sensor.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "sns_pb_util.h"
#include "sns_suid.pb.h"

#define CONFIG_ENG            ".eng.config"
#define CONFIG                 ".config"
#define PLATFORM_CONFIG         "_platform.config"
#define PLATFORM_QVAR_CONFIG      "_platform.qvar_config"

#define STENG1AX_GEN_GROUP(x,group) NAME "_"#x group

#define MAX_DEP_LENGTH 30

// temp structure for pb arg
typedef struct pb_arg_reg_group_arg
{
  sns_sensor* this;
  const char*          name;
  steng1ax_sensor_type sensor;
  uint32_t version;
}pb_arg_reg_group_arg;

#if !STENG1AX_REGISTRY_DISABLED
enum {
  REG_CONFIG_ENG,
  REG_CONFIG,
  REG_PLATFORM_CONFIG,
  REG_PLATFORM_QVAR_CONFIG,
  REG_MAX_CONFIGS,
};

static char steng1ax_reg_config[SENSOR_CNT][REG_MAX_CONFIGS][40] = {
  {
    STENG1AX_GEN_GROUP(0, CONFIG_ENG),
    STENG1AX_GEN_GROUP(0, CONFIG),
    STENG1AX_GEN_GROUP(0, PLATFORM_CONFIG),
    STENG1AX_GEN_GROUP(0, PLATFORM_QVAR_CONFIG),
  },
  {
    STENG1AX_GEN_GROUP(1, CONFIG_ENG),
    STENG1AX_GEN_GROUP(1, CONFIG),
    STENG1AX_GEN_GROUP(1, PLATFORM_CONFIG),
    STENG1AX_GEN_GROUP(1, PLATFORM_QVAR_CONFIG),
  },
  {
    STENG1AX_GEN_GROUP(2, CONFIG_ENG),
    STENG1AX_GEN_GROUP(2, CONFIG),
    STENG1AX_GEN_GROUP(2, PLATFORM_CONFIG),
    STENG1AX_GEN_GROUP(2, PLATFORM_QVAR_CONFIG),
  },
  {
    STENG1AX_GEN_GROUP(3, CONFIG_ENG),
    STENG1AX_GEN_GROUP(3, CONFIG),
    STENG1AX_GEN_GROUP(3, PLATFORM_CONFIG),
    STENG1AX_GEN_GROUP(3, PLATFORM_QVAR_CONFIG),
  },
};
#endif

extern const odr_reg_map steng1ax_odr_map[];
extern const uint32_t steng1ax_odr_map_len;

static char def_dependency[][MAX_DEP_LENGTH] =  {
  "interrupt", "async_com_port", "timer",
#if STENG1AX_DAE_ENABLED
  "data_acquisition_engine",
#endif
#if !STENG1AX_REGISTRY_DISABLED
  // Only depend registry if registry support is enabled
   "registry"
#endif
};

#if !STENG1AX_ATTRIBUTE_DISABLED
static const char name[] = NAME;
static const char vendor[] = VENDOR;
static const uint32_t version = SNS_VERSION_STENG1AX; // major[31:16].minor[15:0]

static const uint32_t max_fifo_depth = STENG1AX_MAX_FIFO; // samples

static const sns_std_sensor_stream_type stream_type = SNS_STD_SENSOR_STREAM_TYPE_STREAMING;
static const bool is_dynamic = false;
static const sns_std_sensor_rigid_body_type rigid_body = SNS_STD_SENSOR_RIGID_BODY_TYPE_DISPLAY;
static const uint32_t hardware_id = 0;
static const bool supports_dri = true;
static const bool supports_sync_stream = true;

void steng1ax_publish_def_attributes(sns_sensor *const this)
{
  steng1ax_state *state = (steng1ax_state*)this->state->state;

  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg = &((pb_buffer_arg)
        { .buf = name, .buf_len = sizeof(name) });
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_NAME, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg = &((pb_buffer_arg)
        { .buf = vendor, .buf_len = sizeof(vendor) });
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_VENDOR, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = version;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_VERSION, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = max_fifo_depth;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_FIFO_SIZE, &value, 1, false);
  }
  {
    sns_std_attr_value_data values[] = {SNS_ATTR, SNS_ATTR};
    char const op_mode1[] = STENG1AX_NORMAL;
    char const op_mode2[] = STENG1AX_OFF;

    values[0].str.funcs.encode = pb_encode_string_cb;
    values[0].str.arg = &((pb_buffer_arg)
        { .buf = op_mode1, .buf_len = sizeof(op_mode1) });
    values[1].str.funcs.encode = pb_encode_string_cb;
    values[1].str.arg = &((pb_buffer_arg)
        { .buf = op_mode2, .buf_len = sizeof(op_mode2) });
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_OP_MODES,
        values, ARR_SIZE(values), false);
  }
  {
    float data[SENSOR_CNT] = {0};
    state->encoded_event_len =
        pb_get_encoded_size_sensor_stream_event(data, SENSOR_CNT);
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = state->encoded_event_len;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_EVENT_SIZE, &value, 1, false);
  }
  {
    sns_std_attr_value_data values[] = {SNS_ATTR};
    values[0].has_sint = true;
    values[0].sint = stream_type;
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_STREAM_TYPE,
        values, ARR_SIZE(values), false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_boolean = true;
    value.boolean = is_dynamic;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_DYNAMIC, &value, 1, false);
  }
{
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = rigid_body;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_RIGID_BODY, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = hardware_id;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_HW_ID, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_boolean = true;
    value.boolean = supports_dri;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_DRI, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_boolean = true;
    value.boolean = true;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_PHYSICAL_SENSOR, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_boolean = true;
    value.boolean = supports_sync_stream;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_STREAM_SYNC, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_boolean = true;
    value.boolean = state->available;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_AVAILABLE, &value, 1, true);
  }
}
#endif
/**
 * Publish attributes read from registry
 *
 * @param[i] this    reference to this Sensor
 *
 * @return none
 */
static void publish_registry_attributes(
  sns_sensor *const this,
  steng1ax_shared_state *shared_state)
{
#if !STENG1AX_ATTRIBUTE_DISABLED
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    bool dri_enabled = false;

    dri_enabled = shared_state->inst_cfg.eng_stream_mode;
    value.has_boolean = true;
    value.boolean = dri_enabled;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_DRI, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_boolean = true;
    value.boolean = state->supports_sync_stream;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_STREAM_SYNC, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = 0;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_HW_ID, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = shared_state->rigid_body_type;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_RIGID_BODY, &value, 1, false);
  }

  if(state->sensor == STENG1AX_ENG)
  {
    /** Only eng use registry information for min and max ODRs */
    {
      int i, j =0;
      int num_odrs = steng1ax_odr_map_len - 1;
      sns_std_attr_value_data rates[] = {SNS_ATTR, SNS_ATTR};

      for(i=0, j=shared_state->inst_cfg.min_odr_idx[0];
          i<ARR_SIZE(rates) && j<=num_odrs && j<=shared_state->inst_cfg.max_odr_idx[0];
          i++, j++)
      {
        rates[i].has_flt = true;
        rates[i].flt = steng1ax_odr_map[j].odr;
      }
      sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_RATES, rates, i, false);
    }

    /** Only eng use registry information for selected resolution. */
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_flt = true;
    value.flt = 1/STENG1AX_ENG_RESOLUTION;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_SELECTED_RESOLUTION, &value, 1, false);

    /** Only eng and use registry information for selected range. */
    sns_std_attr_value_data values[] = {SNS_ATTR};
    sns_std_attr_value_data rangeMinMax[] = {SNS_ATTR, SNS_ATTR};
    rangeMinMax[0].has_flt = true;
    rangeMinMax[1].has_flt = true;
    if(state->sensor == STENG1AX_ENG) {
      rangeMinMax[0].flt = STENG1AX_ENG_RANGE_MIN / STENG1AX_ENG_RESOLUTION;
      rangeMinMax[1].flt = STENG1AX_ENG_RANGE_MAX / STENG1AX_ENG_RESOLUTION;
    }
    values[0].has_subtype = true;
    values[0].subtype.values.funcs.encode = sns_pb_encode_attr_cb;
    values[0].subtype.values.arg =
      &((pb_buffer_arg){ .buf = rangeMinMax, .buf_len = ARR_SIZE(rangeMinMax) });
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_SELECTED_RANGE, &values[0], ARR_SIZE(values), true);
  }
#else
  UNUSED_VAR(this);
  UNUSED_VAR(shared_state);
#endif
}

static void publish_available(sns_sensor *const this)
{
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
  value.has_boolean = true;
  value.boolean = true;
  sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_AVAILABLE, &value, 1, true);
  state->available = true;
}

#if !STENG1AX_REGISTRY_DISABLED
bool steng1ax_send_registry_request(sns_sensor *const this, char *reg_group_name)
{
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  uint8_t buffer[100];
  int32_t encoded_len;
  bool ret = false;

  sns_registry_read_req read_request;
  pb_buffer_arg data = (pb_buffer_arg){
    .buf = reg_group_name,
    .buf_len = (strlen(reg_group_name) + 1) };

  read_request.name.arg = &data;
  read_request.name.funcs.encode = pb_encode_string_cb;

  encoded_len = pb_encode_request(buffer, sizeof(buffer),
                                  &read_request, sns_registry_read_req_fields, NULL);
  if(0 < encoded_len)
  {
    sns_request request = (sns_request){
      .request_len = encoded_len, .request = buffer,
      .message_id = SNS_REGISTRY_MSGID_SNS_REGISTRY_READ_REQ };
    sns_rc rc = state->reg_data_stream->api->send_request(state->reg_data_stream, &request);
    if(SNS_RC_SUCCESS == rc)
    {
      ret = true;
    }
  }
  else
  {
    SNS_PRINTF(ERROR, this, "send_reg_req: group not sent");
  }
  return ret;
}

static void send_registry_requests(sns_sensor *const this,  steng1ax_shared_state* shared_state)
{
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  UNUSED_VAR(shared_state);
  // uint8_t hw_id = shared_state->hw_idx;
  int i = 0;

  if(steng1ax_send_registry_request(this, steng1ax_reg_config[i][REG_CONFIG_ENG]))
    shared_state->outstanding_reg_requests++;

  if(steng1ax_send_registry_request(this, steng1ax_reg_config[i][REG_CONFIG]))
    shared_state->outstanding_reg_requests++;

  for (i = 0; i < SENSOR_CNT; i++)
  {
    if(STENG1AX_ENG == state->sensor)
    {
      if(steng1ax_send_registry_request(this, steng1ax_reg_config[i][REG_PLATFORM_CONFIG]))
        shared_state->outstanding_reg_platform_requests++;
      if(steng1ax_send_registry_request(this, steng1ax_reg_config[i][REG_PLATFORM_QVAR_CONFIG]))
        shared_state->outstanding_reg_platform_requests++;
    }
    else if(STENG1AX_IS_ESP_SENSOR(state->sensor))
    {
      steng1ax_send_esp_registry_requests(this, i);
    }
  }
}

static void process_registry_suid(sns_sensor *const this)
{
  sns_sensor *sensor;
  sns_service_manager *service_mgr = this->cb->get_service_manager(this);
  sns_stream_service  *stream_svc  =
    (sns_stream_service*) service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);

  for(sensor = this->cb->get_library_sensor(this, true);
      NULL != sensor;
      sensor = this->cb->get_library_sensor(this, false))
  {
    steng1ax_state *state = (steng1ax_state*)sensor->state->state;

    stream_svc->api->create_sensor_stream(stream_svc, sensor,
                                          shared_state->inst_cfg.reg_suid,
                                          &state->reg_data_stream);
    if(NULL != state->reg_data_stream)
    {
      send_registry_requests(sensor, shared_state);

      DBG_PRINTF_EX(HIGH, sensor, "process_registry_suid: sensor=%u reg_stream=%x #req=%u",
                 state->sensor, state->reg_data_stream, shared_state->outstanding_reg_requests);
    }
    else
    {
      SNS_PRINTF(ERROR, sensor, "Failed to create registry stream");
    }
  }
}
#endif

void steng1ax_sensor_save_registry_pf_cfg(
  sns_sensor *const this,
  sns_registry_phy_sensor_pf_cfg const * phy_sensor_pf_cfg)
{
  sns_rc rc;
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
  sns_service_manager *service_mgr = this->cb->get_service_manager(this);
  uint8_t hw_id = shared_state->hw_idx;

  shared_state->rigid_body_type = phy_sensor_pf_cfg->rigid_body_type;

  {
    steng1ax_instance_config *inst_cfg = &shared_state->inst_cfg;
    inst_cfg->min_odr_idx[hw_id] = 1; // 800Hz
#if STENG1AX_DAE_ENABLED
    inst_cfg->min_odr_idx[hw_id] = 1;
#endif
    inst_cfg->max_odr_idx[hw_id] = 1;
#if STENG1AX_ODR_REGISTRY_FEATURE_ENABLE
    if(phy_sensor_pf_cfg->max_odr != 0 &&
       phy_sensor_pf_cfg->max_odr >= phy_sensor_pf_cfg->min_odr &&
       phy_sensor_pf_cfg->max_odr < (unsigned int)steng1ax_odr_map[steng1ax_odr_map_len-1].odr)
    {
      inst_cfg->min_odr_idx[hw_id] = 1;
#if STENG1AX_DAE_ENABLED
      inst_cfg->min_odr_idx[hw_id] = 1;
#endif
      while(phy_sensor_pf_cfg->min_odr >
            (unsigned int)steng1ax_odr_map[inst_cfg->min_odr_idx[hw_id]].odr)
      {
        inst_cfg->min_odr_idx[hw_id]++;
      }
      DBG_PRINTF_EX(MED, this, "min_odr=%u --> idx=%u odr=%u",
                    phy_sensor_pf_cfg->min_odr, inst_cfg->min_odr_idx[hw_id],
                    (unsigned int)steng1ax_odr_map[inst_cfg->min_odr_idx[hw_id]].odr);

      inst_cfg->max_odr_idx[hw_id] = steng1ax_odr_map_len - 1;
      while(phy_sensor_pf_cfg->max_odr <
            (unsigned int)steng1ax_odr_map[inst_cfg->max_odr_idx[hw_id]].odr &&
            inst_cfg->max_odr_idx[hw_id] > inst_cfg->min_odr_idx[hw_id])
      {
        inst_cfg->max_odr_idx[hw_id]--;
      }
      DBG_PRINTF_EX(MED, this, "max_odr=%u --> idx=%u odr=%u",
                    phy_sensor_pf_cfg->max_odr, inst_cfg->max_odr_idx[hw_id],
                    (unsigned int)steng1ax_odr_map[inst_cfg->max_odr_idx[hw_id]].odr);
    }
#endif
  }
  {
    /**-----------------Register and Open COM Port-------------------------*/
    steng1ax_com_port_info *com_port        = &shared_state->inst_cfg.com_port_info[hw_id];
    com_port->com_config.bus_type          = phy_sensor_pf_cfg->bus_type;
    com_port->com_config.bus_instance      = phy_sensor_pf_cfg->bus_instance;
    com_port->com_config.slave_control     = phy_sensor_pf_cfg->slave_config;
    com_port->com_config.min_bus_speed_KHz = phy_sensor_pf_cfg->min_bus_speed_khz;
    com_port->com_config.max_bus_speed_KHz = phy_sensor_pf_cfg->max_bus_speed_khz;
    com_port->com_config.reg_addr_type     = phy_sensor_pf_cfg->reg_addr_type;
    com_port->i2c_address                  = phy_sensor_pf_cfg->slave_config;
    DBG_PRINTF_EX(MED, this, "[%d] min_bus_speed_KHz:%d max_bus_speed_KHz:%d reg_addr_type:%d bus_type:%u bus_instance:%u slave_control:%u",
               hw_id,
               com_port->com_config.min_bus_speed_KHz,
               com_port->com_config.max_bus_speed_KHz,
               com_port->com_config.reg_addr_type,
               com_port->com_config.bus_type,
               com_port->com_config.bus_instance,
               com_port->com_config.slave_control);
#if STENG1AX_USE_I3C
    // --- if I3C mode, set up the com port to always use the I3C address
    shared_state->scp_service =  (sns_sync_com_port_service *)
        service_mgr->get_service(service_mgr, SNS_SYNC_COM_PORT_SERVICE);

    if(com_port->com_config.bus_type == SNS_BUS_I3C_SDR ||
       com_port->com_config.bus_type == SNS_BUS_I3C_HDR_DDR )
    {
      com_port->i3c_address                  = phy_sensor_pf_cfg->i3c_address;

      com_port->com_config.slave_control   = com_port->i3c_address;
      //fill up for legacy i2c config
      com_port->com_config_ex.bus_type          = SNS_BUS_I3C_I2C_LEGACY;
      com_port->com_config_ex.bus_instance      = phy_sensor_pf_cfg->bus_instance;
      com_port->com_config_ex.slave_control     = phy_sensor_pf_cfg->slave_config;
      com_port->com_config_ex.min_bus_speed_KHz = phy_sensor_pf_cfg->min_bus_speed_khz;
      com_port->com_config_ex.max_bus_speed_KHz = phy_sensor_pf_cfg->max_bus_speed_khz;
      com_port->com_config_ex.reg_addr_type     = phy_sensor_pf_cfg->reg_addr_type;

      DBG_PRINTF_EX(MED, this, "[%d] I3C_I2C_LEGACY: min_bus_speed_KHz:%d max_bus_speed_KHz:%d reg_addr_type:%d bus_type:%u bus_instance:%u slave_control:%u",
                 hw_id,
                 com_port->com_config_ex.min_bus_speed_KHz,
                 com_port->com_config_ex.max_bus_speed_KHz,
                 com_port->com_config_ex.reg_addr_type,
                 com_port->com_config_ex.bus_type,
                 com_port->com_config_ex.bus_instance,
                 com_port->com_config_ex.slave_control);


      rc = shared_state->scp_service->api->
        sns_scp_register_com_port(&com_port->com_config_ex, &com_port->port_handle_ex);

      if(rc == SNS_RC_SUCCESS)
      {
        SNS_PRINTF(MED, this,
                   "save_registry_pf_cfg: open I3C_I2C_LEGACY port %x", com_port->port_handle_ex);
        rc = shared_state->scp_service->api->sns_scp_open(com_port->port_handle_ex);
      }
      else
      {
        SNS_PRINTF(ERROR, this, "register_com_port fail rc:%u",rc);
      }
    }
    else
    {
      rc = shared_state->scp_service->api->
        sns_scp_register_com_port(&com_port->com_config, &com_port->port_handle);

      if(rc == SNS_RC_SUCCESS)
      {
        SNS_PRINTF(MED, this,
                   "[%d] save_registry_pf_cfg: open port %x", hw_id, com_port->port_handle);
        rc = shared_state->scp_service->api->sns_scp_open(com_port->port_handle);
      }
      else
      {
        SNS_PRINTF(ERROR, this, "register_com_port fail rc:%u",rc);
      }
    }
  }
#else
    rc = shared_state->scp_service->api->
      sns_scp_register_com_port(&com_port->com_config, &com_port->port_handle);

    if(rc == SNS_RC_SUCCESS)
    {
      SNS_PRINTF(MED, this,
                 "save_registry_pf_cfg: open port %x", com_port->port_handle);
      rc = shared_state->scp_service->api->sns_scp_open(com_port->port_handle);
    }
    else
    {
      SNS_PRINTF(ERROR, this, "register_com_port fail rc:%u",rc);
    }
  }
#endif //STENG1AX_USE_I3C
  {
    if(shared_state->inst_cfg.irq_config[hw_id].is_ibi)
    {
      sns_ibi_req *ibi_config = &shared_state->inst_cfg.irq_config[hw_id].ibi_config;

      ibi_config->dynamic_slave_addr = phy_sensor_pf_cfg->i3c_address;
      ibi_config->bus_instance = phy_sensor_pf_cfg->bus_instance;
      ibi_config->ibi_data_bytes = 4;
      SNS_PRINTF(MED, this,
                 "[%d] config IBI slave_addr:%d bus instance:%d num_bytes:%d",
                 hw_id,
                 ibi_config->dynamic_slave_addr,
                 ibi_config->bus_instance,
                 ibi_config->ibi_data_bytes);
    }
    else
    {
      sns_interrupt_req *irq_config        = &shared_state->inst_cfg.irq_config[hw_id].irq_config;
      // QC - might be better to add a second interrupt number to registry
      // rather than overloading dri_irq_num
      irq_config->interrupt_num            = (phy_sensor_pf_cfg->dri_irq_num & 0xFFFF);
      irq_config->interrupt_pull_type      = phy_sensor_pf_cfg->irq_pull_type;
      irq_config->is_chip_pin              = phy_sensor_pf_cfg->irq_is_chip_pin;
      irq_config->interrupt_drive_strength = phy_sensor_pf_cfg->irq_drive_strength;
      irq_config->interrupt_trigger_type   = phy_sensor_pf_cfg->irq_trigger_type;

      DBG_PRINTF_EX(MED, this,
                    "[%d] interrupt_num:%d pull_type:%d is_chip_pin:%d drive_strength:%d trigger_type:%d",
                    hw_id,
                    irq_config->interrupt_num,
                    irq_config->interrupt_pull_type,
                    irq_config->is_chip_pin,
                    irq_config->interrupt_drive_strength,
                    irq_config->interrupt_trigger_type);
    }
  }
  {
#if !STENG1AX_POWERRAIL_DISABLED
    shared_state->rail_config.num_of_rails = phy_sensor_pf_cfg->num_rail;
    shared_state->registry_rail_on_state = phy_sensor_pf_cfg->rail_on_state;

    sns_strlcpy(shared_state->rail_config.rails[0].name,
                phy_sensor_pf_cfg->vddio_rail,
                sizeof(shared_state->rail_config.rails[0].name));
    sns_strlcpy(shared_state->rail_config.rails[1].name,
                phy_sensor_pf_cfg->vdd_rail,
                sizeof(shared_state->rail_config.rails[1].name));

    /**---------------------Register Power Rails --------------------------*/

    if(NULL == shared_state->pwr_rail_service && rc == SNS_RC_SUCCESS)
    {
      shared_state->rail_config.rail_vote = SNS_RAIL_OFF;

      shared_state->pwr_rail_service =
        (sns_pwr_rail_service*)service_mgr->get_service(service_mgr,
                                                        SNS_POWER_RAIL_SERVICE);

      shared_state->pwr_rail_service->api->
        sns_register_power_rails(shared_state->pwr_rail_service,
                                 &shared_state->rail_config);
    }

#endif
  }
}

#if !STENG1AX_REGISTRY_DISABLED

static bool
steng1ax_parse_reg_cfg_multi(sns_registry_data_item *reg_item,
                          struct pb_buffer_arg  *item_name,
                          struct pb_buffer_arg  *item_str_val,
                          void *parsed_buffer)
{
  sns_steng1ax_registry_mutli_cfg *data_ptr = (sns_steng1ax_registry_mutli_cfg *)parsed_buffer;
  UNUSED_VAR(item_str_val);

  if(0 == strncmp((char*)item_name->buf, "use_multi_eng", item_name->buf_len))
  {
    data_ptr->use_multi_eng = (bool)reg_item->sint;
  }
  else if(0 == strncmp((char*)item_name->buf, "num_sensors_enable", item_name->buf_len))
  {
    data_ptr->num_sensors_enable = (int)reg_item->sint;
  }

  return true;
}


static bool
steng1ax_parse_reg_cfg_ex(sns_registry_data_item *reg_item,
                          struct pb_buffer_arg  *item_name,
                          struct pb_buffer_arg  *item_str_val,
                          void *parsed_buffer)
{
  sns_steng1ax_registry_cfg *data_ptr = (sns_steng1ax_registry_cfg *)parsed_buffer;
  UNUSED_VAR(item_str_val);

  if(0 == strncmp((char*)item_name->buf, "zin_eng2_disable", item_name->buf_len))
  {
    data_ptr->zin_eng2_disable = (bool)reg_item->sint;
  }
  else if(0 == strncmp((char*)item_name->buf, "zin_eng1_disable", item_name->buf_len))
  {
    data_ptr->zin_eng1_disable = (bool)reg_item->sint;
  }
  else if(0 == strncmp((char*)item_name->buf, "mode", item_name->buf_len))
  {
    data_ptr->eng_mode = (uint8_t)reg_item->sint;
  }
  else if(0 == strncmp((char*)item_name->buf, "impedence", item_name->buf_len))
  {
    data_ptr->eng_impedance = (uint16_t)reg_item->sint;
  }
  else if(0 == strncmp((char*)item_name->buf, "gain", item_name->buf_len))
  {
    data_ptr->eng_gain = (uint8_t)reg_item->sint;
  }

  return true;
}

bool steng1ax_decode_sensor_config_registry_data(
    sns_sensor *const this,
    pb_istream_t* stream,
    struct pb_buffer_arg* group_name,
    sns_registry_read_event* read_event)
{
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
  bool rv = false;
  sns_registry_phy_sensor_cfg sensor_cfg = {0,0,0,0};
  sns_registry_decode_arg arg = {
    .item_group_name = group_name,
    .parse_info_len = 1,
    .parse_info[0] = {
      .group_name = "config",
      .parse_func = sns_registry_parse_phy_sensor_cfg,
      .parsed_buffer = &sensor_cfg
    }
  };

  read_event->data.items.funcs.decode = &sns_registry_item_decode_cb;
  read_event->data.items.arg = &arg;

  rv = pb_decode(stream, sns_registry_read_event_fields, read_event);

  if(rv)
  {
    state->is_dri               = sensor_cfg.is_dri;
#if STENG1AX_FORCE_IBI_DISABLED
    if(state->is_dri == 2)
      state->is_dri = 1; // Disable IBI
#endif
    state->hardware_id          = sensor_cfg.hw_id;
    state->supports_sync_stream = sensor_cfg.sync_stream;
    if(state->sensor == STENG1AX_ENG) {
      shared_state->inst_cfg.eng_stream_mode = (state->is_dri!=0);
      shared_state->inst_cfg.eng_resolution_idx = sensor_cfg.res_idx;
      SNS_PRINTF(MED, this, "is_dri=%d",state->is_dri);
      if(state->is_dri == 2)
      {
        for (int hw_id = 0; hw_id < SENSOR_CNT; hw_id++)
        {
          shared_state->inst_cfg.irq_config[hw_id].is_ibi = true;
        }
      }

    }
  }
  DBG_PRINTF_EX(MED, this, "resolution_idx:%d, supports_sync_stream:%d ",
                sensor_cfg.res_idx, state->supports_sync_stream);
  return rv;
}

static void process_registry_event(sns_sensor *const this, sns_sensor_event *event)
{
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
  uint8_t hw_id = shared_state->hw_idx;

  pb_istream_t stream = pb_istream_from_buffer((void*)event->event, event->event_len);

  if(SNS_REGISTRY_MSGID_SNS_REGISTRY_READ_EVENT == event->message_id)
  {
    sns_registry_read_event read_event = sns_registry_read_event_init_default;
    pb_buffer_arg group_name = {0,0};
    read_event.name.arg = &group_name;
    read_event.name.funcs.decode = pb_decode_string_cb;

    if(!pb_decode(&stream, sns_registry_read_event_fields, &read_event))
    {
      SNS_PRINTF(ERROR, this, "Error decoding registry event");
    }
    else
    {
      SNS_PRINTF(HIGH, this, "outstanding_reg_requests %d %d %d", shared_state->outstanding_reg_requests, shared_state->outstanding_reg_platform_requests, shared_state->hw_idx);
      bool rv = false;
      stream = pb_istream_from_buffer((void*)event->event, event->event_len);

      if(0 == strncmp((char*)group_name.buf, steng1ax_reg_config[0][REG_CONFIG_ENG],
                      group_name.buf_len))
      {
        rv = steng1ax_decode_sensor_config_registry_data(this, &stream, &group_name, &read_event);
        shared_state->outstanding_reg_requests--;
      }
      else if(0 == strncmp((char*)group_name.buf, steng1ax_reg_config[hw_id][REG_PLATFORM_CONFIG],
                           group_name.buf_len))
      {
        sns_registry_phy_sensor_pf_cfg phy_sensor_pf_cfg;
        memset(&phy_sensor_pf_cfg, 0, sizeof(phy_sensor_pf_cfg));
        sns_registry_decode_arg arg = {
          .item_group_name = &group_name,
          .parse_info_len = 1,
          .parse_info[0] = {
              .group_name = "config",
              .parse_func = sns_registry_parse_phy_sensor_pf_cfg,
              .parsed_buffer = &phy_sensor_pf_cfg
          }
        };

        read_event.data.items.funcs.decode = &sns_registry_item_decode_cb;
        read_event.data.items.arg = &arg;

        rv = pb_decode(&stream, sns_registry_read_event_fields, &read_event);

        if(rv)
        {
          steng1ax_sensor_save_registry_pf_cfg(this, &phy_sensor_pf_cfg);
        }
        shared_state->outstanding_reg_platform_requests--;
      }
      else if(0 == strncmp((char*)group_name.buf, steng1ax_reg_config[hw_id][REG_PLATFORM_QVAR_CONFIG],
                      group_name.buf_len))
      {
        sns_steng1ax_registry_cfg *reg_cfg_ex = &shared_state->inst_cfg.eng_reg_cfg[hw_id];

        sns_registry_decode_arg arg = {
          .item_group_name = &group_name,
          .parse_info_len = 1,
          .parse_info[0] = {
              .group_name = "qvar_config",
              .parse_func = steng1ax_parse_reg_cfg_ex,
              .parsed_buffer = reg_cfg_ex
          }
        };

        read_event.data.items.funcs.decode = &sns_registry_item_decode_cb;
        read_event.data.items.arg = &arg;

        rv = pb_decode(&stream, sns_registry_read_event_fields, &read_event);
        if(rv)
        {
          switch (reg_cfg_ex->eng_impedance)
          {
            case (100):
            {
              shared_state->inst_cfg.eng_impedance_idx[hw_id] = STENG1AX_ENG_IMPEDENCE_100;
              break;
            }
            case (200):
            {
              shared_state->inst_cfg.eng_impedance_idx[hw_id] = STENG1AX_ENG_IMPEDENCE_200;
              break;
            }
            case (500):
            {
              shared_state->inst_cfg.eng_impedance_idx[hw_id] = STENG1AX_ENG_IMPEDENCE_500;
              break;
            }
            case (1000):
            {
              shared_state->inst_cfg.eng_impedance_idx[hw_id] = STENG1AX_ENG_IMPEDENCE_1000;
              break;
            }
            default:
            {
              shared_state->inst_cfg.eng_impedance_idx[hw_id] = STENG1AX_ENG_IMPEDENCE_100;
              break;
            }
          }

          switch (reg_cfg_ex->eng_gain)
          {
            case (2):
            {
              shared_state->inst_cfg.eng_gain_idx[hw_id] = STENG1AX_ENG_GAIN_2;
              break;
            }
            case (4):
            {
              shared_state->inst_cfg.eng_gain_idx[hw_id] = STENG1AX_ENG_GAIN_4;
              break;
            }
            case (8):
            {
              shared_state->inst_cfg.eng_gain_idx[hw_id] = STENG1AX_ENG_GAIN_8;
              break;
            }
            case (16):
            {
              shared_state->inst_cfg.eng_gain_idx[hw_id] = STENG1AX_ENG_GAIN_16;
              break;
            }
            default:
            {
              shared_state->inst_cfg.eng_gain_idx[hw_id] = STENG1AX_ENG_GAIN_2;
              break;
            }
          }
          SNS_PRINTF(HIGH, this,
              "[%d] Reg_Cfg_EX: zin_eng2_disable=%u zin_eng1_disable=%u eng_mode=%u eng_impedance=%u(%d) eng_gain=%u(%d) ",
              hw_id, reg_cfg_ex->zin_eng2_disable, reg_cfg_ex->zin_eng1_disable, reg_cfg_ex->eng_mode, reg_cfg_ex->eng_impedance,
              shared_state->inst_cfg.eng_impedance_idx[hw_id], reg_cfg_ex->eng_gain, shared_state->inst_cfg.eng_gain_idx[hw_id]);
        }
        shared_state->outstanding_reg_platform_requests--;
      }
      else if(0 == strncmp((char*)group_name.buf, steng1ax_reg_config[0][REG_CONFIG],
                      group_name.buf_len))
      {
        sns_steng1ax_registry_mutli_cfg *reg_cfg_ex = &shared_state->inst_cfg.multi_eng_cfg;

        sns_registry_decode_arg arg = {
          .item_group_name = &group_name,
          .parse_info_len = 1,
          .parse_info[0] = {
              .group_name = "config",
              .parse_func = steng1ax_parse_reg_cfg_multi,
              .parsed_buffer = reg_cfg_ex
          }
        };

        read_event.data.items.funcs.decode = &sns_registry_item_decode_cb;
        read_event.data.items.arg = &arg;

        rv = pb_decode(&stream, sns_registry_read_event_fields, &read_event);
        if(rv)
        {

          SNS_PRINTF(HIGH, this,
              "[%d] Reg_cfg_multi: use_multi_eng=%u num_sensors_enable=%u",
              hw_id, reg_cfg_ex->use_multi_eng, reg_cfg_ex->num_sensors_enable);
          if (reg_cfg_ex->use_multi_eng)
          {
            if (reg_cfg_ex->num_sensors_enable > SENSOR_CNT)
            {
              reg_cfg_ex->num_sensors_enable = SENSOR_CNT;
            }
          }
          else
          {
            reg_cfg_ex->num_sensors_enable = 1;
          }
          SNS_PRINTF(HIGH, this,
              "[%d] Reg_cfg_multi update: use_multi_eng=%u num_sensors_enable=%u",
              hw_id, reg_cfg_ex->use_multi_eng, reg_cfg_ex->num_sensors_enable);
        }
        shared_state->outstanding_reg_requests--;
      }
      else
      {
        rv = false;
      }

      if(!rv)
      {
        SNS_PRINTF(ERROR, this, "err decoding registry group");
      }
    }
    if ((shared_state->outstanding_reg_platform_requests % 2 == 0) && 
      (shared_state->outstanding_reg_platform_requests != (SENSOR_CNT * 2)))
    {
      shared_state->hw_idx++;
      SNS_PRINTF(HIGH, this, "outstanding_reg_requests %d %d %d", shared_state->outstanding_reg_requests, shared_state->outstanding_reg_platform_requests, shared_state->hw_idx);
    }
  }
#if STENG1AX_REGISTRY_WRITE_EVENT
  else if(SNS_REGISTRY_MSGID_SNS_REGISTRY_WRITE_EVENT == event->message_id)
  {
    sns_registry_write_event write_event = sns_registry_write_event_init_default;

    if(pb_decode(&stream, sns_registry_write_event_fields, &write_event))
    {
      if (write_event.status == SNS_REGISTRY_WRITE_STATUS_SUCCESS)
      {
        SNS_PRINTF(HIGH, this, "Registry updated");
      }
      else
      {
        SNS_PRINTF(ERROR, this, "Registry update failed");
      }
    }
    else
    {
      SNS_PRINTF(ERROR, this, "Error decoding REGISTRY_WRITE_EVENT");
    }
  }
#endif
  else
  {
    DBG_PRINTF(ERROR, this, "Received unsupported registry event msg id %u",
               event->message_id);
  }
}

#endif

static void send_suid_req(sns_sensor *this, char *const data_type, uint32_t data_type_len)
{
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);

  if(shared_state->suid_stream == NULL)
  {
    sns_service_manager *manager = this->cb->get_service_manager(this);
    sns_stream_service *stream_service =
      (sns_stream_service*)manager->get_service(manager, SNS_STREAM_SERVICE);
     stream_service->api->create_sensor_stream(stream_service, this, sns_get_suid_lookup(),
                                               &shared_state->suid_stream);
  }

  if(shared_state->suid_stream != NULL)
  {
    size_t encoded_len;
    pb_buffer_arg data = (pb_buffer_arg){ .buf = data_type, .buf_len = data_type_len };
    uint8_t buffer[50];

    sns_suid_req suid_req = sns_suid_req_init_default;
    suid_req.has_register_updates = true;
    suid_req.register_updates = true;
    suid_req.data_type.funcs.encode = &pb_encode_string_cb;
    suid_req.data_type.arg = &data;
    sns_rc rc = SNS_RC_SUCCESS;

    encoded_len = pb_encode_request(buffer, sizeof(buffer), &suid_req, sns_suid_req_fields, NULL);
    if(0 < encoded_len)
    {
      sns_request request = (sns_request){
        .request_len = encoded_len, .request = buffer, .message_id = SNS_SUID_MSGID_SNS_SUID_REQ };
      rc = shared_state->suid_stream->api->
             send_request(shared_state->suid_stream, &request);
    }
    if(0 >= encoded_len || SNS_RC_SUCCESS != rc)
    {
      SNS_PRINTF(ERROR, this, "encoded_len=%d rc=%u", encoded_len, rc);
    }
  }
}

static void init_dependencies(sns_sensor *const this)
{
  DBG_PRINTF_EX(LOW, this, "init_dependencies sensor");

  for(int i=0;i<ARR_SIZE(def_dependency);i++)
  {
    send_suid_req(this, def_dependency[i], strlen(def_dependency[i]));
  }
}

void steng1ax_init_sensor_info(sns_sensor *const this,
                              sns_sensor_uid const *suid,
                              steng1ax_sensor_type sensor_type)
{
  steng1ax_state *state = (steng1ax_state*)this->state->state;

  state->sensor = sensor_type;
  sns_memscpy(&state->my_suid, sizeof(state->my_suid), suid, sizeof(sns_sensor_uid));

  DBG_PRINTF_EX(LOW, this, "init_sensor_info: sensor=%u", sensor_type);

  if(STENG1AX_ENG == sensor_type)
  {
    init_dependencies(this);
    steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
#if STENG1AX_REGISTRY_DISABLED
	int hw_idx_store = shared_state->hw_idx;
    for ( int i = 0; i < SENSOR_CNT; i++ ){
      sns_registry_phy_sensor_pf_cfg cfg;
      sns_steng1ax_registry_def_config(this, &cfg);
      steng1ax_sensor_save_registry_pf_cfg(this, &cfg);
      shared_state->hw_idx++;
    }
	shared_state->hw_idx = hw_idx_store;
#else
    UNUSED_VAR(shared_state);
#endif
  }
}

void steng1ax_process_suid_events(sns_sensor *const this)
{
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
  sns_data_stream *stream = shared_state->suid_stream;
  sns_service_manager *service_mgr;
  sns_stream_service  *stream_svc;

  if(NULL == stream || 0 == stream->api->get_input_cnt(stream))
  {
    return;
  }

  service_mgr = this->cb->get_service_manager(this);
  stream_svc = (sns_stream_service*) service_mgr->get_service(service_mgr,
                                                              SNS_STREAM_SERVICE);
  UNUSED_VAR(stream_svc);
  for(sns_sensor_event *event = stream->api->peek_input(stream);
      NULL != event;
      event = stream->api->get_next_input(stream))
  {
    if(SNS_SUID_MSGID_SNS_SUID_EVENT == event->message_id)
    {
      pb_istream_t pbstream = pb_istream_from_buffer((void*)event->event, event->event_len);
      sns_suid_event suid_event = sns_suid_event_init_default;
      pb_buffer_arg data_type_arg = { .buf = NULL, .buf_len = 0 };
      sns_sensor_uid uid_list;
      sns_suid_search suid_search;
      suid_search.suid = &uid_list;
      suid_search.num_of_suids = 0;

      suid_event.data_type.funcs.decode = &pb_decode_string_cb;
      suid_event.data_type.arg = &data_type_arg;
      suid_event.suid.funcs.decode = &pb_decode_suid_event;
      suid_event.suid.arg = &suid_search;

      if(!pb_decode(&pbstream, sns_suid_event_fields, &suid_event))
      {
         SNS_PRINTF(ERROR, this, "pb_decode() failed");
         continue;
       }

      /* if no suids found, ignore the event */
      if(suid_search.num_of_suids == 0)
      {
        continue;
      }

      /* save suid based on incoming data type name */
      if(0 == strncmp(data_type_arg.buf, "interrupt", data_type_arg.buf_len))
      {
        shared_state->inst_cfg.irq_suid = uid_list;
      }
      else if(0 == strncmp(data_type_arg.buf, "timer", data_type_arg.buf_len))
      {
        shared_state->inst_cfg.timer_suid = uid_list;
#if !STENG1AX_POWERRAIL_DISABLED
        stream_svc->api->create_sensor_stream(stream_svc, this,
                                              shared_state->inst_cfg.timer_suid,
                                              &shared_state->timer_stream);
        if(NULL == shared_state->timer_stream)
        {
          DBG_PRINTF(ERROR, this, "process_suid_events: Failed to create timer stream");
        }
#endif
      }
      else if (0 == strncmp(data_type_arg.buf, "async_com_port",
                            data_type_arg.buf_len))
      {
        shared_state->inst_cfg.acp_suid = uid_list;
      }
#if !STENG1AX_REGISTRY_DISABLED
      else if (0 == strncmp(data_type_arg.buf, "registry", data_type_arg.buf_len))
      {
        shared_state->inst_cfg.reg_suid = uid_list;
        process_registry_suid(this);
      }
#endif
#if STENG1AX_DAE_ENABLED
      else if (0 == strncmp(data_type_arg.buf, "data_acquisition_engine",
                            data_type_arg.buf_len))
      {
        shared_state->inst_cfg.dae_suid = uid_list;
      }
#endif
      else
      {
        SNS_PRINTF(ERROR, this, "process_suid_events: invalid datatype_name");
      }
    }
  }
  return;
}

void steng1ax_process_registry_events(sns_sensor *const this)
{
#if !STENG1AX_REGISTRY_DISABLED
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
  sns_data_stream *stream = state->reg_data_stream;
  if(NULL == stream || 0 == stream->api->get_input_cnt(stream))
  {
    return;
  }

  DBG_PRINTF_EX(HIGH, this, "registry_event: sensor=%u stream=%x", state->sensor, stream);

  for(; 0 != stream->api->get_input_cnt(stream); stream->api->get_next_input(stream))
  {
    sns_sensor_event *event = stream->api->peek_input(stream);
    process_registry_event(this, event);
  }
  if ((shared_state->outstanding_reg_requests == 0) && (shared_state->outstanding_reg_platform_requests == 0))
  {
    sns_sensor_util_remove_sensor_stream(this, &state->reg_data_stream);
  }
#else
  UNUSED_VAR(this);
#endif
}

void steng1ax_set_soft_pd(sns_sensor *const this, uint8_t hw_id)
{
  sns_rc rv = SNS_RC_SUCCESS;
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
  steng1ax_com_port_info *com_port = &shared_state->inst_cfg.com_port_info[hw_id];
  uint8_t buf = 0;
  uint32_t xfer_bytes;
  bool open_port = false;

  sns_service_manager *service_mgr = this->cb->get_service_manager(this);
  shared_state->scp_service =  (sns_sync_com_port_service *)
      service_mgr->get_service(service_mgr, SNS_SYNC_COM_PORT_SERVICE);

  if(com_port->com_config.bus_type == SNS_BUS_SPI)
  {
    buf = 0x1;
    if(com_port->port_handle == NULL)
    {
      rv = shared_state->scp_service->api->
        sns_scp_register_com_port(&com_port->com_config, &com_port->port_handle);

      if(rv == SNS_RC_SUCCESS)
      {
        DBG_PRINTF_EX(MED, this, "set_soft_pd: SPI: open port %x", com_port->port_handle);
        rv = shared_state->scp_service->api->sns_scp_open(com_port->port_handle);
      }
      else
      {
        SNS_PRINTF(ERROR, this, "register_com_port fail rc:%u",rv);
      }
      open_port = true;
    }
    if(com_port->port_handle)
    {
      rv = steng1ax_com_write_wrapper_scp(shared_state->scp_service,
                                com_port->port_handle,
                                0x3e,
                                &buf,
                                1,
                                &xfer_bytes);
      DBG_PRINTF_EX(MED, this, "set_soft_pd: SPI: reg 0x3e write 0x1 rv=%u", rv);
    }
    if(open_port)
    {
      shared_state->scp_service->api->sns_scp_close(com_port->port_handle);
      DBG_PRINTF_EX(MED, this, "set_soft_pd: SPI: close port %x", com_port->port_handle);
      if(com_port->port_handle != NULL)
      {
        shared_state->scp_service->api->sns_scp_deregister_com_port(&com_port->port_handle);
        DBG_PRINTF_EX(MED, this, "set_soft_pd: SPI: close port %x", com_port->port_handle);
      }
    }
  }
  else if(com_port->com_config.bus_type == SNS_BUS_I3C_I2C_LEGACY || com_port->com_config.bus_type == SNS_BUS_I2C)
  {
    if(com_port->port_handle == NULL)
    {
      rv = shared_state->scp_service->api->
        sns_scp_register_com_port(&com_port->com_config, &com_port->port_handle);

      if(rv == SNS_RC_SUCCESS)
      {
        DBG_PRINTF_EX(MED, this, "set_soft_pd: I2C: open port %x", com_port->port_handle);
        rv = shared_state->scp_service->api->sns_scp_open(com_port->port_handle);
      }
      else
      {
        SNS_PRINTF(ERROR, this, "register_com_port fail rc:%u",rv);
      }
      open_port = true;
    }
    if(com_port->port_handle)
    {
      steng1ax_com_read_wrapper(shared_state->scp_service,
                                com_port->port_handle,
                                STM_STENG1AX_REG_WHO_AM_I,
                                &buf,
                                1,
                                &xfer_bytes);
      DBG_PRINTF_EX(MED, this, "set_soft_pd: I2C: read WHO_AM_I rv=%u", rv);
    }
    if(open_port)
    {
      shared_state->scp_service->api->sns_scp_close(com_port->port_handle);
      DBG_PRINTF_EX(MED, this, "set_soft_pd: I2C: close port %x", com_port->port_handle);
      if(com_port->port_handle != NULL)
      {
        shared_state->scp_service->api->sns_scp_deregister_com_port(&com_port->port_handle);
      }
    }
  }
  else if(com_port->com_config.bus_type == SNS_BUS_I3C ||
          com_port->com_config.bus_type == SNS_BUS_I3C_SDR)
  {
    if(com_port->port_handle_ex == NULL)
    {
      rv = shared_state->scp_service->api->
             sns_scp_register_com_port(&com_port->com_config_ex, &com_port->port_handle_ex);

      if(rv == SNS_RC_SUCCESS)
      {
        DBG_PRINTF_EX(MED, this, "set_soft_pd: I3C: open I2C_LEGACY_port %x", com_port->port_handle_ex);
        rv = shared_state->scp_service->api->sns_scp_open(com_port->port_handle_ex);
      }
      else
      {
        SNS_PRINTF(ERROR, this, "register_com_port fail rc:%u",rv);
      }
      open_port = true;
    }
    if(com_port->port_handle_ex)
    {
      rv = steng1ax_com_read_wrapper(shared_state->scp_service,
                                 com_port->port_handle_ex,
                                 STM_STENG1AX_REG_WHO_AM_I,
                                 &buf,
                                 1,
                                 &xfer_bytes);
    }
    DBG_PRINTF_EX(MED, this, "set_soft_pd: I3C: read WHO_AM_I rv=%u", rv);
    if(open_port)
    {
      shared_state->scp_service->api->sns_scp_close(com_port->port_handle_ex);
      DBG_PRINTF_EX(MED, this, "set_soft_pd: I3C: close I2C_LEGACY_port %x", com_port->port_handle_ex);
      if(com_port->port_handle_ex != NULL)
      {
        shared_state->scp_service->api->sns_scp_deregister_com_port(&com_port->port_handle_ex);
      }
    }
  }
}

sns_rc steng1ax_set_dynamic_addr(sns_sensor *const this, uint8_t hw_id)
{
  sns_rc rv = SNS_RC_SUCCESS;

#if STENG1AX_USE_I3C
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
  steng1ax_com_port_info *com_port = &shared_state->inst_cfg.com_port_info[hw_id];
  sns_sync_com_port_service *scp_service = shared_state->scp_service;
  sns_sync_com_port_handle    *i2c_port_handle = NULL;
  sns_com_port_config          i2c_com_config = com_port->com_config;
  uint32_t                     xfer_bytes;
  uint8_t                      buffer[1];

  if(com_port->com_config.bus_type != SNS_BUS_I3C_SDR &&
     com_port->com_config.bus_type != SNS_BUS_I3C_HDR_DDR)
  {
    return SNS_RC_SUCCESS;
  }
  if(com_port->is_in_i3c)
  {
    return SNS_RC_SUCCESS;
  }
  {
    i2c_com_config.slave_control = com_port->i2c_address;
    rv = scp_service->api->sns_scp_register_com_port(&i2c_com_config, &i2c_port_handle);
    if( rv == SNS_RC_SUCCESS )
    {
      SNS_PRINTF(HIGH, this, "set_dynamic_addr: open SETDASA port %x", i2c_port_handle);
      rv = scp_service->api->sns_scp_open(i2c_port_handle);
      if( rv == SNS_RC_SUCCESS )
      {
        /**-------------------Assign I3C dynamic address------------------------*/
        buffer[0] = (com_port->i3c_address & 0xFF)<<1;
        rv = scp_service->api->
          sns_scp_issue_ccc( i2c_port_handle,
                             SNS_SYNC_COM_PORT_CCC_SETDASA,
                             buffer, 1, &xfer_bytes );
#if STENG1AX_USE_RSTDAA
        if( rv == SNS_RC_SUCCESS )
        {
          com_port->is_in_i3c = true;
        }
#endif
        SNS_PRINTF(HIGH, this, "set_dynamic_addr: SETDASA %u is_in_i3c %d",
          com_port->i3c_address, com_port->is_in_i3c);
        scp_service->api->sns_scp_close(i2c_port_handle);
        SNS_PRINTF(HIGH, this, "set_dynamic_addr: close SETDASA port %x", i2c_port_handle);
      }
    }
    if( i2c_port_handle != NULL )
    {
      scp_service->api->sns_scp_deregister_com_port(&i2c_port_handle);
    }
  }
#else
  UNUSED_VAR(this);
#endif
  return rv;
}
sns_rc steng1ax_discover_hw(sns_sensor *const this, uint8_t hw_id)
{
  sns_rc rv = SNS_RC_SUCCESS;
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);
  steng1ax_com_port_info *com_port = &shared_state->inst_cfg.com_port_info[hw_id];
  uint8_t buffer[1];
  if(shared_state->soft_pd == false)
  {
    return SNS_RC_FAILED;
  }

  if(com_port->com_config.bus_type == SNS_BUS_I3C ||
     com_port->com_config.bus_type == SNS_BUS_I3C_SDR)
  {
    //open I3C port
    rv = shared_state->scp_service->api->
      sns_scp_register_com_port(&com_port->com_config, &com_port->port_handle);
    if(rv == SNS_RC_SUCCESS)
    {
      DBG_PRINTF_EX(HIGH, this, "discover_hw: I3C open port %x", com_port->port_handle);
      DBG_PRINTF_EX(MED, this, "[%d] min_bus_speed_KHz:%d max_bus_speed_KHz:%d reg_addr_type:%d bus_type:%u bus_instance:%u slave_control:%u",
                 hw_id,
                 com_port->com_config.min_bus_speed_KHz,
                 com_port->com_config.max_bus_speed_KHz,
                 com_port->com_config.reg_addr_type,
                 com_port->com_config.bus_type,
                 com_port->com_config.bus_instance,
                 com_port->com_config.slave_control);
      rv = shared_state->scp_service->api->sns_scp_open(com_port->port_handle);
    }
    else
    {
      SNS_PRINTF(ERROR, this, "register_com_port fail rc:%u",rv);
    }
  }

  rv = steng1ax_set_dynamic_addr(this, hw_id);

  buffer[0] = 0x0;
  if(rv == SNS_RC_SUCCESS)
  {
    /**-------------------Read and Confirm WHO-AM-I------------------------*/
    rv = steng1ax_get_who_am_i(shared_state->scp_service, com_port->port_handle, &buffer[0]);
  }
  else
  {
    SNS_PRINTF(ERROR, this, "[%u] failed to enter I3C", hw_id);
  }

  if(rv == SNS_RC_SUCCESS && 
      (buffer[0] == STENG1AX_WHOAMI_VALUE))
  {
    DBG_PRINTF_EX(HIGH, this, "[%u] WhoAmI Got 0x%x", hw_id, buffer[0]);
    sns_sensor_instance *instance = sns_sensor_util_get_shared_instance(this);
    if(instance)
    {
      DBG_PRINTF_EX(HIGH, this, "Instance available resetting device");
      // Reset Sensor
      rv = steng1ax_reset_device(instance,
          STENG1AX_ENG);
      if(rv == SNS_RC_SUCCESS)
      {
        shared_state->hw_is_present = true;
      }
    }
    else
    {
      shared_state->hw_is_present = true;
    }
  }
  else
  {
    SNS_PRINTF(ERROR, this, "[%u] err=0x%x WhoAmI=0x%x", hw_id, rv, buffer[0]);
  }
  shared_state->who_am_i = buffer[0];

  rv = steng1ax_exit_i3c_mode(&shared_state->inst_cfg.com_port_info[hw_id], shared_state->scp_service);
  if(rv != SNS_RC_SUCCESS)
  {
    SNS_PRINTF(ERROR, this, "[%u] exit_i3c_mode failed", shared_state->hw_idx);
  }

  /**------------------Power Down and Close COM Port--------------------*/
  shared_state->scp_service->api->sns_scp_update_bus_power(com_port->port_handle, false);

  shared_state->scp_service->api->sns_scp_close(com_port->port_handle);
  shared_state->scp_service->api->sns_scp_deregister_com_port(&com_port->port_handle);

  if(rv != SNS_RC_SUCCESS)
  {
    SNS_PRINTF(HIGH, this, "discover_hw failed %d", rv);
    rv = SNS_RC_INVALID_LIBRARY_STATE;
  }

  return rv;
}

void steng1ax_update_siblings(sns_sensor *const this, steng1ax_shared_state *shared_state)
{
  bool all_suids_available = true;
  steng1ax_state *state = (steng1ax_state*)this->state->state;

  if(   SUID_IS_NULL(&shared_state->inst_cfg.irq_suid)
     || SUID_IS_NULL(&shared_state->inst_cfg.acp_suid)
     || SUID_IS_NULL(&shared_state->inst_cfg.timer_suid)
#if !STENG1AX_REGISTRY_DISABLED
     || SUID_IS_NULL(&shared_state->inst_cfg.reg_suid)
#endif
#if STENG1AX_DAE_ENABLED
    || SUID_IS_NULL(&shared_state->inst_cfg.dae_suid)
#endif
    )
  {
    all_suids_available = false;
  }

  if(all_suids_available)
  {
    sns_sensor *lib_sensor;

    DBG_PRINTF_EX(HIGH, this, "siblings: publishing registry attributes and avail");

    for(lib_sensor = this->cb->get_library_sensor(this, true);
        NULL != lib_sensor;
        lib_sensor = this->cb->get_library_sensor(this, false))
    {
      DBG_PRINTF(HIGH, this, "[%u] siblings: sensor=0x%x",
                 shared_state->hw_idx, ((steng1ax_state*)lib_sensor->state->state)->sensor);
      publish_registry_attributes(lib_sensor, shared_state);
      publish_available(lib_sensor);
    }
    //Remove registry data stream
    sns_sensor_util_remove_sensor_stream(this, &state->reg_data_stream);
    sns_sensor_util_remove_sensor_stream(this, &shared_state->suid_stream);
  }
}

void steng1ax_handle_selftest_request_removal(
  sns_sensor          *const this,
  sns_sensor_instance *const instance,
  steng1ax_shared_state *shared_state)
{
  steng1ax_instance_state *inst_state   = (steng1ax_instance_state*)instance->state->state;

  DBG_PRINTF_EX(MED, this, "selftest_removal: config=0x%x",
             shared_state->inst_cfg.config_sensors);

  //if reconfigure hw has been postponed due to a remove request during self test. Do it now
  if(inst_state->self_test_info.reconfig_postpone)
  {
    DBG_PRINTF_EX(MED, this, "Reconfiguring HW for request received during self-test");
    steng1ax_set_client_config(this, instance, shared_state);
    inst_state->self_test_info.reconfig_postpone = false;
  }
}

