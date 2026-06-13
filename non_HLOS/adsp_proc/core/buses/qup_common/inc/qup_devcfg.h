/**
    @file  qup_devcfg.h
    @brief interface to device configuration
 */

/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
09/23/25   MG      Added ibi irq type support in ibi_dev_cfg
04/15/25   SS      Added changes to handle bus contention
04/07/25   GKR     Changed EXCLUSIVE_SE to SHARED_SE
02/03/25   GKR     Added changes to support I2C HS mode
12/10/24   JAY     Added New param is_pipeline_enable in se_dev_cfg struct
10/07/24   GKR     Added New param log_level in se_dev_cfg struct
07/29/24   GKR     Added NEW SE Flag to control vote against Sleep & added npa_handle in se_cfg
06/26/24   GKR     DT Cleanup, tcsr dt migration & Multiple SSC QUP support
*/
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __QUP_DEVCONFIG_H__
#define __QUP_DEVCONFIG_H__

/* *************************************************************** */
/*                          INCLUDE FILES                          */
/* *************************************************************** */

#include "comdef.h"
#include "qup_common_internal.h"
#include "gpi.h"
#include "ClockDefs.h"
#include "DTBDefs.h"
#include "GPIO.h"

#define MAX_GPI_IDX 3

/*Supports upto 6 QUP IO Pads*/
#define MAX_IO_PADS               6

/* *************************************************************** */
/*                         DATA STRUCTURES                         */
/* *************************************************************** */

typedef struct qup_dev_cfg qup_dev_cfg;
typedef struct se_dev_cfg  se_dev_cfg;
typedef struct ibi_dev_cfg  ibi_dev_cfg;


/*================================================================================================
                                   FEATURE SPECIFIC DEFINITIONS
==================================================================================================*/

/*Flags To hold QUP Specific Driver Features*/
typedef enum QUP_FEATURES
{
    QUP_FLAGS_UNUSED           = 0,
    VOTE_FOR_CORE2X_CLOCK      = (1 << 1),
    VOTE_FOR_SRC_CLOCK         = (1 << 2),

    /*Work arounds for the QUP*/
    IBI_VOTE_FOR_CLOCK         = (1 << 3),
    IBI_AUTO_PROCESS_WA_ENABLE = (1 << 4),
    IBI_RSC_HSHAKE_ECO_ENABLE  = (1 << 5),
}QUP_FEATURES;

/*Flags To hold Serial Engine Specific Driver Features*/
typedef enum SE_FEATURES
{
    SE_FLAGS_UNUSED                            = 0,
    SHARED_SE                                  = (1 << 1),
    OPTIMISE_TRANSFERS                         = (1 << 2),
    POLLED_MODE                                = (1 << 3),
    USES_DDR_BUFFER                            = (1 << 4),
    ENABLE_TIMEOUT                             = (1 << 5),
    ENABLE_REGDUMP                             = (1 << 6),
    ENABLE_FATAL                               = (1 << 7),
    ENABLE_FATAL_ON_TIMEOUT                    = ENABLE_FATAL | ENABLE_TIMEOUT,
    ENABLE_RECOVER_ON_TIMEOUT                  = ENABLE_TIMEOUT,
    ENABLE_FATAL_ON_TIMEOUT_WITH_REGDUMP       = ENABLE_FATAL_ON_TIMEOUT   | ENABLE_REGDUMP,
    ENABLE_RECOVER_ON_TIMEOUT_WITH_REGDUMP     = ENABLE_RECOVER_ON_TIMEOUT | ENABLE_REGDUMP,
    ENABLE_FIFO_MODE                           = (1 << 8),

    ENABLE_VOTE_AGAINST_ALL_SLEEP              = (1 << 9),

    /** Internal used flags **/
    USES_INTERNAL_DDR_MEM                      = (1 << 24),
    STRETCH_CLK_POST_IBI                       = (1 << 25),
    ENABLE_IBI_MANAGER                         = (1 << 26),
}SE_FEATURES;

/*Flags To hold QUP Specific Driver Features*/
typedef enum IBI_STATES
{
    IBI_CRTL_INITIALIZED              = (1 << 0),
    IBI_CTRL_MGR_INITIALIZED          = (1 << 1),
    IBI_CTRL_USER_INITIALIZED         = (1 << 2),
    IBI_CTRL_SW_RST_INITIATED         = (1 << 3),
    IBI_CTRL_COMMON_CLOCKS_ENABLED    = (1 << 4),
}IBI_STATES;

/*Bit Map to hold what protocols are supported by the SE*/
typedef enum PROTOCOLS_SUPPORTED
{
    SPI_SUPPORTED              = (1 << 1),
    UART_SUPPORTED             = (1 << 2),
    I2C_SUPPORTED              = (1 << 3),
    I3C_SUPPORTED              = (1 << 4),
    I3C_DATA_ONLY_SUPPORTED    = (1 << 5),
    LEGACY_IBI_SUPPORTED       = (1 << 6),
    IBI_CONTROLLER_SUPPORTED   = (1 << 7),
}PROTOCOLS_SUPPORTED;

typedef enum _qup_common_clock_idx_
{
    QUP_CORE_2X_CLOCK_IDX      = 0,
    QUP_CORE_CLOCK_IDX            ,
    QUP_S_AHB_CLOCK_IDX           ,
    QUP_M_AHB_CLOCK_IDX           ,
    QUP_COMMON_CLOCKS_MAX         ,
}qup_common_clock_idx;

/*Sequenced clock ON states*/
typedef enum _qup_resource_state_
{
    QUP_RESOURCE_UNUSED         =  0,
    QUP_SE_CLOCK_FREQ_SET       ,
    QUP_NO_CLOCK_ON             ,
    QUP_CORE_2X_CLOCK_ON        ,
    QUP_CORE_CLOCK_ON           ,
    QUP_S_AHB_CLOCK_ON          ,
    QUP_M_AHB_CLOCK_ON          ,
    QUP_SE_CLOCK_ON             ,
    QUP_ALL_CLOCKS_ON           = QUP_SE_CLOCK_ON,
    QUP_CLOCK_ON_FAILURE        = (1 << 7),
}qup_resource_state;

typedef enum _i3c_od_type_
{
    I3C_OD_FREQ_100 = 0,
    I3C_OD_FREQ_400,
    I3C_OD_FREQ_1000,
    I3C_OD_FREQ_3400,
    I3C_OD_FREQ_MAX,
}i3c_od_type;

/*==================================================================================================
                                    GPIO SPECIFIC DEFINITIONS
==================================================================================================*/

/* Structure to hold Pin GPIO configurations
 * Bit field inline with TLMM Macro's Revisit if that changes
 */
typedef struct
{
    uint16 pin_no;
    GPIOConfigType gpio_cfg;
    GPIOConfigType gpio_sleep_cfg;
    boolean valid;
}io_cfg;

/*Structre to hold clock ON staus in power_on*/
typedef struct _qup_resource_votes_
{
    uint32               island_setup_votes : 16;
    qup_resource_state   state              :  8;
    qup_resource_state   failure_state      :  8;
}qup_resource_votes;


/*==================================================================================================
                                  IFACE SPECIFIC DEFINITIONS
==================================================================================================*/

/*Avaiable interfaces*/
typedef enum hw_interface
{
    CORE_IRQ                   = (1 << 1),
    GSI_MODE                   = (1 << 2),
    IBI_PDC_SUPPORT            = (1 << 3),
    UART_RX_PDC_SUPPORT        = (1 << 4)
}hw_interface;

typedef struct gpi_interface
{
    uint8  gpi_idx[MAX_GPI_IDX];
    uint8  num_gpi_idx;
    uint8  ring_size_multiplier;
}gpi_interface;

/*Interface related Data*/
typedef struct se_interface
{
    uint8  interface_supported;
    uint16 core_irq;
    uint32 pdc_irq;
    uint32 wakeup_gpio;    /* This is the parent wakeup capable GPIO num for the respective SSC GPIO Pin */
}se_interface;

/*==================================================================================================
                                    PROTOCOL SPECIFIC DEFINITIONS
==================================================================================================*/

/* counters required to be filed in config0 TRE */
typedef struct i2c_clock_config
{
    uint32  se_clock_frequency_khz;
    uint32  bus_speed_khz;
    uint32  od_freq;
    uint8   clk_div;
    uint8   i2c_t_cycle;
    uint8   i2c_t_high;
    uint8   i2c_t_low;
    uint8   i2c_scale;
    uint8   i3c_t_high;
    uint8   i3c_t_cycle;
} i2c_clock_config;

typedef struct i3c_ibi_clk_config
{
    uint32  od_freq;
    uint8   OD_type;
    uint16  scl_high;
    uint16  scl_low;
    uint16  sda_delay;
} i3c_ibi_clk_config;


/*==================================================================================================
                                 TCSR CONFIGURATION DEFINITIONS
==================================================================================================*/

typedef enum TCSR_IRQ_TYPE
{
    QUP_SE_IRQ    = 1,
    QUP_GSI_IRQ,
}TCSR_IRQ_TYPE;

typedef struct qup_tcsr_reg_info
{
    uint32    addr;
    uint32    se_bit_offset;
} qup_tcsr_reg_info;

typedef struct qup_tcsr_config
{
    uint32                irq_num;
    qup_tcsr_reg_info     reg_info[QUP_MAX];
    uint32                used;
} qup_tcsr_config;

#define MAX_QUP_TCSR_CFGS    3

/*==================================================================================================
                                    SE SPECIFIC DEFINITIONS
==================================================================================================*/

typedef struct se_clk_dfs_config
{
    uint8 num_entries;
    uint8 freq_ref_count[MAX_FREQ_PLAN_TABLE_SIZE];  // This array stores num of times each freq is requested by clients
    ClockFreqPlanType    freq_list_local[MAX_FREQ_PLAN_TABLE_SIZE];
}se_clk_dfs_config;

typedef struct se_dev_cfg
{
    /* Pointers*/
    qup_dev_cfg    *qup_data;
    ibi_dev_cfg    *ibi_data;
    se_clk_dfs_config *freq_plan;
    boolean         force_default_reg_write;
    uint8          *se_base_addr;

    /*SE Clock*/
    uint8         **se_clock;
    ClockIdType     se_clk_id;
    uint32          current_se_freq;

    /*Protocol Specific Config Clock*/
    void           *protocol_clock_cfg;

    /*Platform Function table*/
    void           *qup_plat_fcn_table;

    /* Logging Handle */
    void           *log_handle;
    uint8           log_level;

    uint8           se_id;

    /*Protocol specific*/
    uint8           protocols_supported;
    SE_PROTOCOL     protocol_in_use;
    uint32          se_src_frequency;
    uint32          od_freq;

    uint16          island_spec_in_use;

    uint32          gdsc_ref_count;
    uint32          vote_against_sleep_count;

    /*GSI Data*/
    se_interface    iface_data;
    gpi_interface   gpi_data;

    /*GPIO DATA*/
    io_cfg          io[MAX_IO_PADS];

    /*For SE specific features*/
    SE_FEATURES     flags;
    boolean         is_pipeline_enable;

    /*For SE specific  resources*/
    qup_resource_votes    resources;

    /*store se dtsi node handle*/
    fdt_node_handle       se_node;

    /* store se specific gpio key*/
    GPIOKeyType       se_gpio_enable_key;
    GPIOKeyType       se_gpio_disable_key;

    /* sleep specifc npa handles */
    void*  npa_handle;

}se_dev_cfg ;

/*==================================================================================================
                                 IBI CONTROLLER SPECIFIC DEFINITIONS
==================================================================================================*/

/*IBI controller Config structure*/
typedef struct ibi_dev_cfg
{
    uint8          ibi_id;
    uint8          gpii;
    uint32         gpii_irq;
    uint32         mgr_irq;

    /* HW Base Address */
    uint8         *core_base_addr;

    /*For Qup specific features*/
    IBI_STATES     flags;
    uint8          ibi_bus_error_count;

    boolean        ibi_bus_contention_status;
    uint8          ibi_irq_type;
}ibi_dev_cfg;

/*==================================================================================================
                                 QUP SPECIFIC DEFINITIONS
==================================================================================================*/

/*QUP Config structure*/
typedef struct qup_dev_cfg
{
    QUP_TYPE       qup_id;

    /* HW Base Address */
    uint8         *core_base_addr;
    uint8         *se_wrapper_base_addr;
    uint8         *common_base_addr;

    /*Common Clocks*/
    ClockIdType   common_clk_id[QUP_COMMON_CLOCKS_MAX];
    uint32        core_freq_hz;

    /*For Qup specific features*/
    QUP_FEATURES  flags;

    uint8         num_se;

    fdt_node_handle qup_node;
}qup_dev_cfg;

/* *************************************************************** */
/*                            FUNCTIONS                            */
/* *************************************************************** */

/* Get HW CFG for QUP COMMON */
qup_dev_cfg* qup_get_common_dev_cfg (QUP_TYPE qup_type);

/* Get HW devcfg based on QUP type and SE id*/
se_dev_cfg*  qup_get_hw_cfg(QUP_TYPE qup_type, uint32 se_idx);


ibi_dev_cfg *qup_get_ibi_cfg(QUP_TYPE qup_id, uint8 ibi_id);
/* Configure TCSR Register
   Returns corresponding interrupt number if successful
   else returns negative value.Should be called holding
   qup_global_lock from upper layer*/
int          qup_config_tcsr(QUP_TYPE qup, TCSR_IRQ_TYPE type, uint8 idx);

/* Configure Extra IO other then protocol Minimum requirement
 * Returns true if successfully configured
 */
boolean      config_qup_io(se_dev_cfg  *cfg, uint8 io_idx);

/*For Legacy compatibility*/
boolean      qup_config_get_info(uint32 instance, QUP_TYPE *qup_type, uint32 *se_idx);


/*To get QUP Type and SE Index from handle*/
boolean      qup_get_qup_se_instance(void *handle, QUP_TYPE *qup_type, uint32 *se_idx);


/*To be called from RCINIT*/
void         qup_config_init(void);

#endif
