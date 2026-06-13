#pragma once
/**
 * @file sns_steng1ax_dae_if.h
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

#include <stdint.h>
#include "sns_dae.pb.h"
#include "sns_sensor_instance.h"
#include "sns_data_stream.h"
#include "sns_stream_service.h"
#include "sns_time.h"

//DAE Error event macros
#if STENG1AX_DAE_ERROR_HANDLING_ENABLED 

#define MAX_HEART_ATTACKS 6
#define MAX_DAE_ERROR_COUNT MAX_HEART_ATTACKS

#define SNS_DRIVER_ERROR_TYPE_BYTES_GREATER_THAN_FIFO_SIZE 0x01
#define SNS_DRIVER_ERROR_TYPE_REGISTER_READ 0x02
#define MAX_DRIVER_ERROR_EVENTS  2

#define MAX_FRAMEWORK_ERROR_EVENTS _sns_dae_framework_error_type_MAX

#define MAX_DAE_ERROR_EVENTS MAX_DRIVER_ERROR_EVENTS+MAX_FRAMEWORK_ERROR_EVENTS

#endif

struct sns_stream_service;
struct sns_data_stream;
struct steng1ax_instance_state;
struct steng1ax_instance_config;

typedef enum
{
  PRE_INIT,
  INIT_PENDING,
  UNAVAILABLE,
  IDLE,
  STREAM_STARTING,
  STREAMING,
  STREAM_STOPPING,

} steng1ax_dae_if_state;

typedef struct
{
  struct sns_data_stream *stream;
  const char             *nano_hal_vtable_name;
  uint32_t               dae_wm;
  uint8_t                status_bytes_per_fifo;
  steng1ax_dae_if_state   state;
  bool                   stream_usable:1;
  bool                   flushing_hw:1;
  bool                   flushing_data:1;
} steng1ax_dae_stream;

typedef struct steng1ax_dae_if_info
{
  steng1ax_dae_stream   ag;
  steng1ax_dae_stream   temp;
} steng1ax_dae_if_info;

typedef struct steng1ax_dae_error
{
  sns_time    last_error_ts;    //Last error timestamp
  uint8_t     error_event_cnt;  //Error event count
} steng1ax_dae_error;

// for use by master sensor
bool steng1ax_dae_if_support_known(sns_sensor *this);
void steng1ax_dae_if_check_support(sns_sensor *this);
void steng1ax_dae_if_process_sensor_events(sns_sensor *this);

sns_rc steng1ax_dae_if_init(
  sns_sensor_instance                  *const this,
  struct sns_stream_service            *stream_mgr,
  struct steng1ax_instance_config const *);

void steng1ax_dae_if_deinit(sns_sensor_instance *const this);

bool steng1ax_dae_if_stop_streaming(sns_sensor_instance *this, uint8_t sensors);

bool steng1ax_dae_if_start_streaming(sns_sensor_instance *this);


void steng1ax_dae_if_process_events(sns_sensor_instance *this);

void steng1ax_set_status_dae_error_event(sns_sensor_instance *this, uint8_t error_count);

void steng1ax_reset_dae_error_event_count(sns_sensor_instance *this);

bool util_last_error_over_60secs(sns_time last_error_ts, sns_time now);

void process_dae_error_event(
    sns_sensor_instance *this, 
    steng1ax_dae_stream  *dae_stream, 
    pb_istream_t        *pbstream);

// for use by instance
bool steng1ax_dae_if_available(sns_sensor_instance *this);

bool steng1ax_dae_if_flush_hw(sns_sensor_instance *this);

void steng1ax_dae_if_build_static_config_request(
    struct steng1ax_instance_config const *inst_cfg,
    sns_dae_set_static_config      *config_req,
    uint8_t                        hw_idx,
    uint8_t                        rigid_body_type,
    bool                           for_ag);

sns_rc steng1ax_dae_if_send_static_config_request(
    sns_data_stream           *stream,
    sns_dae_set_static_config *config_req);
bool steng1ax_dae_if_flush_samples(sns_sensor_instance *this);

