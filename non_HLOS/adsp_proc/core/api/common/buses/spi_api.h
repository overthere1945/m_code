/**
    @file  spi_api.h
    @brief SPI Driver API
 */
/*
  ===========================================================================

  Copyright (c) 2016, 2020-2021, 2023 Qualcomm Technologies Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary

  ===========================================================================

  $Header: //components/rel/core.qdsp6/8.2.3/api/common/buses/spi_api.h#1 $
  $DateTime: 2025/11/18 04:23:34 $
  $Author: pwbldsvc $

  ===========================================================================
*/
#ifndef __SPIAPI_H__
#define __SPIAPI_H__

#include "comdef.h"
#include "qup_common.h"

/**
  @file  spi_api.h
  @addtogroup spi_api
  @brief Serial Peripheral Interface Driver
@{ */

/** @name SPI Driver Version 
@{ */

/** @version 6 @{ */
/*                         VERSION HISTORY                         
Version     Details                                                 
-------     --------------------------------------------------------
  6         Added spi API to set/get power/performance levels
  5         Added new APIs to support batched/single transfers for latency sensitive usecase
  4         Added new argument frequency to API spi_reset_island_resource_ex() to support DFS
  3         Add open APIs to support QUP type and reorder spi_status_t
  2         Added Support for 64 Bit timestamp and Clients to pass data for Multi CS
  1         Initial version
====================================================================*/

#define SPI_API_H_VERSION  6

/** @} */ /* end_namegroup */

/** @name Preprocessor Definitions and Constants
@{ */

/** Check for API Return Values */
#define SPI_SUCCESS(x)  (x == SPI_SUCCESS)
#define SPI_ERROR(x)    (x != SPI_SUCCESS)

/** Values to be passed to #timestamp param in spi_full_duplex() to capture timestamp*/
#define GET_SPI_TIMESTAMP_NONE           0x0
#define GET_SPI_CS_ASSERT_TIMESTAMP      0x1
#define GET_SPI_CS_DE_ASSERT_TIMESTAMP   0x2

/** @} */ /* end_namegroup */

/** @name Type Definitions
@{ */

/** 
  Callback prototype for Async Transfers, 
  pointer of the same is passed to spi_full_duplex
  */
typedef void (*callback_fn) (uint32 transfer_status, void *callback_ctxt);

/** SPI Instance which client wants to use, 
    should be passed to spi_open()*/
typedef enum
{
   SPI_INSTANCE_1 = 1,
   SPI_INSTANCE_2,
   SPI_INSTANCE_3,
   SPI_INSTANCE_4,
   SPI_INSTANCE_5,
   SPI_INSTANCE_6,
   SPI_INSTANCE_7,
   SPI_INSTANCE_8,
   SPI_INSTANCE_9,
   SPI_INSTANCE_10,
   SPI_INSTANCE_11,
   SPI_INSTANCE_12,
   SPI_INSTANCE_13,
   SPI_INSTANCE_14,
   SPI_INSTANCE_15,
   SPI_INSTANCE_16,
   SPI_INSTANCE_17,
   SPI_INSTANCE_18,
   SPI_INSTANCE_19,
   SPI_INSTANCE_20,
   SPI_INSTANCE_21,
   SPI_INSTANCE_22,
   SPI_INSTANCE_23,
   SPI_INSTANCE_24,
   SPI_INSTANCE_25,
   SPI_INSTANCE_26,
   SPI_INSTANCE_27,
   SPI_INSTANCE_28,
   SPI_INSTANCE_29,
   SPI_INSTANCE_30,
   SPI_INSTANCE_31,
   SPI_INSTANCE_32,
   SPI_INSTANCE_MAX,
} spi_instance_t;

/** Function return status and error code*/    
typedef enum                                   
{                                                       
   SPI_SUCCESS  = 0x0000,                               
                                                        
   /** parameter and system errors */                   
   SPI_ERROR_GENERIC                           = 0x1000, 
   SPI_ERROR,                                           
   SPI_ERROR_INVALID_PARAM,                             
   SPI_ERROR_HW_INFO_ALLOCATION,                        
   SPI_ERROR_MEM_ALLOC,                                 
   SPI_ERROR_MUTEX,                                     
   SPI_ERROR_HANDLE_ALLOCATION,                         
   SPI_ERROR_INVALID_EXECUTION_LEVEL,                   
   SPI_ERROR_UNSUPPORTED_IN_ISLAND_MODE,                
   SPI_ERROR_UNCLOCKED_ACCESS,                          
   SPI_ERROR_CORE_NOT_OPEN,                             
                                                        
   /** transfer errors */               
   SPI_ERROR_TRANSFER                          = 0x2000,
   SPI_ERROR_TRANSFER_TIMEOUT,                          
   SPI_ERROR_PENDING_TRANSFER,                          
                                                        
   /** dma related errors */         
   SPI_ERROR_DMA_OPS                           = 0x3000,
   SPI_ERROR_DMA_PROCESS_TRE_FAIL,                      
   SPI_ERROR_DMA_INSUFFICIENT_RESOURCES,                
   SPI_ERROR_DMA_EVT_OTHER,                             
   SPI_ERROR_DMA_QUP_NOTIF,                             
   SPI_ERROR_DMA_REG_FAIL,                              
   SPI_ERROR_DMA_DEREG_FAIL,                            
   SPI_ERROR_DMA_TX_CHAN_ALLOC_FAIL,                    
   SPI_ERROR_DMA_RX_CHAN_ALLOC_FAIL,                    
   SPI_ERROR_DMA_EV_CHAN_ALLOC_FAIL,                    
   SPI_ERROR_DMA_TX_CHAN_START_FAIL,                    
   SPI_ERROR_DMA_RX_CHAN_START_FAIL,                    
   SPI_ERROR_DMA_TX_CHAN_STOP_FAIL,                     
   SPI_ERROR_DMA_RX_CHAN_STOP_FAIL,                     
   SPI_ERROR_DMA_TX_CHAN_RESET_FAIL,                    
   SPI_ERROR_DMA_RX_CHAN_RESET_FAIL,                    
   SPI_ERROR_DMA_TX_CHAN_DEALLOC_FAIL,                  
   SPI_ERROR_DMA_RX_CHAN_DEALLOC_FAIL,                  
   SPI_ERROR_DMA_EV_CHAN_DEALLOC_FAIL,                  
                                                        
   /** platform errors */                               
   SPI_ERROR_PLATFORM                           = 0x4000,
   SPI_ERROR_PLAT_GET_CONFIG_FAIL,                      
   SPI_ERROR_PLAT_REG_INT_FAIL,                         
   SPI_ERROR_PLAT_CLK_ENABLE_FAIL,                      
   SPI_ERROR_PLAT_GPIO_ENABLE_FAIL,                     
   SPI_ERROR_PLAT_CLK_DISABLE_FAIL,                     
   SPI_ERROR_PLAT_GPIO_DISABLE_FAIL,                    
   SPI_ERROR_PLAT_CLK_SET_FREQ_FAIL,                    
   SPI_ERROR_PLAT_INTERRUPT_REGISTER,                   
   SPI_ERROR_PLAT_INTERRUPT_DEREGISTER,                 
   SPI_ERROR_PLAT_SET_RESOURCE_FAIL,                    
   SPI_ERROR_PLAT_RESET_RESOURCE_FAIL,                  
                                                        
   SPI_ERROR_QDI_OPS                           = 0x5000,
   SPI_ERROR_QDI_ALLOC_FAIL,                            
   SPI_ERROR_QDI_COPY_FAIL,                             
   SPI_ERROR_QDI_MMAP_FAIL,                             
                                                        
   /**< SW code errors. */                              
} spi_status_t;

/**
  Enum definiton of SPI Modes
  
  @note In the legacy (QUP v2 SPI) driver, the "Input-first/ Output first"
  terminology was used for denoting Clock Phase. We are using the more
  widely accepted MODE_0 to MODE_3 convention in this driver.
  If you're converting from the old SPI API to the new one, use this guide.
 
  Clk Polarity: SPI_CLK_IDLE_HIGH corresponds to CPOL = 0
  Clk Phase: SPI_OUTPUT_FIRST_MODE correposnds to CPHA = 0
 
  For new SPI slaves, refer to the slave specifications to decide which
  mode to use.
*/
typedef enum
{
   SPI_MODE_0,   /**< CPOL = 0, CPHA = 0*/
   SPI_MODE_1,   /**< CPOL = 0, CPHA = 1*/
   SPI_MODE_2,   /**< CPOL = 1, CPHA = 0*/
   SPI_MODE_3,   /**< CPOL = 1, CPHA = 1*/
}spi_mode_t;

/**
  Enum Definiton of Chip Select Polarity
 */
typedef enum
{
   SPI_CS_ACTIVE_LOW,      /**< During idle state CS  line is held low*/
   SPI_CS_ACTIVE_HIGH,     /**< During idle state CS line is held high*/
   SPI_CS_ACTIVE_INVALID = 0x7FFFFFFF
}spi_cs_polarity_t;

/** 
  Enum Definiton of Data Order for buffers
  
  @note Endianness change is presently not supported by QUPv3
 */
typedef enum
{
   SPI_NATIVE = 0,
   SPI_LITTLE_ENDIAN = 0,
   SPI_BIG_ENGIAN          /**< Network Usage*/
}spi_byte_order_t;

/**
  Enum to indicate the type of timestamp returned to the client from #spi_get_timestamp_64 api .
  The bit positions of the snapshot captured by the HW have been mentioned with each enum definition.
*/
typedef enum
{
   SPI_TIMESTAMP_32_BIT_LEGACY = 1,    /**< Returned value is 32 bit timestamp [38:7] of QTimer value 
                                            as provided by HW.*/
   SPI_TIMESTAMP_64_BIT,               /**< Returned value is 64 bit timestamp [63:0] of QTimer value 
                                            as provided by HW.*/
} spi_timestamp_type;

/**
  Enum to indicate Chip Select Index
*/
typedef enum
{
   SPI_CHIP_SELECT_0 = 0,   /**< CS 0 */
   SPI_CHIP_SELECT_1,       /**< CS 1 */
   SPI_CHIP_SELECT_2,       /**< CS 2 */
   SPI_CHIP_SELECT_3,       /**< CS 3 */
   SPI_CHIP_SELECT_INVALID,
} spi_cs_index;

/** @} */ /* end_namegroup */

/** @name Structure Definitions
@{ */

/** SPI configuration passed to the spi_full_duplex() function. */
typedef struct
{ 
   spi_mode_t         spi_mode;                 /**< SPI Mode*/
   spi_cs_polarity_t  spi_cs_polarity;          /**< CS Polartity*/
   spi_byte_order_t   endianness;               /**< Data Endianness*/
   uint8              spi_bits_per_word;        /**< 4 <= N <= 32 */
   uint8              spi_slave_index;          /**< Slave index, beginning at 0, if mulitple SPI devices are connected 
                                                     to the same master. At most 4 slaves are allowed as per present HW. 
                                                     If an invalid number (4 or higher) is set, CS signal will not be used */
   uint32             clk_freq_hz;              /**< Host will set the SPI clk frequency closest to the requested frequency */
   uint8              cs_clk_delay_cycles;      /**< Num of clk cycles to wait after asserting CS before starting txfer */
   uint8              inter_word_delay_cycles;  /**< Num of clk cycles to wait between SPI words */
   boolean            cs_toggle;                /**< 1 = CS deasserts between words , 0 = CS stays asserted between words. */
   boolean            loopback_mode;            /**< Normally 0. If set, SPI controller will enable loopback mode, 
                                                     used primarily for testing*/
} spi_config_t;

/** 
  SPI Descriptor passed to the spi_full_duplex() function, holds data to be transferred
  @note For Half Duplex Transfers i.e. only Rx or TX, pass the other buf as NULL.
 */
typedef struct
{
   uint8  *tx_buf;   
   uint8  *rx_buf;   
   uint32  len;
   boolean leave_cs_asserted;
}spi_descriptor_t;

typedef struct spi_config_desc_pairs
{
    spi_config_t *config;                       /**< Bus configuration. See #spi_config_t for details.*/
    spi_descriptor_t *desc;                     /**<SPI transfer descriptor. See #spi_descriptor_t for details. 
                                                   This can be an array of descriptors.*/ 
    uint16 num_desc;                            /**< Number of descriptors in the descriptor array.*/
}spi_config_desc_pairs;

/** @} */ /* end_namegroup */

/** @name Function Declarations
@{ */

/**
  @brief Client calls this api to initalise a SPI port and get a handle for the same,
  This api allocates and initializes resources reuqired ,but does not talk to the SPI 
  hardware. These resources are released in spi_close() call.
  
  @param[in]  instance            SPI instance that the client intends to open
  @param[out] spi_handle          Pointer location filled by the driver with a handle 
                                  used as a reference to this port in other apis.

  @note Clients are advised to use spi_open_ex().

  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR -- Generic Error. \n
  SPI_ERROR_INVALID_PARAM -- Invalid parameter. \n
  SPI_ERROR_MEM_ALLOC  -- Memory allocation error. \n
  SPI_ERROR_HW_INFO_ALLOCATION -- Hardware information allocation error. \n
  SPI_ERROR_MUTEX -- Mutex initalisation failed. \n
  SPI_ERROR_DMA_REG_FAIL -- Registration failure with bus interface. \n
  SPI_ERROR_DMA_EV_CHAN_ALLOC_FAIL -- Event channel allocation failure. \n
  SPI_ERROR_DMA_TX_CHAN_ALLOC_FAIL -- TX channel allocation failure. \n
  SPI_ERROR_DMA_RX_CHAN_ALLOC_FAIL -- RX channel allocation failure. \n
  SPI_ERROR_DMA_TX_CHAN_START_FAIL -- TX channel start failure. \n
  SPI_ERROR_DMA_RX_CHAN_START_FAIL -- RX channel start failure.
*/
spi_status_t spi_open (spi_instance_t instance, void **spi_handle);

/**
  @brief Client calls this api to initalise a SPI port and get a handle for the same,
  This api allocates and initializes resources reuqired ,but does not talk to the SPI 
  hardware. These resources are released in spi_close() call.
  
  @param[in]  qup                 Qup Type the client intends to use, see #QUP_TYPE in qup_common.h for details.
  @param[in]  se_index           Serial Engine index in QUP client intends to use.
  @param[out] spi_handle          Pointer location filled by the driver with a handle 
                                  used as a reference to this port in other apis.

  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR -- Generic Error. \n
  SPI_ERROR_INVALID_PARAM -- Invalid parameter. \n
  SPI_ERROR_MEM_ALLOC  -- Memory allocation error. \n
  SPI_ERROR_HW_INFO_ALLOCATION -- Hardware information allocation error. \n
  SPI_ERROR_MUTEX -- Mutex initalisation failed. \n
  SPI_ERROR_DMA_REG_FAIL -- Registration failure with bus interface. \n
  SPI_ERROR_DMA_EV_CHAN_ALLOC_FAIL -- Event channel allocation failure. \n
  SPI_ERROR_DMA_TX_CHAN_ALLOC_FAIL -- TX channel allocation failure. \n
  SPI_ERROR_DMA_RX_CHAN_ALLOC_FAIL -- RX channel allocation failure. \n
  SPI_ERROR_DMA_TX_CHAN_START_FAIL -- TX channel start failure. \n
  SPI_ERROR_DMA_RX_CHAN_START_FAIL -- RX channel start failure.
  
  @version API header version 3
*/
spi_status_t spi_open_ex (QUP_TYPE qup, uint32 se_index, void **spi_handle);

/**
  @brief Enables the clocks, gpios and any other power resources used by the SPI instance.

  @param[in]  spi_handle            SPI handle
  
  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR_INVALID_PARAM -- Invalid parameter. \n
  SPI_ERROR_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  SPI_ERROR_PLAT_CLK_ENABLE_FAIL -- Platform clock enable failure. \n
  SPI_ERROR_PLAT_GPIO_ENABLE_FAIL -- Platform GPIO enable failure.
*/
spi_status_t spi_power_on (void *spi_handle);

/**
  @brief Disables the clocks, gpios and any other power resources used by the SPI instance.

  @param[in]  spi_handle            SPI handle
  
  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR_INVALID_PARAM -- Invalid parameter. \n
  SPI_ERROR_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  SPI_ERROR_PLAT_CLK_DISABLE_FAIL -- Platform clock enable failure. \n
  SPI_ERROR_PLAT_GPIO_DISABLE_FAIL -- Platform GPIO enable failure.
*/
spi_status_t spi_power_off (void *spi_handle);

/**
  @brief Performs an SPI full duplex, half duplex or simplex transfer over SPI BUS 
  synchronously or asynchronously. 

  If the callback parameter is NULL, the function executes synchronously. When
  the function returns, the transfer is complete.

  If the callback parameter is not NULL, the functions executes
  asynchronously. The function returns by queueing the transfer to the
  underlying driver. Once the transfer is complete, the driver calls the
  callback provided. Refer to #callback_fn for callback definition. If the
  function returns a failure, the transfer has not been queued and no
  callback will occur. If the transfer returns SPI_SUCCESS, the transfer has
  been queued and a further status of the transfer can only be obtained when
  the callback is called. After a client calls this API, it must wait for the
  completion callback to occur before it can call the API again. If the
  client wishes to queue mutliple transfers, it must use an array of
  descriptors of type spi_descriptor_t instead of calling the API multiple
  times.
  
  ***********************************************************************************
  For SPI 3 Wire only TX or TX followed by RX is supported. RX alone is not Supported
  ***********************************************************************************

  ************************
  Desc format for TX alone
  ************************
  desc[0].tx_buf = tx_buffer;
  desc[0].rx_buf = NULL;
  desc[0].len = Number of bytes to be transferred;
  desc[0].leave_cs_asserted = 0;
  
  num_descriptors = 1;

  *********************************
  Desc format for TX followed by RX
  *********************************
  desc[0].tx_buf = tx_buffer;
  desc[0].rx_buf = NULL;
  desc[0].len = Number of bytes to be transferred;
  desc[0].leave_cs_asserted = 0;

  desc[1].tx_buf = NULL;
  desc[1].rx_buf = rx_buffer;
  desc[1].len = Number of bytes to be received;
  desc[1].leave_cs_asserted = 0;
  
  num_descriptors = 2;


  **Note for SPI 3 Wire :
  Single Descriptor with RX alone is not supported
  RX descriptor must always be followed with TX descriptor
  Eg : TX RX TX RX is supported, TX RX RX RX invalid descriptors

  @param[in]  spi_handle          SPI handle
  @param[in]  config              Bus configuration. See #spi_config_t for details.
  @param[in]  desc                SPI transfer descriptor. See #spi_descriptor_t for details. 
                                  This can be an array of descriptors.
  @param[in]  num_descriptors     Number of descriptors in the descriptor array.
  @param[in]  c_fn                The callback function that is called at the completion of 
                                  the transfer. This callback occurs in a dispatch context. 
                                  The callback must do minimal processing and must not call 
                                  any API defined here. 
                                  If the callback is NULL, the function executes
                                  synchronously.
  @param[in]  ctxt                The context that the client passes here is
                                  returned as is in the callback function.
  @param[in]  timestamp           Following Macro's to be passed for timestamp Capture
                                  GET_SPI_TIMESTAMP_NONE : Do not capture any timestamp
                                  GET_SPI_CS_ASSERT_TIMESTAMP : Get CS assert Timestamp
                                  GET_SPI_CS_DE_ASSERT_TIMESTAMP : Get CS deassert Timestamp

  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR_INVALID_PARAM -- Invalid parameter. \n
  SPI_ERROR_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  SPI_ERROR_TRANSFER_TIMEOUT -- Transfer timed out. \n
  SPI_ERROR_PENDING_TRANSFER -- Client IO is pending. \n
  SPI_ERROR_DMA_INSUFFICIENT_RESOURCES -- Insufficient resources to execute the transfer. \n
  SPI_ERROR_DMA_PROCESS_TRE_FAIL -- Transfer processing failure in bus interface.
*/
spi_status_t spi_full_duplex (void *spi_handle, spi_config_t *config, spi_descriptor_t *desc, uint32 num_descriptors, callback_fn c_fn, void *c_ctxt, uint8 timestamp);

/**
  Saves context of SPI transfer sin underlying driver. A callback
  function is passed as a parameter to this function. It is saved in underlying driver and used as a callback function in spi_submit_transfer() API.

  If the callback parameter is NULL, it means spi_submit_transfer() API executes synchronously. When
  the function returns, the transfer is complete.

  If the callback parameter is not NULL, it means pi_submit_transfer() API executes
  asynchronously.

  The function returns by saving context in underlying driver.  If the transfer returns SPI_SUCCESS, the transfer context has been saved. If the function returns a failure otherwise. After a client calls this API, it must wait for the completion callback to occur before it can call the API again with same multi_transfer_handle.

  If client wishes to queue mutliple transfers, it must use an array of
  descriptors of type spi_descriptor inside spi_config_desc_pairs instead of calling the API multiple
  times. 

  If client wishes to queue transfer for  multiple slaves, it must use an array of spi_config_desc_pairs which contains spi_descriptors_t per slave config.

  Underlying implementation may not support asynchronous mode. The return
  value will indicate the lack of support in which case client must pass a
  NULL value for func.
  
  NOTE: Client should call spi_get_timestamp_64() or spi_get_timestamp() with multi_transfer_handle handle instead of spi_handle to get timestamp for this transfer context. This timestamp should be called API spi_submit_transfer() completes successfully in sync mode. OR client registered callback function is called for Asynchronous mode of spi_submit_transfer() API.

  
  ***********************************************************************************
  For SPI 3 Wire only TX or TX followed by RX is supported. RX alone is not Supported
  ***********************************************************************************

  ************************
  Desc format for TX alone
  ************************
  spi_cfg_desc_pairs[0].desc[0].tx_buf = tx_buffer;
  spi_cfg_desc_pairs[0].desc[0].rx_buf = NULL;
  spi_cfg_desc_pairs[0].desc[0].len = Number of bytes to be transferred;
  spi_cfg_desc_pairs[0].desc[0].leave_cs_asserted = 0;
  
  spi_cfg_desc_pairs[0].num_descriptors = 1;

  *********************************
  Desc format for TX followed by RX
  *********************************
  spi_cfg_desc_pairs[1].desc[0].tx_buf = tx_buffer;
  spi_cfg_desc_pairs[1].desc[0].rx_buf = NULL;
  spi_cfg_desc_pairs[1].desc[0].len = Number of bytes to be transferred;
  spi_cfg_desc_pairs[1].desc[0].leave_cs_asserted = 0;

  spi_cfg_desc_pairs[1].desc[1].tx_buf = NULL;
  spi_cfg_desc_pairs[1].desc[1].rx_buf = rx_buffer;
  spi_cfg_desc_pairs[1].desc[1].len = Number of bytes to be received;
  spi_cfg_desc_pairs[1].desc[1].leave_cs_asserted = 0;
  
  spi_cfg_desc_pairs[1].num_descriptors = 2;


  **Note for SPI 3 Wire :
  Descriptor pair with RX alone is not supported
  RX descriptor must always be followed with TX descriptor in any cfg_desc_pair
  Eg : cfg_desc_pair with TX RX TX RX is supported,  TX RX RX RX is not supported


  @param[in]  spi_handle              SPI handle
  @param[in]  spi_cfg_desc_pairs      An array of slave configs and corresponding multiple descriptors per slave 
  @param[in]  num_desc_pairs          Number of config descriptors pairs in the spi_config_desc_pairs array.
  @param[in]  c_fn                    The callback function that is called at the completion of 
                                      the transfer. This callback occurs in a dispatch context. 
                                      The callback must do minimal processing and must not call 
                                      any API defined here. 
                                      If the callback is NULL, the function executes
                                      synchronously.
  @param[in]  ctxt                    The context that the client passes here is
                                      returned as is in the callback function.
  @param[in]  timestamp               Following Macro's to be passed for timestamp Capture
                                      GET_SPI_TIMESTAMP_NONE : Do not capture any timestamp
                                      GET_SPI_CS_ASSERT_TIMESTAMP : Get CS assert Timestamp
                                      GET_SPI_CS_DE_ASSERT_TIMESTAMP : Get CS deassert Timestamp
 
  @param[out] multi_transfer_handle   Pointer location to be filled by the driver with a handle to this transfer.

  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR_INVALID_PARAM -- Invalid parameter. \n
  SPI_ERROR_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  SPI_ERROR_TRANSFER_TIMEOUT -- Transfer timed out. \n
  SPI_ERROR_PENDING_TRANSFER -- Client IO is pending. \n
  SPI_ERROR_DMA_INSUFFICIENT_RESOURCES -- Insufficient resources to execute the transfer. \n
  SPI_ERROR_DMA_PROCESS_TRE_FAIL -- Transfer processing failure in bus interface.
*/
spi_status_t
spi_prepare_transfer
(
    void *spi_handle,
    spi_config_desc_pairs *spi_cfg_desc_pairs,
    uint16 num_desc_pairs,
    callback_fn c_fn,
    void *ctxt,
    uint8 timestamp,
    void **multi_transfer_handle    
);

/**
  Queues transfer to HW with already provided descriptors from spi_prepare_transfer() API synchronously or asynchronously based on callback function passed in spi_prepare_transfer() API.   

  If the transfer returns SPI_SUCCESS, the transfer has been queued and a further status of the transfer can only be obtained when the callback is called.

  @param[in]  spi_handle             Handle to the SPI instance.
  @param[in]  multi_transfer_handle  Handle to transfer 
  @param[in]  func                   The callback function that is called at the completion of the transfer. This callback occurs in a dispatch context. The callback
                                     must do minimal processing and must not call any API defined here. If the callback is NULL, the function executes
                                     synchronously.

  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR_INVALID_PARAM -- Invalid parameter. \n
  SPI_ERROR_INVALID_EXECUTION_LEVEL -- Invalid execution level. \n
  SPI_ERROR_TRANSFER_TIMEOUT -- Transfer timed out. \n
  SPI_ERROR_PENDING_TRANSFER -- Client IO is pending. \n
  SPI_ERROR_DMA_INSUFFICIENT_RESOURCES -- Insufficient resources to execute the transfer. \n
  SPI_ERROR_DMA_PROCESS_TRE_FAIL -- Transfer processing failure in bus interface.
  */
spi_status_t
spi_submit_transfer
(
    void *spi_handle,
    void *multi_transfer_handle
);
/**
  @brief Client calls this api to de-initalise resources allocated by spi_open() api.
  
  @param[in]  spi_handle            SPI handle
  
  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR -- Generic Error. \n
  SPI_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  SPI_ERROR_DMA_DEREG_FAIL -- Platform de-registration internal failure. \n
  SPI_ERROR_DMA_TX_CHAN_STOP_FAIL -- TX channel stop failure. \n
  SPI_ERROR_DMA_RX_CHAN_STOP_FAIL -- RX channel stop failure. \n
  SPI_ERROR_DMA_TX_CHAN_RESET_FAIL -- TX channel reset failure. \n
  SPI_ERROR_DMA_RX_CHAN_RESET_FAIL -- RX channel reset failure. \n
  SPI_ERROR_DMA_EV_CHAN_DEALLOC_FAIL -- Event channel de-allocation failure. \n
  SPI_ERROR_DMA_TX_CHAN_DEALLOC_FAIL -- TX channel de-allocation failure. \n
  SPI_ERROR_DMA_RX_CHAN_DEALLOC_FAIL -- RX channel de-allocation failure.
*/
spi_status_t spi_close (void *spi_handle);


/**
  @brief Client Calls this api to retrieve the CS Assert/De-Assert Timestamp captured by the QUP
  HW based on #timestamp parameter in spi_full_duplex api.
  
  In most generic cases, the value captured by HW is a snapshot of the QTIMER which has a 56 bit register,
  consisting of a 32 bit register (LSB) and 24 bit register(MSB).The logic to detect a roll-over is not 
  supported by this api.
  This api returns a 32 bit timestamp [38:7] of QTimer value as provided by HW.

  @note Legacy API, will return SPI_ERROR for QUP Versions 3.0 and higher.

  @param[in]  spi_handle            SPI handle to the bus instance. In case if spi_prepare_transfer() and
                                    spi_submit_transfer() is used, use multi_transfer_handle instead of spi_handle to get timestamp of that particular transfer.
  @param[out] start_time            Returns the CS assert timestamp if #timestamp in spi_transfer() 
                                    is non-zero.
  @param[out] completed_time        Returns the CS de-assert timestamp if #timestamp in spi_transfer() 
                                    is non-zero.

  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR -- Generic Error/QUP HW Version 3.0 and higher. \n
  SPI_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  SPI_ERROR_PENDING_TRANSFER -- Transfer Not completed.
*/
spi_status_t spi_get_timestamp (void *spi_handle, uint64 *start_time, uint64 *completed_time);

/**
  @brief In some low power operating modes, the expected hw resources required to
  operate at a specific bus frequency/speed need to be setup before entering the
  low power modes. 
  This API sets the resources required for the specific bus speed required 
  by the use case in low power mode.

  @note Should be called before spi_open(). Clients are advised to use spi_setup_island_resource_ex()

  @param[in]  instance            SPI instance that the client intends to
                                  initialize; see #spi_instance_t for details.
  @param[in]  frequency_hz        Bus speed in Hz.

  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR_CORE_NOT_OPEN; -- Invalid instance for SPI. \n
  SPI_ERROR_PLAT_SET_RESOURCE_FAIL -- Hardware resource setup failure.
*/
spi_status_t spi_setup_island_resource (spi_instance_t instance, uint32 frequency_hz);

/**
  @brief In some low power operating modes, the expected hw resources required to
  operate at a specific bus frequency/speed need to be setup before entering the
  low power modes. 
  This API sets the resources required for the specific bus speed required 
  by the use case in low power mode.

  @note Should be called before spi_open_ex()

  @param[in]  qup                 Qup Type the client intends to use, see #QUP_TYPE in qup_common.h for details.
  @param[in]  se_index           Serial Engine index in QUP client intends to use.
  @param[in]  frequency_hz        Bus speed in Hz.

  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR_CORE_NOT_OPEN; -- Invalid instance for SPI. \n
  SPI_ERROR_PLAT_SET_RESOURCE_FAIL -- Hardware resource setup failure.
  
  @version API header version 3
*/
spi_status_t spi_setup_island_resource_ex (QUP_TYPE qup, uint32 se_index, uint32 frequency_hz);

/**
  @brief Releases Resources set up for low power modes by spi_setup_island_resource()

  @param[in]  instance            SPI instance that the client intends to
                                  initialize; see #spi_instance_t for details.
  
  @note Clients are advised to migrate to spi_reset_island_resource_ex().
  
  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR_CORE_NOT_OPEN; -- Invalid instance for SPI. \n
  SPI_ERROR_PLAT_RESET_RESOURCE_FAIL -- Hardware resource setup failure.
*/
spi_status_t spi_reset_island_resource (spi_instance_t instance);

/**
  @brief Releases Resources set up for low power modes by spi_setup_island_resource()

  @param[in]  qup                 Qup Type the client intends to use, see #QUP_TYPE in qup_common.h for details.
  @param[in]  se_index           Serial Engine index in QUP client intends to use.
  @param[in]  instance            SPI instance that the client intends to
                                  initialize; see #spi_instance_t for details.
  @param[in]  frequency_hz        Bus speed in Hz

  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR_CORE_NOT_OPEN; -- Invalid instance for SPI. \n
  SPI_ERROR_PLAT_RESET_RESOURCE_FAIL -- Hardware resource setup failure.
  
  @version API header version 3
*/
spi_status_t spi_reset_island_resource_ex (QUP_TYPE qup, uint32 se_index, uint32 frequency_hz);

/**
  @brief Client Calls this api to retrieve the CS Assert/De-Assert Timestamp captured by the QUP
  HW based on #timestamp parameter passed in spi_full_duplex api.
  
  In most generic cases, the value captured by HW is a snapshot of the QTIMER which has a 56 bit register,
  consisting of a 32 bit register (LSB) and 24 bit register(MSB).The logic to detect a roll-over is not 
  supported by this api.
  This api returns a 32/64 bit timestamp based on QUP HW Version, clients can check value filled in #ts_type
  parameter for the same,

  @param[in]  spi_handle            SPI handle to the bus instance. In case if spi_prepare_transfer() and
                                    spi_submit_transfer() is used, use multi_transfer_handle instead of spi_handle to get timestamp of that particular transfer.
  @param[out] ts_type               Returns type of timestamp, refer to #spi_timestamp_type.
  @param[out] timestamp_val         Returns the CS assert or de-assert timestamp based on value passed to
                                    #timestamp parameter in spi_transfer() api.

  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR -- Generic Error. \n
  SPI_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  SPI_ERROR_PENDING_TRANSFER -- Transfer Not completed.
*/
spi_status_t spi_get_timestamp_64 (void *spi_handle, spi_timestamp_type *ts_type, uint64 *timestamp_val);

/**
  @brief Client Calls this api to let the driver know that it intends to use extra Chip Select 
  indexes (i.e. N > 0) for a SPI port.
  
  @note This api needs to be called after spi_open() before calling spi_power_on() for a non-zero CS index,
  this would allow the driver to configure the GPIO resource in the next spi_power_on() call.
  For multiple CS enablement the api needs to be called for each CS index value.
  Slave Index 0 gets enabled by default, it can be skipped for the same.

  @param[in]  spi_handle            SPI handle
  @param[in]  cs_index              Enables Chip Select for Slave index.

  @return
  SPI_SUCCESS -- Function was successful. \n
  SPI_ERROR -- Generic Error. \n
  SPI_ERROR_INVALID_PARAMETER -- Invalid parameter. \n
  SPI_ERROR_HW_INFO_ALLOCATION -- GPIO Resource unavailable for extra CS. \n
  SPI_ERROR_PLAT_GPIO_ENABLE_FAIL -- GPIO Resource Enable Fail. \n
*/
spi_status_t spi_enable_slave_index (void *spi_handle, spi_cs_index cs_index);

/**

  Client can call the API to set different power profiles, this API alone can be used instead of 
  using spi_power_on() and spi_power_off(). This API also facilitates clients to set a performance level
  like turbo, where client can expect better KPIs as qup resources are voted for higer performance.
  
  Flow of usage can be as below
  spi_open(.., &spi_handle)
  spi_set_power_level(spi_handle, QUP_POWER_ON) -> set to low svs
  spi_write(...)
  spi_set_power_level(spi_handle, QUP_POWER_TURBO) -> set for turbo
  spi_write(...)
  spi_set_power_level(spi_handle, QUP_POWER_OFF) -> set to off
  spi_close(...)

  @param[in]  spi_handle           Handle to the SPI instance.
  @param[in]  profile              Indicates which performance profile to set (off/low/nominal/turbo),
                                   refer qup_common.h for details.
  @return
  SPI_SUCCESS                           -- Function was successful. \n
  SPI_ERROR_INVALID_PARAMETER           -- Invalid parameter. \n
  SPI_ERROR_INVALID_EXECUTION_LEVEL     -- Invalid execution level. \n
  SPI_ERROR_PLAT_CLK_ENABLE_FAIL        -- Platform clock enable failure. \n
  SPI_ERROR_PLAT_GPIO_ENABLE_FAIL       -- Platform GPIO enable failure. \n
*/
spi_status_t 
spi_set_power_level
(
    void *spi_handle,
    qup_power_profile profile
);

/**

  Client can call the API to get the current power profile choosen, then take next appropriate action,
  like updating to power level higer or lower based on the use case need.

  @param[in]  spi_handle           Handle to the SPI instance.

  @return
  0 -- Getting power level failed. \n
  N -- N is the power level set on the SPI instance.

*/
qup_power_profile
spi_get_power_level
(
    void *spi_handle
);


/** @} */ /* end_namegroup */
/** @} */ /* end_addtogroup spi_api */

#endif  /* Double inclusion protection*/