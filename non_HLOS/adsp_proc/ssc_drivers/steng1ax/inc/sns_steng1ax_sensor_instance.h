#pragma once
/**
 * @file sns_steng1ax_sensor_instance.h
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

#include "sns_steng1ax_build_config.h"
#include "sns_async_com_port.pb.h"
#include "sns_com_port_types.h"
#include "sns_data_stream.h"
#include "sns_diag_service.h"
#include "sns_interrupt.pb.h"
#include "sns_steng1ax_dae_if.h"
#include "sns_math_util.h"
#include "sns_physical_sensor_test.pb.h"
#include "sns_printf.h"
#include "sns_registry_util.h"
#include "sns_sensor_instance.h"
#include "sns_sensor_uid.h"
#include "sns_std_sensor.pb.h"
#include "sns_sync_com_port_service.h"
#include "sns_time.h"
#include "sns_timer.pb.h"
#include "sns_steng1ax_esp.h"
#include "sns_steng1ax_xsensor.h"
#include "sns_steng1ax_dae_if.h"

/** Forward Declaration of Instance API */
extern sns_sensor_instance_api steng1ax_sensor_instance_api;

/** Number of registers to read for debug */
#define STENG1AX_DEBUG_REGISTERS          (32)

#define SELF_TEST_DATA_COUNT_MAX         (25)

#define MAX_INTERRUPT_CNT  2

#define STENG1AX_MAX_FIFO                100

typedef enum
{
  STENG1AX_ENG_GAIN_2  = 0,
  STENG1AX_ENG_GAIN_4  = 1,
  STENG1AX_ENG_GAIN_8  = 2,
  STENG1AX_ENG_GAIN_16 = 3,
} steng1ax_eng_gain;

typedef enum
{
  STENG1AX_ENG_IMPEDENCE_100   = 0,
  STENG1AX_ENG_IMPEDENCE_200   = 1,
  STENG1AX_ENG_IMPEDENCE_500   = 2,
  STENG1AX_ENG_IMPEDENCE_1000  = 3,
} steng1ax_eng_impedance;

/**
 * ENG STENG1AX filter bandwidth in register setting
 */
typedef enum
{
  STENG1AX_ODR_BW_HALF         = 0x00,  /* 400Hz or 1600Hz bandwidth  */
  STENG1AX_ODR_BW_FOURTH       = 0x40,  /* 200Hz or 800Hz bandwidth  */
  STENG1AX_ODR_BW_EIGHTH       = 0x80,  /* 100Hz or 400Hz Hz bandwidth  */
  STENG1AX_ODR_BW_SIXTEENTH    = 0xC0,  /* 50Hz or 200Hz bandwidth  */

} steng1ax_eng_bw;

/**
 * ENG STENG1AX output data rate in register setting
 */
typedef enum
{
  STENG1AX_ENG_ODR_OFF   = 0x00,  /* power down output data rate */
  STENG1AX_ENG_ODR800    = 0xB0,  /* 800 Hz output data rate   */
  STENG1AX_ENG_ODR3200   = 0XB0,  /* 1600 Hz output data rate   */
} steng1ax_eng_odr;

/*
 * COM Port Configuration.
 */
//typedef struct
//{
//  sns_bus_type          bus_type;           /* Bus type from sns_bus_type.*/
//  uint32_t              slave_control;      /* Slave Address for I2C.
//                                             * Dynamic Slave Address for I3C.
//                                             * Chip Select for SPI.*/
//  sns_reg_addr_type     reg_addr_type;      /* Register address type for the slave.*/
//  uint32_t              min_bus_speed_KHz;  /* Minimum bus clock supported by slave in kHz.*/
//  uint32_t              max_bus_speed_KHz;  /* Maximum bus clock supported by slave in kHz.*/
//  uint8_t               bus_instance;       /* Platform bus instance number (BLSP number).*/
//} sns_com_port_config;

typedef struct steng1ax_com_port_info
{
  sns_com_port_config          com_config;
  sns_com_port_config          com_config_ex;
  sns_sync_com_port_handle     *port_handle;
  sns_sync_com_port_handle     *port_handle_ex;
  uint8_t                      i2c_address;
  uint8_t                      i3c_address;
  bool                         is_in_i3c;
} steng1ax_com_port_info;

/**
 * Range attribute.
 */
typedef struct range_attr {
  float min;
  float max;
} range_attr;

typedef enum
{
  STENG1AX_ENG           = 0x01,
  STENG1AX_XSENSOR_1     = 0x02,
  STENG1AX_XSENSOR_2     = 0x04,
  STENG1AX_XSENSOR_3     = 0x08,
  STENG1AX_XSENSOR_4     = 0x10,

} steng1ax_sensor_type;

//only for ENG
typedef enum
{
  POLLING,
  DRI,
} steng1ax_stream_mode;

#if STENG1AX_DAE_ENABLED
typedef enum
{
  STENG1AX_CONFIG_IDLE,            /** not configuring */
  STENG1AX_CONFIG_POWERING_DOWN,   /** cleaning up when no clients left */
  STENG1AX_CONFIG_STOPPING_STREAM, /** stream stop initiated, waiting for completion */
  STENG1AX_CONFIG_FLUSHING_HW,     /** FIFO flush initiated, waiting for completion */
  STENG1AX_CONFIG_FLUSHING_DATA,   /** FIFO flush initiated, waiting for completion */
  STENG1AX_CONFIG_UPDATING_HW      /** updating sensor HW, when done goes back to IDLE */
} steng1ax_config_step;
#endif

// QC - Fields in structures should be sorted by size to optimize code space

/*contains fifo reading req information
 * either interrupt or flush */


typedef enum
{
  STENG1AX_MODE_POLLING    = 0x1,
  STENG1AX_MODE_FIFO       = 0x2,
  STENG1AX_MODE_SELF_TEST  = 0x4
} steng1ax_streaming_mode;

typedef enum
{
  STENG1AX_SELF_TEST_STAGE_0    = 0x1,
  STENG1AX_SELF_TEST_STAGE_1    = 0x2,
  STENG1AX_SELF_TEST_STAGE_2    = 0x3,
  STENG1AX_SELF_TEST_STAGE_3    = 0x4,
  STENG1AX_SELF_TEST_STAGE_4    = 0x5,
  STENG1AX_SELF_TEST_STAGE_5    = 0x6,
} steng1ax_self_test_stage;

typedef enum
{
  FLUSH_TO_BE_DONE,           // 0
  FLUSH_DONE_CONFIGURING,     // 1
  FLUSH_DONE_NOT_ENG,       // 2
  FLUSH_DONE_NOT_FIFO,        // 3
  FLUSH_DONE_FIFO_EMPTY,      // 4
  FLUSH_DONE_AFTER_DATA,      // 5
  FLUSH_DONE_NOT_SOFT_PD,     // 6
} steng1ax_flush_done_reason;

typedef enum {
  CONFIG_INIT,
  CONFIG_REQUEST,
  CONFIG_SETTLED,
  CONFIG_EVENT,

  CONFIG_IDLE,
  CONFIG_FIFO,
  CONFIG_LPF,
  CONFIG_ODR,
} steng1ax_config_stage;


typedef struct steng1ax_odr_change_info {
  sns_time            eng_odr_settime;
  sns_time            hw_timer_start_time;
  sns_time            odr_change_timestamp;
  /** sampling interval calculated from ODR */
  uint32_t            nominal_sampling_intvl;
  /*sensor whose odr is actually changed */
  steng1ax_sensor_type changed;
  /*sensor which requested for change */
  steng1ax_sensor_type change_req;
  uint8_t             odr_idx;
  bool                lpf0_en_set;
  /* know whether hw_odr is changed or not */
  /* useful if eng introduced while
   * the sensor is active */
  bool                hw_odr_changed;
} steng1ax_odr_change_info;

typedef struct steng1ax_common_info
{
  steng1ax_eng_odr eng_curr_odr;
  // is lpf0_en set in CTRL5
  bool lpf0_en_set;
  uint8_t mode;
  //add few more hre
} steng1ax_common_info;

typedef struct steng1ax_self_test_info
{
  bool test_alive;
  bool reconfig_postpone;
  steng1ax_self_test_stage  self_test_stage;
  uint16_t  polling_count;
  steng1ax_sensor_type sensor;
  steng1ax_eng_odr curr_odr;
  uint8_t odr_idx;
  uint8_t skip_count;
  sns_physical_sensor_test_type test_type;
#if STENG1AX_OEM_FACTORY_CONFIG
  bool return_now;
#endif
  //add few more hre
} steng1ax_self_test_info;

typedef struct steng1ax_std_sensor_event {
  sns_std_sensor_sample_status opdata_status;
} steng1ax_std_sensor_event;

typedef struct steng1ax_fifo_req
{
  bool              interrupt_fired;
  bool              recheck_int;
  bool              flush_req;
  bool              is_dae_ts_reliable;
  sns_time          interrupt_ts;
  sns_time          cur_time;
  uint16_t          wmk;
  steng1ax_eng_odr eng_odr;
  bool               lpf0_en_set;
  sns_time                 last_sync_ts;
  sns_time                 ideal_sync_interval;
  uint16_t                 t_ph;
}steng1ax_fifo_req;

typedef struct steng1ax_config_event_info
{
  float     sample_rate;
  uint32_t  fifo_watermark;
  sns_time  timestamp;
#if STENG1AX_DAE_ENABLED
  uint32_t  dae_watermark;
#endif
} steng1ax_config_event_info;

/** HW FIFO information */
typedef struct steng1ax_fifo_info
{
  /** is fifo reconfiguration is req for the new config req*/
  bool reconfig_req;

  bool wmk_postpone;
  /** is full fifo reconfiguration is req for the new config req*/
  bool full_reconf_req;

  /** reconfig time active */
  bool timer_active;

  /** is fifo data pushing to clients */
  bool                       is_streaming;
  /** is last timestamp valid for calculating sample time*/
  bool                       last_ts_valid;
  /** to know current batch is orphan or not*/
  bool                       orphan_batch;

  /** FIFO enabled or not. Uses steng1ax_sensor_type as bit mask
   *  to determine which FIFO Sensors are enabled */
  uint8_t fifo_enabled;
  uint8_t fifo_enabled_intr;

  uint8_t eng_tag_cnt;

  /** time slot of the last sample in fifo 
   * initialize to -1 and then update whenever sample is
   * processed */
  int8_t           last_time_slot;
  /** fifo cur rate index */
  steng1ax_eng_odr fifo_rate;
  bool               lpf0_en_set;
  /** fifo desired rate index */
  steng1ax_eng_odr desired_fifo_rate;

  /** desired FIFO watermark levels for eng*/
  uint16_t desired_wmk;

  /** FIFO watermark levels for eng*/
  uint16_t cur_wmk;

  /** number of interrupts fired without reconfig*/
  uint16_t                   interrupt_cnt;

  /** max requested FIFO watermark levels; possibly larger than max HW FIFO */
  uint32_t max_requested_wmk;
  /** avg interrupt interval without reconfiguring*/
  uint32_t                   avg_interrupt_intvl;
  /** interrupt thresholds - average interrupt interval must be recalculated if outside this window*/
  uint32_t                   interrupt_intvl_upper_bound;
  uint32_t                   interrupt_intvl_lower_bound;
  /** avg sampling interval without reconfiguring*/
  uint32_t                   avg_sampling_intvl;
  uint32_t                   nominal_dae_intvl;

  /** timestamp of last sample sent to framework*/
  sns_time                   last_timestamp;
  /** timestamp of last sample sent to framework with group delay*/
  sns_time                   acc_last_timestamp_gd;
  /** ascp event timestamp
   * ascp fills timestamp once reading completes */
  sns_time                   ascp_event_timestamp;
  /** timestamp when fifo interrupt fired*/
  sns_time                   interrupt_timestamp;

  /*contains info before sending ascp req*/
  steng1ax_fifo_req th_info;
  /*contains info to use after ascp returns */
  steng1ax_fifo_req bh_info;

  steng1ax_config_event_info last_sent_config;
  steng1ax_config_event_info new_config;

} steng1ax_fifo_info;

typedef struct steng1ax_data_info
{
  bool         data_ready;
  uint8_t      num_samples;
  uint16_t     num_of_bytes;
  int16_t      eng_raw_data[STENG1AX_MAX_FIFO];
  sns_time     timestamp;
}steng1ax_data_info;

typedef struct steng1ax_eng_info
{
  /** max flush ticks*/
  uint64_t desired_max_requested_flush_ticks;
  uint64_t curr_max_requested_flush_ticks;

  bool                   config_event;
  bool                   lpf0_en_set;
  steng1ax_config_stage  stage;
  steng1ax_config_stage  config_stage;
  steng1ax_eng_odr       desired_odr_reg_val;
  uint8_t                desired_odr_idx;
  uint16_t               desired_wmk;
  float                  desired_odr;
  uint8_t                curr_odr_idx;
  uint16_t               curr_wmk;
  float                  curr_odr;
  steng1ax_eng_odr       curr_odr_reg_val;
  float                  sstvt;
  steng1ax_eng_bw        bw;
  sns_sensor_uid          suid;
  uint16_t                num_samples_to_discard;
  steng1ax_std_sensor_event sample;
  sns_time                sampling_intvl;
  sns_time                last_ts;
  sns_time                last_sent_config_ts;
  steng1ax_data_info      eng_data_info[SENSOR_CNT];
} steng1ax_eng_info;

typedef struct steng1ax_irq_info
{
  union {
    sns_ibi_req             ibi_config;
    sns_interrupt_req       irq_config;
  };
  bool                    irq_registered:1;
  bool                    irq_ready:1;
  bool                    is_ibi:1;
} steng1ax_irq_info;

typedef struct steng1ax_async_com_port_info
{
  uint32_t                port_handle;
}steng1ax_async_com_port_info;

typedef struct sns_steng1ax_registry_cfg
{
  steng1ax_sensor_type sensor_type;
  bool                zin_eng2_disable;
  bool                zin_eng1_disable;
  uint8_t             eng_mode;
  uint16_t            eng_impedance;
  uint8_t             eng_gain;
}sns_steng1ax_registry_cfg;

typedef struct sns_steng1ax_registry_mutli_cfg
{
  bool                use_multi_eng;
  uint8_t             num_sensors_enable;
}sns_steng1ax_registry_mutli_cfg;


typedef struct steng1ax_health
{
  sns_time    expected_expiration;
  uint64_t    heart_beat_timeout;
  uint64_t    sampling_interval;
  bool heart_attack;
  uint8_t heart_attack_cnt;
} steng1ax_health;

/** Private state. */
typedef struct steng1ax_instance_state
{
  bool soft_pd:1;
  uint8_t *fifo_start_address;

  steng1ax_stream_mode   eng_stream_mode;

  /** detail about self test params*/
  steng1ax_self_test_info self_test_info;

  /** Debug counter for sample count */
  uint32_t eng_sample_counter;

  /** which sensors are being flushed */
  steng1ax_sensor_type flushing_sensors;

  /** which sensors are being (re)configured */
  steng1ax_sensor_type config_sensors;

  /** which sensors are being (re)configured */
  steng1ax_sensor_type desired_sensors;

  steng1ax_sensor_type enabled_sensors;

  /** fifo details*/
  steng1ax_fifo_info       fifo_info;

  /** eng HW config details*/
  steng1ax_eng_info      eng_info;

#if STENG1AX_ESP_ENABLED
  /** ESP information */
  steng1ax_esp_info esp_info;
  steng1ax_xsensor_group_info xgroup_info;
#endif

  /** Interrupt dependency info. */
  steng1ax_irq_info        irq_info[SENSOR_CNT];
  bool                    irq_ready;
  sns_time                irq_ts;

  /** hw index */
  uint8_t hw_idx;

  /** rigid body type */
  uint8_t rigid_body_type;

  /** precentage of ODR variation w.r.t nominal value calculated
   * from fine freq register: odr percent variation (nominal - actual) * ODR
   * nominal: calc from fine freq register
   * actual: averaged from interrupt ts difference */
  int32_t odr_percent_var_eng;

  /** which entry in steng1ax_odr_map[] to use for min ODR */
  uint8_t min_odr_idx;

  uint8_t eng_gain_idx[SENSOR_CNT];
  uint8_t eng_impedance_idx[SENSOR_CNT];

  /** COM port info */
  steng1ax_com_port_info   com_port_info[SENSOR_CNT];

  /**--------Async Com Port--------*/
  sns_async_com_port_config  ascp_config[SENSOR_CNT];
  int16_t ascp_req_count[SENSOR_CNT];
  uint8_t ascp_hw_id;
  bool  bus_pwr_on[SENSOR_CNT];

#if STENG1AX_DAE_ENABLED
  /**--------DAE interface---------*/
  steng1ax_dae_if_info       dae_if;
  steng1ax_config_step       config_step;
  steng1ax_eng_odr         dae_prev_odr;
  bool                       dae_prev_lpf0_en;
#endif

  /** Data streams from dependentcies. */
  sns_data_stream      *interrupt_data_stream;
  sns_data_stream      *timer_self_test_data_stream;
  sns_data_stream      *timer_heart_beat_data_stream;
  sns_data_stream      *timer_sensor_polling_data_stream;
  sns_data_stream      *async_com_port_data_stream[SENSOR_CNT];
  sns_data_stream      *timer_config_data_stream;

  sns_sensor_uid          timer_suid;

  steng1ax_health  health;

#if STENG1AX_DAE_ERROR_HANDLING_ENABLED
  steng1ax_dae_error dae_error[MAX_DAE_ERROR_EVENTS];
  sns_rc dae_error_status;
#endif

  size_t               encoded_eng_event_len;


  /**----------Sensor specific registry configuration----------*/
  sns_steng1ax_registry_mutli_cfg multi_eng_cfg;
  sns_steng1ax_registry_cfg eng_registry_cfg[SENSOR_CNT];


  /**----------debug----------*/
  uint32_t  num_ascp_null_events[SENSOR_CNT];
  uint8_t   reg_status[STENG1AX_DEBUG_REGISTERS];

  sns_diag_service *diag_service;
  sns_sync_com_port_service * scp_service;
  size_t           log_interrupt_encoded_size;
  size_t           log_raw_encoded_size;
  size_t           log_temp_raw_encoded_size;
} steng1ax_instance_state;

typedef struct odr_reg_map
{
  float              odr;
  float              eng_group_delay;  //ms
  steng1ax_eng_odr   eng_odr_reg_value;
  uint16_t           odr_coeff;
  uint16_t           eng_discard_samples;
  uint8_t            exp_lpf0_en;
} odr_reg_map;


typedef struct steng1ax_selftest_state
{
  steng1ax_sensor_type             sensor;
  sns_physical_sensor_test_type   test_type;
  bool                            requested:1;
} steng1ax_selftest_state;

/** Instance read-only state */
typedef struct steng1ax_instance_config
{
  sns_sensor_uid                    irq_suid;
  sns_sensor_uid                    timer_suid;
  sns_sensor_uid                    acp_suid;
  sns_sensor_uid                    reg_suid;
#if STENG1AX_DAE_ENABLED
  sns_sensor_uid                    dae_suid;
  steng1ax_dae_if_state              dae_ag_state;
  steng1ax_dae_if_state              dae_temper_state;
#endif
  steng1ax_com_port_info             com_port_info[SENSOR_CNT];
  steng1ax_irq_info                  irq_config[SENSOR_CNT];
#if STENG1AX_ESP_ENABLED
  //esp reg config
  steng1ax_esp_registry_cfg          esp_reg_cfg[SENSOR_CNT];
#endif

  uint8_t                           eng_resolution_idx;
  uint8_t                           eng_gain_idx[SENSOR_CNT];
  uint8_t                           eng_impedance_idx[SENSOR_CNT];
  uint8_t                           min_odr_idx[SENSOR_CNT];
  uint8_t                           max_odr_idx[SENSOR_CNT];
  steng1ax_selftest_state            selftest;
  steng1ax_stream_mode               eng_stream_mode;

  sns_steng1ax_registry_mutli_cfg    multi_eng_cfg;
  sns_steng1ax_registry_cfg          eng_reg_cfg[SENSOR_CNT];

  float                             sample_rate;
  float                             report_rate;
  float                             dae_report_rate;
  uint64_t                          flush_period_ticks;

  steng1ax_sensor_type               flushing_sensors;
  steng1ax_sensor_type               config_sensors;
  steng1ax_sensor_type               fifo_enable;
  steng1ax_sensor_type               client_present;
  steng1ax_sensor_type               selftest_client_present;
} steng1ax_instance_config;


sns_rc steng1ax_inst_init(sns_sensor_instance *const this,
    sns_sensor_state const *sstate);

sns_rc steng1ax_inst_deinit(sns_sensor_instance *const this);
void steng1ax_set_client_test_config(sns_sensor_instance *this,
                                    sns_request const *client_request);
void steng1ax_inst_hw_self_test(sns_sensor_instance *const this);
void steng1ax_context_save(sns_sensor_instance * const this,uint8_t context_buffer [ ],uint8_t reg_map [ ],uint8_t reg_num);
void steng1ax_context_restore(sns_sensor_instance * const this,uint8_t context_buffer [ ],uint8_t reg_map [ ],uint8_t reg_num);
/**
 * Sends a FIFO complete event.
 *
 * @param instance   Instance reference
 */
void steng1ax_send_fifo_flush_done(sns_sensor_instance*,
                                  steng1ax_sensor_type,
                                  steng1ax_flush_done_reason);
sns_time steng1ax_estimate_avg_st(
  sns_sensor_instance *const instance,
  sns_time irq_timestamp,
  uint16_t num_samples);

uint8_t steng1ax_get_odr_rate_idx(float desired_sample_rate);
void steng1ax_restart_hb_timer(sns_sensor_instance *const this, bool reset);

sns_rc steng1ax_handle_timer(
    sns_sensor_instance *const instance, sns_data_stream* timer_data_stream,
    sns_rc (*timer_handler)(sns_sensor_instance *const , sns_time, sns_timer_sensor_event* latest_timer_event));
void steng1ax_clear_interrupt_q(sns_sensor_instance *const instance,
    sns_data_stream* interrupt_data_stream, uint8_t hw_id);


void steng1ax_handle_sensor_sample(sns_sensor_instance *const instance, sns_time timestamp);

sns_time steng1ax_recover_ibi_irq_ts(sns_sensor_instance *const this, sns_time master_ref, const uint8_t* ibi_data);
//OEM specific headers
bool steng1ax_is_valid_oem_request(uint32_t message_id);
void steng1ax_handle_oem_request(sns_sensor *const this, sns_sensor_instance *instance, struct sns_request const *request);

void steng1ax_oem_factory_test_config(sns_sensor_instance * const this, bool enable);

