/**
 * @file sns_steng1ax_oem.h
 *
 * OEM to add MLC related info
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
#include "sns_steng1ax_sensor_instance.h"
#ifndef STENG1AX_XSENSOR_CONFIG_H
#define STENG1AX_XSENSOR_CONFIG_H 1
#if (STENG1AX_MLC_ENABLED | STENG1AX_FSM_ENABLED)

/* Fixed code - Not expected to change */
#define STENG1AX_DATA_MAX_SIZE          2048

struct steng1ax_sensor_data {
  uint8_t data[STENG1AX_DATA_MAX_SIZE];
  uint16_t len;
};

struct steng1ax_xsensor {
  uint16_t id;
};


#define MLC_SENSOR_CNT ARR_SIZE(steng1ax_mlc_sensor_list)
#define FSM_SENSOR_CNT ARR_SIZE(steng1ax_fsm_sensor_list)
/* Variable code to be updated by OEM */

/* XSENSOR_1 related flags */
#define STENG1AX_XSENSOR_1_NAME  "st_fsm_1"
//#define STENG1AX_XSENSOR_1_STREAM_TYPE  SNS_STD_SENSOR_STREAM_TYPE_SINGLE_OUTPUT
#define STENG1AX_XSENSOR_1_STREAM_TYPE  SNS_STD_SENSOR_STREAM_TYPE_ON_CHANGE
#define STENG1AX_XSENSOR_1_INT   XSENSOR_INT_1
#define STENG1AX_XSENSOR_1_TYPE  XSENSOR_TYPE_FSM /* XSensor 1  is FSM */

/* XSENSOR_2 related flags */
#define STENG1AX_XSENSOR_2_NAME  "stm_fsm_2"
#define STENG1AX_XSENSOR_2_STREAM_TYPE  SNS_STD_SENSOR_STREAM_TYPE_SINGLE_OUTPUT
#define STENG1AX_XSENSOR_2_INT   XSENSOR_INT_1
#define STENG1AX_XSENSOR_2_TYPE  XSENSOR_TYPE_FSM /* XSensor 2  is FSM */

/* ODR to be used by FSM/MLC - based on ODR that all MLC/FSM were designed for */
#define STENG1AX_FSM_MLC_ODR            (25.0f)

/* USE COMBINED FSM/MLC */
#define STENG1AX_MLC_FSM_COMBINE     0

static const struct steng1ax_xsensor steng1ax_mlc_sensor_list[] = {
};

static const struct steng1ax_xsensor steng1ax_fsm_sensor_list[] = {
  {
    .id = STENG1AX_XSENSOR_1,
  },
};

static const struct steng1ax_sensor_data steng1ax_xsensor_data = {
  /*  FSM*/
  .data = {

  },
  .len = 0,
};
#endif //(STENG1AX_MLC_ENABLED | STENG1AX_FSM_ENABLED)
#endif //STENG1AX_XSENSOR_H
