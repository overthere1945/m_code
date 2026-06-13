/**
    @file  qup_error.c
    @brief Error implementation
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/


/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/
#include "qup_error.h"

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/


/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/

/*==================================================================================================
                                              VARIABLES
==================================================================================================*/
const qup_error_map e_map[QUP_ERROR_MAX] =
{
    [QUP_ERROR_INVALID_PARAMETER]           = { I2C_ERROR_INVALID_PARAMETER            , SPI_ERROR_INVALID_PARAM              },
    [QUP_ERROR_UNSUPPORTED_CORE_INSTANCE]   = { I2C_ERROR_UNSUPPORTED_CORE_INSTANCE    , SPI_ERROR_HW_INFO_ALLOCATION         },
    [QUP_ERROR_NO_MEM]                      = { I2C_ERROR_NO_MEM                       , SPI_ERROR_MEM_ALLOC                  },
    [QUP_ERROR_API_INVALID_EXECUTION_LEVEL] = { I2C_ERROR_API_INVALID_EXECUTION_LEVEL  , SPI_ERROR_INVALID_EXECUTION_LEVEL    },
    [QUP_ERROR_API_NOT_SUPPORTED]           = { I2C_ERROR_API_NOT_SUPPORTED            , SPI_ERROR                            },
    [QUP_ERROR_API_ASYNC_MODE_NOT_SUPPORTED]= { I2C_ERROR_API_ASYNC_MODE_NOT_SUPPORTED , SPI_ERROR                            },
    [QUP_ERROR_API_PROTOCOL_NOT_SUPPORTED]  = { I2C_ERROR_API_PROTOCOL_NOT_SUPPORTED   , SPI_ERROR                            },
    [QUP_ERROR_HANDLE_ALLOCATION]           = { I2C_ERROR_HANDLE_ALLOCATION            , SPI_ERROR_HANDLE_ALLOCATION          },
    [QUP_ERROR_HW_INFO_ALLOCATION]          = { I2C_ERROR_HW_INFO_ALLOCATION           , SPI_ERROR_HW_INFO_ALLOCATION         },
    [QUP_ERROR_IO_CTXT_ALLOCATION]          = { I2C_ERROR_IO_CTXT_ALLOCATION           , SPI_ERROR                            },
    [QUP_ERROR_INPUT_FIFO_OVER_RUN]         = { I2C_ERROR_INPUT_FIFO_OVER_RUN          , SPI_ERROR                            },
    [QUP_ERROR_OUTPUT_FIFO_UNDER_RUN]       = { I2C_ERROR_OUTPUT_FIFO_UNDER_RUN        , SPI_ERROR                            },
    [QUP_ERROR_INPUT_FIFO_UNDER_RUN]        = { I2C_ERROR_INPUT_FIFO_UNDER_RUN         , SPI_ERROR                            },
    [QUP_ERROR_OUTPUT_FIFO_OVER_RUN]        = { I2C_ERROR_OUTPUT_FIFO_OVER_RUN         , SPI_ERROR                            },
    [QUP_ERROR_COMMAND_OVER_RUN]            = { I2C_ERROR_COMMAND_OVER_RUN             , SPI_ERROR                            },
    [QUP_ERROR_COMMAND_ILLEGAL]             = { I2C_ERROR_COMMAND_ILLEGAL              , SPI_ERROR                            },
    [QUP_ERROR_COMMAND_FAIL]                = { I2C_ERROR_COMMAND_FAIL                 , SPI_ERROR                            },
    [QUP_ERROR_COMMAND_INVALID_OPCODE]      = { I2C_ERROR_COMMAND_INVALID_OPCODE       , SPI_ERROR                            },
    [QUP_ERROR_PLATFORM_INIT_FAIL]          = { I2C_ERROR_PLATFORM_INIT_FAIL           , SPI_ERROR                            },
    [QUP_ERROR_PLATFORM_DEINIT_FAIL]        = { I2C_ERROR_PLATFORM_DEINIT_FAIL         , SPI_ERROR                            },
    [QUP_ERROR_PLATFORM_CRIT_SEC_FAIL]      = { I2C_ERROR_PLATFORM_CRIT_SEC_FAIL       , SPI_ERROR_MUTEX                      },
    [QUP_ERROR_PLATFORM_SIGNAL_FAIL]        = { I2C_ERROR_PLATFORM_SIGNAL_FAIL         , SPI_ERROR                            },
    [QUP_ERROR_PLATFORM_TIMER_INIT_FAIL]    = { /*I2C_ERROR_PLATFORM_TIMER_INIT_FAIL*/I2C_ERROR_NO_MEM     , SPI_ERROR        },
    [QUP_ERROR_PLATFORM_TIMER_SET_FAIL]     = { /*I2C_ERROR_PLATFORM_TIMER_SET_FAIL*/ I2C_ERROR_NO_MEM     , SPI_ERROR        },
    [QUP_ERROR_PLATFORM_TIMER_CLR_FAIL]     = { /*I2C_ERROR_PLATFORM_TIMER_CLR_FAIL*/I2C_ERROR_NO_MEM      , SPI_ERROR        },
    [QUP_ERROR_PLATFORM_TIMER_DEINIT_FAIL]  = { /*I2C_ERROR_PLATFORM_TIMER_DEINIT_FAIL*/ I2C_ERROR_NO_MEM  , SPI_ERROR        },
    [QUP_ERROR_PLATFORM_GET_CONFIG_FAIL]    = { I2C_ERROR_PLATFORM_GET_CONFIG_FAIL     , SPI_ERROR_PLAT_GET_CONFIG_FAIL       },
    [QUP_ERROR_PLATFORM_REG_INT_FAIL]       = { I2C_ERROR_PLATFORM_REG_INT_FAIL        , SPI_ERROR_PLAT_REG_INT_FAIL          },
    [QUP_ERROR_PLATFORM_DE_REG_INT_FAIL]    = { I2C_ERROR_PLATFORM_DE_REG_INT_FAIL     , SPI_ERROR                            },
    [QUP_ERROR_PLATFORM_CLOCK_ENABLE_FAIL]  = { I2C_ERROR_PLATFORM_CLOCK_ENABLE_FAIL   , SPI_ERROR_PLAT_CLK_ENABLE_FAIL       },
    [QUP_ERROR_PLATFORM_GPIO_ENABLE_FAIL]   = { I2C_ERROR_PLATFORM_GPIO_ENABLE_FAIL    , SPI_ERROR_PLAT_GPIO_ENABLE_FAIL      },
    [QUP_ERROR_PLATFORM_CLOCK_DISABLE_FAIL] = { I2C_ERROR_PLATFORM_CLOCK_DISABLE_FAIL  , SPI_ERROR_PLAT_CLK_DISABLE_FAIL      },
    [QUP_ERROR_PLATFORM_GPIO_DISABLE_FAIL]  = { I2C_ERROR_PLATFORM_GPIO_DISABLE_FAIL   , SPI_ERROR_PLAT_GPIO_DISABLE_FAIL     },
    [QUP_ERROR_PLATFORM_RESOURCE_SETUP_FAIL]= { I2C_ERROR_PLATFORM_RESOURCE_SETUP_FAIL , SPI_ERROR_PLAT_SET_RESOURCE_FAIL     },
    [QUP_ERROR_PLATFORM_RESOURCE_RESET_FAIL]= { I2C_ERROR_PLATFORM_RESOURCE_RESET_FAIL , SPI_ERROR_PLAT_RESET_RESOURCE_FAIL   },
    [QUP_TRANSFER_COMPLETED]                = { I2C_TRANSFER_COMPLETED                 , SPI_ERROR                            },
    [QUP_ERROR_HANDLE_ALREADY_IN_QUEUE]     = { I2C_ERROR_HANDLE_ALREADY_IN_QUEUE      , SPI_ERROR_PENDING_TRANSFER           },
    [QUP_ERROR_DMA_REG_FAIL]                = { I2C_ERROR_DMA_REG_FAIL                 , SPI_ERROR_DMA_REG_FAIL               },
    [QUP_ERROR_DMA_DE_REG_FAIL]             = { I2C_ERROR_DMA_DE_REG_FAIL              , SPI_ERROR_DMA_DEREG_FAIL             },
    [QUP_ERROR_DMA_TX_CHAN_ADDRESS_MAP_FAIL]= { I2C_ERROR_DMA_TX_CHAN_ADDRESS_MAP_FAIL , SPI_ERROR                            },
    [QUP_ERROR_DMA_RX_CHAN_ADDRESS_MAP_FAIL]= { I2C_ERROR_DMA_RX_CHAN_ADDRESS_MAP_FAIL , SPI_ERROR                            },
    [QUP_ERROR_DMA_EV_CHAN_ADDRESS_MAP_FAIL]= { I2C_ERROR_DMA_EV_CHAN_ADDRESS_MAP_FAIL , SPI_ERROR                            },
    [QUP_ERROR_DMA_EV_CHAN_ALLOC_FAIL]      = { I2C_ERROR_DMA_EV_CHAN_ALLOC_FAIL       , SPI_ERROR_DMA_EV_CHAN_ALLOC_FAIL     },
    [QUP_ERROR_DMA_TX_CHAN_ALLOC_FAIL]      = { I2C_ERROR_DMA_TX_CHAN_ALLOC_FAIL       , SPI_ERROR_DMA_TX_CHAN_ALLOC_FAIL     },
    [QUP_ERROR_DMA_RX_CHAN_ALLOC_FAIL]      = { I2C_ERROR_DMA_RX_CHAN_ALLOC_FAIL       , SPI_ERROR_DMA_RX_CHAN_ALLOC_FAIL     },
    [QUP_ERROR_DMA_TX_CHAN_START_FAIL]      = { I2C_ERROR_DMA_TX_CHAN_START_FAIL       , SPI_ERROR_DMA_TX_CHAN_START_FAIL     },
    [QUP_ERROR_DMA_RX_CHAN_START_FAIL]      = { I2C_ERROR_DMA_RX_CHAN_START_FAIL       , SPI_ERROR_DMA_RX_CHAN_START_FAIL     },
    [QUP_ERROR_DMA_TX_CHAN_STOP_FAIL]       = { I2C_ERROR_DMA_TX_CHAN_STOP_FAIL        , SPI_ERROR_DMA_TX_CHAN_STOP_FAIL      },
    [QUP_ERROR_DMA_RX_CHAN_STOP_FAIL]       = { I2C_ERROR_DMA_RX_CHAN_STOP_FAIL        , SPI_ERROR_DMA_RX_CHAN_STOP_FAIL      },
    [QUP_ERROR_DMA_TX_CHAN_RESET_FAIL]      = { I2C_ERROR_DMA_TX_CHAN_RESET_FAIL       , SPI_ERROR_DMA_TX_CHAN_RESET_FAIL     },
    [QUP_ERROR_DMA_RX_CHAN_RESET_FAIL]      = { I2C_ERROR_DMA_RX_CHAN_RESET_FAIL       , SPI_ERROR_DMA_RX_CHAN_RESET_FAIL     },
    [QUP_ERROR_DMA_EV_CHAN_DE_ALLOC_FAIL]   = { I2C_ERROR_DMA_EV_CHAN_DE_ALLOC_FAIL    , SPI_ERROR_DMA_EV_CHAN_DEALLOC_FAIL   },
    [QUP_ERROR_DMA_TX_CHAN_DE_ALLOC_FAIL]   = { I2C_ERROR_DMA_TX_CHAN_DE_ALLOC_FAIL    , SPI_ERROR_DMA_TX_CHAN_DEALLOC_FAIL   },
    [QUP_ERROR_DMA_RX_CHAN_DE_ALLOC_FAIL]   = { I2C_ERROR_DMA_RX_CHAN_DE_ALLOC_FAIL    , SPI_ERROR_DMA_RX_CHAN_DEALLOC_FAIL   },
    [QUP_ERROR_DMA_INSUFFICIENT_RESOURCES]  = { I2C_ERROR_DMA_INSUFFICIENT_RESOURCES   , SPI_ERROR_DMA_INSUFFICIENT_RESOURCES },
    [QUP_ERROR_DMA_CONFIGURATION_FAIL]      = { I2C_ERROR_DMA_CONFIGURATION_FAIL       , SPI_ERROR                            },
    [QUP_ERROR_DMA_PROCESS_TRANSFER_FAIL]   = { I2C_ERROR_DMA_PROCESS_TRANSFER_FAIL    , SPI_ERROR_DMA_PROCESS_TRE_FAIL       },
    [QUP_ERROR_DMA_EVT_OTHER]               = { I2C_ERROR_DMA_EVT_OTHER                , SPI_ERROR_DMA_EVT_OTHER              },
    [QUP_ERROR_I3C_DEVICE_ALLOCATION_FAILED]= { I2C_ERROR_I3C_DEVICE_ALLOCATION_FAILED , SPI_ERROR                            },
};

/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/
