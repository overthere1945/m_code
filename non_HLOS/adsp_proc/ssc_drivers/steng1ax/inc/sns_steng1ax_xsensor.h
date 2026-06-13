#pragma once
/**
 * @file sns_steng1ax_xsensor.h
 *
 * Common implementation for STENG1AX XSensor - header
 * 
 * Copyright (c) 2021, STMicroelectronics.
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
#include "sns_steng1ax_sensor_instance.h"

/** Enable FSM/MLC based user defined sensors */
#if(STENG1AX_ESP_XSENSOR_1 || \
    STENG1AX_ESP_XSENSOR_2 || \
    STENG1AX_ESP_XSENSOR_3 || \
    STENG1AX_ESP_XSENSOR_4 || \
    STENG1AX_ESP_XSENSOR_5 || \
    STENG1AX_ESP_XSENSOR_6 )
#define STENG1AX_ESP_XSENSOR         1
#else
#define STENG1AX_ESP_XSENSOR         0
#endif

#if STENG1AX_ESP_XSENSOR
#include "sns_xsensor.pb.h"
#define XSENSOR_1_SUID_0 \
  {  \
    .sensor_uid =  \
      {  \
        0x74, 0x57, 0x1a, 0xa9, 0x42, 0x14, 0x47, 0x1a,  \
        0x83, 0x70, 0x41, 0x8b, 0x19, 0x87, 0x81, 0x84  \
      }  \
  }

#define XSENSOR_2_SUID_0 \
  {  \
    .sensor_uid =  \
      {  \
        0x3b, 0x14, 0x3d, 0xb7, 0xa1, 0x0f, 0x47, 0x28,  \
        0xa8, 0xbc, 0xa3, 0xd0, 0x36, 0xd5, 0x02, 0xe5  \
      }  \
  }

#define XSENSOR_3_SUID_0 \
  {  \
    .sensor_uid =  \
      {  \
        0x7c, 0x84, 0x8f, 0xcf, 0x8e, 0x52, 0x44, 0x74,  \
        0xa0, 0x4b, 0x9b, 0x24, 0x3e, 0x36, 0x20, 0x0c  \
      }  \
  }

#define XSENSOR_4_SUID_0 \
  {  \
    .sensor_uid =  \
      {  \
        0x4a, 0xec, 0x5a, 0xfb, 0x88, 0x5f, 0x4e, 0x28,  \
        0x92, 0x9e, 0xf4, 0x80, 0xda, 0x4d, 0xe4, 0x8b  \
      }  \
  }

#define XSENSOR_5_SUID_0 \
  {  \
    .sensor_uid =  \
      {  \
        0xa3, 0x09, 0x7a, 0x52, 0x27, 0x8a, 0x40, 0x55,  \
        0x83, 0xb8, 0x78, 0xff, 0xba, 0x1e, 0x3e, 0xc1  \
      }  \
  }
// 62865da1-0eeb-4819-b1f8-a460ae1f3a58
#define XSENSOR_6_SUID_0 \
  {  \
    .sensor_uid =  \
      {  \
        0x62, 0x86, 0x5d, 0xa1, 0x0e, 0xeb, 0x48, 0x19,  \
        0xb1, 0xf8, 0xa4, 0x60, 0xae, 0x1f, 0x3a, 0x58  \
      }  \
  }


typedef enum
{
#if STENG1AX_ESP_XSENSOR_1
    XSENSOR_1_IDX,
#endif
#if STENG1AX_ESP_XSENSOR_2
    XSENSOR_2_IDX,
#endif
#if STENG1AX_ESP_XSENSOR_3
    XSENSOR_3_IDX,
#endif
#if STENG1AX_ESP_XSENSOR_4
    XSENSOR_4_IDX,
#endif
#if STENG1AX_ESP_XSENSOR_5
    XSENSOR_5_IDX,
#endif
#if STENG1AX_ESP_XSENSOR_6
    XSENSOR_6_IDX,
#endif
    MAX_XSENSORS,
} xsensor_index;

typedef enum
{
  XSENSOR_INT_1,
} xsensor_int;

typedef enum
{
  XSENSOR_TYPE_FSM,
  XSENSOR_TYPE_MLC,
} xsensor_type;

typedef struct steng1ax_xsensor_info
{
  bool                     self_test_is_successful;
  bool                     enable_int;
  bool                     client_present;
  size_t                   encoded_event_len;
} steng1ax_xsensor_info;

typedef struct steng1ax_xsensor_group_info {
  sns_sensor_uid          suid[MAX_XSENSORS];
  uint16_t                enabled_sensors;
  uint16_t                desired_sensors;
  sns_data_stream         *timer_data_stream;
  steng1ax_xsensor_info    xsensor_info[MAX_XSENSORS];
#if STENG1AX_FSM_ENABLED
  uint8_t                 fsm_status;
  uint8_t                 fsm_outs[8];
  uint8_t                 fsm_lc[2];
  uint8_t                 fsm_lc_status;
#endif
#if STENG1AX_MLC_ENABLED
  uint8_t                 mlc_status;
  uint8_t                 mlc_src[4];
#endif
} steng1ax_xsensor_group_info;

extern steng1ax_esp_sensors steng1ax_supported_xsensors[MAX_XSENSORS];

#define STENG1AX_IS_XSENSOR(s) ((s == STENG1AX_XSENSOR_1)|| \
                                  (s == STENG1AX_XSENSOR_2)|| \
                                  (s == STENG1AX_XSENSOR_3)|| \
                                  (s == STENG1AX_XSENSOR_4)) ? (true) : (false)

#define STENG1AX_XSENSOR_ENABLED_MASK ( 0 | \
                                    STENG1AX_XSENSOR_1 | \
                                    STENG1AX_XSENSOR_2 | \
                                    STENG1AX_XSENSOR_3 | \
                                    STENG1AX_XSENSOR_4 | \
                                    0                     )

#define STENG1AX_IS_XSENSOR_ENABLED(s) (s->xgroup_info.enabled_sensors)
#define STENG1AX_IS_XSENSOR_DESIRED(s) (s->xgroup_info.desired_sensors)
#define STENG1AX_IS_XSENSOR_BIT_SET(s) (s & STENG1AX_XSENSOR_ENABLED_MASK) ? (true) : (false)
#define STENG1AX_IS_XSENSOR_TIMER_ON(s)(s->xgroup_info.timer_data_stream ? (true) : (false))
sns_sensor_instance* steng1ax_set_xsensor_request(sns_sensor *const this,
                                                struct sns_request const *exist_request,
                                                struct sns_request const *new_request,
                                                bool remove,
                                                uint8_t idx,
                                                sns_sensor_uid* suid);
void steng1ax_send_xsensor_event(sns_sensor_instance *const instance,
                                   xsensor_type type,
                                   uint8_t xsensor_idx,
                                   sns_xsensor_event_type event,
                                   sns_time ts,
                                   float *data);

void steng1ax_handle_xsensor_interrupt(sns_sensor_instance *const instance,
                                 sns_time irq_timestamp,
                                 uint8_t const *wake_src,
                                 uint8_t const *emb_src,
                                 uint8_t hw_id);
void steng1ax_init_xsensor_instance(sns_sensor_instance *instance);
sns_rc steng1ax_xsensor_enable(sns_sensor_instance *instance, xsensor_type type, uint16_t sensor,  uint16_t xsensor_idx, xsensor_int int_line, bool enable, uint8_t hw_id);
bool steng1ax_get_xsensor_interrupt_status(sns_sensor_instance *instance, xsensor_type type, uint16_t sensor);
void steng1ax_reconfig_xsensor(sns_sensor_instance *const instance);
sns_rc steng1ax_handle_xsensor_timer_events(sns_sensor_instance *const instance);
float steng1ax_get_xsensor_rate(sns_sensor_instance *instance);
void steng1ax_send_xsensor_oem_event(sns_sensor_instance *const instance,
                                   xsensor_type type,
                                   uint8_t xsensor_idx,
                                   sns_xsensor_event_type event,
                                   sns_time ts,
                                   float *data);


#else
void steng1ax_reconfig_xsensor(sns_sensor_instance *const instance);
sns_rc steng1ax_handle_xsensor_timer_events(sns_sensor_instance *const instance);
#define STENG1AX_IS_XSENSOR_ENABLED(s) 0
#define STENG1AX_IS_XSENSOR_DESIRED(s) 0
#define STENG1AX_IS_XSENSOR(s) (false)
#define STENG1AX_IS_XSENSOR_TIMER_ON(s) false
typedef struct steng1ax_xsensor_group_info {
} steng1ax_xsensor_group_info;

#endif //STENG1AX_ESP_XSENSOR
