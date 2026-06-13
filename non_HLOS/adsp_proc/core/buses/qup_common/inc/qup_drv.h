/**
    @file  qup_drv.h
    @brief Core Driver interface
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __QUP_DRV_H__
#define __QUP_DRV_H__

/* *************************************************************** */
/*                          INCLUDE FILES                          */
/* *************************************************************** */

#include "qup_devcfg.h"
#include "qup_log.h"
#include "qup_error.h"

/* *************************************************************** */
/*                     I2C DATA STRUCTURES                         */
/* *************************************************************** */

#define MAX_TRANSFER_TIMEOUT 2000
#define MAX_CANCEL_TIMEOUT   50

#define I2C_NOISE_REJECT_LEVEL_DEFAULT      1
#define I2C_PACKING_DEFAULT                 3

#define I2C_HS_TLOW_COUNT                   38
#define I2C_HS_TCYCLE_COUNT                 28

#define I2C_HS_MODE_HOLD_TIME               0
#define THIGH_POST_ACK_COUNT                0
#define I2C_NOISE_REJECT_ENABLE             0

#define I2C_STANDARD_MODE_SPEC_T_LOW_NS     4700    /* 4700 ns */
#define I2C_FAST_MODE_SPEC_T_LOW_NS         1300    /* 1300 ns */
#define I2C_FAST_MODE_PLUS_SPEC_T_LOW_NS    500     /* 500  ns */

#define I2C_STANDARD_MODE_SPEC_T_HIGH_NS    4000    /* 4000 ns */
#define I2C_FAST_MODE_SPEC_T_HIGH_NS        600     /* 600  ns */
#define I2C_FAST_MODE_PLUS_SPEC_T_HIGH_NS   260     /* 260  ns */

#define I2C_SLAVE_CONFIG_1_T_LOW_MASK               0x000000ff
#define I2C_SLAVE_CONFIG_1_T_LOW_SHFT               0

#define I2C_SLAVE_CONFIG_1_T_HIGH_MASK              0x0000ff00
#define I2C_SLAVE_CONFIG_1_T_HIGH_SHFT              8

#define I2C_SLAVE_CONFIG_1_NOISE_REJECT_LEVEL_MASK  0x00ff0000
#define I2C_SLAVE_CONFIG_1_NOISE_REJECT_LEVEL_SHFT  16

#define I2C_HS_I3C_SE_RUMI_SRC_FREQ                 19200
#define I3C_RUMI_OD_FREQ                            400

#define I2C_SLAVE_CONFIG_1_GET(x, CONFIG) ((x & I2C_SLAVE_CONFIG_1_##CONFIG##_MASK) >> (I2C_SLAVE_CONFIG_1_##CONFIG##_SHFT))
#define I2C_SLAVE_CONFIG_2_GET(x, CONFIG) ((x & I2C_SLAVE_CONFIG_2_##CONFIG##_MASK) >> (I2C_SLAVE_CONFIG_2_##CONFIG##_SHFT))

#define I2C_MAX_DESCS_PAIRS                       10       /* Max number of descriptors depends on GSI transfer rings length */
#define SPI_MAX_DESCS_PAIRS                       10       /* Max number of descriptors depends on GSI transfer rings length */
#define I2C_MAX_DESCS                             10
#define SPI_MAX_DESCS                             10
#define I2C_MAX_CONFIG                            10
#define SPI_MAX_CONFIG                            10

typedef struct transfer_ctxt    transfer_ctxt;

struct transfer_ctxt
{
    // client parameters
    i2c_slave_config    *config;
    i2c_descriptor      *desc;
    i2c_config_desc_pairs *config_desc_pairs;
    callback_func       callback;
    void                *ctxt;
    uint32              delay;
    uint16              num_descs;
    uint16              num_descs_pairs;

    // track the transfer

    i2c_status          transfer_status;

};

/* *************************************************************** */
/*                     SPI DATA STRUCTURES                         */
/* *************************************************************** */

#define SPI_RX_TX_PACKING_DISABLE 0  //See SPI Config 0
#define SPI_RX_TX_PACKING_ENABLE  3

typedef struct spi_xfer_ctxt        spi_xfer_ctxt;
// Transfer related data
struct spi_xfer_ctxt
{
    // client parameters
    spi_config_t       *config;
    spi_descriptor_t   *desc;
    spi_config_desc_pairs *config_desc_pairs;
    callback_fn         callback;
    void               *ctxt;
    uint32              num_descs;
    uint16              num_descs_pairs;

    // track the transfer
    spi_status_t        xfer_status;

    boolean             async;
    uint8               timestamp;
};

/* *************************************************************** */
/*                     QUP DATA STRUCTURES                         */
/* *************************************************************** */

#define TRANSFER_SIGNAL_MASK         0x00000001
#define CMD_SIGNAL_MASK              0x00000010
#define CMD_IBI_SIGNAL_MASK          0x00000100

/*QUP Flag Operations*/
#define QUP_IS_SET(flags,mask)      (flags & (mask))
#define QUP_IS_NOT_SET(flags,mask)  (!(flags & (mask)))
#define QUP_FLAG_SET(flags,mask)    flags |= (mask)
#define QUP_FLAG_UNSET(flags,mask)  flags &= (~(mask))

/* 64 Bit Timestamp manipulation Macro */
#define QUP_TS_64(msb,lsb) ((((uint64)msb << 32)) | (uint64)(lsb))

#define QUP_HW_CTXT_REF_CNT(h_ctxt) (h_ctxt->core_ref_count+h_ctxt->bmi_ref_count)

/* Flags to maintain core state */
typedef enum qup_core_state
{
    QUP_CORE_UNUSED                        = 0,
    QUP_CORE_IN_USE                        = (1 << 0),
    QUP_CORE_INTERFACE_INITIALIZED         = (1 << 1),
    QUP_CORE_CONFIG_TRE_ISSUED             = (1 << 2),
    QUP_CORE_IFACE_FUNCTIONS_INITIALIZED   = (1 << 3),
    QUP_CORE_HW_HAS_INTERNAL_QUEUE         = (1 << 4),
}qup_core_state;

/* enum for geni ios status */
typedef enum qup_geni_ios
{
    QUP_GENI_IO_LOW     = 0,  /* 00*/
    QUP_GENI_SCL_LOW    = 1,  /* 01 */
    QUP_GENI_SDA_LOW    = 2,  /* 10 */
    QUP_GENI_IO_HIGH    = 3,  /* 11 */
    QUP_GENI_IO_INVALID = 0xF,
}qup_geni_ios;

/* all about controller */
typedef struct qup_hw_ctxt          qup_hw_ctxt;

/* all about transfer */
typedef struct qup_transfer_ctxt    qup_transfer_ctxt;

/* all about client */
typedef struct qup_client_ctxt      qup_client_ctxt;

/* Flags to maintain transfer state per client*/
typedef enum _qup_transfer_state_
{
    TRANSFER_INVALID = 0,
    TRANSFER_IN_PROGRESS,
    TRANSFER_QUEUED,
    TRANSFER_OUTPUT_DONE,
    TRANSFER_INPUT_DONE,
    TRANSFER_DONE,
    TRANSFER_BUS_ERR,
    TRANSFER_CANCELED,
    TRANSFER_TO_BE_CANCELLED,
    TRANSFER_TIMED_OUT,
    TRANSFER_BUS_RESET,
}qup_transfer_state;

/* Union of Protocol specific bus configuration*/
typedef union bus_cfg
{
    i2c_slave_config  i2c_bus_cfg;
    spi_config_t      spi_bus_cfg;
}bus_cfg;


/* Structure for Protocol Specific functions */
typedef struct ProtocolIfaceFcnTable
{
   uint32      (*queue_transfer)              (qup_client_ctxt *ctxt);
   uint32      (*prepare_transfer)            (qup_client_ctxt *ctxt);
   uint32      (*submit_transfer)             (qup_client_ctxt *ctxt);
   qup_status  (*cancel_transfer)             (qup_hw_ctxt  *h_ctxt, qup_client_ctxt *ctxt, boolean force_reset);
   boolean     (*iface_enable)                (qup_hw_ctxt  *h_ctxt, boolean enable);
} proto_iface_func_table;


/* Core HW structure maintained in per SE basis */
struct qup_hw_ctxt
{
    // handle to device config
    void            *core_config;
    // handle to bus interface
    void            *core_iface;
    // for critical sections
    void            *core_lock;

    // for critical sections for queueing trransfers
    void            *queue_lock;

    // signal transfer completion
    void            *transfer_signal;

    // store head and tail of the client requests in queue for async transfers
    qup_client_ctxt *client_ctxt_head;

    // reference counts
    uint32           core_ref_count;
    uint32           power_ref_count;
    uint32           bmi_ref_count;

    //timer handle
    void            *timer_handle;

    //Auxillary controller
    void            *aux_controller;

    //In Band Wake controller
    void            *ibs_controller;

    //Protocol Specific Function Table
    proto_iface_func_table    iface_functions;

    // next controller
    qup_hw_ctxt     *next;

    //Transfer Optimisations
    bus_cfg          curr_bus_cfg;

    // core states
    qup_core_state   core_state;

};

/* Union of Protocol specific transfer configuration*/
typedef union proto_cfg
{
    transfer_ctxt i2c;
    spi_xfer_ctxt spi;
}proto_cfg;

/* Transfer Structure, Used to bookmark transfers on
 * a per client basis
 */
struct qup_transfer_ctxt
{
   // Protocol Specfic Transfer Configuration
    proto_cfg            cfg;

   // Track  transfer state
    qup_transfer_state   transfer_state;
    uint32               transferred;
    uint32               transfer_count;

    // sticky timestamp
    uint64               start_stop_bit_timestamp;

    // bus iface io context
    void                *io_ctxt;

   // Track Time taken by transfer APIs
#ifdef QUP_KPI_PROFILING
    uint64               xfer_start_timestamp;
    uint64               xfer_end_timestamp;
#endif

    void                *buffer_list[2];
    boolean             multi_transfer_config;
    boolean             is_bus_clear_issued;
};

/* Client Structure, Reference of the same
 * is returned as a part of qup_open
 */
struct qup_client_ctxt
{
    qup_hw_ctxt             *h_ctxt;
    qup_transfer_ctxt        t_ctxt;
    qup_client_ctxt         *next;
    qup_client_ctxt         *slave_handle_next;
    int                      pid;
    boolean                  clnt_active_in_island;
};

/* *************************************************************** */
/*                            FUNCTIONS                            */
/* *************************************************************** */
/**
 * @brief Initializes the QUP hardware context core lock.
 *
 * This function sets up the core lock context for SE.
 *
 * @param h_ctxt Pointer to the QUP hardware context structure.
 * @return qup_status Status of the initialization.
 */
qup_status init_hw_ctxt_core_lock(qup_hw_ctxt *h_ctxt);

/**
 * @brief De Initializes the QUP hardware context core lock.
 *
 * This function de init the core lock for given SE.
 *
 * @param h_ctxt Pointer to the QUP hardware context structure.
 * @return qup_status Status of the initialization.
 */
qup_status de_init_hw_ctxt_core_lock (qup_hw_ctxt *h_ctxt);

/**
 * @brief Allocates memory for QUP hardware context.
 *
 * This function allocates and returns a pointer to a QUP hardware context
 * based on the specified QUP type and SE index.
 *
 * @param qup_type Type of QUP interface.
 * @param se_index Serial engine index.
 * @return Pointer to the allocated QUP hardware context.
 */
qup_hw_ctxt *alloc_hw_ctxt(QUP_TYPE qup_type, uint32 se_index);

/**
 * @brief Frees the QUP hardware context.
 *
 * This function releases the memory allocated for the QUP hardware context.
 *
 * @param h_ctxt Pointer to the QUP hardware context to be freed.
 */
void free_hw_ctxt(qup_hw_ctxt *h_ctxt);


/* Open Handle based on instance*/
qup_status  qup_open(QUP_TYPE qup, uint32 se_id, void** handle, SE_PROTOCOL req_protocol);

/* Turn ON Qup Resources */
qup_status  qup_power_on(void* handle);

/* Turn OFF Qup Resource */
qup_status  qup_power_off(void* handle);

/* Release QUP Handle */
qup_status  qup_close(void* handle, SE_PROTOCOL req_protocol);

/* Dump register and resources from Fatal Path */
void        qup_error_dump_reg(void);

/* Check if Instance is in FIFO mode */
boolean     qup_in_fifo_mode(void* handle);

/* Allocates and Initiaizes  client ctxt*/
qup_status init_client_ctxt (qup_hw_ctxt *h_ctxt, qup_client_ctxt **clnt_ctxt);

/* De-Allocates and De-Initiaizes  client ctxt*/
void de_init_client_ctxt (se_dev_cfg *dcfg, qup_client_ctxt *c_ctxt);
#endif
