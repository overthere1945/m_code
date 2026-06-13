#pragma once
/**
 * @file sns_steng1ax_fsm.h
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
#include "sns_sensor_instance.h"

#define STENG1AX_REG_FSM_ENABLE_ADDR      0x1A
#define STENG1AX_REG_FSM_INT1             0x0b
#define STENG1AX_FSM_INIT_MASK            0x01
#define STENG1AX_REG_FSM_INIT_B           0x2D
#define STENG1AX_REG_FSM_LC_STATUS        0x35
#define STENG1AX_REG_FSM_OUTS1            0x20
#define STENG1AX_REG_FSM_LC               0x1C

#if STENG1AX_FSM_ENABLED

void steng1ax_init_fsm_instance(sns_sensor_instance *instance);

void steng1ax_fsm_deinit(sns_sensor_instance *instance);

void steng1ax_device_set_fsm_default_state(sns_sensor_instance *instance, uint8_t hw_id);

sns_rc steng1ax_fsm_set_enable(sns_sensor_instance *instance, uint16_t sensor,
                                        xsensor_int int_line, bool enable, uint8_t hw_id);

void steng1ax_fsm_get_interrupt_status(sns_sensor_instance *instance, uint8_t const *wake_src, uint8_t const *emb_src, uint8_t hw_id);

bool steng1ax_check_fsm_sensor_interrupt(sns_sensor_instance *const instance, uint16_t idx);
#endif
