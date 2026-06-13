#ifndef UART_H
#define UART_H
/*==================================================================================================

FILE: Uart.h

DESCRIPTION: This module provides the driver software for the UART.

Copyright (c) 2013-2015, 2018, 2021, 2023 Qualcomm Technologies, Inc.
        All Rights Reserved.
Qualcomm Technologies, Inc. Confidential and Proprietary.

==================================================================================================*/
/*==================================================================================================
                                            DESCRIPTION
====================================================================================================

GLOBAL FUNCTIONS:
   uart_close
   uart_open
   uart_open_ex
   uart_receive
   uart_transmit
   uart_transmit_v2
   uart_tx_inject_break
   uart_is_clnt_active_in_island
==================================================================================================*/
/*==================================================================================================
Edit History

$Header: //components/rel/core.qdsp6/8.2.3/api/common/buses/uart.h#1 $

when       who     what, where, why
--------   ---     --------------------------------------------------------
10/03/23   pcr     Added uart API to set/get power/performance levels
03/28/23   gkr     Added UART VFCP support
01/04/23   gkr     Added New API uart_is_clnt_active_in_island()
11/02/21   jc      Add support for Scatter gather list and timestamps in API Version 3.

==================================================================================================*/

/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/
#include "com_dtypes.h"
#include "qup_common.h"

#define  UART_LITE_API_H_VERSION  7

/*==================================================================================================
                                             ENUMERATIONS
==================================================================================================*/
/*!
 * \note Clients are advised to move to uart_open_ex().
 */
typedef enum
{
   UART_INSTANCE_01,
   UART_INSTANCE_02,
   UART_INSTANCE_03,
   UART_INSTANCE_04,
   UART_INSTANCE_05,
   UART_INSTANCE_06,
   UART_INSTANCE_07,
   UART_INSTANCE_08,
   UART_INSTANCE_09,
   UART_INSTANCE_10,
   UART_INSTANCE_11,
   UART_INSTANCE_12,
   UART_INSTANCE_13,
   UART_INSTANCE_14,
   UART_INSTANCE_15,
   UART_INSTANCE_16,
   UART_INSTANCE_17,
   UART_INSTANCE_18,
   UART_INSTANCE_19,
   UART_INSTANCE_20,
   UART_INSTANCE_21,
   UART_INSTANCE_22,
   UART_INSTANCE_23,
   UART_INSTANCE_24,
   UART_INSTANCE_25,
   UART_INSTANCE_26,
   UART_INSTANCE_27,
   UART_INSTANCE_28,
   UART_INSTANCE_29,
   UART_INSTANCE_30,
   UART_INSTANCE_31,
   UART_INSTANCE_32,

   UART_MAX_PORTS,
   UART_DEBUG_INSTANCE,
}uart_port_id;

typedef enum
{
   UART_SUCCESS = 0,
   
   UART_PROTOCOL_ERROR = 0x1000,
   UART_PROTOCOL_ERROR_BREAK_START,
   UART_PROTOCOL_ERROR_BREAK_END,
   UART_PROTOCOL_ERROR_PARITY,
   UART_PROTOCOL_ERROR_FRAMING,
   
   UART_GENERIC_ERROR = 0x8000,
   UART_ERROR,
}uart_result;

typedef enum
{
  UART_5_BITS_PER_CHAR  = 0,
  UART_6_BITS_PER_CHAR  = 1,
  UART_7_BITS_PER_CHAR  = 2,
  UART_8_BITS_PER_CHAR  = 3,
} uart_bits_per_char;

typedef enum
{
  UART_0_5_STOP_BITS    = 0,
  UART_1_0_STOP_BITS    = 1,
  UART_1_5_STOP_BITS    = 2,
  UART_2_0_STOP_BITS    = 3,
} uart_num_stop_bits;

typedef enum
{
  UART_NO_PARITY        = 0,
  UART_ODD_PARITY       = 1,
  UART_EVEN_PARITY      = 2,
  UART_SPACE_PARITY     = 3,
} uart_parity_mode;

typedef enum
{
   UART_NORMAL_MODE = 0xABCDEF00, // UART port will be used only in non-island mode.
                                  // The interrupts will not be marked as island and the
                                  // UART calls from client are expected to be non-island code.

   UART_ISLAND_MODE = 0xF1E2D3C4, // UART port will be used in island mode. The interrupts will be
                                  // marked as island interrupts. Wake up feature, if enabled in
                                  // configuration file, will be registered at uart_open with PDC and
                                  // gets disabled at the Q6 level.
}uart_operating_mode;

typedef enum
{
   UART_CALLBACK_TYPE_DEFAULT    = 0,
   UART_CALLBACK_TYPE_V1         = UART_CALLBACK_TYPE_DEFAULT,
   UART_CALLBACK_TYPE_V2         = 1,
}uart_callback_type;

typedef struct
{
   uint64       timestamp;
   boolean      valid;
}uart_timestamp_type;

typedef struct
{
   uint8* buf;
   uint32 len;
   uint32 flags;
} uart_descriptor;
 
typedef void* uart_handle;

typedef void(*UART_CALLBACK)(uint32 num_bytes, void *cb_data);

/*!
 * \brief 
 * #tf_cb_data holds cb_data passed to uart_transmit_v2 / uart_receive api.
 * #open_cb_data holds cb_data passed to as a part of #uart_open_config to uart_open/uart_open_ex api.
 */
typedef void(*UART_CALLBACK_V2)(uart_result status, uint32 num_bytes, void *tf_cb_data, void *open_cb_data, uart_timestamp_type* timestamp);

typedef void(*UART_WAKEUP_CALLBACK)(void *cb_data);

#define UART_TIMESTAMP_ENABLE         0x00000001     /* Enables Timestamp */
#define UART_TIMESTAMP_CONFIG_OFFSET  0x00000002     /* Configures Byte Offset for Timestamp capture */
#define UART_TIMESTAMP_AUTO_DISABLE   0x00000004     /* Auto disables timestamp logic after 1 valid capture*/

/*==================================================================================================
                                             STRUCTURES
==================================================================================================*/

typedef struct
{
   uint32                baud_rate;
   uart_parity_mode      parity_mode;
   uart_num_stop_bits    num_stop_bits;
   uart_bits_per_char    bits_per_char;
   uint32                enable_loopback;
   uint32                enable_flow_ctrl;

   // The callbacks will be called from ISR context.
   // Necessary precautions needs to be taken in these funtions to make sure not ISR guidelines
   // are violated.
   // DONT call uart_transmit or uart_receive API from these callbacks.
   uart_callback_type    cb_type;
   void*                 cb_data;
   union
   {
      UART_CALLBACK         tx_cb_isr;
      UART_CALLBACK_V2      tx_cb_isr_v2;
   };
   union
   {
      UART_CALLBACK         rx_cb_isr;
      UART_CALLBACK_V2      rx_cb_isr_v2;
   };
   uart_operating_mode   mode;
   boolean               enable_wake_feature;
   boolean               is_uart_vfcp;
}uart_open_config;

/*==================================================================================================
                                        FUNCTION PROTOTYPES
==================================================================================================*/


/*!
 * \brief De-initializes the UART port
 *
 * Releases  clock, interrupt, and gpio handles related to this UART
 * Cancels any pending transfers.
 *
 * DON'T call from ISR context.
 * DON'T call from island mode.
 *
 * \param in uart_handle Handle
 * \return UART_SUCCESS|UART_ERROR
 */

uart_result      uart_close(uart_handle h);

/*!
 * \brief Initializes UART port
 *
 * Opens the UART port and configures the corresponding clocks, interrupts, and gpio.
 *
 * If the wakeup feature and island mode is enabled in the open configuration, the wakeup 
 * interrupt gets registered at the PDC during UART open itself(as PDC is not accessible in 
 * island).
 * In Island mode, the power APIs will work only for disabling/enabling of clocks and interrupt 
 * at Q6 l2vic and PDC enablement/disablement is not possible.
 * Hence, to make this feature work PDC enablement happens at UART open itself.
 *
 * If the use case is complete and there is no expectation for UART to work in sleep mode,
 * clients are expected to call uart_close from non-island mode. 
 * This will make sure the PDC interrupt gets disabled.
 *
 * DON'T call from ISR context.
 * DON'T call from island mode.
 *
 * \param  in uart_handle* h to get handle
 * \param  in uart_port_id id
 * \param  in uart_open_config* config structure that holds all config data
 * \return UART_SUCCESS|UART_ERROR
 * \note   Clients are advised to migrate to uart_open_ex().
 */

uart_result      uart_open(uart_handle* h, uart_port_id id, uart_open_config* config);

/*!
 * \brief Initializes UART port
 *
 * Opens the UART port and configures the corresponding clocks, interrupts, and gpio.
 *
 * If the wakeup feature and island mode is enabled in the open configuration, the wakeup 
 * interrupt gets registered at the PDC during UART open itself(as PDC is not accessible in 
 * island).
 * In Island mode, the power APIs will work only for disabling/enabling of clocks and interrupt 
 * at Q6 l2vic and PDC enablement/disablement is not possible.
 * Hence, to make this feature work PDC enablement happens at UART open itself.
 *
 * If the use case is complete and there is no expectation for UART to work in sleep mode,
 * clients are expected to call uart_close from non-island mode. 
 * This will make sure the PDC interrupt gets disabled.
 *
 * DON'T call from ISR context.
 * DON'T call from island mode.
 *
 * \param in uart_handle* h to get handle
 * \param in qup QUP type to be used, refer qup_common.h
 * \param in se_index index of SE in QUP.
 * \param in uart_open_config* config structure that holds all config data
 * \return UART_SUCCESS|UART_ERROR
 * \version API header version 2
 */

uart_result      uart_open_ex(uart_handle* h, QUP_TYPE qup, uint32 se_index, uart_open_config* config);

/*!
 * \brief Queues the buffer provided for receiving the data
 *
 * Asynchronous call. The rx_cb_isr will be called when the rx transfer completes.
 * The buffer is owned by the UART driver till the rx_cb_isr is called.
 *
 * There has to be always a pending RX. UART HW has a limited buffer(FIFO) and if there is
 * no SW buffer available,
 *    For HS-UART, the flow control will de-assert the RFR line.
 *    For Debug UART, the data will be lost as there is no HW flow control lines available.
 *
 * Call uart_receive immediately after uart_open to queue a buffer.
 * After every rx_cb_isr, from a different non-ISR thread, queue the next transfer.
 *
 * Multiple Buffers can be queued. Max number is based on driver internal memory constraints.
 *
 * DON'T call from ISR context
 * If Buffers are cached then its address needs to be aligned to the processors cache line size.
 *
 * \param in uart_handle Handle
 * \param in char* buf Buffer to be filled with data.
 * \param in uint32 buf_size Size of the buffer being passed. Must be >=4 and a multiple of 4.
 * \param in void* cb_data Call back data to be passed when rx_cb_isr is called during rx completion
 * \return UART_SUCCESS|UART_ERROR
 */

uart_result      uart_receive(uart_handle h, uint8* buf, uint32 buf_size, void* cb_data);

/*!
 * \brief Transmits the data from given buffer.
 *
 * Asynchronous call. The buffer will be queued for TX and when transmit is completed, the
 * tx_cb_isr will be called.
 *
 * The buffer is owned by the UART driver till the tx_cb_isr is called.
 *
 * DON'T call from ISR context
 * If Buffers are cached then its address needs to be aligned to the processors cache line size.
 *
 * There can be maximum of only one buffer queued at a time.
 *
 * \param in uart_handle Handle
 * \param in char* buf Buffer with data for transmit.
 * \param in uint32 bytes_to_tx bytes of data to transmit
 * \param in void* cb_data Call back data to be passed when tx_cb_isr is called during tx completion
 * \return UART_SUCCESS|UART_ERROR
 */

uart_result      uart_transmit(uart_handle h, uint8* buf, uint32 bytes_to_tx, void* cb_data);

/*!
 * \brief Transmits the data of Multiple Vectors.
 *
 * Asynchronous call. The buffer will be queued for TX and when transmit is completed, the
 * tx_cb_isr will be called.
 *
 * The buffer is owned by the UART driver till the tx_cb_isr is called.
 *
 * DON'T call from ISR context
 * If Buffers are cached then its address needs to be aligned to the processors cache line size.
 *
 * There can be maximum of only one buffer queued at a time.
 *
 * \param in uart_handle Handle
 * \param in uart_descriptor* desc Array of descriptors
 * \param in uint16 num_descriptors  Number of descriptors
 * \param in void* cb_data Call back data to be passed when tx_cb_isr is called during tx completion
 * \return UART_SUCCESS|UART_ERROR
 */

uart_result      uart_transmit_v2(uart_handle h, uart_descriptor* desc, uint16 num_descriptors, void* cb_data);

/*!
 * \brief Powers down the UART core by clocking off all the resources.
 *
 * It is client's responsibility to make sure pending transmit is completed.
 * if wake_on_rx is enabled, then the wakeup interrupt is registered.
 *
 * if UART is opened with non-island only mode, this call will enable the wakeup interrupt at PDC
 *
 * if UART is opened with island mode, this call will enable the wakeup interrupt at the Q6
 * level. ( PDC will enabled at the UART open itself )
 *
 * DON'T call from ISR context
 *
 *
 * \param in uart_handle Handle
 * \param in boolean wake_on_rx whether to wake up the system on receiving data ( falling edge on line )
 * \param in UART_WAKEUP_CALLBACK callback function to call when wake_on_rx is enabled and device wakeups
 * \param in void* wakeup_cb_data data to pass to the callback function on wakeup
 * \return UART_SUCCESS|UART_ERROR
 */

uart_result      uart_power_off(uart_handle h, boolean wake_on_rx, UART_WAKEUP_CALLBACK wake_cb, void* wake_cb_data);

/*!
 * \brief Powers on the UART core by clocking on all the resources.
 *
 * DON'T call from ISR context
 *
 * \param in uart_handle Handle
 * \return UART_SUCCESS|UART_ERROR
 */
uart_result      uart_power_on(uart_handle h);

/*!
 * \brief Induces break on TX line when Uart is active.
 *
 * DON'T call from ISR context, If Break is Set further TX requests will fail.
 * Make sure to release break for further transmit requests.
 *
 * \param in uart_handle   Handle
 * \param in set_release   Set TRUE to set break and FALSE to release break
 * \return UART_SUCCESS|UART_ERROR
 */
uart_result      uart_tx_inject_break(uart_handle h, boolean set_release);

/*!
 * \brief Enable capture of Timestamp from RX Engine
 *
 * DON'T call from ISR context
 *
 * \param in uart_handle Handle
 * \param in byte_offset Offset of byte at which timestamp is to be captured.
 * \param in ts_flags Timestamp Flags, Client to send OR'd #UART_TIMESTAMP_* flags.
 * \return UART_SUCCESS|UART_ERROR
 */
uart_result      uart_enable_rx_timestamp(uart_handle h, uint32 byte_offset, uint32 ts_flags);

/*!
 * \brief Disables capture of Timestamp from RX Engine
 *
 * DON'T call from ISR context
 *
 * \param in uart_handle Handle
 * \return UART_SUCCESS|UART_ERROR
 */
uart_result      uart_disable_rx_timestamp(uart_handle h);

/*!
 * \brief Adds/Removes Uart RX wake up interrupt from island list based on client
 *
 * DON'T call from ISR context, to be called after uart_open() and before uart_power_off()
 *
 * \param in uart_handle Handle
 * \param in boolean TRUE if client will be in island / FALSE if client is not in island
 * \return UART_SUCCESS|UART_ERROR
 */
uart_result      uart_is_clnt_active_in_island(uart_handle handle, boolean is_active_in_island);


/*!
*
*  \brief Client can call the API to set different power profiles, this API alone can be used instead of 
*  using uart_power_on() and uart_power_off(). This API also facilitates clients to set a performance level
*  like turbo, where client can expect better KPIs as qup resources are voted for higer performance.
*  
*  Flow of usage can be as below
*  uart_open(.., &uart_handle)
*  uart_set_power_level(uart_handle, QUP_POWER_ON) -> set to low svs
*  uart_write(...)
*  uart_set_power_level(uart_handle, QUP_POWER_TURBO) -> set for turbo
*  uart_write(...)
*  uart_set_power_level(uart_handle, QUP_POWER_OFF) -> set to off
*  uart_close(...)
*
*  \param in  handle               Handle to the UART instance.
*  \param in  profile              Indicates which performance profile to set (off/low/nominal/turbo),
*                                  refer qup_common.h for details.
*  \return UART_SUCCESS|UART_ERROR
*/
uart_result      uart_set_power_level(uart_handle handle, qup_power_profile profile);


/*!
*
*  \brief Client can call the API to get the current power profile choosen, then take next appropriate action,
*  like updating to power level higer or lower based on the use case need.
*
* \param in handle           Handle to the UART instance.
*
*  \return
*  0 -- Getting power level failed. 
*  N -- N is the power level set on the UART instance.
*
*/
qup_power_profile      uart_get_power_level(uart_handle handle);

#endif
