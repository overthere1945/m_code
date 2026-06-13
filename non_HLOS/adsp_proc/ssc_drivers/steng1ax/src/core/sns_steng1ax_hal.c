/**
 * @file sns_steng1ax_hal.c
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
#include "sns_com_port_types.h"
#include "sns_diag_service.h"
#include "sns_math_util.h"
#include "sns_mem_util.h"
#include "sns_rc.h"
#include "sns_service_manager.h"
#include "sns_sync_com_port_service.h"
#include "sns_time.h"
#include "sns_types.h"
#include "sns_sensor_util.h"
#include "sns_steng1ax_hal.h"
#include "sns_steng1ax_sensor.h"
#include "sns_steng1ax_sensor_instance.h"

#include "sns_printf.h"
#include "sns_async_com_port.pb.h"
#include "sns_async_com_port_pb_utils.h"
#include "sns_diag.pb.h"
#include "sns_pb_util.h"
#include "sns_std.pb.h"
/**
 * see sns_steng1ax_hal.h
 */
sns_rc steng1ax_inst_set_dynamic_addr(sns_sensor_instance *const instance, uint8_t hw_id)
{
  sns_rc rv = SNS_RC_SUCCESS;

#if STENG1AX_USE_I3C
  steng1ax_instance_state *inst_state = (steng1ax_instance_state*)instance->state->state;
  steng1ax_com_port_info *com_port = &inst_state->com_port_info[hw_id];
  sns_sync_com_port_service *scp_service = inst_state->scp_service;
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
        DBG_INST_PRINTF_EX(HIGH, instance, "inst_set_dynamic_addr: SETDASA %u is_in_i3c %d",
          com_port->i3c_address, com_port->is_in_i3c);
        scp_service->api->sns_scp_close(i2c_port_handle);
      }
    }
    if( i2c_port_handle != NULL )
    {
      scp_service->api->sns_scp_deregister_com_port(&i2c_port_handle);
    }
  }
#else
  UNUSED_VAR(instance);
#endif
  return rv;
}

sns_rc steng1ax_inst_enter_i3c_mode(sns_sensor_instance *const instance, uint8_t hw_id)
{
  sns_rc                       rv = SNS_RC_SUCCESS;

#if STENG1AX_USE_I3C
  steng1ax_instance_state *inst_state = (steng1ax_instance_state*)instance->state->state;
  steng1ax_com_port_info *com_port = &inst_state->com_port_info[hw_id];
  sns_sync_com_port_service *scp_service = inst_state->scp_service;
  uint32_t xfer_bytes;
  uint8_t buffer[6];

  rv = steng1ax_inst_set_dynamic_addr(instance, hw_id);
  if(rv != SNS_RC_SUCCESS)
    return rv;

  #define SEND_CCC_CMD(ccc_cmd, byte_count) \
    scp_service->api->sns_scp_issue_ccc(com_port->port_handle, (ccc_cmd), buffer, (byte_count), &xfer_bytes)
  if(com_port->com_config.bus_type == SNS_BUS_I3C_SDR ||
     com_port->com_config.bus_type == SNS_BUS_I3C_HDR_DDR )
  {
    /**-------------Set max read size to the size of the FIFO------------------*/
    buffer[0] = (uint8_t)((STM_STENG1AX_MAX_FIFO_SIZE >> 8) & 0xFF);
    buffer[1] = (uint8_t)(STM_STENG1AX_MAX_FIFO_SIZE & 0xFF);
    buffer[2] = 4;
    rv = SEND_CCC_CMD(SNS_SYNC_COM_PORT_CCC_SETMRL, 3);
    if( rv != SNS_RC_SUCCESS ) {
      SNS_INST_PRINTF(ERROR, instance, "Set max read length failed!");
    }

    /**-------------------Enable/Disable IBI------------------------*/
    if( inst_state->irq_info[hw_id].is_ibi )
    {
      buffer[0] = 0x1;
      rv = SEND_CCC_CMD(SNS_SYNC_COM_PORT_CCC_ENEC, 1);
      if( rv == SNS_RC_SUCCESS ) {
        DBG_INST_PRINTF_EX(HIGH, instance, "IBI enabled");
      } else {
        SNS_INST_PRINTF(ERROR, instance, "IBI enable FAILED!");
      }
    }
    else
    {
      buffer[0] = 0x1;
      rv = SEND_CCC_CMD(SNS_SYNC_COM_PORT_CCC_DISEC, 1);
      if( rv == SNS_RC_SUCCESS ) {
        DBG_INST_PRINTF_EX(HIGH, instance, "IBI disabled");
      } else {
        SNS_INST_PRINTF(ERROR, instance, "IBI disable FAILED!");
      }
    }
  }
#if STENG1AX_DEBUG_I3C
  /**-------------------Debug -- read all CCC info------------------------*/
  sns_memset(buffer, 0, sizeof(buffer));
  rv = SEND_CCC_CMD(SNS_SYNC_COM_PORT_CCC_GETMWL, 2);
  if( rv == SNS_RC_SUCCESS ) {
    DBG_INST_PRINTF_EX(LOW, instance, "max write length:0x%02x%02x", buffer[0], buffer[1]);
  } else {
    DBG_INST_PRINTF(ERROR, instance, "Get max write length failed!");
  }
  rv = SEND_CCC_CMD(SNS_SYNC_COM_PORT_CCC_SETMWL, 2);
  if( rv != SNS_RC_SUCCESS ) {
    DBG_INST_PRINTF(ERROR, instance, "Set max write length failed!");
  }

  sns_memset(buffer, 0, sizeof(buffer));
  rv = SEND_CCC_CMD(SNS_SYNC_COM_PORT_CCC_GETMRL, 3);
  if( rv == SNS_RC_SUCCESS ) {
    DBG_INST_PRINTF_EX(LOW, instance, "max read length:0x%02x%02x%02x",
                       buffer[0], buffer[1], buffer[2]);
  } else {
    DBG_INST_PRINTF(ERROR, instance, "Get max read length failed!");
  }

  sns_memset(buffer, 0, sizeof(buffer));
  rv = SEND_CCC_CMD(SNS_SYNC_COM_PORT_CCC_GETPID, 6);
  if( rv == SNS_RC_SUCCESS ) {
    DBG_INST_PRINTF(LOW, instance, "PID:0x%02x%02x:%02x%02x:%02x%02x",
                    buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
  } else {
    DBG_INST_PRINTF(ERROR, instance, "Get PID failed!");
  }

  sns_memset(buffer, 0, sizeof(buffer));
  rv = SEND_CCC_CMD(SNS_SYNC_COM_PORT_CCC_GETBCR, 1);
  if( rv == SNS_RC_SUCCESS ) {
    DBG_INST_PRINTF_EX(LOW, instance, "bus charactaristics register:0x%x", buffer[0]);
  } else {
    DBG_INST_PRINTF(ERROR, instance, "Get BCR failed!");
  }

  sns_memset(buffer, 0, sizeof(buffer));
  rv = SEND_CCC_CMD(SNS_SYNC_COM_PORT_CCC_GETDCR, 1);
  if( rv == SNS_RC_SUCCESS ) {
    DBG_INST_PRINTF_EX(LOW, instance, "device charactaristics register:0x%x", buffer[0]);
  } else {
    DBG_INST_PRINTF(ERROR, instance, "Get DCR failed!");
  }

  sns_memset(buffer, 0, sizeof(buffer));
  rv = SEND_CCC_CMD(SNS_SYNC_COM_PORT_CCC_GETSTATUS, 2);
  if( rv == SNS_RC_SUCCESS ) {
    uint32_t status_reg =  buffer[0] | buffer[1] << 8;
    DBG_INST_PRINTF_EX(LOW, instance, "status register:0x%x", status_reg);
  } else {
    DBG_INST_PRINTF(ERROR, instance, "Get status failed!");
  }

  sns_memset(buffer, 0, sizeof(buffer));
  rv = SEND_CCC_CMD(SNS_SYNC_COM_PORT_CCC_GETMXDS, 2);
  if( rv == SNS_RC_SUCCESS && xfer_bytes == 2) {
    DBG_INST_PRINTF_EX(LOW, instance, "MXDS :0x%02x%02x", buffer[0], buffer[1]);
  } else {
    DBG_INST_PRINTF(ERROR, instance, "Get MXDS failed! rv:%d xfer_bytes:%d", rv, xfer_bytes);
  }
#endif /* STENG1AX_DEBUG_I3C */
#else
  UNUSED_VAR(instance);
#endif

  return rv;
}

sns_rc steng1ax_exit_i3c_mode(
  steng1ax_com_port_info     *com_port,
  sns_sync_com_port_service *scp_service)
{
  sns_rc rc = SNS_RC_SUCCESS;

#if STENG1AX_USE_I3C && STENG1AX_USE_RSTDAA
  if((com_port->com_config.bus_type == SNS_BUS_I3C_SDR ||
      com_port->com_config.bus_type == SNS_BUS_I3C_HDR_DDR) && com_port->is_in_i3c)
  {
    uint32_t xfer_bytes;
    uint8_t buffer = 0x1;
    (void)scp_service->api->sns_scp_issue_ccc(com_port->port_handle,
                                              SNS_SYNC_COM_PORT_CCC_DISEC,
                                              &buffer, 1, &xfer_bytes);
    buffer = 0x00;
    rc = steng1ax_com_write_wrapper_scp(scp_service,
                          com_port->port_handle,
                          STM_STENG1AX_REG_I3C_IF_CTRL,
                          &buffer,
                          1,
                          &xfer_bytes);

    if(SNS_RC_SUCCESS == rc)
    {
      com_port->is_in_i3c = false;
    }
  }
#else
  UNUSED_VAR(com_port);
  UNUSED_VAR(scp_service);
#endif

  return rc;
}

/**
 * see sns_steng1ax_hal.h
 */
sns_rc steng1ax_device_sw_reset(
    sns_sensor_instance *const instance,
    steng1ax_sensor_type sensor,
    uint8_t hw_id)
{
  UNUSED_VAR(sensor);
  uint8_t buffer = 0x20;
  int8_t num_attempts = 5;
  sns_rc rc = SNS_RC_FAILED;

  DBG_INST_PRINTF_EX(LOW, instance, "sw_rst");

  while(num_attempts-- > 0 && SNS_RC_SUCCESS != rc)
  {
    rc = steng1ax_write_regs_scp(instance, STM_STENG1AX_REG_CTRL1, 1, &buffer, hw_id);
    if(SNS_RC_SUCCESS != rc)
    {
      DBG_INST_PRINTF(ERROR, instance, "sw_rst: failed; wait and try again");
      sns_busy_wait(sns_convert_ns_to_ticks(100*1000));
    }
  }

  if(num_attempts <= 0)
  {
    DBG_INST_PRINTF(ERROR, instance, "sw_rst: failed all attempts");
  }

  num_attempts = 10;
  do
  {
    if(num_attempts-- <= 0)
    {
      DBG_INST_PRINTF(ERROR, instance, "sw_rst: failed due to timeout- attempts:%d", 10-num_attempts);
      // Sensor HW has not recovered from SW reset.
      return SNS_RC_FAILED;
    }
    else
    {
      //0.1ms wait
      sns_busy_wait(sns_convert_ns_to_ticks(100*1000));
      steng1ax_read_regs_scp(instance, STM_STENG1AX_REG_CTRL1, 1, &buffer, hw_id);
    }

  } while((buffer & 0x20));

  DBG_INST_PRINTF(HIGH, instance, "sw_rst: success in %d attempts", 10-num_attempts);
  steng1ax_inst_enter_i3c_mode(instance, hw_id);
  return SNS_RC_SUCCESS;
}

void steng1ax_start_sensor_config_timer(
  sns_sensor_instance *const instance)
{
steng1ax_instance_state *state =
   (steng1ax_instance_state*)instance->state->state;
sns_timer_sensor_config req_payload = sns_timer_sensor_config_init_default;
req_payload.is_periodic = false;
req_payload.start_time = sns_get_system_time();
#if STENG1AX_SUPPORT_NEW_DISABLE_RESPONSE
req_payload.has_disable_response = true;
req_payload.disable_response = true;
#endif
//timeout in sec, convert to ticks
req_payload.timeout_period = sns_convert_ns_to_ticks(STENG1AX_CONFIG_TIMER_MS * 1000 * 1000);

DBG_INST_PRINTF_EX(LOW, instance, "create config timer = odr=%u to=%u", 
                  (uint32_t)STENG1AX_CONFIG_TIMER_MS,
                  (uint32_t)req_payload.timeout_period);

steng1ax_inst_create_timer(instance, &state->timer_config_data_stream, &req_payload);
}

/**
 * see sns_steng1ax_hal.h
 */
static sns_rc steng1ax_device_set_default_state(
    sns_sensor_instance *const instance,
    steng1ax_sensor_type sensor,
    uint8_t hw_id)
{
  uint8_t buffer[1];
  sns_rc rv = SNS_RC_SUCCESS;
  uint32_t xfer_bytes;
  steng1ax_instance_state *state =
    (steng1ax_instance_state*)instance->state->state;
  steng1ax_com_port_info *com_port = &state->com_port_info[hw_id];

  DBG_INST_PRINTF_EX(LOW, instance, "set_default_state: sensor=0x%x", sensor);

  if(sensor == (STENG1AX_ENG))
  {
    if( com_port->com_config.bus_type == SNS_BUS_I3C_SDR || //TODO
        com_port->com_config.bus_type == SNS_BUS_I3C_HDR_DDR )
    {
      buffer[0] = 0x00;
      rv = steng1ax_read_modify_write(instance,
                            STM_STENG1AX_REG_I3C_IF_CTRL,
                            &buffer[0],
                            1,
                            &xfer_bytes,
                            false,
                            0x80,
                            hw_id);
       if(rv != SNS_RC_SUCCESS || xfer_bytes != 1)
       {
          return SNS_RC_FAILED;
       }
    }
#if STENG1AX_EXT_CLK_ENABLE
     buffer[0] = 0x80; // EXT_CLK_EN
     rv = steng1ax_read_modify_write(
            instance,
            STM_STENG1AX_REG_EXT_CLK_CFG,
            &buffer[0],
            1,
            &xfer_bytes,
            false,
            0x80,
            hw_id);
     if(rv != SNS_RC_SUCCESS || xfer_bytes != 1)
     {
        return SNS_RC_FAILED;
     }

     buffer[0] = 0x10; // IF_ADD_INC
#else
     buffer[0] = 0x50; // INT_PIN_EN | IF_ADD_INC
#endif
     rv = steng1ax_read_modify_write(
            instance,
            STM_STENG1AX_REG_CTRL1,
            &buffer[0],
            1,
            &xfer_bytes,
            false,
            0x50,
            hw_id);
     if(rv != SNS_RC_SUCCESS || xfer_bytes != 1)
     {
        return SNS_RC_FAILED;
     }

#if !STENG1AX_DRDY_OUT_ENABLED
     buffer[0] = 0x08; // FIFO_EN
     rv = steng1ax_read_modify_write(
            instance,
            STM_STENG1AX_REG_CTRL4,
            &buffer[0],
            1,
            &xfer_bytes,
            false,
            0x08,
            hw_id);
     if(rv != SNS_RC_SUCCESS || xfer_bytes != 1)
     {
        return SNS_RC_FAILED;
     }
#endif

     rv = steng1ax_set_eng_config(instance, hw_id);
     if(rv != SNS_RC_SUCCESS || xfer_bytes != 1)
     {
        return SNS_RC_FAILED;
     }

#if STENG1AX_USE_I3C
     DBG_INST_PRINTF_EX(LOW, instance, "set_default_state: bus_type=%u is_ibi=%d",
       state->com_port_info[hw_id].com_config.bus_type, state->irq_info[hw_id].is_ibi);
#endif

     steng1ax_set_odr_config(instance,
                            STENG1AX_ENG_ODR_OFF,
                            state->eng_info.sstvt,
                            STENG1AX_ODR_BW_HALF,
                            hw_id);
  }

  return SNS_RC_SUCCESS;
}

/**
 * see sns_steng1ax_hal.h
 */
sns_rc steng1ax_reset_device(
    sns_sensor_instance *const instance,
    steng1ax_sensor_type sensor)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)instance->state->state;
  sns_rc rv = SNS_RC_SUCCESS;
  int i =0;
  for (i=0; i < state->multi_eng_cfg.num_sensors_enable; i++)
  {
    DBG_INST_PRINTF_EX(HIGH, instance, "[%d] reset_device", i);
    /** HW reset only when Eng is requested for
     *  reset. */
    if( sensor == (STENG1AX_ENG))
    {
       rv = steng1ax_device_sw_reset(instance, sensor, i);
    }
    else
    {
      steng1ax_inst_enter_i3c_mode(instance, i);
    }

    if(rv == SNS_RC_SUCCESS)
    {
      rv = steng1ax_device_set_default_state(instance, sensor, i);
      steng1ax_device_set_esp_default_state(instance, i);
    }

    if(rv != SNS_RC_SUCCESS)
    {
      DBG_INST_PRINTF(ERROR, instance, "reset_device failed!");
    }
  }
  return rv;
}

sns_rc steng1ax_recover_device(sns_sensor_instance *const this, uint8_t hw_id)
{
  steng1ax_instance_state *state = (steng1ax_instance_state*)this->state->state;
  sns_rc rv = SNS_RC_SUCCESS;

  struct group_read {
    uint32_t first_reg;
    uint8_t  num_regs;
  } groups[] = { /* must fit within state->reg_status[] */
    {STM_STENG1AX_REG_EXT_CLK_CFG, 1},
    { STM_STENG1AX_REG_PIN_CTRL, 1 },
    { STM_STENG1AX_REG_CTRL1, 8 },
    { STM_STENG1AX_REG_MD1_CFG, 1 },
    { STM_STENG1AX_REG_AH_ENG_CFG1, 4},
    { STM_STENG1AX_REG_EN_DEVICE_CONFIG, 1},
    { STM_STENG1AX_REG_FIFO_BATCH_DEC, 1 }
  };

  //Save Context
  {
    uint8_t *dest = state->reg_status;

    for(uint32_t i=0; i<ARR_SIZE(groups); i++)
    {
      steng1ax_read_regs_scp(this, groups[i].first_reg, groups[i].num_regs, dest, hw_id);
#if 1 //STENG1AX_DUMP_REG
      for(uint32_t j=0; j<groups[i].num_regs; j++)
      {
        DBG_INST_PRINTF(LOW, this, "dump: 0x%02x=0x%02x",
                        groups[i].first_reg+j, dest[j]);
      }
#endif
      dest += groups[i].num_regs;
    }
    DBG_INST_PRINTF(MED, this, "Context saved");
  }

  //disable IBI and exit I3C.
  steng1ax_exit_i3c_mode(&state->com_port_info[hw_id], state->scp_service);
  state->com_port_info[hw_id].is_in_i3c = false;

  // Reset Sensor
  rv = steng1ax_reset_device(this,
      STENG1AX_ENG);

  //Recover ESP and XSESNOR after device reset
  steng1ax_recover_esp(this);

  //Power up Eng if needed. It was powered down during reset_device
  steng1ax_set_odr_config(this,
      state->eng_info.desired_odr_reg_val,
      state->eng_info.sstvt,
      state->eng_info.bw,
      hw_id);

  //Restore context
  {
    uint8_t *src = state->reg_status;

    for(uint32_t i=0; i<ARR_SIZE(groups); i++)
    {
      steng1ax_write_regs_scp(this, groups[i].first_reg, groups[i].num_regs, src, hw_id);
      src += groups[i].num_regs;
    }
    DBG_INST_PRINTF(MED, this, "Context restored");
  }

  return rv;
}

/**
 * see sns_steng1ax_hal.h
 */
sns_rc steng1ax_get_who_am_i(sns_sync_com_port_service *scp_service,
                            sns_sync_com_port_handle *port_handle,
                            uint8_t *buffer)
{
  sns_rc rv = SNS_RC_SUCCESS;
  uint32_t xfer_bytes;

  rv = steng1ax_com_read_wrapper(scp_service,
                                port_handle,
                                STM_STENG1AX_REG_WHO_AM_I,
                                buffer,
                                1,
                                &xfer_bytes);

  if(rv != SNS_RC_SUCCESS
     ||
     xfer_bytes != 1)
  {
    rv = SNS_RC_FAILED;
  }

  return rv;
}


