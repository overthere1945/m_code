#ifndef SLIMBUS_ISLAND__H
#define SLIMBUS_ISLAND__H
/*=========================================================================*/
/**
   @file  SlimbusIsland.h

   This module provides the interface to the SLIMbus driver software.
*/
/*=========================================================================

                             Edit History

when       who     what, where, why
--------   ---     --------------------------------------------------------
04/14/21   RK      Initial revision.

===========================================================================
             Copyright (c) 2021 QUALCOMM Technologies Incorporated.
                    All Rights Reserved.
                  QUALCOMM Proprietary/GTDR
===========================================================================
*/

#include <stdlib.h>

/** This enum indicates the event associated with a client event trigger */
typedef enum
{
   SLIMBUS_BAM_EVENT_INVALID = 0,

   SLIMBUS_BAM_EVENT_EOT,          /**< End-of-transfer indication from hardware */
   SLIMBUS_BAM_EVENT_DESC_DONE,    /**< Descriptor processed indication from hardware */
   SLIMBUS_BAM_EVENT_OUT_OF_DESC,  /**< Out of descriptors indication from hardware */
   SLIMBUS_BAM_EVENT_WAKEUP,       /**< Peripheral wake up indication from hardware */
   SLIMBUS_BAM_EVENT_FLOWOFF,      /**< Graceful halt (idle) indication from hardware */
   SLIMBUS_BAM_EVENT_INACTIVE,     /**< Inactivity timeout indication from hardware */
   SLIMBUS_BAM_EVENT_ERROR,        /**< Error indication from hardware */

   SLIMBUS_BAM_EVENT_MAX,

  _PLACEHOLDER_SlimBusBamEventType = 0x7fffffff
} SlimBusBamEventType;

/** Enumeration of SLIMbus data transfer directions (receive
    or transmit).  Default direction can be used if data
    channel is unidirectional. */
typedef enum
{
  SLIMBUS_BAM_TRANSMIT,
  SLIMBUS_BAM_RECEIVE,
  SLIMBUS_BAM_DEFAULT,
  SLIMBUS_BAM_NUM_ENUMS,
  _PLACEHOLDER_SlimBusBamTransferType = 0x7fffffff
} SlimBusBamTransferType;

/** This data type corresponds to the native I/O vector (BAM
    descriptor) supported by BAM hardware */
typedef struct
{
  uint32 uAddr;        /**< Buffer physical address */
  uint32 uSize : 16;   /**< Buffer size in bytes */
  uint32 uFlags : 16;  /**< Flag bitmask (see SLIMBUS_BAM_IOVEC_FLAG_ #defines) */
} SlimBusBamIOVecType;

/** SLIMbus resource handle */
typedef uint32 SlimBusResourceHandle;

/** Enumeration of SLIMbus master port asynchronous events */
typedef enum
{
  SLIMBUS_EVENT_O_NONE = 0,
  SLIMBUS_EVENT_O_FIFO_OVERRUN = 0x2,
  SLIMBUS_EVENT_O_FIFO_RECEIVE_OVERRUN = SLIMBUS_EVENT_O_FIFO_OVERRUN,
  SLIMBUS_EVENT_O_FIFO_TRANSMIT_OVERRUN = SLIMBUS_EVENT_O_FIFO_OVERRUN,
  SLIMBUS_EVENT_O_FIFO_UNDERRUN = 0x4,
  SLIMBUS_EVENT_O_FIFO_TRANSMIT_UNDERRUN = SLIMBUS_EVENT_O_FIFO_UNDERRUN,
  SLIMBUS_EVENT_O_FIFO_RECEIVE_UNDERRUN = SLIMBUS_EVENT_O_FIFO_UNDERRUN,
  SLIMBUS_EVENT_O_PORT_DISCONNECT = 0x10,
  _PLACEHOLDER_SlimBusPortEventType = 0x7fffffff
} SlimBusPortEventType;

/** The default set of SLIMbus master port asynchronous
    events */
#define SLIMBUS_EVENT_O_DEFAULTS \
  ((SlimBusPortEventType)(SLIMBUS_EVENT_O_FIFO_RECEIVE_OVERRUN| \
                          SLIMBUS_EVENT_O_FIFO_TRANSMIT_UNDERRUN| \
                          SLIMBUS_EVENT_O_PORT_DISCONNECT))

/** SLIMbus port event notification structure.  The client
    should zero out the events in the event bitmap for all
    events handled by the client event handler.  Any events
    not handled by the client handler (and zeroed out in the
    bitmap) will be available for retrieval by the
    DalSlimBus_GetPortEvent() function */
typedef struct
{
  void *pUser;  /**< User pointer set during DalSlimBus_RegisterPortEvent() */
  SlimBusPortEventType eEvent;  /**< Bitmap of SLIMbus master port events pending */
  SlimBusResourceHandle hPort;  /**< Handle to SLIMbus master port generating the event */
} SlimBusEventNotifyType;


/**
  @brief Submit a  BAM IO-vector with user data

  This function fetches the next processed BAM IO-vector.  The 
  IO-vector structure will be zeroed if there are no more 
  processed IO-vectors.  The user data pointer is optional and 
  if supplied will be filled with the user data pointer passed 
  when the descriptor was submitted. 

  @param[in] h  Client handle to the SLIMbus driver 
  @param[in] hPort  Master port handle corresponding to the BAM 
        pipe
  @param[in] eTransferDir  Direction of data flow for the BAM
        pipe to get the IO-vector for.  For bi-directional
        ports, there is one BAM pipe for each of the transmit
        and receive directions.
  @param[out] pIOVec  Pointer to the location to send IO-Vector.
  @param[out] ppUser  Pointer to location to store the user data 
        pointer associated with a processed IO-vector.  The
        pointer will be NULL if there are no more processed
        IO-vectors.
   
  @return  SB_SUCCESS on success, an error code on error
  */
int 
SlimBus_SubmitBamTransfer_Island(uint32 Slimbus_ClientId, SlimBusResourceHandle  hPort, SlimBusBamTransferType  eTransferDir, SlimBusBamIOVecType * pIOVec, void * pUser); 

/**
  @brief Get a processed BAM IO-vector with user data

  This function fetches the next processed BAM IO-vector.  The 
  IO-vector structure will be zeroed if there are no more 
  ;processed IO-vectors.  The user data pointer is optional and 
  if supplied will be filled with the user data pointer passed 
  when the descriptor was submitted. 

  @param[in] h  Client handle to the SLIMbus driver 
  @param[in] hPort  Master port handle corresponding to the BAM 
        pipe
  @param[in] eTransferDir  Direction of data flow for the BAM
        pipe to get the IO-vector for.  For bi-directional
        ports, there is one BAM pipe for each of the transmit
        and receive directions.
  @param[out] pIOVec  Pointer to the location to store a 
        processed IO-vector.  The structure will be zeroed if
        there are no more processed IO-vectors.
  @param[out] ppUser  Pointer to location to store the user data 
        pointer associated with a processed IO-vector.  The
        pointer will be NULL if there are no more processed
        IO-vectors.
   
  @return  SB_SUCCESS on success, an error code on error
  */
int
SlimBus_GetBamIOVecEx_island(uint32 Slimbus_ClientId, SlimBusResourceHandle  hPort, SlimBusBamTransferType  eTransferDir, SlimBusBamIOVecType * pIOVec, void **ppUser);

/**
  @brief Read information on recent port events
   
  This function reads information on recent asynchronous port 
  events.
   
  @param[in] h  Client handle to the SLIMbus driver 
  @param[in] hPort  Master port handle; this parameter can be 
        set to NULL to get the pending events for any port owned
        by the client
  @param[out] pNotify  Pointer to event notification structure 

  @return  SB_SUCCESS on success, an error code on error
  */
int 
SlimBus_GetPortEvent_Island(uint32 Slimbus_ClientId, SlimBusResourceHandle  hPort, SlimBusEventNotifyType * pNotify); 

/**
  @brief Read the value of a port shadow progress counter and 
         timestamp.
 
  This function reads the value of a SLIMbus master port shadow 
  progress counter and timestamp.
  
  @param[in] _h  Client handle to the SLIMbus driver
   
  @param[in] hCounter  Progress Counter handle
   
  @param[out] puNumDMA  Pointer to store the number of 32-bit 
        words DMA-ed to or from the associated port at the last
        DMA interrupt
   
  @param[out] puFifoSamples  Number of samples that were in the 
        port FIFO plus samples in the phy/FL stages, taking
        into account segment length and bit packing, at the
        last DMA interrupt
   
  @param[out] puTimestamp  Pointer to store the timestamp value
        for the last DMA interrupt
   
  @param[out] pbSampleMissed  Pointer to store indication of 
        whether and DMA timestamp values have been missed
  
  @return SB_SUCCESS if the timestamp sample and counter values
          are valid, error code otherwise
   
  @see SlimBus_ReadProgressCounter() 
 */
int
SlimBus_ReadProgressCounterTimestamp_island(uint32_t Slimbus_ClientId, SlimBusResourceHandle hCounter, uint32_t *puNumDMA, uint32_t *puFifoSamples, uint64_t *puTimestamp, uint32_t *pbSampleMissed);

#endif 