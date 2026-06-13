/*
*	sns_ct7117x_sensor_instance.c
*	Normal mode functions for the sensor instance
*
*/

/*==============================================================================
  Include Files
  ============================================================================*/
#include "sns_mem_util.h"
#include "sns_sensor_instance.h"
#include "sns_service_manager.h"
#include "sns_stream_service.h"
#include "sns_rc.h"
#include "sns_request.h"
#include "sns_time.h"
#include "sns_sensor_event.h"
#include "sns_types.h"
#include "sns_event_service.h"
#include "sns_memmgr.h"
#include "sns_com_port_priv.h"
#include "sns_gpio_service.h"

#include "sns_ct7117x_hal.h"
#include "sns_ct7117x_sensor.h"
#include "sns_ct7117x_sensor_instance.h"

#include "sns_interrupt.pb.h"
#include "sns_async_com_port.pb.h"
#include "sns_timer.pb.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "sns_pb_util.h"
#include "sns_async_com_port_pb_utils.h"
#include "sns_diag_service.h"
#include "sns_diag.pb.h"
#include "sns_sensor_util.h"
#include "sns_sync_com_port_service.h"
#include "sns_island.h"
#include <qurt.h>
#include <stdio.h>
#define SHARED_PHYS_ADDR  0x81EC0000
#define SHARED_SIZE       0x20000
//#define SHARED_PHYS_ADDR  0x81EE0000
//#define SHARED_SIZE       0x10000
//#define LATENCY_MEASURE
unsigned int myddr_base_addr = 0;

#define LGE_TCM_START_ADDRESS  0x20488000
#define LGE_TCM_SIZE  0x20000
unsigned int mytcm_base_addr;

/*==============================================================================
  Function Definitions
  ============================================================================*/

/*
*	start temperature polling timer
*/
void ct7117x_start_sensor_temp_polling_timer(sns_sensor_instance *this)
{
  ct7117x_instance_state *state = (ct7117x_instance_state*)this->state->state;
  sns_timer_sensor_config req_payload = sns_timer_sensor_config_init_default;
  uint8_t buffer[50] = {0};
  sns_request timer_req = {
    .message_id = SNS_TIMER_MSGID_SNS_TIMER_SENSOR_CONFIG,
    .request    = buffer
  };
  sns_rc rc = SNS_RC_SUCCESS;

  SNS_INST_PRINTF(LOW, this, "ct7117x_start_sensor_temp_polling_timer");

  /*create timer stream*/
  if(NULL == state->temperature_timer_data_stream)
  {
    sns_service_manager *smgr = this->cb->get_service_manager(this);
    sns_stream_service *srtm_svc = (sns_stream_service*)smgr->get_service(smgr, SNS_STREAM_SERVICE);
    rc = srtm_svc->api->create_sensor_instance_stream(srtm_svc,
          this, state->timer_suid, &state->temperature_timer_data_stream);
  }

  if(SNS_RC_SUCCESS != rc
     || NULL == state->temperature_timer_data_stream)
  {
    SNS_INST_PRINTF(ERROR, this, "failed timer stream create rc = %d", rc);
    return;
  }

  /*timer param*/
  req_payload.is_periodic = true;
  req_payload.start_time = sns_get_system_time();
  req_payload.timeout_period = state->temperature_info.sampling_intvl;
  timer_req.request_len = pb_encode_request(buffer, sizeof(buffer), &req_payload,
                                            sns_timer_sensor_config_fields, NULL);
  if(timer_req.request_len > 0)
  {
    state->temperature_timer_data_stream->api->send_request(state->temperature_timer_data_stream, &timer_req);
    state->temperature_info.timer_is_active = true;
  }
}

/*
*	set temperature polling config
*/
void ct7117x_set_temperature_config(sns_sensor_instance *const this)
{
  ct7117x_instance_state *state = (ct7117x_instance_state*)this->state->state;
  
  SNS_INST_PRINTF(LOW, this,
  "temperature_info.timer_is_active:%d state->temperature_info.sampling_intvl:%u is_dri:%d",
  state->temperature_info.timer_is_active,
  state->temperature_info.sampling_intvl, state->is_dri);

  if(state->temperature_info.sampling_intvl > 0)
  {
      if (state->is_dri)
      {

        ct7117x_set_power_mode(state, TEMP_NORMAL_MODE);
        // change-20260204-hyungchul: Log added to confirm mode set
        SNS_INST_PRINTF(HIGH, this, "Set Power Mode: NORMAL (DRI Active)");
      }
      else
      {

        ct7117x_set_power_mode(state, TEMP_FORCED_MODE);
        ct7117x_start_sensor_temp_polling_timer(this);
      }
  }
  else
  {
    if (state->is_dri)
    {

    }
    else
    {
      state->temperature_info.timer_is_active = false;
      sns_sensor_util_remove_sensor_instance_stream(this, &state->temperature_timer_data_stream);
    }
    ct7117x_set_power_mode(state,TEMP_SLEEP_MODE);
    SNS_INST_PRINTF(LOW, this, "Set Power Mode: SLEEP");
  }
}

/*
*	reconfig hardware
*/
void ct7117x_reconfig_hw(sns_sensor_instance *this,
  ct7117x_sensor_type sensor_type)
{
  ct7117x_instance_state *state = (ct7117x_instance_state*)this->state->state;
  int8_t err = 0;
  SNS_INST_PRINTF(LOW, this, "ct7117x_reconfig_hw state->config_step = %d",state->config_step);
  SNS_INST_PRINTF(LOW, this,
      "enable sensor flag:0x%x publish sensor flag:0x%x",
      state->deploy_info.enable,
      state->deploy_info.publish_sensors);
  
  err = ct7117x_set_work_mode(state, TEMP_HIGH_RESOLUTION_MODE);
  if (err)
  {
     SNS_INST_PRINTF(ERROR, this, "set oversample failed error = %d", err);
  }
  // Enable timer in case of sensor pressure clients
  if (sensor_type == TEMP_TEMPERATURE)
  {
    ct7117x_set_temperature_config(this);
  }
  /* done with reconfig */
  state->config_step = TEMP_CONFIG_IDLE; 
  SNS_INST_PRINTF(LOW, this, "ct7117x_reconfig_hw finished");
}

/*
*	Runs a communication test - verfies WHO_AM_I, publishes self
*	test event.
*/
static void ct7117x_send_com_test_event(sns_sensor_instance *instance,
                                        sns_sensor_uid *uid, bool test_result)
{
  uint8_t data[1] = {0};
  pb_buffer_arg buff_arg = (pb_buffer_arg)
      { .buf = &data, .buf_len = sizeof(data) };
  sns_physical_sensor_test_event test_event =
     sns_physical_sensor_test_event_init_default;

  test_event.test_passed = test_result;
  test_event.test_type = SNS_PHYSICAL_SENSOR_TEST_TYPE_COM;
  test_event.test_data.funcs.encode = &pb_encode_string_cb;
  test_event.test_data.arg = &buff_arg;

  pb_send_event(instance,
                sns_physical_sensor_test_event_fields,
                &test_event,
                sns_get_system_time(),
                SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_EVENT,
                uid);
}


/*
*	run self test check hw,com and factory
*/
void ct7117x_run_self_test(sns_sensor_instance *instance)
{
  ct7117x_instance_state *state = (ct7117x_instance_state*)instance->state->state;
  sns_rc rv = SNS_RC_SUCCESS;
  uint8_t buffer = 0;
  bool who_am_i_success = false;

  rv = ct7117x_get_who_am_i(state->scp_service,
                            state->com_port_info.port_handle,
                            &buffer);
  if(rv == SNS_RC_SUCCESS
     &&
     buffer == TEMP_WHOAMI_VALUE)
  {
    who_am_i_success = true;
  }
  if(state->temperature_info.test_info.test_client_present)
  {
    if(state->temperature_info.test_info.test_type == SNS_PHYSICAL_SENSOR_TEST_TYPE_COM)
    {
      ct7117x_send_com_test_event(instance, &state->temperature_info.suid, who_am_i_success);
    }
    else if(state->temperature_info.test_info.test_type == SNS_PHYSICAL_SENSOR_TEST_TYPE_FACTORY)
    {
      // Handle factory test. The driver may choose to reject any new
      // streaming/self-test requests when factory test is in progress.
    }
    state->temperature_info.test_info.test_client_present = false;
  }
}

void ct7117x_handle_interrupt_event(sns_sensor_instance *const instance, sns_time timestamp)
{
  ct7117x_instance_state *state = (ct7117x_instance_state*)instance->state->state;
  SNS_INST_PRINTF(LOW, instance, "interrupt event");
  state->interrupt_timestamp = timestamp;

#ifdef	LATENCY_MEASURE
  sns_time t_start = sns_get_system_time();
  sns_time t_h_s = 0, t_h_e = 0, t_r_s = 0, t_r_e = 0, t_p_s = 0, t_p_e = 0, t_w_s = 0, t_w_e = 0;
#endif	//LATENCY_MEASURE
  {
    // [Update] First read the header to determine the data length, then read only that amount of data.
    // This prevents the driver from occupying the SPI bus for an unnecessarily long time
    // and fundamentally resolves handshake timing issues with the sensor.
    uint8_t header[11];
    uint32_t xfer_bytes = 0;
    uint32_t image_data_len = 0;
    uint32_t total_read_bytes = 0;

    sns_island_exit_internal();

    // Control Vision ON/OFF by App
    // Read Control Bit : 0x81ED FF00
    if (myddr_base_addr != 0)
    {
          uint8_t* base_ptr = (uint8_t*)myddr_base_addr + 0x1ff00;
          uint8_t* current_dest;

          current_dest = base_ptr + 0x10;
          if(*current_dest != 0x0)
          {
              uint8_t* blesend_ptr = (uint8_t*)mytcm_base_addr + 0x1ff00 + 0x10;
              *blesend_ptr = *current_dest;
          }

          // on/off
          current_dest = base_ptr + 0x07;
          if(*current_dest != 0x0)
          {
              struct sns_service_manager *smgr = instance->cb->get_service_manager(instance);
              sns_gpio_service *gpio_service;
              gpio_service = (sns_gpio_service *)smgr->get_service(smgr, SNS_GPIO_SERVICE);

              ct7117x_instance_state *state = (ct7117x_instance_state*)instance->state->state;

              if (NULL != gpio_service && NULL != gpio_service->api)
              {
                   if(*current_dest == 0xFF)
                   {
                       *current_dest = 0xF0;

                       // Set GPIO 106 to HIGH
                       gpio_service->api->write_gpio(106, true, SNS_GPIO_DRIVE_STRENGTH_2_MILLI_AMP, SNS_GPIO_PULL_TYPE_NO_PULL, SNS_GPIO_STATE_LOW);

                       // Set GPIO 14 to HIGH
                       gpio_service->api->write_gpio(14, true, SNS_GPIO_DRIVE_STRENGTH_2_MILLI_AMP, SNS_GPIO_PULL_TYPE_NO_PULL, SNS_GPIO_STATE_LOW);

                       // Set GPIO 15 to HIGH
                       gpio_service->api->write_gpio(15, true, SNS_GPIO_DRIVE_STRENGTH_2_MILLI_AMP, SNS_GPIO_PULL_TYPE_NO_PULL, SNS_GPIO_STATE_LOW);

                       state->temperature_info.sampling_rate_hz = 1.0f; // 1Hz
                       state->temperature_info.sampling_intvl = sns_convert_ns_to_ticks(1000000000.0 / state->temperature_info.sampling_rate_hz);
                       ct7117x_start_sensor_temp_polling_timer(instance);
                   }
                   else if(*current_dest == 0x01)
                   {

                      *current_dest = 0x0;

                      // Set GPIO 106 to HIGH
                       gpio_service->api->write_gpio(106, true, SNS_GPIO_DRIVE_STRENGTH_2_MILLI_AMP, SNS_GPIO_PULL_TYPE_NO_PULL, SNS_GPIO_STATE_HIGH);

                       // Set GPIO 14 to HIGH
                       gpio_service->api->write_gpio(14, true, SNS_GPIO_DRIVE_STRENGTH_2_MILLI_AMP, SNS_GPIO_PULL_TYPE_NO_PULL, SNS_GPIO_STATE_HIGH);

                       // Set GPIO 15 to HIGH
                       gpio_service->api->write_gpio(15, true, SNS_GPIO_DRIVE_STRENGTH_2_MILLI_AMP, SNS_GPIO_PULL_TYPE_NO_PULL, SNS_GPIO_STATE_HIGH);
                  }
              }

          return;

          }
    }

    // Disable Timer
    if(state->temperature_info.timer_is_active == true)
    {
                      state->temperature_info.timer_is_active = false;
                      sns_sensor_util_remove_sensor_instance_stream(instance, &state->temperature_timer_data_stream);
    }

    // 0. During audio recording, can caputre gs25/dark images
    for(volatile int i = 0; i < 50000; i++);
    // 1. Read header (11 bytes)
#ifdef	LATENCY_MEASURE
    t_h_s = sns_get_system_time();
#endif	//LATENCY_MEASURE
    state->com_read(state->scp_service, state->com_port_info.port_handle, 0x00, header, sizeof(header), &xfer_bytes);
#ifdef	LATENCY_MEASURE
    t_h_e = sns_get_system_time();
#endif	//LATENCY_MEASURE
    if (xfer_bytes != sizeof(header))
    {
        SNS_INST_PRINTF(ERROR, instance, "Failed to read header, read %u bytes", xfer_bytes);
        // [Modified] Exit without data transfer if header read fails (effectively ignores the second interrupt)
        return;
    }

    // 2. Parse data length from the header (little-endian)
    // Corresponds to sensor¡¯s spim_ptl_hdr_buf[3]~[6]
#ifdef	LATENCY_MEASURE
    t_p_s = sns_get_system_time();
#endif	//LATENCY_MEASURE
    image_data_len = header[3] | (header[4] << 8) | (header[5] << 16) | (header[6] << 24);
    SNS_INST_PRINTF(HIGH, instance, "Parsed image data length from header: %u bytes", image_data_len);
#ifdef	LATENCY_MEASURE
    t_p_e = sns_get_system_time();
#endif	//LATENCY_MEASURE
    if (image_data_len == 0 || image_data_len > (120 * 1024 - sizeof(header))) // Validation check
    {
        SNS_INST_PRINTF(HIGH, instance, "Data length is 0 (Sensor startup/idle?), ignoring.");
        return;
    }

    // 3. Read image data up to the parsed length
    uint8_t buffer[2048];
#ifdef	LATENCY_MEASURE
    t_r_s = sns_get_system_time();
#endif	//LATENCY_MEASURE
    for(uint32_t read_len = 0; read_len < image_data_len; read_len += xfer_bytes)
    {
      uint32_t chunk = (image_data_len - read_len) > sizeof(buffer) ? sizeof(buffer) : (image_data_len - read_len);
      state->com_read(state->scp_service, state->com_port_info.port_handle, 0x00, buffer, chunk, &xfer_bytes);

      // if (read_len == 0 && xfer_bytes >= 5)
      // {
      //   SNS_INST_PRINTF(HIGH, instance, "First 5 bytes of image data: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
      //                   buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
      // }

      if (myddr_base_addr != 0)
      {
          void* current_dest = (void*)myddr_base_addr;
          void* current_tcmdest = (void*)mytcm_base_addr;
          // For the first chunk, write the first 11 bytes of the header to the destination.
          if(read_len == 0)
          {
              sns_memscpy(current_dest, 11, header, 11);
              sns_memscpy(current_tcmdest, 11, header, 11);
          }

          // Write the image data after the header.
          memcpy((uint8_t*)current_dest + 11 + read_len, buffer, xfer_bytes);
          memcpy((uint8_t*)current_tcmdest + 11 + read_len, buffer, xfer_bytes);
      }
      total_read_bytes += xfer_bytes;
    }

    // LPAI BLE Data End
    uint8_t* jpgend_ptr = (uint8_t*)mytcm_base_addr + 11 + total_read_bytes;
    *jpgend_ptr = 0x0F;

    qurt_mem_barrier();

    SNS_INST_PRINTF(HIGH, instance, "Total bytes: %u (tcm:%x)", total_read_bytes, mytcm_base_addr);
  }
#ifdef	LATENCY_MEASURE
  t_r_e = sns_get_system_time();
#endif	//LATENCY_MEASURE

    sns_busy_wait(sns_convert_ns_to_ticks(7 * 1000 * 1000)); // 10ms delay

  // After synchronization, transmit a specific value to the sensor.
  // Sleep Time, Similarity, Occlusion, Low Illuminance, Blur, Key Frame cnt, Testmode on/off, Crop Size
  //uint8_t write_buffer[8] = {0x01, 0x40, 0x42, 0x40, 0x00, 0x3C, 0x01, 0x00};
  uint8_t write_buffer[8] = {0x01, 0x40, 0x42, 0x40, 0x00, 0x00, 0x00, 0x00};

  // Read Control Bit : 0x81ED FF00
  if (myddr_base_addr != 0)
  {
          uint8_t* base_ptr = (uint8_t*)myddr_base_addr + 0x1ff00;
          uint8_t* current_dest;

          // Sleep Time: 0x01
          current_dest = base_ptr + 0x00;
          if(*current_dest != 0x0)
          {
              write_buffer[0] = *current_dest;
          }

          // Similarity: 0x40
          current_dest = base_ptr + 0x01;
          if(*current_dest != 0x0)
          {
              write_buffer[1] = *current_dest;
          }

          // Occlusion: 0x42
          current_dest = base_ptr + 0x02;
          if((*current_dest != 0x0) && (*current_dest != write_buffer[2]))
          {
              write_buffer[2] = *current_dest;
          }

          // A-Gain: 0x40
          current_dest = base_ptr + 0x03;
          if((*current_dest != 0x0) && (*current_dest != write_buffer[3]))
          {
              write_buffer[3] = *current_dest;
          }

          // Blur: 0x00
          current_dest = base_ptr + 0x04;
          if((*current_dest != 0x0) && (*current_dest != write_buffer[4]))
          {
              write_buffer[4] = *current_dest;
          }

          // KeyFrame: 0x3C
          current_dest = base_ptr + 0x05;
          if((*current_dest != 0x0))
          {
              if(*current_dest == 0xFF)
                  write_buffer[5] = 0x00;
              else
                  write_buffer[5] = *current_dest;
          }

          // Testmode: 
          current_dest = base_ptr + 0x06;
          if(*current_dest != 0x0)
          {
              if(*current_dest == 0xFF)
                  write_buffer[6] = 0x00;
              else
                  write_buffer[6] = 0x1;
          }

  }

  uint32_t xfer_bytes = 0;
#ifdef	LATENCY_MEASURE  
  t_w_s = sns_get_system_time();
#endif	//LATENCY_MEASURE
  state->com_write(state->scp_service, state->com_port_info.port_handle,
                   0x00, write_buffer, sizeof(write_buffer), &xfer_bytes, false);
#ifdef	LATENCY_MEASURE
  t_w_e = sns_get_system_time();

  SNS_INST_PRINTF(HIGH, instance, "Latency(us): Header read=%u Header Parsing=%u Image read=%u Param write=%u Total=%u",
                   (uint32_t)sns_convert_ticks_to_usec(t_h_e - t_h_s), (uint32_t)sns_convert_ticks_to_usec(t_p_e - t_p_s), (uint32_t)sns_convert_ticks_to_usec(t_r_e - t_r_s),
                   (uint32_t)sns_convert_ticks_to_usec(t_w_e - t_w_s), (uint32_t)sns_convert_ticks_to_usec(sns_get_system_time() - t_start));
#endif	//LATENCY_MEASURE
}

/*
*	op_mode,physical sensor config 
*/
void ct7117x_send_config_event(sns_sensor_instance *const instance)
{
  ct7117x_instance_state *state = (ct7117x_instance_state*)instance->state->state;
  sns_std_sensor_physical_config_event phy_sensor_config =
  sns_std_sensor_physical_config_event_init_default;
  char operating_mode[] = "NORMAL";
  pb_buffer_arg op_mode_args;
  /*op mode*/
  op_mode_args.buf = &operating_mode[0];
  op_mode_args.buf_len = sizeof(operating_mode);
  /*physical sensor config*/
  phy_sensor_config.has_sample_rate = true;
  phy_sensor_config.has_water_mark = true;
  phy_sensor_config.water_mark = 1;
  phy_sensor_config.operation_mode.funcs.encode = &pb_encode_string_cb;
  phy_sensor_config.operation_mode.arg = &op_mode_args;
  phy_sensor_config.has_active_current = true;
  phy_sensor_config.has_resolution = true;
  phy_sensor_config.range_count = 2;
  phy_sensor_config.stream_is_synchronous = false;
  phy_sensor_config.has_dri_enabled= true;
  phy_sensor_config.dri_enabled=false;

  if(state->deploy_info.publish_sensors & TEMP_TEMPERATURE)
  {
    phy_sensor_config.sample_rate = state->temperature_info.sampling_rate_hz;
    phy_sensor_config.has_active_current = true;
    phy_sensor_config.active_current = 3;
    phy_sensor_config.resolution = CT7117X_TEMPERATURE_RESOLUTION;
    phy_sensor_config.range_count = 2;
    phy_sensor_config.range[0] = CT7117X_TEMPERATURE_RANGE_MIN;
    phy_sensor_config.range[1] = CT7117X_TEMPERATURE_RANGE_MAX;

    pb_send_event(
        instance,
        sns_std_sensor_physical_config_event_fields,
        &phy_sensor_config,
        sns_get_system_time(),
        SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT,
        &state->temperature_info.suid);
  }
}

/*
*	limit temperature odr
*/
static sns_rc ct7117x_validate_sensor_temp_odr(sns_sensor_instance *this)
{
  sns_rc rc = SNS_RC_SUCCESS;
  ct7117x_instance_state *state = (ct7117x_instance_state*)this->state->state;
  SNS_INST_PRINTF(LOW, this, "temperature odr = %d", (int8_t)state->temperature_info.sampling_rate_hz);
  if(state->temperature_info.sampling_rate_hz > TEMP_ODR_0
     &&
     state->temperature_info.sampling_rate_hz <= TEMP_ODR_1)
  {
    state->temperature_info.sampling_rate_hz = TEMP_ODR_1;
  }
  else if(state->temperature_info.sampling_rate_hz > TEMP_ODR_1)
  {
    state->temperature_info.sampling_rate_hz = TEMP_ODR_5;
  }
  /*ODR < 0*/
  else
  {
    state->temperature_info.sampling_intvl = 0;
    state->temperature_info.timer_is_active = 0;
    SNS_INST_PRINTF(LOW, this, "close temperature sensor = %d, timer_is_active =%d",
           (uint32_t)state->temperature_info.sampling_rate_hz, state->temperature_info.timer_is_active);
    rc = SNS_RC_NOT_SUPPORTED;
  }
  if (rc == SNS_RC_SUCCESS)
  {
    state->temperature_info.sampling_intvl =
      sns_convert_ns_to_ticks(1000000000.0 / state->temperature_info.sampling_rate_hz);
    SNS_INST_PRINTF(LOW, this, "temperature timer_value = %u", (uint32_t)state->temperature_info.sampling_intvl);
  }

  return rc;
}

/*
*	
*/
static void inst_cleanup(ct7117x_instance_state *state, sns_stream_service *stream_mgr)
{

  if(NULL != state->async_com_port_data_stream)
  {
    stream_mgr->api->remove_stream(stream_mgr, state->async_com_port_data_stream);
    state->async_com_port_data_stream = NULL;
  }
  if(NULL != state->temperature_timer_data_stream)
  {
    stream_mgr->api->remove_stream(stream_mgr, state->temperature_timer_data_stream);
    state->temperature_timer_data_stream = NULL;
  }

  if(NULL != state->interrupt_data_stream)
  {
    stream_mgr->api->remove_stream(stream_mgr, state->interrupt_data_stream);
    state->interrupt_data_stream = NULL;
  }

  if(NULL != state->scp_service)
  {
  state->scp_service->api->sns_scp_close(state->com_port_info.port_handle);
    state->scp_service->api->sns_scp_deregister_com_port(&state->com_port_info.port_handle);
    state->scp_service = NULL;
  }
}

/*
*	instance_api:instance_init
*/
sns_rc ct7117x_temp_inst_init(sns_sensor_instance * const this,
    sns_sensor_state const *sstate)
{
  ct7117x_instance_state *state =
              (ct7117x_instance_state*) this->state->state;
  ct7117x_state *sensor_state =
              (ct7117x_state*) sstate->state;
  float stream_data[1] = {0};
  sns_service_manager *service_mgr = this->cb->get_service_manager(this);
  sns_stream_service *stream_mgr = (sns_stream_service*)
              service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);

  // Shared memory region initialization
  if (myddr_base_addr == 0)
  {
    qurt_mem_pool_t hwio_pool;
    qurt_mem_region_t shared_mem_region;
    qurt_mem_region_attr_t hwio_attr;

    if(QURT_EOK == qurt_mem_pool_attach("smem_pool", &hwio_pool))
    {
        qurt_mem_region_attr_init(&hwio_attr);
        qurt_mem_region_attr_set_cache_mode(&hwio_attr, QURT_MEM_CACHE_NONE_SHARED);
        qurt_mem_region_attr_set_mapping(&hwio_attr, QURT_MEM_MAPPING_PHYS_CONTIGUOUS);
        qurt_mem_region_attr_set_physaddr(&hwio_attr, SHARED_PHYS_ADDR);

        if (QURT_EOK == qurt_mem_region_create(&shared_mem_region, SHARED_SIZE, hwio_pool, &hwio_attr))
        {
           if (QURT_EOK == qurt_mem_region_attr_get(shared_mem_region, &hwio_attr))
           {
             qurt_mem_region_attr_get_virtaddr(&hwio_attr, &myddr_base_addr);
             SNS_INST_PRINTF(HIGH, this, "Shared memory region created successfully. Virtual address: 0x%x", myddr_base_addr);
           }
        }
    }
    if (myddr_base_addr == 0) {
        SNS_INST_PRINTF(ERROR, this, "Failed to create shared memory region.");
    }
  }

  if (mytcm_base_addr == 0 )
  {
    qurt_mem_pool_t hwio_pool = 0;
    qurt_mem_region_t shared_mem_region = 0;
    qurt_mem_region_attr_t hwio_attr;

    mytcm_base_addr = 1;
    if(QURT_EOK == qurt_mem_pool_attach("LGE_TCM_POOL", &hwio_pool))
    {
	mytcm_base_addr = 2;
        qurt_mem_region_attr_init(&hwio_attr);
        qurt_mem_region_attr_set_cache_mode(&hwio_attr, QURT_MEM_CACHE_NONE);
        qurt_mem_region_attr_set_mapping(&hwio_attr, QURT_MEM_MAPPING_PHYS_CONTIGUOUS);
        qurt_mem_region_attr_set_physaddr(&hwio_attr, LGE_TCM_START_ADDRESS);

        if (QURT_EOK == qurt_mem_region_create(&shared_mem_region, LGE_TCM_SIZE, hwio_pool, &hwio_attr))
        {
	   mytcm_base_addr = 3;
           if (QURT_EOK == qurt_mem_region_attr_get(shared_mem_region, &hwio_attr))
           {
             qurt_mem_region_attr_get_virtaddr(&hwio_attr, &mytcm_base_addr);
           }
        }
    }
  }

  uint64_t buffer[10];
  pb_ostream_t stream = pb_ostream_from_buffer((pb_byte_t *)buffer, sizeof(buffer));
  sns_diag_batch_sample batch_sample = sns_diag_batch_sample_init_default;
  uint8_t arr_index = 0;
  float diag_temp[TEMP_NUM_AXES];
  pb_float_arr_arg arg = {.arr = (float*)diag_temp, .arr_len = TEMP_NUM_AXES,
    .arr_index = &arr_index};
  batch_sample.sample.funcs.encode = &pb_encode_float_arr_cb;
  batch_sample.sample.arg = &arg;

  state->scp_service = (sns_sync_com_port_service*)
              service_mgr->get_service(service_mgr, SNS_SYNC_COM_PORT_SERVICE);

  SNS_INST_PRINTF(LOW, this, "<sns_see_if__  init> from sensor:0x%x", sensor_state->sensor);
  /**---------Setup stream connections with dependent Sensors---------*/
  stream_mgr->api->create_sensor_instance_stream(stream_mgr,
                          this,
                          sensor_state->acp_suid,
                          &state->async_com_port_data_stream);

  /*Initialize COM port to be used by the Instance */
  sns_memscpy(&state->com_port_info.com_config,
              sizeof(state->com_port_info.com_config),
              &sensor_state->com_port_info.com_config,
              sizeof(sensor_state->com_port_info.com_config));

  state->scp_service->api->sns_scp_register_com_port(&state->com_port_info.com_config,
                                              &state->com_port_info.port_handle);

  if(NULL == state->async_com_port_data_stream ||
     NULL == state->com_port_info.port_handle)
  {
    inst_cleanup(state, stream_mgr);
    return SNS_RC_FAILED;
  }

  /**----------- Copy all Sensor UIDs in instance state -------------*/
  sns_memscpy(&state->temperature_info.suid,
              sizeof(state->temperature_info.suid),
              &sensor_state->my_suid,
              sizeof(state->temperature_info.suid));
  sns_memscpy(&state->timer_suid,
              sizeof(state->timer_suid),
              &sensor_state->timer_suid,
              sizeof(sensor_state->timer_suid));
  sns_memscpy(&state->irq_suid,
              sizeof(state->irq_suid),
              &sensor_state->irq_suid,
              sizeof(sensor_state->irq_suid));
  state->irq_num = 102;

  /** Copy calibration data*/
  sns_memscpy(&state->calib_param,
              sizeof(state->calib_param),
              &sensor_state->calib_param,
              sizeof(sensor_state->calib_param));
  state->interface = sensor_state->com_port_info.com_config.bus_instance;
  
  /* change-20260204-hyungchul: Force DRI enabled in init for auto-start */

  state->is_dri = true;
  
  state->op_mode = FORCED_MODE;
  /* com read function*/
  state->com_read = ct7117x_com_read_wrapper;
  /*com write function*/
  state->com_write = ct7117x_com_write_wrapper;
  /* set the data report length to the framework */
  state->encoded_imu_event_len = pb_get_encoded_size_sensor_stream_event(stream_data, 1);

  state->diag_service =  (sns_diag_service*)
      service_mgr->get_service(service_mgr, SNS_DIAG_SERVICE);

  state->scp_service =  (sns_sync_com_port_service*)
      service_mgr->get_service(service_mgr, SNS_SYNC_COM_PORT_SERVICE);

  state->scp_service->api->sns_scp_open(state->com_port_info.port_handle);


  state->scp_service->api->sns_scp_update_bus_power(state->com_port_info.port_handle,
                                                                           false);


  /** Configure the Async Com Port */
  {
    sns_data_stream* data_stream = state->async_com_port_data_stream;
    sns_com_port_config* com_config = &sensor_state->com_port_info.com_config;
    uint8_t pb_encode_buffer[100];
    sns_request async_com_port_request =
    {
      .message_id  = SNS_ASYNC_COM_PORT_MSGID_SNS_ASYNC_COM_PORT_CONFIG,
      .request     = &pb_encode_buffer
    };

    state->ascp_config.bus_type          = (com_config->bus_type == SNS_BUS_I2C) ?
      SNS_ASYNC_COM_PORT_BUS_TYPE_I2C : SNS_ASYNC_COM_PORT_BUS_TYPE_SPI;
    state->ascp_config.slave_control     = com_config->slave_control;
    state->ascp_config.reg_addr_type     = SNS_ASYNC_COM_PORT_REG_ADDR_TYPE_8_BIT;
    state->ascp_config.min_bus_speed_kHz = com_config->min_bus_speed_KHz;
    state->ascp_config.max_bus_speed_kHz = com_config->max_bus_speed_KHz;
    state->ascp_config.bus_instance      = com_config->bus_instance;

    async_com_port_request.request_len =
      pb_encode_request(pb_encode_buffer,
                        sizeof(pb_encode_buffer),
                        &state->ascp_config,
                        sns_async_com_port_config_fields,
                        NULL);
    data_stream->api->send_request(data_stream, &async_com_port_request);
  }

  /** Determine size of sns_diag_sensor_state_raw as defined in
   *  sns_diag.proto
   *  sns_diag_sensor_state_raw is a repeated array of samples of
   *  type sns_diag_batch sample. The following determines the
   *  size of sns_diag_sensor_state_raw with a single batch
   *  sample */
  if(pb_encode_tag(&stream, PB_WT_STRING,
                    sns_diag_sensor_state_raw_sample_tag))
  {
    if(pb_encode_delimited(&stream, sns_diag_batch_sample_fields,
                               &batch_sample))
    {
      state->log_raw_encoded_size = stream.bytes_written;
    }
  }

  /* change-20260204-hyungchul: Register Interrupt immediately during init */

  if (state->is_dri)
  {

    if (NULL == state->interrupt_data_stream)
    {
      sns_service_manager *smgr = this->cb->get_service_manager(this);
      sns_stream_service *stream_svc = (sns_stream_service*)smgr->get_service(smgr, SNS_STREAM_SERVICE);
      stream_svc->api->create_sensor_instance_stream(stream_svc, this, state->irq_suid, &state->interrupt_data_stream);
    }

    if (NULL != state->interrupt_data_stream)
    {
      uint8_t buffer[20];
      sns_request irq_req = {
        .message_id = SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ,
        .request = buffer
      };
      sns_interrupt_req req_payload = sns_interrupt_req_init_default;
      

      req_payload.interrupt_trigger_type = SNS_INTERRUPT_TRIGGER_TYPE_RISING;
      req_payload.interrupt_num = 102;
      req_payload.interrupt_pull_type = SNS_INTERRUPT_PULL_TYPE_PULL_DOWN;
      req_payload.is_chip_pin = true;
      req_payload.interrupt_drive_strength = SNS_INTERRUPT_DRIVE_STRENGTH_2_MILLI_AMP;

      irq_req.request_len = pb_encode_request(buffer, sizeof(buffer), &req_payload, sns_interrupt_req_fields, NULL);
      if (irq_req.request_len > 0)
      {
        state->interrupt_data_stream->api->send_request(state->interrupt_data_stream, &irq_req);
        SNS_INST_PRINTF(HIGH, this, "change-20260204-hyungchul: Interrupt auto-registered on pin %d in init", req_payload.interrupt_num);
      }
    }
  }

  /* change-20260204-hyungchul: Force default ODR and activate sensor in init */

  state->temperature_info.sampling_rate_hz = TEMP_ODR_5;
  state->temperature_info.sampling_intvl = sns_convert_ns_to_ticks(1000000000.0 / state->temperature_info.sampling_rate_hz);
  
  ct7117x_set_temperature_config(this);
  SNS_INST_PRINTF(HIGH, this, "change-20260204-hyungchul: Auto-started sensor with ODR 5Hz");

  SNS_INST_PRINTF(LOW, this, "<sns_see_if__ init> success");
  return SNS_RC_SUCCESS;
}

/*
*	instance deinit
*/
sns_rc ct7117x_temp_inst_deinit(sns_sensor_instance *const this)
{
  ct7117x_instance_state *state = (ct7117x_instance_state*) this->state->state;
  sns_service_manager *service_mgr = this->cb->get_service_manager(this);
  sns_stream_service *stream_mgr = (sns_stream_service*) service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);
  inst_cleanup(state, stream_mgr);
  return SNS_RC_SUCCESS;
}

/*
*	instance_api:set_client_config
*/
sns_rc ct7117x_temp_inst_set_client_config(
    sns_sensor_instance * const this,
    sns_request const *client_request)
{
  ct7117x_instance_state *state = (ct7117x_instance_state*) this->state->state;

  if(client_request->message_id != SNS_STD_MSGID_SNS_STD_FLUSH_REQ)
  {
    SNS_INST_PRINTF(MED, this, "[%u] client_config: temp=%u",
                    state->temperature_info, client_request->message_id);
  }
  SNS_INST_PRINTF(HIGH, this, "set_client_config: temp=%u", client_request->message_id);
  
  state->client_req_id = client_request->message_id;
  float desired_sample_rate = 0;
  float desired_report_rate = 0;
  ct7117x_sensor_type sensor_type = TEMP_SENSOR_INVALID;
 // ct7117x_power_mode op_mode = INVALID_WORK_MODE;
  sns_temp_cfg_req *payload = (sns_temp_cfg_req*)client_request->request;
  sns_rc rv = SNS_RC_SUCCESS;
  bool temp_odr_change = false;
  sns_service_manager *mgr = this->cb->get_service_manager(this);
  sns_event_service *event_service = (sns_event_service*)mgr->get_service(mgr, SNS_EVENT_SERVICE);

  SNS_INST_PRINTF(LOW, this, "<sns_see_if__  set_client_config>");

  /* Turn COM port ON, *physical* */
  state->scp_service->api->sns_scp_update_bus_power(
      state->com_port_info.port_handle, true);

  if (client_request->message_id == SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG) 
  {
    // 1. Extract sample, report rates from client_request.
    // 2. Configure sensor HW.
    // 3. sendRequest() for Timer to start/stop in case of polling using timer_data_stream.
    // 4. sendRequest() for Intrerupt register/de-register in case of DRI using interrupt_data_stream.
    // 5. Save the current config information like type, sample_rate, report_rate, etc.
    if (client_request->message_id == SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG) 
	{
        desired_sample_rate = payload->sample_rate;
        desired_report_rate = payload->report_rate;
        sensor_type = payload->sensor_type;
        //op_mode = payload->op_mode;
    }
    if (desired_report_rate > desired_sample_rate) 
	{
    /* bad request. Return error or default report_rate to sample_rate */
    desired_report_rate = desired_sample_rate;
    }
    if (sensor_type == TEMP_TEMPERATURE) 
	{
	  /* change-20260204-hyungchul: Ignore ODR 0 request and Force 5Hz */

      if (desired_sample_rate == 0.0f) 
      {
         SNS_INST_PRINTF(HIGH, this, "change-20260204-hyungchul: Force ODR 5Hz to prevent shutdown (Ignored ODR 0)");
         desired_sample_rate = TEMP_ODR_5;
         state->temperature_info.sampling_rate_hz = desired_sample_rate;
         sensor_type = TEMP_TEMPERATURE;
      }
      
      sns_time temp_interval = (uint32_t)state->temperature_info.sampling_intvl;
      rv = ct7117x_validate_sensor_temp_odr(this);
	  SNS_INST_PRINTF(LOW, this, "temperature temp_interval = %u,state->temperature_info.sampling_intvl= %u", (uint32_t)temp_interval, (uint32_t)state->temperature_info.sampling_intvl);
	   if(temp_interval != state->temperature_info.sampling_intvl)
	  {
	    temp_odr_change = true;
		SNS_INST_PRINTF(LOW, this, "odr_change!!!!");
	  }
      if(rv != SNS_RC_SUCCESS
         && desired_sample_rate != 0)
      {
        // TODO Unsupported rate. Report error using sns_std_error_event.
        SNS_INST_PRINTF(ERROR, this, "sensor_temp ODR match error %d", rv);
		 sns_sensor_event *event = event_service->api->alloc_event(event_service, this, 0);
        if(NULL != event)
        {
          event->message_id = SNS_STD_MSGID_SNS_STD_ERROR_EVENT;
          event->event_len = 0;
          event->timestamp = sns_get_system_time();
          event_service->api->publish_event(event_service, this, event,
		  	&state->temperature_info.suid);
        }
        //return rv;
      }
    }
    if(true == temp_odr_change)
	{
	  ct7117x_set_temperature_config(this);
	}
	
 //   if (state->config_step == TEMP_CONFIG_IDLE) 
//	{
//	SNS_INST_PRINTF(ERROR, this, "call config function");                 //0122
//      ct7117x_reconfig_hw(this, sensor_type);
//    }

    ct7117x_send_config_event(this);
  }
  else if (client_request->message_id ==
          SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG) 
  {
    // ct7117x_run_self_test(this); // Original COM test.

    bool is_irq_suid_valid = (0 != sns_memcmp(&state->irq_suid, &((sns_sensor_uid){{0}}), sizeof(state->irq_suid)));

    // Per request, register interrupt on self-test request.
    if (NULL == state->interrupt_data_stream && is_irq_suid_valid)
    {
      sns_service_manager *smgr = this->cb->get_service_manager(this);
      sns_stream_service *stream_svc = (sns_stream_service*)smgr->get_service(smgr, SNS_STREAM_SERVICE);
      stream_svc->api->create_sensor_instance_stream(stream_svc, this, state->irq_suid, &state->interrupt_data_stream);
    }

    if (NULL != state->interrupt_data_stream)
    {
      uint8_t buffer[20];
      sns_request irq_req = {
        .message_id = SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ,
        .request = buffer
      };
      sns_interrupt_req req_payload = sns_interrupt_req_init_default;
      req_payload.interrupt_trigger_type = SNS_INTERRUPT_TRIGGER_TYPE_RISING;
      req_payload.interrupt_num = 102;
      req_payload.interrupt_pull_type = SNS_INTERRUPT_PULL_TYPE_PULL_DOWN;
      req_payload.is_chip_pin = true;
      req_payload.interrupt_drive_strength = SNS_INTERRUPT_DRIVE_STRENGTH_2_MILLI_AMP;

      irq_req.request_len = pb_encode_request(buffer, sizeof(buffer), &req_payload, sns_interrupt_req_fields, NULL);
      if (irq_req.request_len > 0)
      {
        state->interrupt_data_stream->api->send_request(state->interrupt_data_stream, &irq_req);
        SNS_INST_PRINTF(HIGH, this, "Interrupt registered for self-test on pin %d", req_payload.interrupt_num);
      }
    }
    else
    {
      SNS_INST_PRINTF(ERROR, this, "Failed to create interrupt stream for self-test");
    }
    state->new_self_test_request = false;
  }
  // Turn COM port OFF
  state->scp_service->api->sns_scp_update_bus_power(
      state->com_port_info.port_handle, false);

  SNS_INST_PRINTF(LOW, this, "<sns_see_if__  set_client_config> exit");
  return SNS_RC_SUCCESS;
}
