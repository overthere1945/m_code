/**
    @file  i2c_api.h
    @brief I2C APIs
 */
/*=============================================================================
         Copyright (c) Qualcomm Technologies, Incorporated.
                        All rights reserved.
         Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

/**

 @file i2c_api.h

 @addtogroup i2c_api
 @{

 @brief Inter-Integrated Circuit (I2C) and Improved I2C (I3C)

 @details I2C and I3C are 2-wire buses used to connect high and low speed
          peripherals to a processor or a microcontroller. Common peripherals
          include touch screen controllers, accelerometers, gyros, and ambient
          light and temperature sensors. All I2C api's, status codes and definitions
          are applicable to I3C unless specified otherwise.

          The 2-wire bus comprises a data line, a clock line, and basic START,
          STOP, ACK/NACK/TRANSITION and PARITY framing format to drive transfers
          on the bus. A peripheral is also referred to as a slave. The processor
          or microcontroller implements the master as defined in the I2C
          specification Rev 6 and the MIPI I3C specification Rev 1.0. This
          API provides the software interface to access the I2C and I3C master
          hardware.

 @code {.c}

 //
 // The code sample below demonstrates the use of this interface.
 //

 typedef struct client_ctxt
 {
   i2c_slave_config *config;
   i2c_descriptor   *desc;
   uint32           num_descs;
 } client_ctxt;

 void sample (void)
 {
   void     *client_handle  = NULL;
   uint8_t  buffer[4]       = { 1, 2, 3, 4 };
   uint32   transferred     = 0;

   i2c_status       res = I2C_SUCCESS;
   i2c_descriptor   desc[2];
   i2c_slave_config config;

   client_ctxt ctxt;

   // ASYNCHRONOUS MODE
   // Obtain a client specific connection handle to the i2c bus instance 1
   res = i2c_open_ex (QUP_0, 0, &client_handle);

   // Configure the bus speed and slave address
   config.bus_frequency_khz             = I2C_FAST_MODE_FREQ_KHZ;
   config.slave_address                 = 0x36;
   config.mode                          = I2C;
   config.slave_max_clock_stretch_us    = 500;
   config.core_configuration1           = 0;
   config.core_configuration2           = 0;

   // <S>  - START
   // <P>  - STOP
   // <Sr> - Repeat Start or Re-Start
   // <A>  - Acknowledge
   // <T>  - Transition
   // <N>  - Not-Acknowledge
   // <R>  - Read Transfer
   // <W>  - Write Transfer

   // Single write transfer of 4 bytes
   // I2C: <S><slave_address><W><A><data1><A><data2><A>...<dataN><A><P>
   desc[0].buffer      = buffer;
   desc[0].length      = 4;
   desc[0].flags       = I2C_WRITE_TRANSFER;

   ctxt.config         = &config;
   ctxt.desc           = desc;
   ctxt.num_descs      = 1;

   res = i2c_transfer (client_handle, &config, &desc[0], 1, client_callback, &ctxt, 0, NULL);

   // Single read transfer of 4 bytes
   // I2C: <S><slave_address><R><A><data1><A><data2><A>...<dataN><N><P>
   desc[0].buffer      = buffer;
   desc[0].length      = 4;
   desc[0].flags       = I2C_READ_TRANSFER;

   ctxt.config         = &config;
   ctxt.desc           = desc;
   ctxt.num_descs      = 1;

   res = i2c_transfer (client_handle, &config, &desc[0], 1, client_callback, &ctxt, 0, NULL);

   // Read 4 bytes from a register 0x01 on a sensor device
   // I2C: <S><slave_address><W><A><0x01><A><Sr><slave_address><R><A><data1><A><data2><A>...<dataN><N><P>
   uint8_t reg = 0x01;

   desc[0].buffer      = &reg;
   desc[0].length      = 1;
   desc[0].flags       = I2C_WR_RD_WRITE;

   desc[1].buffer      = buffer;
   desc[1].length      = 4;
   desc[1].flags       = I2C_WR_RD_READ;

   ctxt.config         = &config;
   ctxt.desc           = desc;
   ctxt.num_descs      = 2;

   res = i2c_transfer (client_handle, &config, &desc[0], 2, client_callback, &ctxt, 0, NULL);

   // Read 4 bytes from eeprom address 0x0102
   // I2C: <S><slave_address><W><A><0x01><A><0x02><A><Sr><slave_address><R><A><data1><A><data2><A>...<dataN><N><P>
   uint8_t reg[2]      = { 0x01, 0x02 };

   desc[0].buffer      = reg;
   desc[0].length      = 2;
   desc[0].flags       = I2C_WR_RD_WRITE;

   desc[1].buffer      = buffer;
   desc[1].length      = 4;
   desc[1].flags       = I2C_WR_RD_READ;

   ctxt.config         = &config;
   ctxt.desc           = desc;
   ctxt.num_descs      = 2;

   res = i2c_transfer (client_handle, &config, &desc[0], 2, client_callback, &ctxt, 0, NULL);

   // Close the connection handle to the i2c bus instance
   res = i2c_close (client_handle);

   // SYNCHRONOUS MODE
   // Obtain a client specific connection handle to the i2c bus instance 2
   res = i2c_open_ex (QUP_0, 0, &client_handle);

   // Read 4 bytes from a register 0x01 on a sensor device
   // I2C: <S><slave_address><W><A><0x01><A><Sr><slave_address><R><A><data1><A><data2><A>...<dataN><N><P>
   uint8_t reg = 0x01;

   desc[0].buffer      = &reg;
   desc[0].length      = 1;
   desc[0].flags       = I2C_WR_RD_WRITE;

   desc[1].buffer      = buffer;
   desc[1].length      = 4;
   desc[1].flags       = I2C_WR_RD_READ;

   ctxt.config         = &config;
   ctxt.desc           = desc;
   ctxt.num_descs      = 2;

   res = i2c_transfer (client_handle, &config, &desc[0], 2, NULL, NULL, 0, &transferred);
   if (res == I2C_SUCCESS)
   {
       if (transferred == (desc[0].length + desc[1].length))
       {
           // transfer passed
       }
       else
       {
           // transfer failed. number of bytes transferred = transferred
       }
   }

   // Close the connection handle to the i2c bus instance
   res = i2c_close (client_handle);
 }

 void client_callback (uint32_t status, uint32 transferred, void *ctxt)
 {
   uint32 i;
   client_ctxt *c_ctxt = (client_ctxt *) ctxt;
   i2c_descriptor *desc;

   if ((i2c_status) status != I2C_SUCCESS)
   {
     // Transfer failed
   }

   for (i = 0; i < c_ctxt->num_descs; i++)
   {
     desc = (c_ctxt->desc + i);
     if (transferred >= desc->length)
     {
       // descriptor i passed
       transferred -= desc->length;
     }
     else
     {
       // descriptor i failed
       // subsequenct descriptors failed
       break;
     }
   }
 }

 @endcode
*/

/** @} */ /* end_addtogroup i2c_api */

#ifndef __I2C_API_H__
#define __I2C_API_H__

#include "qup_common.h"
#include "i2c_types.h"

/** @addtogroup i2c_api
@{ */

/** @name I2C Driver Version 
@{ */

/** @version 15 @{ */
/**                         VERSION HISTORY                         
Version     Details                                                 
-------     --------------------------------------------------------
 15         Added new API to support I3C HOT JOIN feature.
 14         Added new flag for I3C wake-up reset pattern support in the i2c_transfer() API.
 13         Added error code for slave in bad state.
 12         Added Support for I2C High Speed Mode
 11         Added SETAASA support in CCC commands.
 10         Added i2c API to set/get power/performance levels
  9         Added new APIs to support batched/single transfers for latency sensitive usecase
  7         Added new argument frequncy to i2c_reset_lpi_resource_ex api() to support DFS
  6         Added API and Error codes to support I3C IBI controller 
  5         Add open API to support QUP type and re-order i2c_status.
  4         Add HDR EXIT Enumeration
  3         Added API and Enumeration to support 64 bit Timestamp.
  2         Modified Callback type to support IBI Timestamping
  1         Initial version
====================================================================*/

#define I2C_API_H_VERSION 15

/** @} */ /* end_namegroup */
/** @} */ /* end_addtogroup i2c_api */

/** @addtogroup i2c_api
@{ */

/** @name Preprocessor Definitions and Constants
@{ */

/** FLAGs for transfer descriptor. */
#define I2C_FLAG_START          0x00000001 /**< Specifies that the transfer begins with a START - S. */
#define I2C_FLAG_STOP           0x00000002 /**< Specifies that the transfer ends with a STOP - P. */
#define I2C_FLAG_WRITE          0x00000004 /**< Must be set to indicate a WRITE transfer. */
#define I2C_FLAG_READ           0x00000008 /**< Must be set to indicate a READ transfer. */

#define I2C_FLAG_TIMESTAMP      0x00000010 /**< Captures START timestamp if START flag is set. */
#define I2C_FLAG_TIMESTAMP_S    0x00000010 /**< Captures START timestamp if START flag is set. */
#define I2C_FLAG_TIMESTAMP_P    0x00000020 /**< Captures STOP timestamp if STOP flag is set. */
#define I2C_FLAG_IGNORE_ERROR   0x00000040 /**< Ignores errors like NACK. */

#define I3C_FLAG_USE_7E         0x00010000 /**< Must be set to send out a 0x7E for I3C. */
#define I3C_FLAG_IBI_CTRL       0x00020000 /**< When set IBI is either ACKed or NACKed based on a preconfigured table in HW. */
#define I3C_FLAG_CONTINUE       0x00040000 /**< Set internally for DAA transfers. */

/**
 * @brief Generates an I3C wake-up or reset pattern using the public API i2c_transfer().
 *
 * To use this feature, configure the `i2c_descriptor` with the following parameters:
 * - `buffer` = Dummy buffer
 * - `length` = 0
 * - `flags` = I2C_WRITE_TRANSFER | I3C_FLAG_USE_7E | I3C_FLAG_IBI_CTRL | I3C_FLAG_QC_WAKE_UP_PATTERN_EN
 * - `sub_mode` = I3C_SDR
 * - `cmd` = 0
 * - `num_descriptors` = 1
 *
 *   Configure the `i2c_slave_config` as follows:
 * - I3C bus speed 'I3C_SDR_DATA_RATE_12500_KHZ'
 * - `Slave_address` is ignored
 * - Set `mode` in `i2c_slave_config` to I3C
 * - All other parameters can be set to zero
 * - All other `i2c_transfer()` parameters should be configured as per a standard I3C transfer.
 */

#define I3C_FLAG_QC_WAKE_UP_PATTERN_EN          0x00100000  /**< @brief Enable flag for Qualcomm wake-up pattern. */
#define I3C_FLAG_QC_SLAVE_RESET_PATTERN_EN      0x00200000  /**< @brief Enable flag for Qualcomm slave reset pattern. */


#define I2C_FLAG_MEM_DDR        0x10000000 /**< Internal use. */

/** Flags used for a simple I2C write trasnfer. */
#define I2C_WRITE_TRANSFER  (I2C_FLAG_START | I2C_FLAG_WRITE | I2C_FLAG_STOP)

/** Flags used for a simple I2C read trasnfer. */
#define I2C_READ_TRANSFER   (I2C_FLAG_START | I2C_FLAG_READ  | I2C_FLAG_STOP)

/** Flags used for the write transfer of a Write followed by Read sequence. */
#define I2C_WR_RD_WRITE     (I2C_FLAG_START | I2C_FLAG_WRITE)

/** Flags used for the read transfer of a Write followed by Read sequence. */
#define I2C_WR_RD_READ      I2C_READ_TRANSFER

/** Flags used for I2C address query. #i2c_descriptor buffer and length are set
    to NULL and 0 respectively. */
#define I2C_ADDRESS_QUERY   (I2C_FLAG_START)

/** Flags used to send a separate I2C STOP bit on the bus. #i2c_descriptor buffer
    and length are set to NULL and 0 respectively. */
#define I2C_STOP_COMMAND    (I2C_FLAG_STOP)

/** Flags used for I2C bus clear/I3C HDR Exit. #i2c_descriptor buffer and length are set to
    NULL and 0 respectively.
    To generate HDR EXIT pattern "mode" in #i2c_slave_config is set to I3C and "sub_mode" in 
    #i2c_descriptor is set to I3C_HDR_DDR.
    To Generate Bus Clear "mode" in #i2c_slave_config is set to I2C or "mode" in #i2c_slave_config 
    is set to I3C and "sub_mode" in #i2c_descriptor is set to I2C_LEGACY. */
#define I2C_BUS_CLEAR       0
#define I3C_HDR_EXIT        0

#define I2C_TRANSFER_MASK      (I2C_FLAG_WRITE | I2C_FLAG_READ)

#define I2C_SUCCESS(x)  (x == I2C_SUCCESS)
#define I2C_ERROR(x)    (x != I2C_SUCCESS)

#define VALID_FLAGS(x) (((x & I2C_TRANSFER_MASK) == I2C_FLAG_READ) || ((x & I2C_TRANSFER_MASK) == I2C_FLAG_WRITE))

/** Bus speeds supported by the master implementation. */
#define I2C_STANDARD_MODE_FREQ_KHZ          100     /**< I2C stadard speed 100 KHz. */
#define I2C_FAST_MODE_FREQ_KHZ              400     /**< I2C fast mode speed 400 KHz. */
#define I2C_FAST_MODE_PLUS_FREQ_KHZ         1000    /**< I2C fast mode plus speed 1 MHz. */
#define I2C_HIGH_SPEED_MODE_FREQ_KHZ        3400    /**< I2C high speed 3.4 MHz. */
#define I3C_I2C_ENUMERATE_MODE_FREQ_KHZ     370     /**< I3C enumeration speed 370 KHz. */
#define I3C_SDR_DATA_RATE_12500_KHZ         12500   /**< I3C SDR speed 12.5 MHz. */

/** I3C CCC types. */
#define I3C_CCC_WRITE_PAYLOAD       0x1
#define I3C_CCC_READ_PAYLOAD        0x2
#define I3C_CCC_SLAVE_PAYLOAD       0x4
#define I3C_CCC_NO_PAYLOAD          0x8

/** I3C BCR bit mask. */
#define I3C_BCR_MAX_DATA_SPEED_LIMITATION   0x01
#define I3C_BCR_IBI_REQUEST_CAPABLE         0x02
#define I3C_BCR_IBI_PAYLOAD                 0x04
#define I3C_BCR_MAX_OFFLINE_CAPABLE         0x08
#define I3C_BCR_MAX_BRIDGE_IDENTIFIER       0x10
#define I3C_BCR_MAX_HDR_CAPABLE             0x20

/** I3C_IBI_CONTROLLER operations Flags used in i3c_ibi_table_params  */
#define	I3C_IBI_FLAG_TBIT_EXT_EN            0x01    /**< To enable T-bit extension feature for ASYNC0 timestamp. */
#define	I3C_IBI_FLAG_STALL_EN               0x02    /**< To enable Stall on the bus post IBI till the next transfer. */
#define	I3C_IBI_FLAG_MDB_MASK_EN            0x04    /**< To enable Mandatory Data Byte MASK if num_MDB > 0. */

/** @} */ /* end_namegroup */
/** @} */ /* end_addtogroup i2c_api */

/** @addtogroup i2c_api
@{ */

/** @name Type Definitions
@{ */

/**
  Instance of the I2C core that the client wants to use. This instance is
  passed in i2c_open().
*/
typedef enum
{
    I2C_INSTANCE_001 = 1,      /**< Instance 01. */
    I2C_INSTANCE_002,          /**< Instance 02. */
    I2C_INSTANCE_003,          /**< Instance 03. */
    I2C_INSTANCE_004,          /**< Instance 04. */
    I2C_INSTANCE_005,          /**< Instance 05. */
    I2C_INSTANCE_006,          /**< Instance 06. */
    I2C_INSTANCE_007,          /**< Instance 07. */
    I2C_INSTANCE_008,          /**< Instance 08. */
    I2C_INSTANCE_009,          /**< Instance 09. */
    I2C_INSTANCE_010,          /**< Instance 10. */
    I2C_INSTANCE_011,          /**< Instance 11. */
    I2C_INSTANCE_012,          /**< Instance 12. */
    I2C_INSTANCE_013,          /**< Instance 13. */
    I2C_INSTANCE_014,          /**< Instance 14. */
    I2C_INSTANCE_015,          /**< Instance 15. */
    I2C_INSTANCE_016,          /**< Instance 16. */
    I2C_INSTANCE_017,          /**< Instance 17. */
    I2C_INSTANCE_018,          /**< Instance 18. */
    I2C_INSTANCE_019,          /**< Instance 19. */
    I2C_INSTANCE_020,          /**< Instance 20. */
    I2C_INSTANCE_021,          /**< Instance 21. */
    I2C_INSTANCE_022,          /**< Instance 22. */
    I2C_INSTANCE_023,          /**< Instance 23. */
    I2C_INSTANCE_024,          /**< Instance 24. */
    I2C_INSTANCE_025,          /**< Instance 25. */
    I2C_INSTANCE_026,          /**< Instance 26. */
    I2C_INSTANCE_027,          /**< Instance 27. */
    I2C_INSTANCE_028,          /**< Instance 28. */
    I2C_INSTANCE_029,          /**< Instance 29. */
    I2C_INSTANCE_030,          /**< Instance 30. */
    I2C_INSTANCE_031,          /**< Instance 31. */
    I2C_INSTANCE_032,          /**< Instance 32. */

    I2C_INSTANCE_MAX,          /**< Instance Max. */

} i2c_instance;

/**
  Enum to indicate the type of timestamp returned to the client from #i2c_get_timestamp_64 api .

  In most generic cases, the value captured by HW is a snapshot of the QTIMER. It has a 56 bit register,
  consisting of a 32 bit register (LSB) and 24 bit register(MSB).The logic to detect a roll-over is not 
  supported by any i2c API and can be implemented by the client if they choose to.

  The bit positions of the snapshot captured by the HW have been mentioned with each enum definition.
*/

typedef enum
{
    I2C_TIMESTAMP_32_BIT_LEGACY = 1,    /**< Returned value is 32 bit timestamp [38:7] of QTimer value 
                                             as provided by HW.*/
    I2C_TIMESTAMP_64_BIT,               /**< Returned value is 64 bit timestamp [63:0] of QTimer value 
                                             as provided by HW.*/
} i2c_timestamp_type;


/**
  Enum to describe the type of Event for which the driver called the callback.
*/
typedef enum
{
    TRANSFER_CALLBACK = 1,           /**< To notify a completion of a transfer on I2C/I3C bus.*/
    IBI_CALLBACK,                    /**< To notify an IBI event on I3C bus with 32 bit timestamp[38:7] of QTimer
                                          as provided by HW, if IBI_TIMESTAMP passed in #ibi_register() api.*/
    IBI_CALLBACK_TIMESTAMP_48_BIT,   /**< To notify an IBI event on I3C bus with 48 bit timestamp[47:0] of QTimer
                                          as provided by HW, if IBI_TIMESTAMP passed in #ibi_register() api.*/
    IBI_CALLBACK_TIMESTAMP_ASYNC0,   /**< To notify EOSC1 and EOSC2 values with 64 bit timestamp[63:0] of QTimer 
                                          as provided by HW, if IBI_TIMESTAMP passed in #ibi_register_ex() api.*/ 
    HOT_JOIN_CALLBACK,                /**< To notify when a new device joins the I3C bus dynamically (hot-join event). */
} bus_callback_type;


/**
  Enum to indicate the type of timestamp the client would want to capture when an IBI event happens.
*/
typedef enum
{
    IBI_TIMESTAMP_NONE,        /**< Don't capture any timestamp */
    IBI_TIMESTAMP,             /**< Timestamp is captured at the Bus START condition for an IBI event */
    IBI_TIMESTAMP_ASYNC0,      /**< Timestamp is captured in terms of EOSC1 and EOSC2 values for an IBI event */
} ibi_timestamp_type;

/**
  Function status codes returned by the APIs or the transfer_status field of
  the callback.
*/
typedef enum
{
    I2C_SUCCESS = 0,
    
    I2C_ERROR_GENERIC                                   =  0x1000,
    I2C_ERROR_INVALID_PARAMETER,                                  
    I2C_ERROR_UNSUPPORTED_CORE_INSTANCE,                          
    I2C_ERROR_NO_MEM,                                             
                                                                  
    I2C_ERROR_API_INVALID_EXECUTION_LEVEL,                        
    I2C_ERROR_API_NOT_SUPPORTED,                                  
    I2C_ERROR_API_ASYNC_MODE_NOT_SUPPORTED,                       
    I2C_ERROR_API_PROTOCOL_NOT_SUPPORTED,                         
                                                                  
    I2C_ERROR_HANDLE_ALLOCATION,                                  
    I2C_ERROR_HW_INFO_ALLOCATION,                                 
    I2C_ERROR_IO_CTXT_ALLOCATION,                                 

    I2C_ERROR_UNCLOCKED_ACCESS,                                   
    I2C_ERROR_SLAVE_BAD_STATE,                                    

                                                                  
    I2C_ERROR_QDI                                       =  0x2000,
    I2C_ERROR_QDI_CTXT_ALLOC_FAIL,                                
    I2C_ERROR_QDI_USER_MALLOC_FAIL,                               
    I2C_ERROR_QDI_COPY_FAIL,                                      
    I2C_ERROR_QDI_MEM_MAP_FAIL,                                   
                                                                  
    I2C_ERROR_HARDWARE                                  =  0x3000,
    I2C_ERROR_INPUT_FIFO_OVER_RUN,                                
    I2C_ERROR_OUTPUT_FIFO_UNDER_RUN,                              
    I2C_ERROR_INPUT_FIFO_UNDER_RUN,                               
    I2C_ERROR_OUTPUT_FIFO_OVER_RUN,                               
    I2C_ERROR_COMMAND_OVER_RUN,                                   
    I2C_ERROR_COMMAND_ILLEGAL,                                    
    I2C_ERROR_COMMAND_FAIL,                                       
    I2C_ERROR_COMMAND_INVALID_OPCODE,                             
                                                                  
    I2C_ERROR_PROTOCOL                                  =  0x4000,
    I2C_ERROR_START_STOP_UNEXPECTED,                              
    I2C_ERROR_DATA_NACK,                                          
    I2C_ERROR_ADDR_NACK,                                          
    I2C_ERROR_ARBITRATION_LOST,                                   
    I2C_ERROR_IBI_NACK,                                           
    I2C_ERROR_SLAVE_READ_TERMINATED_EARLY,                        
    I2C_ERROR_CRC_PARITY,                                         
    I2C_ERROR_BUS_NOT_IDLE,                                       
    I2C_ERROR_TRANSFER_TIMEOUT,                                   
                                                                  
    I2C_ERROR_PLATFORM                                  =  0x5000,
    I2C_ERROR_PLATFORM_INIT_FAIL,                                 
    I2C_ERROR_PLATFORM_DEINIT_FAIL,                               
    I2C_ERROR_PLATFORM_CRIT_SEC_FAIL,                             
    I2C_ERROR_PLATFORM_SIGNAL_FAIL,                               
    I2C_ERROR_PLATFORM_TIMER_INIT_FAIL,                           
    I2C_ERROR_PLATFORM_TIMER_SET_FAIL,                            
    I2C_ERROR_PLATFORM_TIMER_CLR_FAIL,                            
    I2C_ERROR_PLATFORM_TIMER_DEINIT_FAIL,                         
    I2C_ERROR_PLATFORM_GET_CONFIG_FAIL,                           
    I2C_ERROR_PLATFORM_REG_INT_FAIL,                              
    I2C_ERROR_PLATFORM_DE_REG_INT_FAIL,                           
    I2C_ERROR_PLATFORM_CLOCK_ENABLE_FAIL,                         
    I2C_ERROR_PLATFORM_GPIO_ENABLE_FAIL,                          
    I2C_ERROR_PLATFORM_CLOCK_DISABLE_FAIL,                        
    I2C_ERROR_PLATFORM_GPIO_DISABLE_FAIL,                         
    I2C_ERROR_PLATFORM_RESOURCE_SETUP_FAIL,                       
    I2C_ERROR_PLATFORM_RESOURCE_RESET_FAIL,                       
    I2C_ERROR_PLATFORM_GET_CLOCK_CONFIG_FAIL,                     
                                                                  
    I2C_ERROR_TRANSFER                                  =  0x6000,
    I2C_TRANSFER_CANCELED,                                        
    I2C_TRANSFER_FORCE_TERMINATED,                                
    I2C_TRANSFER_COMPLETED,                                       
    I2C_TRANSFER_INVALID,                                         
    I2C_ERROR_HANDLE_ALREADY_IN_QUEUE,                            
                                                                  
    I2C_ERROR_DMA_OPS                                   =  0x7000,
    I2C_ERROR_DMA_REG_FAIL,                                       
    I2C_ERROR_DMA_DE_REG_FAIL,                                    
    I2C_ERROR_DMA_TX_CHAN_ADDRESS_MAP_FAIL,                       
    I2C_ERROR_DMA_RX_CHAN_ADDRESS_MAP_FAIL,                       
    I2C_ERROR_DMA_EV_CHAN_ADDRESS_MAP_FAIL,                       
    I2C_ERROR_DMA_EV_CHAN_ALLOC_FAIL,                             
    I2C_ERROR_DMA_TX_CHAN_ALLOC_FAIL,                             
    I2C_ERROR_DMA_RX_CHAN_ALLOC_FAIL,                             
    I2C_ERROR_DMA_TX_CHAN_START_FAIL,                             
    I2C_ERROR_DMA_RX_CHAN_START_FAIL,                             
    I2C_ERROR_DMA_TX_CHAN_STOP_FAIL,                              
    I2C_ERROR_DMA_RX_CHAN_STOP_FAIL,                              
    I2C_ERROR_DMA_TX_CHAN_RESET_FAIL,                             
    I2C_ERROR_DMA_RX_CHAN_RESET_FAIL,                             
    I2C_ERROR_DMA_EV_CHAN_DE_ALLOC_FAIL,                          
    I2C_ERROR_DMA_TX_CHAN_DE_ALLOC_FAIL,                          
    I2C_ERROR_DMA_RX_CHAN_DE_ALLOC_FAIL,                          
    I2C_ERROR_DMA_IBI_DEVICE_ADD_FAIL,                            
    I2C_ERROR_DMA_IBI_EN_DETECT_FAIL,                             
    I2C_ERROR_DMA_INSUFFICIENT_RESOURCES,                         
    I2C_ERROR_DMA_CONFIGURATION_FAIL,                             
    I2C_ERROR_DMA_PROCESS_TRANSFER_FAIL,                          
    I2C_ERROR_DMA_EVT_OTHER,                                      
                                                                  
    I2C_ERROR_I3C                                       =  0x8000,
    I2C_ERROR_I3C_GETMXDS_FAIL,                                   
    I2C_ERROR_DEVICE_IBI_NOT_SUPPORTED,                           
    I2C_ERROR_DEVICE_NOT_ENUMERATED,                              
    I2C_ERROR_ADDRESS_POOL_RESERVED_BY_DRIVER,                    
    I2C_ERROR_I3C_DEVICE_ALLOCATION_FAILED,                       
    I2C_ERROR_I3C_ENUMERATE_CALLED_MULTIPLE_TIMES,                
    I2C_ERROR_IBI_REGISTER_CALLED_MULTIPLE_TIMES,                 
    I2C_ERROR_I3C_DEREGISTER_CALLED_MULTIPLE_TIMES,               
    I2C_ERROR_I3C_DEREGISTER_FAILED,
    I2C_ERROR_I3C_INVALID_USE_OF_COMMAND,

    I2C_ERROR_I3C_IBI                                    =  0x9000,
    I2C_ERROR_I3C_IBI_STATUS_INVALID,                              
    I2C_ERROR_I3C_IBI_INVALID_SLAVE_ADDRESS_TO_GPII,               
    I2C_ERROR_I3C_IBI_INVALID_STALL_BIT,                           
    I2C_ERROR_I3C_IBI_INVALID_AUTO_NACK,                           
    I2C_ERROR_I3C_IBI_INVALID_NUM_MDB,                             
    I2C_ERROR_I3C_IBI_INVALID_MASK_EN,                             
    I2C_ERROR_I3C_IBI_INVALID_TBIT_EN,                             
    I2C_ERROR_I3C_IBI_CFG_FAIL_IBI_NOT_ENABLED,                    
    I2C_ERROR_I3C_IBI_CFG_FAIL,                                    
    I2C_ERROR_I3C_IBI_CFG_TABLE_FULL,                              
    I2C_ERROR_I3C_IBI_SLAVE_ADDRESS_NOT_FOUND,                     
    I2C_ERROR_I3C_IBI_BUS_ERROR,                                   
    I2C_ERROR_I3C_IBI_SW_RESET_DONE,                               
    I2C_ERROR_I3C_IBI_UNEXPECTED_IBI_ADDRESS,    
    I2C_ERROR_I3C_IBI_RECEIVE_FAILED,
    I2C_ERROR_I3C_IBI_CTRL_INIT_FAILED,
    I2C_ERROR_I3C_IBI_CTRL_DEINIT_FAILED,
    I2C_ERROR_I3C_IBI_CTRL_USER_INIT_FAILED,
    I2C_ERROR_I3C_IBI_CTRL_USER_DEINIT_FAILED,
    I2C_ERROR_I3C_IBI_MGR_INIT_FAILED,
    I2C_ERROR_I3C_IBI_MGR_DEINIT_FAILED,
    I2C_ERROR_I3C_INVALID_WAKEUP_PARAM,
    I2C_ERROR_I3C_HJ_INVALID_PARAM,
    I2C_ERROR_I3C_HJ_SLAVE_ALREADY_EXISTS,
} i2c_status;

/**
  Bus protocol. Underlying implementation may not support some protocols. Refer
  i2c_transfer() for more details.
*/
typedef enum
{
    I2C     = 0,    /**< I2C protocol. */
    SMBUS   = 1,    /**< SMBUS protocol. */
    I3C     = 2     /**< I3C protocol. */

} bus_protocol;

typedef enum
{
    I2C_LEGACY      = 0,    /**< Communicate with an I2C device on I3C bus. */
    I3C_SDR         = 1,    /**< Communicate with a slave supporting I3C Standard Data Rate. */
    I3C_HDR_DDR     = 2,    /**< Communicate with a slave supporting I3C HDR Dual Data Rate. */
    I3C_CCC         = 3,    /**< Common Command Code transfers. */
    I3C_IBI_READ    = 4     /**< Read IBI payload from slaves that support it. */

} i3c_mode;

/**< Instance 01. */

typedef enum i3c_ccc
{
    B_ENEC        = (0x0000 | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Enable slave event driven interrupts. */
    B_DISEC       = (0x0001 | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Disable slave event driven interrupts. */
    B_ENTAS0      = (0x0002 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Set activity state 0 (normal operation). */
    B_ENTAS1      = (0x0003 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Set activity state 1. */
    B_ENTAS2      = (0x0004 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Set activity state 2. */
    B_ENTAS3      = (0x0005 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Set activity state 3. */
    B_RSTDAA      = (0x0006 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Forget current dynamic dddress and wait for new assignment. */
    B_ENTDAA      = (0x0007 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Entering master initiation of slave dynamic address assignment. */
    B_DEFSLVS     = (0x0008 | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Master defines dynamic address, dcr type, and static address (or 0) per slave. */
    B_SETMWL      = (0x0009 | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Maximum write length in a single command. */
    B_SETMRL      = (0x000a | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Maximum read length in a single command. */
    B_ENTTM       = (0x000b | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Master has entered test mode. */
    B_ENTHDR0     = (0x0020 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Master has entered hdr - ddr mode. */
    B_ENTHDR1     = (0x0021 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Master has entered hdr - tsp mode. */
    B_ENTHDR2     = (0x0022 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Master has entered hdr - tsl mode. */
    B_ENTHDR3     = (0x0023 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Master has entered hdr - future. */
    B_ENTHDR4     = (0x0024 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Master has entered hdr - future. */
    B_ENTHDR5     = (0x0025 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Master has entered hdr - future. */
    B_ENTHDR6     = (0x0026 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Master has entered hdr - future. */
    B_ENTHDR7     = (0x0027 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Master has entered hdr - future. */
    B_SETXTIME    = (0x0028 | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Framework for exchanging event timing information. */
    B_SETAASA     = (0x0029 | (I3C_CCC_NO_PAYLOAD    << 8)), /**< Master requests all I3C slaves with I2C Static Addresses to use their I2C Static Address as their Dynamic Address  >*/
    B_RSTACT      = (0x002A | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Master requests all I3C slaves to configure the next Target Reset action and may be used to retrieve a Targets reset recovery timing.>*/ 
    D_ENEC        = (0x0080 | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Enable slave event driven interrupts. */
    D_DISEC       = (0x0081 | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Disable slave event driven interrupts. */
    D_ENTAS0      = (0x0082 | (I3C_CCC_SLAVE_PAYLOAD << 8)), /**< Set activity state 0 (normal operation). */
    D_ENTAS1      = (0x0083 | (I3C_CCC_SLAVE_PAYLOAD << 8)), /**< Set activity state 1. */
    D_ENTAS2      = (0x0084 | (I3C_CCC_SLAVE_PAYLOAD << 8)), /**< Set activity state 2. */
    D_ENTAS3      = (0x0085 | (I3C_CCC_SLAVE_PAYLOAD << 8)), /**< Set activity state 3. */
    D_RSTDAA      = (0x0086 | (I3C_CCC_SLAVE_PAYLOAD << 8)), /**< Forget current dynamic address and wait for new assignment. D_RSTDAA is not supported in i3c specv1.1. */
    D_SETDASA     = (0x0087 | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Master assigns a dynamic address to a slave with a known static address. */
    D_SETNEWDA    = (0x0088 | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Master assigns a new dynamic address to any i3c slave. */
    D_SETMWL      = (0x0089 | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Maximum write length in a single command. */
    D_SETMRL      = (0x008a | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Maximum read length in a single command. */
    D_GETMWL      = (0x008b | (I3C_CCC_READ_PAYLOAD  << 8)), /**< Get slave's maximum possible write length. */
    D_GETMRL      = (0x008c | (I3C_CCC_READ_PAYLOAD  << 8)), /**< Get a slave's maximum possible read length. */
    D_GETPID      = (0x008d | (I3C_CCC_READ_PAYLOAD  << 8)), /**< Get a slave's provisional id. */
    D_GETBCR      = (0x008e | (I3C_CCC_READ_PAYLOAD  << 8)), /**< Get a device's bus characteristic register (bcr). */
    D_GETDCR      = (0x008f | (I3C_CCC_READ_PAYLOAD  << 8)), /**< Get a device's device characteristics register (dcr). */
    D_GETSTATUS   = (0x0090 | (I3C_CCC_READ_PAYLOAD  << 8)), /**< Get a device's operating status. */
    D_GETACCMST   = (0x0091 | (I3C_CCC_READ_PAYLOAD  << 8)), /**< Current master is requesting and confirming a bus mastership from a secondary master. */
    D_SETBRGTGT   = (0x0093 | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Master tells bridge (to/from i2c, spi, uart, etc.) what endpoints it is talking to (by dynamic address and type/id). */
    D_GETMXDS     = (0x0094 | (I3C_CCC_READ_PAYLOAD  << 8)), /**< Master asks slave for its sdr mode max. read and write data speeds (& optionally max. read turnaround time). */
    D_GETHDRCAP   = (0x0095 | (I3C_CCC_READ_PAYLOAD  << 8)), /**< Master asks slave what hdr modes it supports. */
    D_SETXTIME    = (0x0098 | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Framework for exchanging event timing information. */
    D_GETXTIME    = (0x0099 | (I3C_CCC_READ_PAYLOAD  << 8)), /**< Framework for exchanging event timing information. */
    D_RSTACT_W    = (0x009A | (I3C_CCC_WRITE_PAYLOAD << 8)), /**< Master requests single I3C slaves to configure the next Target Reset action and may be used to retrieve a Targets reset recovery timing.>*/ 
    D_RSTACT_R    = (0x009A | (I3C_CCC_READ_PAYLOAD  << 8)), /**< This command is currently not supported. */ 


} i3c_ccc;

/** @} */ /* end_namegroup */
/** @} */ /* end_addtogroup i2c_api */

/** @addtogroup i2c_api
@{ */

/** @name Structure Definitions
@{ */

/**
  Slave configuration parameters that the client uses to communicate to a slave.
*/
typedef struct
{
    uint32          bus_frequency_khz;          /**< Bus speed in kHz. */
    uint32          slave_address;              /**< 7-bit I2C slave address or
                                                     7-bit I3C dynamic address. */
    bus_protocol    mode;                       /**< Protocol mode. Refer #bus_protocol. */
    uint32          slave_max_clock_stretch_us; /**< Maximum time in microseconds that an
                                                     I2C slave may hold the SCL line low.
                                                     Not applicable for I3C. */
    uint32          core_configuration1;        /**< Core Specific Configuration. Recommended 0. */
    uint32          core_configuration2;        /**< Core Specific Configuration. Recommended 0. */

} i2c_slave_config;

/**
  Transfer descriptor that the client uses to perform a read or a write.
  Clients pass this descriptor or an array of these descriptors to the
  i2c_transfer() API.
*/
typedef struct
{
    uint8       *buffer;        /**< Buffer for the data transfer. */
    uint32      length;         /**< Length of the data to be transferred in bytes. */
    uint32      flags;          /**< Flags used for the transfer descriptor. */
    i3c_mode    sub_mode;       /**< Sub modes for I3C protocol. */
    uint32      cmd;            /**< If protocol is I3C, this field holds the HDR/CCC command. */

} i2c_descriptor;

/**
  Structure describing the I3C Device Characteristic Register + Provisional ID that slave provides during Bus Enumeration
*/
typedef struct bus_enum_table
{
    uint8           dynamic_slave_addr;    /**< Dynamic slave address assigned to the slave. */
    uint8           bcr;                   /**< BCR value obtained during broadcast ENTDAA CCC. */
    uint8           dcr;                   /**< DCR value obtained during broadcast ENTDAA CCC. */
    uint8           pid[6];                /**< PID value obtained during broadcast ENTDAA CCC. */                   
} bus_enum_table;

/**
 * @struct i3c_hot_join_cb_status
 * @brief Status information passed to the hot-join callback.
 *
 * This structure contains the result of a hot-join operation and details
 * about the newly joined I3C slave device.
 */
typedef struct i3c_hot_join_cb_status
{
    i2c_status hot_join_status;          /**< Completion status of the hot-join operation.
                                           Refer to @ref i2c_status for possible values. */

    bus_enum_table slave_device_info[8]; /**< Information about the newly joined slave device,
                                           including dynamic address, BCR, DCR, and PID. */
    uint8           slave_device_count;  /**< Number of valid slave device entries populated in @ref slave_device_info. */

} i3c_hot_join_cb_status;


/**
  Definition of the callback structure that is passed to client for the completion i2c_transfer()
*/
typedef struct transfer_status
{

    i2c_status      transfer_status;  /**< The completion status of the transfer; see
                                           #i2c_status for actual values.*/
 
    uint32           transferred;     /**< Total number of bytes transferred. If
                                           transfer_status is I2C_SUCCESS, then this value
                                           is equal to the sum of lengths of all the
                                           descriptors passed in a call to i2c_transfer().
                                           If transfer_status is not I2C_SUCCESS, this
                                           value is less than the sum of lengths of all the
                                           descriptors passed in a call to i2c_transfer().*/
} transfer_status;

/**
  Definition of the callback structure that is passed to client on an IBI Event registered with i3c_register_ibi() or i3c_register_ibi_ex()
*/
typedef struct ibi_cb_status
{
    i2c_status status_ibi;           /**< The indication of IBI event over the BUS; see
                                          #i2c_status for Master result of the event ACK/NACK*/

    uint32     slave_address;        /**< The address of the slave that generated IBI on the Bus*/

    uint8      *ibi_payload_buff;    /**< The buffer passed during ibi_register operation*/
    
    uint32     ibi_rlen;             /**< The number of bytes sent by the slave during an IBI event on the bus*/   
      
    uint64     ibi_timestamp;        /**< The value updated by this API is a read operation of HW timestamp register
                                          if IBI_CALLBACK_TIMESTAMP_48_BIT cb_type passed as argument to #callback_func.
                                          Otherwise the value updated by this API is a read operation of EOSC1 HW timestamp register 
                                          if IBI_CALLBACK_TIMESTAMP_ASYNC0 cb_type passed as argument to #callback_func. */
    uint64     ibi_timestamp_1;	     /**< The value updated is a read operation of EOSC2 HW timestamp register,
                                          Value dependent on IBI_CALLBACK_TIMESTAMP_ASYNC0 cb_type passed as argument to #callback_func. */

} ibi_cb_status;

/**
  Union of all callback structures type.
*/
typedef union 
{
    ibi_cb_status           ibi_status;           /** < see #ibi_cb_status for details*/
    transfer_status         xfer_status;          /** < see #transfer_status for details*/
    i3c_hot_join_cb_status  i3c_hot_join_status;  /** < see #i3c_hot_join_cb_status for details*/
} callback_ctxt;

/**
  Structure describing the I2C/I3C slaves + Descriptors per slave
*/
typedef struct i2c_config_desc_pairs
{
    i2c_slave_config *config;              /**< Slave configuration. See #i2c_slave_config for details.*/
    i2c_descriptor *desc;                  /**< I2C transfer descriptor. See #i2c_descriptor for details. This can be an array of descriptors.*/
    uint16 num_desc;                       /**< Number of descriptors in the descriptor array.*/
}i2c_config_desc_pairs;

/**
  Structure describing I3C IBI Table parameters that is passed to i3c_register_ibi_ex() on IBI Event registration or in case of modification to table entry 
*/
typedef struct i3c_ibi_table_params
{
  uint8 flags;                               /**< Flags to determine which IBI Table parameter to change. */
  uint8 mdb_mask;                            /**< Specify value for mandatory data byte mask if mask is enabled in flags. */
  uint16 tbit_ext_value;                     /**< Specify value for T-bit extension in terms of #cycles if T-bit extension is 
                                                  enabled in flags,supports upto 3840cycles for max 100us. cycle time is 0.026usec. */
} i3c_ibi_table_params;

/** @} */ /* end_namegroup */
/** @} */ /* end_addtogroup i2c_api */

/** @addtogroup i2c_api
@{ */

/** @name Function Declarations
@{ */

/**
  Bus callback. Applicable only to asynchronous transfer mode. Refer to
  i2c_transfer() & i3c_register_ibi()/i3c_register_ibi_ex().

  Declares the type of callback function that needs to be defined by the client.
  The callback is called when the bus driver want to indicate the client when it
  has completed a particular task that client registered.
  
  For i2c_transfer(): The callback is called when the data is completely 
  transferred on the bus or the transfer ends due to an error or cancellation.

  For i3c_register_ibi(): The callback is called when and IBI event occurs on the I3C bus.
  IBI payload is also transferred along with the callback and address of the slave that generated the IBI.

  For i3c_register_hot_join(): The callback is called when a hot-join event occurs on the I3C bus.
  It invokes registered callbacks for matching devices based on PID.

  Clients pass the callback function pointer and the callback context to the
  driver in calling function.

  @note
  The callback is called in a thread that processes multiple clients. Calling
  any of the APIs defined here in this callback is not recommended. Processing
  in the callback function must be kept to a minimum to avoid system latencies.
  In some implementations the callback occurs from interrupt context and calling
  any of the APIs defined here will result in an
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL error.

  @param[out] cb_type           The type of callback received; see
                                #bus_callback_type for type of callback received.
  @param[out] cb_ctxt           If cb_ctxt is a union see #callback_ctxt, the
                                interpretation of which would be based on the type of callback received.
                                
                                TRANSFER_CALLBACK   - see #transfer_status type for results
                                IBI_CALLBACK        - see #ibi_cb_status   type for results  

  @param[out] usr_ctxt          The callback context parameter that was passed
                                during callback registration.
*/
typedef void (*callback_func) (bus_callback_type cb_type, callback_ctxt *cb_ctxt, void *usr_ctxt);

/**
  Called by the client code to initialize the respective I2C/I3C instance. On
  success, i2c_handle points to the handle for the I2C/I3C instance. The API
  allocates resources for use by the client handle and the I2C/I3C instance.
  These resources are released in the i2c_close() call.

  @param[in]  instance            I2C/I3C instance that the client intends to
                                  initialize; see #i2c_instance for details.
  @param[out] i2c_handle          Pointer location to be filled by the
                                  driver with a handle to the instance.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_UNSUPPORTED_CORE_INSTANCE --  Unsupported core instance. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_HANDLE_ALLOCATION -- Handle allocation error. \n
  I2C_ERROR_HW_INFO_ALLOCATION -- Hardware information allocation error. \n
  I2C_ERROR_PLATFORM_INIT_FAIL -- Platform initialization failure. \n
  I2C_ERROR_PLATFORM_CRIT_SEC_FAIL -- Critical section initialization failure. \n
  I2C_ERROR_PLATFORM_GET_CONFIG_FAIL -- Could not get HW configuration. \n
  I2C_ERROR_PLATFORM_REG_INT_FAIL -- Platform registration internal failure. \n
  I2C_ERROR_DMA_REG_FAIL -- Registration failure with bus interface. \n
  I2C_ERROR_DMA_EV_CHAN_ALLOC_FAIL -- Event channel allocation failure. \n
  I2C_ERROR_DMA_TX_CHAN_ALLOC_FAIL -- TX channel allocation failure. \n
  I2C_ERROR_DMA_RX_CHAN_ALLOC_FAIL -- RX channel allocation failure. \n
  I2C_ERROR_DMA_TX_CHAN_START_FAIL -- TX channel start failure. \n
  I2C_ERROR_DMA_RX_CHAN_START_FAIL -- RX channel start failure.
  
  @note
  Clients are advised to migrate to i2c_open_ex().
*/
i2c_status
i2c_open
(
    i2c_instance instance,
    void **i2c_handle
);

/**
  Called by the client code to initialize the respective I2C/I3C instance. On
  success, i2c_handle points to the handle for the I2C/I3C instance. The API
  allocates resources for use by the client handle and the I2C/I3C instance.
  These resources are released in the i2c_close() call.

  @param[in]  qup                 Qup Type the client intends to use, see #QUP_TYPE in qup_common.h for details.
  @param[in]  se_index            Serial Engine index in QUP client intends to use. (eg: QUP_0_SE_1 -> se_index = 1)
  @param[out] i2c_handle          Pointer location to be filled by the
                                  driver with a handle to the instance.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_UNSUPPORTED_CORE_INSTANCE --  Unsupported core instance. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_HANDLE_ALLOCATION -- Handle allocation error. \n
  I2C_ERROR_HW_INFO_ALLOCATION -- Hardware information allocation error. \n
  I2C_ERROR_PLATFORM_INIT_FAIL -- Platform initialization failure. \n
  I2C_ERROR_PLATFORM_CRIT_SEC_FAIL -- Critical section initialization failure. \n
  I2C_ERROR_PLATFORM_GET_CONFIG_FAIL -- Could not get HW configuration. \n
  I2C_ERROR_PLATFORM_REG_INT_FAIL -- Platform registration internal failure. \n
  I2C_ERROR_DMA_REG_FAIL -- Registration failure with bus interface. \n
  I2C_ERROR_DMA_EV_CHAN_ALLOC_FAIL -- Event channel allocation failure. \n
  I2C_ERROR_DMA_TX_CHAN_ALLOC_FAIL -- TX channel allocation failure. \n
  I2C_ERROR_DMA_RX_CHAN_ALLOC_FAIL -- RX channel allocation failure. \n
  I2C_ERROR_DMA_TX_CHAN_START_FAIL -- TX channel start failure. \n
  I2C_ERROR_DMA_RX_CHAN_START_FAIL -- RX channel start failure.
  
  @version API header version 5
*/
i2c_status
i2c_open_ex
(
    QUP_TYPE qup,
    uint32 se_index,
    void **i2c_handle
);


/**
  De-initializes the I2C/I3C instance and releases any resources allocated by the
  i2c_open_ex()/i2c_open() API.

  @param[in] i2c_handle           Handle to the I2C/I3C instance.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_PLATFORM_DEINIT_FAIL -- Platform de-initialization failure. \n
  I2C_ERROR_PLATFORM_DE_REG_INT_FAIL -- Platform de-registration internal failure. \n
  I2C_ERROR_DMA_TX_CHAN_STOP_FAIL -- TX channel stop failure. \n
  I2C_ERROR_DMA_RX_CHAN_STOP_FAIL -- RX channel stop failure. \n
  I2C_ERROR_DMA_TX_CHAN_RESET_FAIL -- TX channel reset failure. \n
  I2C_ERROR_DMA_RX_CHAN_RESET_FAIL -- RX channel reset failure. \n
  I2C_ERROR_DMA_EV_CHAN_DE_ALLOC_FAIL -- Event channel de-allocation failure. \n
  I2C_ERROR_DMA_TX_CHAN_DE_ALLOC_FAIL -- TX channel de-allocation failure. \n
  I2C_ERROR_DMA_RX_CHAN_DE_ALLOC_FAIL -- RX channel de-allocation failure.
*/
i2c_status
i2c_close
(
    void *i2c_handle
);

/**
  Transfer callback. Applicable only to asynchronous transfer mode. Refer to
  i2c_transfer(). Underlying implmentation may not support the API and return
  I2C_ERROR_API_NOT_SUPPORTED.

  Cancels a transfer. A transfer that has been initiated successfully
  by calling i2c_transfer() may be canceled. Based on the internal state
  of the transfer, this function will either immediately cancel the transfer or end
  the transfer at a later time. If the function returns I2C_TRANSFER_CANCELED,
  the transfer was canceled successfully and the client callback will
  not be called. If the function returns I2C_TRANSFER_FORCE_TERMINATED,
  the client must wait for the callback to occur in order to make sure that the
  transfer has ended. If the function returns I2C_TRANSFER_COMPLETED, the
  transfer has already completed and the callback has already been called.
  If the function returns I2C_TRANSFER_INVALID, the client is trying to cancel a
  transfer that is not queued to the HW with an earlier call to
  i2c_transfer().

  @param[in] i2c_handle           Handle to the I2C/I3C instance.

  @return
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_API_NOT_SUPPORTED -- API is not supported. \n
  I2C_TRANSFER_INVALID -- Invalid transfer. \n
  I2C_TRANSFER_CANCELED -- Transfer is canceled successfully. \n
  I2C_TRANSFER_COMPLETED -- Transfer completed before canceling. \n
  I2C_TRANSFER_FORCE_TERMINATED -- Transfer aborted on the bus.
*/
i2c_status
i2c_cancel_transfer
(
    void *i2c_handle
);

/**
  Enables the clocks, gpios and any other power resources used by the I2C/I3C
  instance.

  @param[in] i2c_handle           Handle to the I2C/I3C instance.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_PLATFORM_CLOCK_ENABLE_FAIL -- Platform clock enable failure. \n
  I2C_ERROR_PLATFORM_GPIO_ENABLE_FAIL -- Platform GPIO enable failure.
*/
i2c_status
i2c_power_on
(
    void *i2c_handle
);

/**
  Disables the clocks, gpios and any other power resources used by the I2C
  instance.

  @param[in] i2c_handle           Handle to the I2C/I3C instance.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_PLATFORM_CLOCK_DISABLE_FAIL -- Platform clock disable failure. \n
  I2C_ERROR_PLATFORM_GPIO_DISABLE_FAIL -- Platform GPIO disable failure.
*/
i2c_status
i2c_power_off
(
    void *i2c_handle
);

/**
  Locks the I2C/I3C bus for exclusive access. Underlying implementation may not
  support the API and return I2C_ERROR_API_NOT_SUPPORTED.

  @param[in] i2c_handle           Handle to the I2C/I3C instance.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_NOT_SUPPORTED -- API is not supported. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level.
*/
i2c_status
i2c_lock
(
    void *i2c_handle
);

/**
  Releases the I2C/I3C bus exclusive access. Underlying implementation may not
  support the API and return I2C_ERROR_API_NOT_SUPPORTED.

  @param[in] i2c_handle           Handle to the I2C/I3C instance.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_NOT_SUPPORTED -- API is not supported. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level.
*/
i2c_status
i2c_unlock
(
    void *i2c_handle
);

/**
  Performs an I2C/I3C transfer synchronously or asynchronously. A callback
  function is passed as a parameter to this function.

  If the callback parameter is NULL, the function executes synchronously. When
  the function returns, the transfer is complete.

  If the callback parameter is not NULL, the functions executes
  asynchronously. The function returns by queueing the transfer to the
  underlying driver. Once the transfer is complete, the driver calls the
  callback provided. Refer to #callback_func for callback definition. If the
  function returns a failure, the transfer has not been queued and no
  callback will occur. If the transfer returns I2C_SUCCESS, the transfer has
  been queued and a further status of the transfer can only be obtained when
  the callback is called. After a client calls this API, it must wait for the
  completion callback to occur before it can call the API again. If the
  client wishes to queue mutliple transfers, it must use an array of
  descriptors of type i2c_descriptor instead of calling the API multiple
  times.

  Underlying implementation may not support asynchronous mode. The return
  value will indicate the lack of support in which case client must pass a
  NULL value for func.

  @param[in]  i2c_handle          Handle to the I2C/I3C instance.
  @param[in]  config              Slave configuration. See #i2c_slave_config
                                  for details.
  @param[in]  desc                I2C transfer descriptor. See
                                  #i2c_descriptor for details. This
                                  can be an array of descriptors.
  @param[in]  num_descriptors     Number of descriptors in the descriptor array.
  @param[in]  func                The callback function that is called at the
                                  completion of the transfer. This callback
                                  occurs in a dispatch context. The callback
                                  must do minimal processing and must
                                  not call any API defined here. If the
                                  callback is NULL, the function executes
                                  synchronously.
  @param[in]  ctxt                The context that the client passes here is
                                  returned as is in the callback function.
  @param[in]  delay_us            A delay in microseconds that specifies the
                                  time to wait before starting the transfer.
  @param[out] transferred         Total number of bytes transferred. This
                                  value is valid only for synchronous mode.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_API_ASYNC_MODE_NOT_SUPPORTED -- Asynchronous transfer mode is not supported. \n
  I2C_ERROR_API_PROTOCOL_MODE_NOT_SUPPORTED -- The mode parameter passed in #i2c_slave_config is not supported. \n
  I2C_ERROR_TRANSFER_TIMEOUT -- Transfer timed out. \n
  I2C_ERROR_HANDLE_ALREADY_IN_QUEUE -- Client IO is pending. \n
  I2C_ERROR_DMA_INSUFFICIENT_RESOURCES -- Insufficient resources to execute the transfer. \n
  I2C_ERROR_DMA_PROCESS_TRANSFER_FAIL -- Transfer processing failure in bus interface.
*/
i2c_status
i2c_transfer
(
    void *i2c_handle,
    i2c_slave_config *config,
    i2c_descriptor *desc,    
    uint16 num_descriptors,
    callback_func func,
    void *ctxt,
    uint32 delay_us,
    uint32 *transferred
);

/**
  Saves I2C/I3C transfer context in underlying driver. A callback
  function is passed as a parameter to this function. It is saved in underlying driver and used as a callback function in i2c_submit_transfer() API.

  If the callback parameter is NULL, it means i2c_submit_transfer() API executes synchronously. When
  the function returns, the transfer is complete.

  If the callback parameter is not NULL, it means i2c_submit_transfer() API executes
  asynchronously.

  The function returns by saving context in underlying driver.  If the transfer returns I2C_SUCCESS, the transfer context has been saved.  If the function returns a failure otherwise. After a client calls this API, it must wait for the completion callback to occur before it can call the API again with same multi_transfer_handle.

  If client wishes to queue mutliple transfers, it must use an array of
  descriptors of type i2c_descriptor inside i2c_config_desc_pairs instead of calling the API multiple
  times. 

  If client wishes to queue transfer for  multiple slaves, it must use an array of i2c_config_desc_pairs which contains i2c_descriptors per slave config.

  Underlying implementation may not support asynchronous mode. The return
  value will indicate the lack of support in which case client must pass a
  NULL value for func.

  @param[in]  i2c_handle           Handle to the I2C/I3C instance.
  @param[in]  i2c_cfg_desc_pairs   An array of slave configs and corresponding multiple descriptors per slave 
  @param[in]  num_desc_pairs       Number of descriptors & config pair in the i2c_cfg_desc_pairs array.
  @param[in]  func                 The callback function that is called at the
                                   completion of the transfer. This callback
                                   occurs in a dispatch context. The callback
                                   must do minimal processing and must
                                   not call any API defined here. If the
                                   callback is NULL, the function executes
                                   synchronously.
  @param[in]  ctxt                 The context that the client passes here is
                                   returned as is in the callback function.
  @param[in]  delay_us             A delay in microseconds that specifies the
                                   time to wait before starting the transfer.
 
  @param[out] multi_transfer_handle  Pointer location to be filled by the driver with a handle to this transfer.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_API_ASYNC_MODE_NOT_SUPPORTED -- Asynchronous transfer mode is not supported. \n
  I2C_ERROR_API_PROTOCOL_MODE_NOT_SUPPORTED -- The mode parameter passed in #i2c_slave_config is not supported. \n
  I2C_ERROR_TRANSFER_TIMEOUT -- Transfer timed out. \n
  I2C_ERROR_HANDLE_ALREADY_IN_QUEUE -- Client IO is pending. \n
  I2C_ERROR_DMA_INSUFFICIENT_RESOURCES -- Insufficient resources to execute the transfer. \n
  I2C_ERROR_DMA_PROCESS_TRANSFER_FAIL -- Transfer processing failure in bus interface.
*/
i2c_status
i2c_prepare_transfer
(
    void *i2c_handle,
    i2c_config_desc_pairs *i2c_cfg_desc_pairs,
    uint16 num_desc_pairs,
    callback_func func,
    void *ctxt,
    uint32 delay_us,
    void **multi_transfer_handle    
);

/**
  Queues transfer to HW with already provided descriptors from i2c_prepare_transfer() API synchronously or asynchronously based on callback function passed in i2c_prepare_transfer() API.   
  If the transfer returns I2C_SUCCESS, the transfer has been queued and a further status of the transfer can only be obtained when the callback is called. After a client calls this API, it must wait for the
  completion callback to occur before it can call the API again.

  @param[in]  i2c_handle             Handle to the I2C/I3C instance.
  @param[in]  multi_transfer_handle  Handle to transfer 
  @param[out] transferred            Total number of bytes transferred. This
                                     value is valid only for synchronous mode.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_API_ASYNC_MODE_NOT_SUPPORTED -- Asynchronous transfer mode is not supported. \n
  I2C_ERROR_API_PROTOCOL_MODE_NOT_SUPPORTED -- The mode parameter passed in #i2c_slave_config is not supported. \n
  I2C_ERROR_TRANSFER_TIMEOUT -- Transfer timed out. \n
  I2C_ERROR_HANDLE_ALREADY_IN_QUEUE -- Client IO is pending. \n
  I2C_ERROR_DMA_INSUFFICIENT_RESOURCES -- Insufficient resources to execute the transfer. \n
  I2C_ERROR_DMA_PROCESS_TRANSFER_FAIL -- Transfer processing failure in bus interface.
*/
i2c_status
i2c_submit_transfer
(
    void *i2c_handle,
    void *multi_transfer_handle,
    uint32 *transferred
);

/**
  Client calls this API to get the timestamp for the START bit for the
  previous transfer that completed successfully. Return value indicates if the
  underlying implementation supports this API or not.

  In order to obtain the timestamp, the client must have set the
  I2C_FLAG_TIMESTAMP_S and I2C_FLAG_START bit in a previous READ or WRITE
  transfer descriptor. Once the transfer completed successfully, the client
  can call this API to obtain the timestamp value.

  The value returned by the function is recorded at the time of sending START command on the I2C bus.
  
  The client shall obtain the value of START or STOP time-stamp per transfer
  and not both due to HW limitation.
  
  In most generic cases, the value is a timer tick at a resolution of
  about 6.66 microseconds(value of 0:6 bits approximation based on 19.2Mhz of QTIMER).
  It has a wrap-around time of around 7.95 Hrs.
  
  The value updated by this API is a read operation of [38:7] bits of QTIMER counter.

  The QTIMER is a 56 bit register, consisting of a 32 bit register (LSB) and 24 bit register(MSB).
  So  the client shall read the upper 24 bits and compare with previous value 
  if the value is 0 or less than that of the previously read value , the client can safely decide that 
  there has been a roll-over. The logic to detect  a roll-over is not supported by this API and can
  be implemented by the client if they choose to. 

  Interpretation of the timestamp value obtained from this API is platform
  specific. 
  
  @param[in] i2c_handle           Handle to the I2C/I3C instance.

  @return
  0 -- Timestamp not supported. \n
  N -- A non-zero timestmamp N, where N is the ticks read from [38:7] bits of QTIMER counter.

  @note
  Legacy API - For HW which supports 64 bit timestamp, this api will return 0.
*/
uint32
i2c_get_start_timestamp
(
    void *i2c_handle
);

/**
  Client calls this API to get the timestamp for the STOP bit for the
  previous transfer that completed successfully. Return value indicates if the
  underlying implementation supports this API or not.

  In order to obtain the timestamp, the client must have set the
  I2C_FLAG_TIMESTAMP_P and I2C_FLAG_STOP bit in a previous READ or WRITE
  transfer descriptor. Once the transfer completed successfully, the client
  can call this API to obtain the timestamp value.
  
  The value returned by the function is recorded at the time of sending STOP command on the I2C bus.  

  The client shall obtain the value of START or STOP time-stamp per transfer
  and not both due to HW limitation.
  
  In most generic cases, the value is a timer tick at a resolution of
  about 6.66 microseconds(value of 0:6 bits approximation based on 19.2Mhz of QTIMER).
  It has a wrap-around time of around 7.95 Hrs.
  
  The value updated by this API is a read operation of [38:7] bits of QTIMER counter.

  The QTIMER is a 56 bit register, consisting of a 32 bit register (LSB) and 24 bit register(MSB).
  So  the client shall read the upper 24 bits and compare with previous value 
  if the value is 0 or less than that of the previously read value , the client can safely decide that 
  there has been a roll-over. The logic to detect  a roll-over is not supported by this API and can
  be implemented by the client if they choose to. 
 
  Interpretation of the timestamp value obtained from this API is platform
  specific. 

  @param[in] i2c_handle           Handle to the I2C/I3C instance.

  @return
  0 -- Timestamp not supported. \n
  N -- A non-zero timestmamp N, where N is the ticks read from [38:7] bits of QTIMER counter.

  @note
  Legacy API - For HW which supports 64 bit timestamp, this api will return 0.
*/
uint32
i2c_get_stop_timestamp
(
    void *i2c_handle
);

/**
  In some low power operating modes, the expected hw resources required to
  operate at a specific bus frequency/speed need to be setup before entering the
  low power mode. This API sets the resources required for the specific bus
  speed required by the use case in low power mode.

  @param[in]  instance            I2C/I3C instance that the client intends to
                                  initialize; see #i2c_instance for details.
  @param[in]  frequency_khz       Bus speed in KHz.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_PLATFORM_RESOURCE_SETUP_FAIL -- Hardware resource setup failure.
  
  @note
  Clients are advised to migrate to i2c_setup_lpi_resource_ex().
*/
i2c_status
i2c_setup_lpi_resource
(
    i2c_instance instance,
    uint32 frequency_khz
);


/**
  In some low power operating modes, the expected hw resources required to
  operate at a specific bus frequency/speed need to be setup before entering the
  low power mode. This API sets the resources required for the specific bus
  speed required by the use case in low power mode.

  @param[in]  qup                 Qup Type the client intends to use, see #QUP_TYPE in qup_common.h for details.
  @param[in]  se_index            Serial Engine index in QUP client intends to use. (eg: QUP_0_SE_1 -> se_index = 1)
  @param[in]  frequency_khz       Bus speed in KHz.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_PLATFORM_RESOURCE_SETUP_FAIL -- Hardware resource setup failure.
  
  @version API header version 5
*/
i2c_status
i2c_setup_lpi_resource_ex
(
    QUP_TYPE qup,
    uint32 se_index,
    uint32 frequency_khz
);

/**
  If a resource was previously setup for low power operation, this API resets
  the resources.

  @param[in]  instance            I2C/I3C instance that the client intends to
                                  initialize; see #i2c_instance for details.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_PLATFORM_RESOURCE_RESET_FAIL -- Hardware resource reset failure.
  
  @note
  Clients are advised to migrate to i2c_reset_lpi_resource_ex().
*/
i2c_status
i2c_reset_lpi_resource
(
    i2c_instance instance
);

/**
  If a resource was previously setup for low power operation, this API resets
  the resources.

  @param[in]  qup                 Qup Type the client intends to use, see #QUP_TYPE in qup_common.h for details.
  @param[in]  se_index            Serial Engine index in QUP client intends to use. (eg: QUP_0_SE_1 -> se_index = 1)
  @param[in]  frequency_khz       Client are expected to pass same frequncy which is passed during i2c_setup_lpi_resource()
  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_PLATFORM_RESOURCE_RESET_FAIL -- Hardware resource reset failure.
  
  @version API header version 5
*/
i2c_status
i2c_reset_lpi_resource_ex
(
    QUP_TYPE qup,
    uint32 se_index,
    uint32 frequency_khz
);

/**
  Sends an I3C common command code.

  The function executes an ccc_cmd synchronously at the bus speed specified in
  bus_speed_khz. In case the CCC is a write operation, the function writes length
  bytes from buffer to the I3C slave as part of the I3C CCC in the format specified
  in the I3C spec. In case the CCC is a read operation, the function reads length 
  bytes from the slave into the buffer in the format specified in the I3C spec. In
  case a read or write operation completed successfully, the transferred parameter
  holds the actual number of bytes read or written. If the ccc_cmd does not require
  data to be read/written from/to the slave, the buffer and length parameters are
  unused.

  Direct CCC is addressed to the slave_address passed to this function. The slave
  address may be a static address or the dynamic address based on the CCC.

  @param[in]        i2c_handle          Handle to the I2C/I3C instance.
  @param[in]        bus_speed_khz       Bus speed to be used for the command.
  @param[in]        ccc_cmd             I3C common command code enum.
                                        Shall not use commands SETDASA and SETNEWDAA.
                                        See
                                        #i3c_ccc for details.
  @param[in]        slave_address       7-bit I3C slave address. This is applicable
                                        only for the direct CCC. For other commands, this value is
                                        the dynamic address assigned by the I3C Master.
  @param[in/out]    buffer              The buffer contains the command payload that is 
                                        either written to the slave or read from the slave.
                                        For commands with no payload, this can be omitted.
  @param[in]        length              Length of the command payload or 0 if the command
                                        has no payload.
  @param[out]       transferred         Total number of bytes transferred as payload.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_API_PROTOCOL_MODE_NOT_SUPPORTED -- The mode parameter passed in #i2c_slave_config is not supported. \n
  I2C_ERROR_TRANSFER_TIMEOUT -- Transfer timed out. \n
  I2C_ERROR_HANDLE_ALREADY_IN_QUEUE -- Client IO is pending. \n
  I2C_ERROR_DMA_INSUFFICIENT_RESOURCES -- Insufficient resources to execute the transfer. \n
  I2C_ERROR_DMA_PROCESS_TRANSFER_FAIL -- Transfer processing failure in bus interface.
*/
i2c_status
i3c_send_ccc
(
    void *i2c_handle,
    uint32 bus_speed_khz,
    i3c_ccc ccc_cmd,
    uint8 slave_address,
    uint8 *buffer,
    uint32 length,
    uint32 *transferred
);

/**
  Enumerates devices in the I3C bus.

  The function enumerates devices on the I3C bus synchronously. This function must
  be called only once for the I3C bus. Before calling this function all slaves that
  need to be enumerated must be powered up.

  The devices that are enumerated and the number of devices are returned by this
  function.

  @param[in]        i2c_handle               Handle to the I2C/I3C instance.
  @param[out]       device_list              List of i3c devices enumerated on the bus. Refer
                                             to #bus_enum_table for the format. The memory allocated for the 
                                             buffer must be minimum of bus_enum_table * num_i3c_devices returned
                                             by #i3c_max_device_supported_onbus API.
  @param[in]        get_only_device_list     Does not perform a bus enumeration on the bus, but updates the latest
                                             device list maintained by the driver, which could have modified due to
                                             events such as hot-join.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
*/
i2c_status
i3c_bus_enumerate
(
    void *i2c_handle,
    bus_enum_table *device_list,
    uint32         *num_devices_enumerated,
    boolean get_only_device_list
);


/**
  The function returns the max number of devices supported on the I3C bus.
  This function can be called to allocate memory for #bus_enum_table type in i3c_bus_enumerate API.

  @param[in]        i2c_handle          Handle to the I2C/I3C instance.
  @param[out]       num_i3c_devices     Number of devices enumerated on the bus.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
*/
i2c_status
i3c_max_device_supported_onbus
(
    void *i2c_handle,
    uint32 *num_i3c_devices
);


/**
  Sets a new dynamic address for a slave with an already assigned dynamic address.

  i3c_enumerate_bus assigns a dynamic address to all the slaves on the bus, however, if
  a new slave address is required for the slave for some reason, this API will change
  the dynamic address.

  @param[in]        i2c_handle          Handle to the I2C/I3C instance.
  @param[in]        curr_dynamic_addr   Current dynamic address of the slave.
  @param[in]        new_dynamic_addr    New slave address that must be assigned. Allocated address pool is  0x00-0x45. Restricted addresses among them are 0x00-0x03 and 0x30-0x3E.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_API_PROTOCOL_MODE_NOT_SUPPORTED -- The mode parameter passed in #i2c_slave_config is not supported. \n
  I2C_ERROR_TRANSFER_TIMEOUT -- Transfer timed out. \n
  I2C_ERROR_HANDLE_ALREADY_IN_QUEUE -- Client IO is pending. \n
  I2C_ERROR_DMA_INSUFFICIENT_RESOURCES -- Insufficient resources to execute the transfer. \n
  I2C_ERROR_DMA_PROCESS_TRANSFER_FAIL -- Transfer processing failure in bus interface.
*/
i2c_status
i3c_set_new_dynamic_address
(
    void *i2c_handle,
    uint8 curr_dynamic_addr,
    uint8 new_dynamic_addr
);

/**
  Sets a dynamic address for a slave with a static address.

  If a slave defines a static address in the datasheet and does not support the ENTDAA
  broadcast CCC, this function is used to assign a dynamic address to the slave.

  @param[in]        i2c_handle      Handle to the I2C/I3C instance.
  @param[in]        static_addr     Static address of the slave as defined in the datasheet.
  @param[in]        dynamic_addr    Dynamic address that must be assigned to the slave. Allocated address pool is  0x00-0x45. Restricted addresses among them are 0x00-0x03 and 0x30-0x3E.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_API_PROTOCOL_MODE_NOT_SUPPORTED -- The mode parameter passed in #i2c_slave_config is not supported. \n
  I2C_ERROR_TRANSFER_TIMEOUT -- Transfer timed out. \n
  I2C_ERROR_HANDLE_ALREADY_IN_QUEUE -- Client IO is pending. \n
  I2C_ERROR_DMA_INSUFFICIENT_RESOURCES -- Insufficient resources to execute the transfer. \n
  I2C_ERROR_DMA_PROCESS_TRANSFER_FAIL -- Transfer processing failure in bus interface.
*/
i2c_status
i3c_set_dynamic_address_for_static_address
(
    void *i2c_handle,
    uint8 static_addr,
    uint8 dynamic_addr
);

/**
  Registers an IBI callback with the I3C driver and updates IBI Table parameters

  This function must be called after i3c_enumerate_bus(). This function stores 
  client callback, context and buffer related info internally in the driver so
  that it can notify the client when the slave asserts an IBI.
  This function can be called to modify entry for a particular slave in IBI Table

  @param[in]  i2c_handle          Handle to the I2C/I3C instance.
  @param[in]  slave_addr          7-bit dynamic address of the slave that will
                                  assert an IBI.
  @param[in]  func                The callback function that is called when the
                                  slave asserts an IBI.
  @param[in]  ctxt                The context that the client passes here is
                                  returned as is in the callback function.
  @param[out] rbuffer             A buffer that stores the IBI mandatory bytes.
  @param[in]  rlen                Size of rbuffer passed in this function.
  @param[in]  type                The type of timestamp to be captured during an IBI event see #ibi_timestamp_type.
  @param[in]  ibi_table_params    Table parameters passed by clients to modify IBI Table entry for respective slave_addr

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_DEVICE_IBI_NOT_SUPPORTED -- Device does not support IBI as per the BCR. \n 
  I2C_ERROR_DEVICE_NOT_ENUMERATED -- Device not present or not enumerated. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter.
*/
i2c_status
i3c_register_ibi_ex
(
    void *i2c_handle,
    uint8 slave_addr,
    callback_func func,
    void *ctxt,
    uint8 *rbuffer,
    uint32 rlen,
    ibi_timestamp_type type,
    i3c_ibi_table_params *ibi_table_params
);

#define i3c_register_ibi(i2c_handle, slave_addr, func, ctxt, rbuffer, rlen, type) \
            i3c_register_ibi_ex(i2c_handle, slave_addr, func, ctxt, rbuffer, rlen, type, NULL)
/**
  De-Registers an IBI callback with the I3C driver.

  This function must be called after i3c_register_ibi() or i3c_register_ibi_ex(). This function removes 
  client callback, context and buffer related info internally in the driver.

  @param[in]  i2c_handle          Handle to the I2C/I3C instance.
  @param[in]  slave_addr          7-bit dynamic address of the slave that will
                                  assert an IBI.
 

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_DEVICE_IBI_NOT_SUPPORTED -- Device does not support IBI as per the BCR. \n 
  I2C_ERROR_DEVICE_NOT_ENUMERATED -- Device not present or not enumerated. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter.
*/

i2c_status
i3c_deregister_ibi
(
    void *i2c_handle,
    uint8 slave_addr
);

/**
  Client calls this API to get the timestamp captured at the START/STOP condition for the
  previous transfer that completed successfully.

  In order to obtain the timestamp of a previous READ or WRITE transfer, the client must set 
  I2C_FLAG_TIMESTAMP_S with I2C_FLAG_START bit to capture start condition's timestamp or set 
  I2C_FLAG_TIMESTAMP_P with I2C_FLAG_STOP bit to capture stop condition's timestamp in the transfer 
  descriptor.
  
  Once the transfer completes successfully, the client can call this API to obtain the timestamp value.

  The timestamp is recorded by HW at the time of sending START/STOP command on the I2C bus, based on 
  whether I2C_FLAG_TIMESTAMP_S or I2C_FLAG_TIMESTAMP_P is set in the previous transfer descriptor.
  
  The client shall obtain the value of START or STOP timestamp per transfer and not both due to HW 
  limitation. If both flags I2C_FLAG_TIMESTAMP_* are set in a transfer descriptor then the client will  
  receive start condition's timestamp by default.

  Interpretation of the timestamp value obtained from this API is platform specific. 
  
  @param[in]  i2c_handle           Handle to the I2C/I3C instance.In case if i2c_prepare_transfer() and
                                   i2c_submit_transfer() is used, use multi_transfer_handle instead of i2c_handle to get timestamp of that particular transfer.
  @param[out] type                 Type of timestamp.See #i2c_timestamp_type for details
  @param[out] timestamp            64bit variable to store the timestamp value.

  @return
  I2C_SUCCESS -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  I2C_ERROR_API_NOT_SUPPORTED -- API is not supported. \n
  
  @note
  This API will be backward compatible with legacy HW versions.
  Clients are suggested to migrate to this api.
*/
i2c_status
i2c_get_timestamp_64
(
    void *i2c_handle,
    i2c_timestamp_type *type,
    uint64 *timestamp
);

/**

  Client can call the API to set different power profiles, this API alone can be used instead of 
  using i2c_power_on() and i2c_power_off(). This API also facilitates clients to set a performance level
  like turbo, where client can expect better KPIs as qup resources are voted for higer performance.
  
  Flow of usage can be as below
  i2c_open(.., &i2c_handle)
  i2c_set_power_level(i2c_handle, QUP_POWER_ON) -> set to low svs
  i2c_write(...)
  i2c_set_power_level(i2c_handle, QUP_POWER_TURBO) -> set for turbo
  i2c_write(...)
  i2c_set_power_level(i2c_handle, QUP_POWER_OFF) -> set to off
  i2c_close(...)

  @param[in]  i2c_handle           Handle to the I2C/I3C instance.
  @param[in]  profile              Indicates which performance profile to set (off/low/nominal/turbo).
  
  @return
  I2C_SUCCESS                           -- Function was successful. \n
  I2C_ERROR_INVALID_PARAMETER           -- Invalid parameter. \n
  I2C_ERROR_API_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  I2C_ERROR_PLATFORM_CLOCK_ENABLE_FAIL  -- Platform clock enable failure. \n
  I2C_ERROR_PLATFORM_GPIO_ENABLE_FAIL   -- Platform GPIO enable failure. \n
*/
i2c_status 
i2c_set_power_level
(
    void *i2c_handle,
    qup_power_profile profile
);

/**

  Client can call the API to get the current power profile choosen, then take next appropriate action,
  like updating to power level higer or lower based on the use case need.

  @param[in]  i2c_handle           Handle to the I2C/I3C instance.

  @return
  0 -- Getting power level failed. \n
  N -- N is the power level set on the I2C instance.

*/
qup_power_profile
i2c_get_power_level
(
    void *i2c_handle
);

/**
 * @brief Registers a callback for I3C hot-join device.
 *
 * This function allows the user to register a callback that will be triggered
 * when a hot-join event occurs on the I3C bus.
 *
 * @param[in] i2c_handle Handle to the I3C instance.
 * @param[in] func       Callback function to be invoked on hot-join. Refer to @ref callback_func for more details.
 * @param[in] pid        Pointer to the PID buffer. The client must provide the expected hot-join slave PID details during registration.
 * @param[in] pid_len    Length of the PID in bytes  (pid_len = 6 or pid_len = 0).
 *                       - If `pid_len = 6`, only the matching PID slave details will be shared via callback.
 *                       - If `pid_len = 0`, all slave information enumerated via ENTDAA will be shared via callback.
 * @param[in] user_ctxt  User-defined context to be passed to the callback.
 *
 * @return i2c_status Status of the registration operation (success or failure) refer to @ref i2c_status.
 */
i2c_status i3c_register_hot_join(void *i2c_handle, callback_func func, uint8 *pid, uint8 pid_len, void *user_ctxt);

/**
 * @brief Deregisters a previously registered I3C hot-join callback.
 *
 * This function removes the hot-join registration for a device identified by its PID.
 *
 * @param[in] i2c_handle Pointer to the I2C client context.
 * @param[in] pid        Pointer to the PID (Provisional ID) of the device to deregister.
 * @param[in] pid_len    Length of the PID in bytes.
 *
 * @return I2C_SUCCESS if deregistration is successful, or an appropriate error code refer to @ref i2c_status.
 */
i2c_status i3c_deregister_hot_join(void *i2c_handle, uint8 *pid, uint32 pid_len);

/**
 * @brief Registers client island/non-island status and stores it for invoking IBI and Hot join callbacks.
 *
 * DON'T call from ISR context, to be called after i2c_open() and before i3c_register_ibi() or i3c_register_hot_join().
 *
 * @param[in] i2c_handle             Pointer to the I2C client context.
 * @param[in] is_active_in_island    TRUE if client will be in island / FALSE if client is not in island
 *
 * @return I2C_SUCCESS|I2C_ERROR_INVALID_PARAMETER
 */
 i2c_status  i3c_is_clnt_active_in_island(void *i2c_handle, boolean is_active_in_island);

/** @} */ /* end_namegroup */
/** @} */ /* end_addtogroup i2c_api */

#endif /* __I2C_API_H__ */
