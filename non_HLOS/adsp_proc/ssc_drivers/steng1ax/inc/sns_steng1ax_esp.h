#pragma once
/**
 * @file sns_steng1ax_esp.h
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

#include "sns_sensor.h"
#include "sns_register.h"
#include "sns_motion_detect.pb.h"
#include "sns_steng1ax_build_config.h"

/** Enable FSM/MLC based user defined sensors */
#if(STENG1AX_ESP_XSENSOR_1     || \
    STENG1AX_ESP_XSENSOR_2     || \
    STENG1AX_ESP_XSENSOR_3     || \
    STENG1AX_ESP_XSENSOR_4     || \
    STENG1AX_ESP_XSENSOR_5     )
#define STENG1AX_ESP_ENABLED   1
#else
#define STENG1AX_ESP_ENABLED   0
#endif

#define STM_STENG1AX_REG_FUNC_CFG_EN               (0x80)
#define STM_STENG1AX_REG_FUNC_CFG_EN_MASK          (0x80)

typedef struct {
  char name[40];
  char proto[40];
  uint32_t sensor;
  uint16_t min_odr;
  uint16_t stream_type;
  void (*register_sensor)(sns_register_cb const *register_api); 
  void (*reconfig)(sns_sensor_instance *const instance, bool enable); 
  void (*handle_intr)(sns_sensor_instance *const instance, 
                                 sns_time ts,
                                 uint8_t const *reg,
                                 uint8_t const *data);
  void (*handle_timer_events)(sns_sensor_instance *const instance); 
} steng1ax_esp_sensors;

#if STENG1AX_ESP_ENABLED
#define STENG1AX_GEN_GROUP(x,group) NAME "_"#x group


typedef enum {
  MAX_ESP_SENSORS,
} esp_sensor_index;

typedef struct
{

} steng1ax_esp_registry_cfg;

typedef struct steng1ax_esp_info
{
  sns_sensor_uid          suid[MAX_ESP_SENSORS];
  uint16_t                desired_sensors;
  uint16_t                enabled_sensors;
  uint16_t                update_int;
} steng1ax_esp_info;

extern steng1ax_esp_sensors steng1ax_supported_esp_sensors[MAX_ESP_SENSORS];


#define STENG1AX_IS_ESP_ENABLED(s) (s->esp_info.enabled_sensors | STENG1AX_IS_XSENSOR_ENABLED(s))
#define STENG1AX_IS_ESP_DESIRED(s) (s->esp_info.desired_sensors | STENG1AX_IS_XSENSOR_DESIRED(s))

#define STENG1AX_IS_ESP_SENSOR(s) (STENG1AX_IS_XSENSOR(s)) ? (true) : (false)

#define STENG1AX_ESP_ENABLED_MASK (0)

#define STENG1AX_IS_ESP_BIT_SET(s) (s & STENG1AX_ESP_ENABLED_MASK) ? (true) : (false)

#define STENG1AX_IS_ESP_CONF_CHANGED(state) (state->esp_info.desired_sensors ^ state->esp_info.enabled_sensors)
#define STENG1AX_IS_ESP_PRESENCE_CHANGED(state) ((state->esp_info.desired_sensors & ~state->esp_info.enabled_sensors) || \
                                                (~state->esp_info.desired_sensors & state->esp_info.enabled_sensors))
#define STENG1AX_UPDATE_ESP_SENSOR_BIT(state, sensor, enable) ((enable) ? (state->esp_info.desired_sensors |= sensor) : \
                                                                      (state->esp_info.desired_sensors &= ~sensor))
#else
#define STENG1AX_IS_ESP_SENSOR(s) (false)
#define STENG1AX_IS_ESP_ENABLED(s) (false)
#define STENG1AX_IS_ESP_DESIRED(s) (false)
#define STENG1AX_IS_ESP_CONF_CHANGED(state) (false)
#define STENG1AX_ESP_ENABLED_MASK 0
#endif

void steng1ax_esp_register(sns_register_cb const *register_api);
void steng1ax_send_esp_registry_requests(sns_sensor *const this, uint8_t hw_id);
void steng1ax_process_esp_registry_event(sns_sensor *const this, sns_sensor_event *event);

void steng1ax_init_esp_instance(sns_sensor_instance *instance, sns_sensor_state const *sstate_ptr);
void steng1ax_device_set_esp_default_state(sns_sensor_instance *instance, uint8_t hw_id);

bool steng1ax_is_esp_request_present(sns_sensor_instance *instance);
void steng1ax_recover_esp(sns_sensor_instance *const instance);
void steng1ax_reconfig_esp(sns_sensor_instance *const instance);
float steng1ax_get_esp_rate(sns_sensor_instance *instance);
uint16_t steng1ax_get_esp_rate_idx(sns_sensor_instance *instance);
void steng1ax_handle_esp_interrupt(sns_sensor_instance *const instance,
                                 sns_time irq_timestamp,
                                 uint8_t const *wake_src,
                                 uint8_t const *emb_src,
                                 uint8_t hw_id);
sns_rc steng1ax_handle_esp_timer_events(sns_sensor_instance *const instance);

sns_rc
steng1ax_publish_esp_attributes(sns_sensor *const this, steng1ax_esp_sensors* esp_sensor_ptr);
sns_rc steng1ax_esp_init(sns_sensor *const this, sns_sensor_uid* suid, steng1ax_esp_sensors* esp_sensor_ptr, uint32_t sensor);
void steng1ax_esp_deinit(sns_sensor_instance *instance);
void steng1ax_clear_request_q(sns_sensor *const this, sns_sensor_instance *instance, struct sns_request const *exist, sns_sensor_uid* suid, bool remove);

sns_rc steng1ax_emb_cfg_access(sns_sensor_instance *instance, bool enable, uint8_t hw_id);

