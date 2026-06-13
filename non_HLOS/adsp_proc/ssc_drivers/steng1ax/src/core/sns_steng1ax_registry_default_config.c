/**
 * @file sns_steng1ax_registry_default_config.c
 *
 * Set default registry configuration in the absence of registry support.
 *
 * Copyright (c) 2020, STMicroelectronics.
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
#include "sns_steng1ax_sensor.h"
#include "sns_steng1ax_sensor_instance.h"
#include "sns_com_port_types.h"
#include "sns_types.h"
#include "sns_mem_util.h"


#if STENG1AX_REGISTRY_DISABLED
#include "sns_interface_defs.h"

#define IRQ_NUM                    _STENG1AX_IRQ_NUM_0
#define IRQ_NUM_1                  _STENG1AX_IRQ_NUM_1
#define IRQ_NUM_2                  _STENG1AX_IRQ_NUM_2
#define IRQ_NUM_3                  _STENG1AX_IRQ_NUM_3
#define BUS_INSTANCE               _STENG1AX_BUS_INSTANCE
#define BUS_INSTANCE_1             _STENG1AX_BUS_INSTANCE_1
#define BUS                        _STENG1AX_BUS_TYPE
#define BUS_MIN_FREQ_KHZ           _STENG1AX_BUS_FREQ_MIN
#define BUS_MAX_FREQ_KHZ           _STENG1AX_BUS_FREQ_MAX
#define SLAVE_CONTROL              _STENG1AX_I2C_ADDR_0
#define SLAVE_CONTROL_1            _STENG1AX_I2C_ADDR_1
#define RAIL_1                     _STENG1AX_RAIL_1
#define NUM_OF_RAILS               _STENG1AX_NUM_RAILS
#define I3C_ADDR                   _STENG1AX_I3C_ADDR_0
#define I3C_ADDR_1                 _STENG1AX_I3C_ADDR_1
#define I3C_ADDR_2                 _STENG1AX_I3C_ADDR_2
#define I3C_ADDR_3                 _STENG1AX_I3C_ADDR_3
#define RIGID_BODY_TYPE            SNS_STD_SENSOR_RIGID_BODY_TYPE_DISPLAY
#define RIGID_BODY_TYPE_1          SNS_STD_SENSOR_RIGID_BODY_TYPE_KEYBOARD

#define STENG1AX_DEFAULT_REG_CFG_RAIL_ON            SNS_RAIL_ON_NPM
#define STENG1AX_DEFAULT_REG_CFG_ISDRI              _STENG1AX_IRQ_MODE
#define STENG1AX_DEFAULT_REG_ENG_RESOLUTION_IDX   0
#define STENG1AX_DEFAULT_REG_CFG_SUPPORT_SYN_STREAM 0

/**
 * Sensor platform resource configuration with hrad coded values
 * @param this -- pointer to sensor
 * @param cfg -- pointer to cfg structure which will be filled in
 *               Caller should pass this to sensor_save_registry_pf_cfg
 */
void sns_steng1ax_registry_def_config(sns_sensor *const this,
                                     sns_registry_phy_sensor_pf_cfg *cfg)
{
  steng1ax_state *state = (steng1ax_state*)this->state->state;
  steng1ax_shared_state *shared_state = steng1ax_get_shared_state(this);

  sns_registry_phy_sensor_pf_cfg def_cfg = {
    .slave_config =       SLAVE_CONTROL,
    .min_bus_speed_khz =  BUS_MIN_FREQ_KHZ,
    .max_bus_speed_khz =  BUS_MAX_FREQ_KHZ,
    .dri_irq_num =        IRQ_NUM,
#if STENG1AX_ODR_REGISTRY_FEATURE_ENABLE
    .max_odr =            500,
    .min_odr =            20,
#endif
    .bus_type =           BUS,
    .bus_instance =       BUS_INSTANCE,
    .reg_addr_type =      SNS_REG_ADDR_8_BIT,
    .irq_pull_type =      2,
    .irq_drive_strength = 0,
    .irq_trigger_type =   1,
    .num_rail =           NUM_OF_RAILS,
    .rail_on_state =      STENG1AX_DEFAULT_REG_CFG_RAIL_ON,
    .rigid_body_type =    RIGID_BODY_TYPE,

#if STENG1AX_USE_I3C
    .i3c_address =        I3C_ADDR,
#endif
    .irq_is_chip_pin =    1,
    .vddio_rail = RAIL_1,
    .vdd_rail = "",
  };

  if( shared_state->hw_idx == 1 )
  {

    def_cfg.slave_config    = SLAVE_CONTROL_1;
    def_cfg.dri_irq_num     = IRQ_NUM_1;
    def_cfg.rigid_body_type = RIGID_BODY_TYPE_1;
    def_cfg.i3c_address     = I3C_ADDR_1;
  }
  if( shared_state->hw_idx == 2 )
  {

    def_cfg.bus_instance    = BUS_INSTANCE_1;
    def_cfg.slave_config    = SLAVE_CONTROL;
    def_cfg.dri_irq_num     = IRQ_NUM_2;
    def_cfg.rigid_body_type = RIGID_BODY_TYPE_1;
    def_cfg.i3c_address     = I3C_ADDR_2;
  }
  if( shared_state->hw_idx == 3 )
  {

    def_cfg.bus_instance    = BUS_INSTANCE_1;
    def_cfg.slave_config    = SLAVE_CONTROL_1;
    def_cfg.dri_irq_num     = IRQ_NUM_3;
    def_cfg.rigid_body_type = RIGID_BODY_TYPE_1;
    def_cfg.i3c_address     = I3C_ADDR_3;
  }

  *cfg = def_cfg;

  state->is_dri               = STENG1AX_DEFAULT_REG_CFG_ISDRI;
  if( state->is_dri == 2 )
  {
    shared_state->inst_cfg.irq_config[shared_state->hw_idx].is_ibi = true;
  }

  state->hardware_id          = shared_state->hw_idx;
  state->supports_sync_stream = STENG1AX_DEFAULT_REG_CFG_SUPPORT_SYN_STREAM;
  shared_state->inst_cfg.eng_resolution_idx = STENG1AX_DEFAULT_REG_ENG_RESOLUTION_IDX;

  shared_state->inst_cfg.eng_stream_mode = DRI;
  steng1ax_init_inst_config(this, shared_state);
}
#endif //STENG1AX_REGISTRY_DISABLED
