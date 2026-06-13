#pragma once
/**
 * @file sns_steng1ax_hal.h
 *
 * Hardware Access Layer functions.
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

#include <stdint.h>

#include "sns_dae.pb.h"
#include "sns_diag.pb.h"
#include "sns_diag_service.h"
#include "sns_steng1ax_sensor_instance.h"
#include "sns_sensor.h"
#include "sns_sensor_uid.h"

///*
// *  Address registers

#define STM_STENG1AX_REG_CNT_BDR1          (0x0B)
#define STM_STENG1AX_REG_TAP_CFG0          (0x56)
#define STM_STENG1AX_REG_TAP_CFG2          (0x58)
#define STM_STENG1AX_REG_TAP_THS_6D        (0x59)
#define STM_STENG1AX_REG_TAP_DUR           (0x5A)


//new and updated
#define STM_STENG1AX_REG_EXT_CLK_CFG       (0x08)
#define STM_STENG1AX_REG_PIN_CTRL          (0x0C)
#define STM_STENG1AX_REG_WHO_AM_I          (0x0F)
#define STM_STENG1AX_REG_CTRL1             (0x10)
#define STM_STENG1AX_REG_CTRL2             (0x11)
#define STM_STENG1AX_REG_CTRL3             (0x12)
#define STM_STENG1AX_REG_CTRL4             (0x13)
#define STM_STENG1AX_REG_CTRL5             (0x14)
#define STM_STENG1AX_REG_FIFO_CTRL         (0x15)
#define STM_STENG1AX_REG_FIFO_WTM          (0x16)
#define STM_STENG1AX_REG_INTERRUPT_CFG     (0x17)
#define STM_STENG1AX_REG_MD1_CFG           (0x1F)
#define STM_STENG1AX_REG_WAKE_SRC          (0x21) //
#define STM_STENG1AX_REG_STATUS            (0x25)
#define STM_STENG1AX_REG_FIFO_STATUS1      (0x26)
#define STM_STENG1AX_REG_FIFO_STATUS2      (0x27)
#define STM_STENG1AX_REG_OUT_AH_ENG_L      (0x2E)
#define STM_STENG1AX_REG_OUT_AH_ENG_H      (0x2F)
#define STM_STENG1AX_REG_AH_ENG_CFG1       (0x30)
#define STM_STENG1AX_REG_AH_ENG_CFG2       (0x31)
#define STM_STENG1AX_REG_AH_ENG_CFG3       (0x32)
#define STM_STENG1AX_REG_I3C_IF_CTRL       (0x33)
#define STM_STENG1AX_REG_EN_DEVICE_CONFIG  (0x3E)
#define STM_STENG1AX_REG_FUNC_CFG          (0x3F)
#define STM_STENG1AX_REG_FIFO_OUT_TAG      (0x40)
#define STM_STENG1AX_REG_FIFO_BATCH_DEC    (0x47)
#define STM_STENG1AX_REG_TIMESTAMP0        (0x7A)
#define STM_STENG1AX_REG_TIMESTAMP1        (0x7B)
#define STM_STENG1AX_REG_TIMESTAMP2        (0x7C)
#define STM_STENG1AX_REG_TIMESTAMP3        (0x7D)

#define STENG1AX_REG_FSM_STATUS_MAINPAGE  (0x35)


#define STENG1AX_FIFO_STREAM_MODE           (0x06) // fifo stream continuous mode
#define STM_STENG1AX_FIFO_TH_MASK           (0x20)
#define STM_STENG1AX_FIFO_OVR_MASK          (0x10)
#define STM_STENG1AX_FIFO_DADY_MASK         (0x08)
//

// *  Embedded functions registers

#define STM_STENG1AX_EMB_FUNC_EN_A         (0x04)
#define STM_STENG1AX_EMB_FUNC_INT1         (0x0A)
#define STM_STENG1AX_PAGE_RW               (0x17)
#define STM_STENG1AX_STEP_COUNTER_L        (0x62)
#define STM_STENG1AX_EMB_FUNC_INIT_A       (0x66)

#define STM_STENG1AX_FIFO_MODE_MASK        (0x07)
#define STM_STENG1AX_FIFO_EN_ADV_MASK      (0x10)
#define STM_STENG1AX_FIFO_STATUS1_WTM_INT  (0x80)
#define STM_STENG1AX_SAMPLE_SIZE            6
#define STM_STENG1AX_TAG_SIZE               1
#define STM_STENG1AX_FIFO_SAMPLE_SIZE   (STM_STENG1AX_SAMPLE_SIZE + \
                     STM_STENG1AX_TAG_SIZE)

#define STM_STENG1AX_MAX_FIFO_SIZE  3584

// Embedded Registers
#define STM_STENG1AX_REG_EMB_FUNC_EN_B_ADDR  0x05
#define STM_STENG1AX_PAGE_RW_ADDR            0x17
#define STM_STENG1AX_REG_EMB_FUNC_LIR_MASK   0x80
#define STM_STENG1AX_REG_EMB_FUNC_INT1_ADDR  0x0a

#define STM_STENG1AX_REG_EMB_FUNC_EN_MASK    0x10
//*/

/** Default values loaded in probe function */
#define STENG1AX_WHOAMI_VALUE              (0x48)  /** Who Am I default value */

/** fifo paramters */
#define STENG1AX_HW_MAX_FIFO         127  // 0x7F, limited by the 7-bit wide value
#define STENG1AX_MAX_FIFO            100  // smaller fifo fits better in DAE buffer

#define STENG1AX_HW_MAX_ODR          (STENG1AX_ODR_3200) //MAX Supported odr

#define STENG1AX_PENDING_SOFT_PD     25  //ms

/** Off to idle time */
#define STENG1AX_OFF_TO_IDLE_MS      10  //ms

#define STENG1AX_CONFIG_TIMER_MS     10  //ms

#define STENG1AX_NUM_DATA           4

/**Current values*/
#define STENG1AX_ENG_ACTIVE_CURRENT          50   //uA
#define STENG1AX_ENG_SLEEP_CURRENT           2    //2.1 uA

/******************* Function Declarations ***********************************/

/**
 * Resets the Sensor HW. Also calls
 * steng1ax_device_set_default_state()
 *
 * @param[i] port_handle   handle to synch COM port
 * @param[i] sensor        bit mask for sensors to reset
 *
 * @return sns_rc
 * SNS_RC_FAILED - COM port failure
 * SNS_RC_SUCCESS
 */
sns_rc steng1ax_reset_device(
    sns_sensor_instance *const instance,
    steng1ax_sensor_type sensor);

/**
 * Puts FIFO in bypass mode.
 *
 * @param[i] state         Instance state
 *
 * @return none
 */
void steng1ax_set_fifo_bypass_mode(sns_sensor_instance *this, uint8_t hw_id);

/**
 * Puts FIFO in stream mode.
 *
 * @param[i] state         Instance state
 *
 * @return none
 */
void steng1ax_set_fifo_stream_mode(sns_sensor_instance *this, uint8_t hw_id);

/**
 * Disables FIFO ODR. Also disables ODR for sensors with
 * non-zero ODR.
 *
 * @param[i] state         Instance state
 *
 * @return none
 */
void steng1ax_stop_fifo_streaming(sns_sensor_instance *const instance, uint8_t hw_id);

/**
 * Sets FIFO WM and decimation config registers.
 *
 * @param[i] state         Instance state
 *
 * @return none
 */
void steng1ax_set_fifo_wmk(sns_sensor_instance *const instance, uint8_t hw_id);

/**
 * Provides sample interval based on current ODR.
 *
 * @param[i] curr_odr              Current FIFO ODR.
 *
 * @return sampling interval time in ticks
 */
sns_time steng1ax_get_sample_interval(sns_sensor_instance *const this, steng1ax_eng_odr curr_odr);

/**
 * Enable FIFO streaming. Also enables FIFO sensors with
 * non-zero desired ODR.
 *
 * @param[i] state         Instance state
 *
 * @return none
 */
void steng1ax_start_fifo_streaming(sns_sensor_instance *const instance, uint8_t hw_id);

/**
 * Gets Who-Am-I register for the sensor.
 *
 * @param[i] state         Instance state
 * @param[o] buffer        who am I value read from HW
 *
 * @return sns_rc
 * SNS_RC_FAILED - COM port failure
 * SNS_RC_SUCCESS
 */
sns_rc steng1ax_get_who_am_i(sns_sync_com_port_service *scp_service,
                            sns_sync_com_port_handle  *port_handle,
                            uint8_t *buffer);

/**
 * Populates Instance state with desired FIFO configuration.
 *
 * @param[i] state                 Instance state
 * @param[i] desired_wmk           desired FIFO WM
 * @param[i] a_chosen_sample_rate  desired Eng ODR
 * @param[i] sensor                bit mask of Sensors to enable
 *
 * @return none
 */
void steng1ax_set_fifo_config(sns_sensor_instance *const instance,
                             uint16_t desired_wmk,
                             steng1ax_eng_odr a_chosen_sample_rate,
                             steng1ax_sensor_type sensor);

/**
 * Enables interrupt for FIFO sensors.
 *
 * @param[i] state         Instance state
 * @param[i] sensors       sensor bit mask to enable
 *
 * @return none
 */
void steng1ax_enable_fifo_intr(sns_sensor_instance *const instance, uint8_t hw_id);
/**
 * Disables interrupt for FIFO sensors.
 *
 * @param[i] state         Instance state
 *
 * @return none
 */
void steng1ax_disable_fifo_intr(sns_sensor_instance *const instance, uint8_t hw_id);


void steng1ax_turn_off_fifo(sns_sensor_instance *this, uint8_t hw_id);

/**
 * Sets AH/ENG configuration.
 *
 * @param[i] state                 Instance state
 *
 * @return sns_rc
 * SNS_RC_FAILED - COM port failure
 * SNS_RC_SUCCESS
 */
sns_rc steng1ax_set_eng_config(sns_sensor_instance *const instance, uint8_t hw_id);

/**
 * Sets Eng ODR, range, BW and sensitivity.
 *
 * @param[i] port_handle     handle to synch COM port
 * @param[i] curr_odr        Eng ODR
 * @param[i] sstvt           Eng sensitivity
 * @param[i] bw              Eng BW
 *
 * @return sns_rc
 * SNS_RC_FAILED - COM port failure
 * SNS_RC_SUCCESS
 */
sns_rc steng1ax_set_odr_config(
   sns_sensor_instance *const instance,
                                steng1ax_eng_odr      curr_odr,
                                float                 sstvt,
                                steng1ax_eng_bw       bw,
                                uint8_t               hw_id);


/**
 * Reads status registers in Instance State.
 * This function is for debug only.
 *
 * @param[i] state                 Instance state
 * @param[i] sensor                bit mask of Sensors to enabl
 *
 * @return none
 */
void steng1ax_dump_reg(sns_sensor_instance *const instance, steng1ax_sensor_type sensor, uint8_t hw_id);

/**
 * Encode Sensor State Log.Interrupt
 *
 * @param[i] log Pointer to log packet information
 * @param[i] log_size Size of log packet information
 * @param[i] encoded_log_size Maximum permitted encoded size of
 *                            the log
 * @param[o] encoded_log Pointer to location where encoded
 *                       log should be generated
 * @param[o] bytes_written Pointer to actual bytes written
 *       during encode
 *
 * @return sns_rc,
 * SNS_RC_SUCCESS if encoding was succesful
 * SNS_RC_FAILED otherwise
 */
sns_rc steng1ax_encode_sensor_state_log_interrupt(
  void *log, size_t log_size, size_t encoded_log_size, void *encoded_log,
  size_t *bytes_written);

/**
 * Gets current Eng ODR.
 *
 * @param[i] curr_odr              Current FIFO ODR.
 *
 */
float steng1ax_get_eng_odr(steng1ax_eng_odr curr_odr, bool lpf0_set);

/**
 * send fifo data , extracts eng samples from the buffer
 * and generates events for each sample.
 *
 * @param[i] instance               Sensor instance
 * @param[i] buffer                 Buffer containing samples read from HW FIFO
 * @param[i] bytes              Number of bytes in fifo buffer
 */
void steng1ax_send_fifo_data(
    sns_sensor_instance *instance,
    const uint8_t* buffer, uint32_t bytes, uint8_t hw_id);
/**
 * read fifo data by reading the Fifo status register and sending out
 * appropriate requests to the asynchronous com port sensor to read the fifo.
 *
 * @param instance                 Sensor Instance
 */
void steng1ax_read_fifo_data(sns_sensor_instance *const instance, sns_time irq_time, bool flush, uint8_t hw_id);

/**
 * flush fifo by reading the fifo data and sending out
 * appropriate requests to the asynchronous com port sensor to read the fifo.
 *
 * @param instance                 Sensor Instance
 * @param scp_read                 flush using sync/async read
 */

void steng1ax_flush_fifo(sns_sensor_instance *const instance, uint8_t hw_id);


/**
 * Starts/restarts polling timer
 *
 * @param this           STENG1AX instance
  *
 * @return none
 */
void steng1ax_start_sensor_polling_timer(sns_sensor_instance *this);


/**
 * Starts polling timer
 *
 * @param this           STENG1AX instance
  *
 * @return none
 */
void steng1ax_start_sensor_config_timer(sns_sensor_instance *this);


/**
 * Sends config update event for the chosen sample_rate
 *
 * @param[i] instance    reference to this Instance
 * @param[i] new_client  if true, send config event even if config has not changed
 */
void steng1ax_send_config_event(sns_sensor_instance *const instance, bool new_client);

/**
 * Starts/restarts polling timer
 *
 * @param instance   Instance reference
 */
void steng1ax_reconfig_hw(sns_sensor_instance *this, uint8_t hw_id);

/**
 * Configures sensor with new/recomputed fifo settings
 *
 * @param instance   Instance reference
 */

void steng1ax_reconfig_fifo(sns_sensor_instance *this, bool flush, uint8_t hw_id);

/**
 * If mask = 0x0 or 0xFF, or if size > 1, write reg_value
 * directly to reg_addr. Else, read value at reg_addr and only
 * modify bits defined by mask.
 *
 * @param[i] port_handle      handle to synch COM port
 * @param[i] reg_addr         reg addr to modify
 * @param[i] reg_value        value to write to register
 * @param[i] size             number of bytes to write
 * @param[o]  xfer_bytes      number of bytes transfered
 * @param[i] save_write_time  save write time input
 * @param[i] mask             bit mask to update
 *
 * @return sns_rc
 * SNS_RC_FAILED - COM port failure
 * SNS_RC_SUCCESS
 */
sns_rc steng1ax_read_modify_write(
    sns_sensor_instance * instance,
    uint32_t reg_addr,
    uint8_t *reg_value,
    uint32_t size,
    uint32_t *xfer_bytes,
    bool save_write_time,
    uint8_t mask,
    uint8_t hw_id);

sns_rc steng1ax_write_regs_scp(sns_sensor_instance *const instance,
                              uint8_t addr, uint16_t num_of_bytes, uint8_t *buffer, uint8_t hw_id);

void steng1ax_process_com_port_vector(sns_port_vector *vector, void *user_arg);


/**
 * Extract a eng sample from a segment of the fifo buffer and generate an
 * event.
 *
 * @param[i] instance           The current steng1ax sensor instance
 * @param[i] sensors[]          Array of sensors for which data is requested
 * @param[i] num_sensors        Number of sensor for which data is requested
 * @param[i] raw_data           Uncalibrated sensor data to be logged
 */
void steng1ax_get_data(sns_sensor_instance *const instance,
                                steng1ax_sensor_type sensors,
                                uint8_t num_sensors,
                                int16_t *raw_data,
                                uint8_t hw_id);


void steng1ax_register_interrupt(sns_sensor_instance *this,
    steng1ax_irq_info* irq_info,
    sns_data_stream* data_stream);


sns_rc steng1ax_recover_device(sns_sensor_instance *const this, uint8_t hw_id);

void steng1ax_inst_exit_island(sns_sensor_instance *this);

void steng1ax_inst_create_timer(sns_sensor_instance *this,
    sns_data_stream** timer_data_stream,
    sns_timer_sensor_config* req_payload);


sns_rc steng1ax_inst_enter_i3c_mode(sns_sensor_instance *const instance, uint8_t hw_id);

sns_rc steng1ax_exit_i3c_mode(steng1ax_com_port_info     *com_port,
                             sns_sync_com_port_service *scp_service);

sns_rc steng1ax_set_interrupts(sns_sensor_instance *const instance, bool enable, uint8_t hw_id);

sns_rc steng1ax_turn_on_bus_power( steng1ax_instance_state * state, bool turn_on, uint8_t hw_id);

// this API will turn on comport bus power if not yet
sns_rc steng1ax_com_write_wrapper(
    sns_sensor_instance * instance,
    uint32_t reg_addr,
    uint8_t *buffer,
    uint32_t bytes,
    uint32_t *xfer_bytes,
    bool save_write_time,
    uint8_t hw_id);

// this API will turn on comport bus power if not yet
sns_rc steng1ax_instance_com_read_wrapper(
   steng1ax_instance_state * state,
   uint32_t reg_addr,
   uint8_t *buffer,
   uint32_t bytes,
   uint32_t *xfer_bytes,
   uint8_t hw_id);

// this API needs com bus power turned on before calling
sns_rc steng1ax_com_read_wrapper(
  sns_sync_com_port_service* scp_service,
  sns_sync_com_port_handle*  port_handle,
   uint32_t reg_addr,
   uint8_t *buffer,
   uint32_t bytes,
   uint32_t *xfer_bytes);

sns_rc steng1ax_com_write_wrapper_scp(
   sns_sync_com_port_service* scp_service,
   sns_sync_com_port_handle*  port_handle,
   uint32_t reg_addr,
   uint8_t *buffer,
   uint32_t bytes,
   uint32_t *xfer_bytes);


sns_rc steng1ax_read_regs_scp(sns_sensor_instance * instance,
                             uint8_t addr, uint16_t num_of_bytes, uint8_t *buffer, uint8_t hw_id);

sns_rc steng1ax_write_regs_scp(sns_sensor_instance * instance,
                              uint8_t addr, uint16_t num_of_bytes, uint8_t *buffer, uint8_t hw_id);

void steng1ax_update_heartbeat_monitor(sns_sensor_instance *const instance);

void steng1ax_exit_island(sns_sensor *const this);

bool is_data_ready_to_process(sns_sensor_instance *const instance, uint8_t num_to_check);