/** ======================================================================================
  @file steng1ax_fifo.c

  @brief steng1ax data acquisition HAL in FIFO mode

  Copyright (c) 2018-2022, STMicroelectronics.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

====================================================================================== **/

/**
*****************************************************************************************
                               Includes
*****************************************************************************************
*/
#include <stdbool.h>
#ifdef STENG1AX_DAE_ERROR_HANDLING_ENABLED
#include "sns_dd_error_event.h"
#endif
#include "sns_dd_if.h"
#include "sns_macros.h"
#include "sns_time.h"

/**
*****************************************************************************************
                               Constants/Macros
*****************************************************************************************
*/
#define STM_STENG1AX_REG_CTRL5               (0x14)
#define STM_STENG1AX_REG_WAKE_SRC            (0x21)
#define STM_STENG1AX_REG_FIFO_STATUS1        (0x26)
#define STM_STENG1AX_REG_FIFO_STATUS2        (0x27)
#define STM_STENG1AX_REG_FIFO_OUT_TAG        (0x40)

#define STM_STENG1AX_FIFO_OVERFLOW_MASK      (0x40)
#define STM_STENG1AX_FIFO_SAMPLE_SIZE	      (1 + 6)
#define STENG1AX_HW_MAX_FIFO                 (127)
#define STENG1AX_MAX_FIFO                    (100)
#define STM_STENG1AX_ACC_TAG                (0x1E)

#define STENG1AX_FSM_ENABLED                 0
#define STENG1AX_MLC_ENABLED                 0
#define STENG1AX_FSM_OUTS_ENABLED            0
#define STENG1AX_FSM_LC_ENABLED              0
#define STENG1AX_EX_TAP_TUNING_ENABLED       0
#define META_DATA_SIZE                        3

#ifdef STENG1AX_DAE_ERROR_HANDLING_ENABLED
#define SNS_DRIVER_ERROR_TYPE_BYTES_GREATER_THAN_FIFO_SIZE 0x01
#define SNS_DRIVER_ERROR_TYPE_REGISTER_READ 0x02
#endif

/**
*****************************************************************************************
                                  Static Functions
*****************************************************************************************
*/

#if 1 //for handling fsm, mlc, tap, etc
/* ------------------------------------------------------------------------------------ */
static void steng1ax_check_other_interrupts(
  sns_dd_handle_s*  dd_handle,
  uint8_t           fsm_reg_vals[8],  /* init to zeroes by caller */
  uint8_t           mlc_reg_vals[8],
  uint8_t           tap_reg_vals[2],
  uint8_t           qvar_reg_vals[3])  /* init to zeroes by caller */
{
#if 0
#if STENG1AX_FSM_ENABLED || STENG1AX_MLC_ENABLED
  uint8_t fsm_mlc_status[3] = {0};
  uint8_t fsm_outs[3]       = {0};
  uint8_t fsm_lc[2]         = {0};
  uint8_t mlc_src[8]        = {0};

  sns_com_port_vector_s state_vectors[] =
  {
    { .reg_addr = STM_STENG1AX_REG_FSM_STATUS_MAINPAGE,
      .buf_sz   = 3,
      .buf      = fsm_mlc_status  },
  };
  sns_com_port_read_reg_v( dd_handle, state_vectors, ARR_SIZE(state_vectors) );

#if (STENG1AX_FSM_OUTS_ENABLED || STENG1AX_MLC_ENABLED)
  if( (fsm_mlc_status[0] != 0) || (fsm_mlc_status[1] != 0) || (fsm_mlc_status[2] != 0))
  {
    /* Enable embedded src */
    uint8_t emb_mask = 0x80;
    sns_com_port_vector_s vectors_emb_w_1[] =
    {
      { .reg_addr = STM_STENG1AX_REG_FUNC_CFG,
        .buf_sz   = 1,
        .buf      = &emb_mask },
    };
    sns_com_port_write_reg_v( dd_handle, vectors_emb_w_1, ARR_SIZE(vectors_emb_w_1) );

    if( (fsm_mlc_status[0] != 0) || (fsm_mlc_status[1] != 0) )
    {
      /* Read fsm_lc from embedded space */
      sns_com_port_vector_s state_vectors_fsm_mlc[] =
      {
#if STENG1AX_FSM_LC_ENABLED
        { .reg_addr = STM_STENG1AX_REG_FSM_LONG_COUNTER_L,
          .buf_sz   = 2,
          .buf      = fsm_lc },
#endif
#if STENG1AX_FSM_OUTS_ENABLED
        { .reg_addr = STM_STENG1AX_REG_FSM_OUTS_1,
          .buf_sz   = 3,
          .buf      = fsm_outs },
#endif
      };
      sns_com_port_read_reg_v( dd_handle, state_vectors_fsm_mlc, ARR_SIZE(state_vectors_fsm_mlc) );
    }

#if STENG1AX_MLC_ENABLED
    if( fsm_mlc_status[2] != 0 )
    {
      /* Read mlc_src from embedded space */
      sns_com_port_vector_s state_vectors_mlc[] =
      {
        { .reg_addr = STM_STENG1AX_REG_MLC_SRC,
          .buf_sz   = 8,
          .buf      = mlc_src },
      };
      sns_com_port_read_reg_v( dd_handle, state_vectors_mlc, ARR_SIZE(state_vectors_mlc) );
    }
#endif

    /* Disable embedded src */
    emb_mask = 0x00;
    sns_com_port_vector_s vectors_emb_w_2[] =
    {
      { .reg_addr = STM_STENG1AX_REG_FUNC_CFG,
        .buf_sz   = 1,
        .buf      = &emb_mask },
    };
    sns_com_port_write_reg_v( dd_handle, vectors_emb_w_2, ARR_SIZE(vectors_emb_w_2) );
  }
#endif

#if STENG1AX_FSM_ENABLED
  if( (fsm_mlc_status[0] != 0) || (fsm_mlc_status[1] != 0) )
  {
    fsm_reg_vals[0] = STM_STENG1AX_REG_FSM_STATUS_MAINPAGE;
    fsm_reg_vals[1] = fsm_mlc_status[0];
    fsm_reg_vals[2] = fsm_mlc_status[1];

    /* Pass FSM_OUTS value for upto 3 FSMs */
    fsm_reg_vals[3] = fsm_outs[0];
    fsm_reg_vals[4] = fsm_outs[1];
    fsm_reg_vals[5] = fsm_outs[2];

    /* Pass FSM Long Counter value */
    fsm_reg_vals[6] = fsm_lc[0];
    fsm_reg_vals[7] = fsm_lc[1];
  }
#endif

#if STENG1AX_MLC_ENABLED
  if( fsm_mlc_status[2] != 0 )
  {
    mlc_reg_vals[0] = STM_STENG1AX_REG_MLC_STATUS_MAINPAGE;
    mlc_reg_vals[1] = fsm_mlc_status[2];
    mlc_reg_vals[2] = mlc_src[0];
    mlc_reg_vals[3] = mlc_src[1];
    mlc_reg_vals[4] = mlc_src[2];
    mlc_reg_vals[5] = mlc_src[3];
    mlc_reg_vals[6] = mlc_src[4];
    mlc_reg_vals[7] = mlc_src[5];
  }
#endif
#endif //STENG1AX_FSM_ENABLED || STENG1AX_MLC_ENABLED

#endif
}

/* ------------------------------------------------------------------------------------ */
static void steng1ax_notify_other_interrupts(
  sns_dd_handle_s*  dd_handle,
  notify_interrupt  notify_int_fptr,
  uint8_t           fsm_reg_vals[8],
  uint8_t           mlc_reg_vals[8],
  uint8_t           tap_reg_vals[2],
  uint8_t           qvar_reg_vals[3])
{
#if STENG1AX_FSM_ENABLED || STENG1AX_MLC_ENABLED
  if( fsm_reg_vals[0] != 0 )
  {
    notify_int_fptr( dd_handle, fsm_reg_vals );
  }
  if( mlc_reg_vals[0] != 0 )
  {
    notify_int_fptr( dd_handle, mlc_reg_vals );
  }
#endif
}
#endif

/* ------------------------------------------------------------------------------------ */
static sns_com_port_status_e
steng1ax_fifo_get_data( sns_dd_handle_s*    dd_handle,
                       read_sensor_data    data_read_fptr,
                       notify_interrupt    notify_int_fptr,
                       int32_t             acc_delay,
                       int32_t*            delay_us,
                       bool*               call_again,
                       int32_t*            num_samples )
{
  sns_com_port_status_e status;
  uint8_t wake_src       = 0;
  uint8_t fifo_status[2] = {0};
  uint8_t ctrl5          = 0;

  /* Status registers for use in this function */
  sns_com_port_vector_s state_vectors[] =
  {
    { .reg_addr = STM_STENG1AX_REG_CTRL5,
      .buf_sz   = 1,
      .buf      = &ctrl5       },
    { .reg_addr = STM_STENG1AX_REG_WAKE_SRC,
      .buf_sz   = 1,
      .buf      = &wake_src       },
    { .reg_addr = STM_STENG1AX_REG_FIFO_STATUS1,
      .buf_sz   = 2,
      .buf      = fifo_status     },
  };
  status = sns_com_port_read_reg_v( dd_handle, state_vectors, ARR_SIZE(state_vectors) );
      
  if( status == SNS_COM_PORT_STATUS_SUCCESS && (wake_src != 0xff))
  {
    *num_samples = (int32_t)fifo_status[1];

#ifdef STENG1AX_DAE_ERROR_HANDLING_ENABLED
    //throw error event if number of samples to read from FIFO is greater than FIFO size
    if (*num_samples > STENG1AX_MAX_FIFO)
    {
      driver_error_data drv_err_data = {0};
      drv_err_data.error_code = SNS_DRIVER_ERROR_TYPE_BYTES_GREATER_THAN_FIFO_SIZE;
      send_driver_error_event(dd_handle,&drv_err_data);
    }
#endif

    if( !*num_samples && (fifo_status[0] & STM_STENG1AX_FIFO_OVERFLOW_MASK) )
    {
      *num_samples = STENG1AX_HW_MAX_FIFO;
    }

#if 1 //for handling fsm, mlc, tap, etc
    uint8_t fsm_reg_vals[8] = {0};
    uint8_t mlc_reg_vals[8] = {0};
    uint8_t tap_reg_vals[2] = {0};
    uint8_t qvar_reg_vals[3] = {0};
    uint16_t count_h = fifo_status[1];
    steng1ax_check_other_interrupts(dd_handle, fsm_reg_vals, mlc_reg_vals, tap_reg_vals, qvar_reg_vals);
#endif

    if( *num_samples > 0 )
    {
      sns_com_port_data_vector_s data_vectors[] = 
      {
        { .mem_addr = &ctrl5, .reg_addr = 0,
          .buf_sz   = 1 },
        { .mem_addr = fifo_status, .reg_addr = 0,
          .buf_sz   = 2 },
        { .reg_addr = STM_STENG1AX_REG_FIFO_OUT_TAG,
          .buf_sz   = *num_samples * STM_STENG1AX_FIFO_SAMPLE_SIZE },
      };

      status = data_read_fptr( dd_handle, data_vectors, ARR_SIZE(data_vectors) );
      *delay_us = 0;
      *call_again = false;
    }

    wake_src &= 0x0F;

    if( wake_src != 0 )
    {
      uint8_t regvals[8] = {0};
      regvals[0] = STM_STENG1AX_REG_WAKE_SRC;
      regvals[1] = wake_src;
      notify_int_fptr( dd_handle, regvals );
    }

#if 1 //for handling fsm, mlc, tap, etc
    steng1ax_notify_other_interrupts(dd_handle, notify_int_fptr, fsm_reg_vals, mlc_reg_vals, tap_reg_vals, qvar_reg_vals);
#endif
  }
#ifdef STENG1AX_DAE_ERROR_HANDLING_ENABLED
  else
  {
    driver_error_data drv_err_data = {0};
    drv_err_data.error_code = SNS_DRIVER_ERROR_TYPE_REGISTER_READ;
    send_driver_error_event(dd_handle,&drv_err_data);
  }
#endif

  return status;
}

/* ------------------------------------------------------------------------------------ */
static sns_com_port_status_e
steng1ax_fifo_parse_eng_data( sns_dd_handle_s*        dd_handle,
                                 uint8_t const*          data_ptr,
                                 uint16_t                data_bytes,
                                 notify_accel_data       notify_data_fptr,
                                 notify_accel_sample_cnt notify_cnt_fptr )
{
  sns_com_port_status_e rv = SNS_COM_PORT_STATUS_ERROR;

  if( data_bytes >= STM_STENG1AX_FIFO_SAMPLE_SIZE )
  {
    int i;
    int32_t eng_sample_count = 0;
    int32_t loop_start = META_DATA_SIZE; /* first 1 bytes are meta data */

    /* counts up number of Eng samples */
    for( i = loop_start; i < data_bytes; i += STM_STENG1AX_FIFO_SAMPLE_SIZE )
    {
      uint8_t tag = data_ptr[i] >> 3;
      if( tag == STM_STENG1AX_ACC_TAG )
      {
        eng_sample_count++;
      }
    }

    if( eng_sample_count > 0 )
    {
      notify_cnt_fptr( dd_handle, eng_sample_count );
      for( i = loop_start; i < data_bytes; i += STM_STENG1AX_FIFO_SAMPLE_SIZE )
      {
        uint8_t tag = data_ptr[i] >> 3;
        if( tag == STM_STENG1AX_ACC_TAG )
        {
          int16_t x,y,z;
          x = ((data_ptr[i+2] << 8) | data_ptr[i+1]);
          y = ((data_ptr[i+4] << 8) | data_ptr[i+3]);
          z = ((data_ptr[i+6] << 8) | data_ptr[i+5]);
          notify_data_fptr( dd_handle, x, y, z );
        }
      }
      rv = SNS_COM_PORT_STATUS_SUCCESS;
    }
  }
  return  rv;
}

/**
*****************************************************************************************
                            Global Function Pointer Table
*****************************************************************************************
*/

sns_dd_if_s steng1ax_fifo_hal_table =
  {
    .get_data = steng1ax_fifo_get_data,
    .parse_accel_data = steng1ax_fifo_parse_eng_data
  };

sns_dd_if_s steng1ax_fifo_hal_table2 =
  {
    .get_data = steng1ax_fifo_get_data,
    .parse_accel_data = steng1ax_fifo_parse_eng_data
  };

