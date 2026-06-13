#pragma once
/**
 * @file sns_steng1ax_build_config.h
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

/** Default build flags for debugging */

/** Enables DBG_PRINTF macro for default debug logs */
#ifndef STENG1AX_LOG_VERBOSE_DEFAULT
#define STENG1AX_LOG_VERBOSE_DEFAULT    1
#endif
/** Enables DBG_PRINTF_EX macro for extensive debug logs */
#define STENG1AX_LOG_VERBOSE_EX         0

/** Enables STENG1AX_INST_DEBUG_TS macro for instance and timestamp logs */
#define STENG1AX_DEBUG_TS               0

/** Enables DEBUG_TS_EST macro for logs in Timestamp estimation function */
#define STENG1AX_DEBUG_TS_EST           0


/** Enables logs of sensor data, very costly, do not enable unless required*/
#define STENG1AX_DEBUG_SENSOR_DATA      0

/** Enables logs of register read/write operation */
#define STENG1AX_DUMP_REG               0

/** Enables the logs for sanity tests */
#define STENG1AX_AUTO_DEBUG             0

/** Enables debug code for i3c, reads all CCC information */
#define STENG1AX_DEBUG_I3C              0

/** Disables raw log packets of samples and interrupts */
#ifndef SNS_LOG_DISABLED
#define STENG1AX_LOGGING_DISABLED       0
#else
#define STENG1AX_LOGGING_DISABLED       1
#endif

/** Enables embedded sensor features */

/** Enables FSM/MLC features */
#define STENG1AX_ESP_XSENSOR_1          0

#define STENG1AX_ESP_XSENSOR_2          0

/** Enabled FSM support for ESP sensors */
#define STENG1AX_FSM_ENABLED            0

/** Enabled FSM support for ESP sensors */
#define STENG1AX_MLC_ENABLED            0

/** ENABLE OEM SPECIFIC XSENSOR EVENT HANDLING */
#define STENG1AX_XSENSOR_OEM_EVENT_HANDLING 0

/** Enables LITE Driver */
#define STENG1AX_LITE_DRIVER_ENABLED    0

/** return special instance pointer if flush req is handled*/
#define STENG1AX_FLUSH_SPECIAL_HANDLING   1

#define MAX_LOW_LATENCY_RATE STENG1AX_ODR_800

/** set sensor count for 4x sensors */
#define SENSOR_CNT                      4

#define STENG1AX_INTR_HW_IDX            0

#define STENG1AX_DRDY_OUT_ENABLED       0

#define STENG1AX_EXT_CLK_ENABLE         0

/** Enables DAE support */
#ifdef SNS_ENABLE_DAE
#define STENG1AX_DAE_ENABLED             1
#else
#define STENG1AX_DAE_ENABLED             0
#endif

/** Disabled IBI support irrespetive of what is set in registry */
#define STENG1AX_FORCE_IBI_DISABLED      0

#ifdef SSC_TARGET_NO_I3C_SUPPORT
#define STENG1AX_USE_I3C                 0
#ifdef BUILD_DB
#define SDM_845_BUILDS                  0
#else
#define SDM_845_BUILDS                  1
#endif
#else
#define STENG1AX_USE_I3C                 1
#define STENG1AX_USE_RSTDAA              1
#define SDM_845_BUILDS                  0
#endif

#define STENG1AX_ODR_REGISTRY_FEATURE_ENABLE  1
#define STENG1AX_DAE_TIMESTAMP_TYPE           1
#define STENG1AX_REGISTRY_WRITE_EVENT         1
#define STENG1AX_FSM_LC_ENABLED               0
#define STENG1AX_FSM_OUTS_ENABLED             0

//By default disable Xsensor for hw_id=1
#define STENG1AX_SUB_XSENSOR_DISABLED         1


/** Handle OEM specific requests */
#define STENG1AX_SENSOR_OEM_CONFIG      0
#define STENG1AX_USE_OEM_TS_OFFSET      0
#define STENG1AX_OEM_FACTORY_CONFIG     0

#if STENG1AX_LOG_VERBOSE_EX
#define STENG1AX_LOG_VERBOSE_DEFAULT    1
#endif

#if STENG1AX_AUTO_DEBUG

#define STENG1AX_INST_AUTO_DEBUG_PRINTF(prio, inst, ...) do { \
  SNS_INST_PRINTF(prio, inst, __VA_ARGS__); \
} while (0)
#define STENG1AX_AUTO_DEBUG_PRINTF(prio, sensor, ...) do { \
  SNS_PRINTF(prio, sensor, __VA_ARGS__); \
} while (0)

#else
#define STENG1AX_AUTO_DEBUG_PRINTF(prio, sensor,...) UNUSED_VAR(sensor);
#define STENG1AX_INST_AUTO_DEBUG_PRINTF(prio, inst,...) UNUSED_VAR(inst);
#endif

#if STENG1AX_DEBUG_TS

#define STENG1AX_INST_DEBUG_TS(prio, inst, ...) do { \
SNS_INST_PRINTF(prio, inst, __VA_ARGS__); \
} while (0)

#else
#define STENG1AX_INST_DEBUG_TS(prio, sensor, ...)
#endif

#if STENG1AX_DEBUG_TS_EST

#define DEBUG_TS_EST(prio, inst, ...) do { \
  SNS_INST_PRINTF(prio, inst, __VA_ARGS__); \
} while (0)

#else
#define DEBUG_TS_EST(prio, sensor, ...)
#endif

#if STENG1AX_LOG_VERBOSE_EX

#define DBG_PRINTF_EX(prio, sensor, ...) do { \
  SNS_PRINTF(prio, sensor, __VA_ARGS__); \
} while (0)

#define DBG_INST_PRINTF_EX(prio, inst, ...) do { \
  SNS_INST_PRINTF(prio, inst , __VA_ARGS__); \
} while (0)

#else
#define DBG_PRINTF_EX(prio, sensor,...) UNUSED_VAR(sensor);
#define DBG_INST_PRINTF_EX(prio, inst,...) UNUSED_VAR(inst);
#endif

#if STENG1AX_LOG_VERBOSE_DEFAULT

#define DBG_PRINTF(prio, sensor, ...) do { \
  SNS_PRINTF(prio, sensor, __VA_ARGS__); \
} while (0)

#define DBG_INST_PRINTF(prio, inst, ...) do { \
  SNS_INST_PRINTF(prio, inst , __VA_ARGS__); \
} while (0)

#else
#define DBG_PRINTF(prio, sensor,...) UNUSED_VAR(sensor);
#define DBG_INST_PRINTF(prio, inst,...) UNUSED_VAR(inst);
#endif


#if STENG1AX_LITE_DRIVER_ENABLED
#define STENG1AX_REGISTRY_DISABLED  1
#define STENG1AX_ATTRIBUTE_DISABLED 1
#define STENG1AX_ISLAND_DISABLED    1
#define STENG1AX_POWERRAIL_DISABLED 1
#endif
