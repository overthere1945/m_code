/**
 * @file sns_steng1ax_esp_island_stub.c
 *
 * Common implementation for STENG1AX esp Sensors.
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
#include <string.h>

#include "sns_steng1ax_sensor.h"
#include "sns_types.h"

#if !STENG1AX_ESP_ENABLED

void steng1ax_init_esp_instance(sns_sensor_instance *instance, sns_sensor_state const *sstate_ptr)
{
  UNUSED_VAR(instance);
  UNUSED_VAR(sstate_ptr);
}

void steng1ax_esp_deinit(sns_sensor_instance *instance)
{
  UNUSED_VAR(instance);
}
void steng1ax_store_esp_registry_data(sns_sensor_instance *instance, sns_sensor_state const *this)
{
  UNUSED_VAR(instance);
  UNUSED_VAR(this);
}

void steng1ax_reconfig_esp(sns_sensor_instance *const instance)
{
  UNUSED_VAR(instance);
}

uint16_t steng1ax_get_esp_rate_idx(sns_sensor_instance *instance)
{
  UNUSED_VAR(instance);
  return STENG1AX_ENG_ODR_OFF;
}
sns_rc steng1ax_handle_esp_timer_events(sns_sensor_instance *const instance)
{
  UNUSED_VAR(instance);
  return SNS_RC_SUCCESS;
}

void steng1ax_handle_esp_interrupt(sns_sensor_instance *const instance,
                                 sns_time irq_timestamp,
                                 uint8_t const *wake_src,
                                 uint8_t const *emb_src,
                                 uint8_t hw_id)
{
  UNUSED_VAR(instance);
  UNUSED_VAR(irq_timestamp);
  UNUSED_VAR(wake_src);
  UNUSED_VAR(emb_src);
  UNUSED_VAR(hw_id);
}

void steng1ax_update_esp_sensor_config(sns_sensor *this,
                              sns_sensor_instance *instance,
                              struct steng1ax_instance_config *inst_cfg)
{
  UNUSED_VAR(this);
  UNUSED_VAR(instance);
  UNUSED_VAR(inst_cfg);
}

bool steng1ax_is_esp_request_present(sns_sensor_instance *instance)
{
  UNUSED_VAR(instance);
  return false;
}

void steng1ax_recover_esp(sns_sensor_instance *const instance)
{
  UNUSED_VAR(instance);
}

#endif

