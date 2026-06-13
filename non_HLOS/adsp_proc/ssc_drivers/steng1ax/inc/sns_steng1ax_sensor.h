#pragma once
/**
 * @file sns_steng1ax_sensor.h
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

#include "sns_data_stream.h"
#include "sns_diag_service.h"
#include "sns_steng1ax_hal.h"
#include "sns_math_util.h"
#include "sns_pwr_rail_service.h"
#include "sns_registry_util.h"
#include "sns_sensor.h"
#include "sns_sensor_uid.h"
#include "sns_sync_com_port_service.h"
#include "sns_steng1ax_ver.h"

#define ENG_SUID_0 \
  {  \
    .sensor_uid =  \
      {  \
        0x86, 0x36, 0xe8, 0xce, 0xe6, 0x85, 0x4d, 0x36,  \
        0x93, 0x9d, 0xe0, 0xfa, 0x69, 0x84, 0x72, 0xa2  \
      }  \
  }


#ifndef SUID_IS_NULL
#define SUID_IS_NULL(suid_ptr) ( sns_memcmp( (suid_ptr),                \
                                             &(sns_sensor_uid){{0}},    \
                                             sizeof(sns_sensor_uid) ) == 0 )
#endif

#define NAME   "steng1ax"
#if BUILD_DB
#define VENDOR "template"
#else
#define VENDOR "STMicro"
#endif


/**
 * STENG1AX ODR definitions
 */

#define STENG1AX_ODR_0                   (0.0f)
#define STENG1AX_ODR_800               (800.0f)
#define STENG1AX_ODR_3200              (3200.0f)

/**
 * ENG ranges in +/-lsb unit
 */
#define STENG1AX_ENG_RANGE_MIN    (-32768)
#define STENG1AX_ENG_RANGE_MAX    (32767)

/**
 * ENG resolutions in LSB/mV
 */
#define STENG1AX_ENG_RESOLUTION     (1311.0f)

/** Supported opertating modes */
#define STENG1AX_OFF          "OFF"
#define STENG1AX_NORMAL       "NORMAL"
typedef enum
{
  ATTR_AVAILABLE = 0,
  ATTR_NAME,
  ATTR_DATA_TYPE,
  ATTR_VENDOR,
  ATTR_VERSION,
  ATTR_RATES,
  ATTR_RESOLUTION,
  ATTR_FIFO_SIZE,
  ATTR_ACTIVE_CURRENT,
  ATTR_SLEEP_CURRENT,
  ATTR_RANGES,
  ATTR_OP_MODES,
  ATTR_API,
  ATTR_EVENT_SIZE,
  ATTR_STREAM_TYPE,
  ATTR_IS_DYNAMIC,
  ATTR_RIGID_BODY,
  ATTR_PLACEMENT,
  ATTR_HW_ID,
  ATTR_DRI_SUPPORT,
  ATTR_SYNC_STREAM_SUPPORT,
  ATTR__MAX,
}steng1ax_attrib;

#if !STENG1AX_POWERRAIL_DISABLED
/** Power rail timeout States for the STENG1AX Sensors.*/
#define STENG1AX_POWER_RAIL_OFF_TIMEOUT_NS 1000000000ULL /* 1 second */
typedef enum
{
  STENG1AX_POWER_RAIL_PENDING_NONE,
  STENG1AX_POWER_RAIL_PENDING_SOFT_PD,
  STENG1AX_POWER_RAIL_PENDING_INIT,
  STENG1AX_POWER_RAIL_PENDING_SET_CLIENT_REQ,
  STENG1AX_POWER_RAIL_PENDING_OFF,
} steng1ax_power_rail_pending_state;
#endif

/** State shared by all STENG1AX sensors. */
typedef struct steng1ax_shared_state
{
  bool                              soft_pd:1;
  bool                              hw_is_present:1;
  uint8_t                           hw_idx;
  uint8_t                           rigid_body_type;
  int8_t                            outstanding_reg_requests;
  int8_t                            outstanding_reg_platform_requests;
  uint16_t                          who_am_i;
  int32_t                           odr_percent_var_eng;
  steng1ax_instance_config         inst_cfg;
  sns_data_stream                   *suid_stream;
  sns_data_stream                   *timer_stream;
  sns_sync_com_port_service         *scp_service;
  sns_pwr_rail_service              *pwr_rail_service;
#if STENG1AX_DAE_ENABLED
  sns_data_stream                   *dae_stream;
#endif
#if !STENG1AX_POWERRAIL_DISABLED
  sns_rail_config                   rail_config;
  sns_power_rail_state              registry_rail_on_state;
  steng1ax_power_rail_pending_state  power_rail_pend_state;
  sns_time                          power_rail_on_timestamp;
  sns_time                          soft_pd_timestamp;
#endif
} steng1ax_shared_state;

typedef struct steng1ax_combined_request
{
  float                   sample_rate;
  float                   report_rate;
  float                   dae_report_rate;
  uint64_t                flush_period_ticks;
  uint32_t                report_per_us;
  uint32_t                dae_report_per_us;
  bool                    max_batch:1;
  bool                    flush_only:1;
  bool                    client_present:1;
} steng1ax_combined_request;

typedef struct steng1ax_state
{
  int64_t                 hardware_id;
  sns_data_stream         *reg_data_stream;

  size_t                  encoded_event_len;
  sns_sensor_uid          my_suid;
  steng1ax_sensor_type   sensor;
  bool                    available:1;
  uint8_t                 is_dri:2;
  bool                    supports_sync_stream:1;
  bool                    flush_req:1;

  void (*send_reg_request)(sns_sensor *const this, uint8_t hw_id); 
  // consolidated from all IMU requests
  steng1ax_combined_request combined_imu;

} steng1ax_state;


#define MAX_SUPPORTED_SENSORS 1
typedef struct {
  steng1ax_sensor_type   sensor;
  size_t                  state_size;
  sns_sensor_uid const    *suid;
  sns_sensor_api          *sensor_api;
  sns_sensor_instance_api *instance_api;
} steng1ax_sensors;


/** Global const tables */
extern const steng1ax_sensors steng1ax_supported_sensors[MAX_SUPPORTED_SENSORS];


/** Functions shared by all STENG1AX Sensors */
/**
 * This function parses the client_request list per Sensor and
 * determines final config for the Sensor Instance.
 *
 * @param[i] this          Sensor reference
 * @param[i] instance      Sensor Instance to config
 * @param[i] sensor_type   Sensor type
 *
 * @return none
 */
void steng1ax_set_client_config(sns_sensor *this,
                               sns_sensor_instance *instance,
                               steng1ax_shared_state *shared_state);

/**
 * set_client_request() Sensor API common between all STENG1AX
 * Sensors.
 *
 * @param this            Sensor reference
 * @param exist_request   existing request
 * @param new_request     new request
 * @param remove          true to remove request
 *
 * @return sns_sensor_instance*
 */
sns_sensor_instance* steng1ax_set_client_request(sns_sensor *const this,
                                                struct sns_request const *exist_request,
                                                struct sns_request const *new_request,
                                                bool remove);

/**
 * Publishes default Sensor attributes.
 *
 * @param this   Sensor reference
 *
 * @return none
 */
void steng1ax_publish_def_attributes(sns_sensor *const this);
void steng1ax_init_sensor_info(sns_sensor *const this,
                              sns_sensor_uid const *suid,
                              steng1ax_sensor_type sensor_type);
void steng1ax_process_suid_events(sns_sensor *const this);
void steng1ax_handle_selftest_request_removal(sns_sensor *const this,
                                             sns_sensor_instance *const instance,
                                             steng1ax_shared_state *shared_state);
void steng1ax_process_registry_events(sns_sensor *const this);
sns_rc steng1ax_discover_hw(sns_sensor *const this, uint8_t hw_id);
void steng1ax_set_soft_pd(sns_sensor *const this, uint8_t hw_id);
void steng1ax_update_siblings(sns_sensor *const this, steng1ax_shared_state *shared_state);
sns_sensor_uid const* steng1ax_get_sensor_uid(sns_sensor const *const this);

sns_rc steng1ax_eng_init(sns_sensor *const this);
sns_rc steng1ax_eng_deinit(sns_sensor *const this);
sns_sensor* steng1ax_get_sensor_by_type(sns_sensor *const this, steng1ax_sensor_type sensor);
sns_sensor* steng1ax_get_master_sensor(sns_sensor *const this);
steng1ax_shared_state* steng1ax_get_shared_state(sns_sensor *const this);
steng1ax_shared_state* steng1ax_get_shared_state_from_state(sns_sensor_state const *state);
//steng1ax_instance_config const* steng1ax_get_instance_config(sns_sensor_state const *state);
void steng1ax_init_inst_config(sns_sensor *const this, steng1ax_shared_state* shared_state);
void steng1ax_update_rail_vote(sns_sensor *this,
                              steng1ax_shared_state* shared_state,
                              sns_power_rail_state vote);
void sns_steng1ax_registry_def_config(sns_sensor *const this,
                                     sns_registry_phy_sensor_pf_cfg *cfg);
bool steng1ax_send_registry_request(sns_sensor *const this, char *reg_group_name);
sns_rc steng1ax_sensor_notify_event(sns_sensor *const this);

sns_sensor_instance* steng1ax_update_request_q(sns_sensor *const this,
    struct sns_request const *exist_request,
    struct sns_request const *new_request,
    bool remove);

sns_sensor_instance* steng1ax_handle_client_request(sns_sensor *const this,
    struct sns_request const *exist_request,
    struct sns_request const *new_request,
    bool remove);

bool steng1ax_get_decoded_self_test_request(
  sns_sensor const                *this,
  sns_request const               *in_request,
  sns_std_request                 *decoded_request,
  sns_physical_sensor_test_config *decoded_payload);

bool steng1ax_decode_sensor_config_registry_data(
    sns_sensor *const this,
    pb_istream_t* stream,
    struct pb_buffer_arg* group_name,
    sns_registry_read_event* read_event);

sns_rc steng1ax_set_dynamic_addr(sns_sensor *const this, uint8_t hw_id);
