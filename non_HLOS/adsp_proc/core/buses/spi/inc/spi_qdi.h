/** 
    @file  spi_qdi.h
    @brief SPI qdi interface
 */
/*=============================================================================
            Copyright (c) 2017-2018,2020-2021, 2023, 2024 Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __SPI_QDI_H__
#define __SPI_QDI_H__

#include "spi_api.h"
#include "qurt_qdi_driver.h"
#include "qurt.h"
#include "qurt_error.h"
#include "qurt_callback.h"

#define SPI_DRIVER_NAME "/dev/spi"
#define SPI_THREAD_NAME "spi_qdi_cb"

#define NUM_PDS                 3
#define NUM_CLIENTS_PER_PD      16
#define CB_PIPE_NUM_PARAMETERS  3
#define NUM_QDI_SPI_MAX_DESCS       4
#define MAX_CONCURRENT_CLIENTS      4
#define SPI_CB_THREAD_PRIORITY     16

#define SPI_QDI_OPEN                      (0 + QDI_PRIVATE)
#define SPI_QDI_CLOSE                     (1 + QDI_PRIVATE)
#define SPI_QDI_POWER_ON                  (2 + QDI_PRIVATE)
#define SPI_QDI_POWER_OFF                 (3 + QDI_PRIVATE)
#define SPI_QDI_FULL_DUPLEX               (4 + QDI_PRIVATE)
#define SPI_QDI_GET_TIMESTAMP             (5 + QDI_PRIVATE)
#define SPI_QDI_GET_CALLBACK_DATA         (6 + QDI_PRIVATE)
#define SPI_QDI_SETUP_ISLAND_RESOURCE     (7 + QDI_PRIVATE)
#define SPI_QDI_RESET_ISLAND_RESOURCE     (8 + QDI_PRIVATE)
#define SPI_QDI_SET_USER_CB_DATA          (9 + QDI_PRIVATE)
#define SPI_QDI_GET_TIMESTAMP_64          (10+ QDI_PRIVATE)
#define SPI_QDI_ENABLE_SLAVE_INDEX        (11+ QDI_PRIVATE)
#define SPI_QDI_OPEN_EX                   (12+ QDI_PRIVATE)
#define SPI_QDI_SETUP_ISLAND_RESOURCE_EX  (13+ QDI_PRIVATE)
#define SPI_QDI_RESET_ISLAND_RESOURCE_EX  (14+ QDI_PRIVATE)
#define SPI_QDI_PREPARE_TRANSFER          (15 + QDI_PRIVATE)
#define SPI_QDI_SUBMIT_TRANSFER           (16 + QDI_PRIVATE)

#define  SPI_QDI_MAGIC 0x0511

typedef struct spi_qdi_opener spi_qdi_opener;
typedef struct spi_qdi_ctxt spi_qdi_ctxt;

typedef struct spi_clnt_qctxt_map
{
    void *handle;
    spi_qdi_ctxt *q_ctxt;
}spi_clnt_qctxt_map;

typedef struct spi_qdi_ctxt
{
    int                 clnt_handle;
    void                *spi_handle;
    void                *ctxt;
    spi_qdi_opener      *opener;
    void                *user_tx_buf[NUM_QDI_SPI_MAX_DESCS];
    void                *user_rx_buf[NUM_QDI_SPI_MAX_DESCS];
    
    /*SPI only in DMA mode, hence Root mappings not required*/
    // void                *root_tx_buf_map[NUM_QDI_SPI_MAX_DESCS];
    // void                *root_rx_buf_map[NUM_QDI_SPI_MAX_DESCS];
    
    callback_fn           cb_fn;
    spi_config_t          config;
    spi_descriptor_t      desc[NUM_QDI_SPI_MAX_DESCS];
#ifdef  ENABLE_XFER_OPTIMIZE_IN_ISLAND
    spi_config_desc_pairs  multi_desc_pairs[NUM_QDI_SPI_MAX_DESCS];
#else
    spi_config_desc_pairs *multi_desc_pairs;
#endif
    uint8                 num_desc;
    int16                 num_desc_pairs;
    boolean               in_use;
    boolean               multi_slave_config;
    spi_qdi_ctxt          *next;
} spi_qdi_ctxt;

typedef struct spi_qdi_opener
{
    qurt_qdi_obj_t      obj;
    qurt_pipe_t         pipe;
    qurt_pipe_attr_t    attr;
    qurt_pipe_data_t    data[CB_PIPE_NUM_PARAMETERS * MAX_CONCURRENT_CLIENTS];
    qurt_mutex_t        mutex;
    spi_clnt_qctxt_map  c_handle[NUM_CLIENTS_PER_PD];
    int                 pid;
    spi_qdi_ctxt        q_ctxt_list[MAX_CONCURRENT_CLIENTS];
    
    /*Not to be dereferenced from root*/
    qurt_cb_data_t*     spi_user_cb;
    
    boolean             is_island_process;
    boolean             in_use;
} spi_qdi_opener; 

typedef struct SPI_qdi_callback
{
    callback_fn     func;
    uint32          status;
    void            *ctxt;
} spi_qdi_callback;

int spi_qdi_root_invoke
(
    int             client_handle,
    qurt_qdi_obj_t *obj,
    int             method,
    qurt_qdi_arg_t  a1,
    qurt_qdi_arg_t  a2,
    qurt_qdi_arg_t  a3,
    qurt_qdi_arg_t  a4,
    qurt_qdi_arg_t  a5,
    qurt_qdi_arg_t  a6,
    qurt_qdi_arg_t  a7,
    qurt_qdi_arg_t  a8,
    qurt_qdi_arg_t  a9
);

int spi_qdi_root_invoke_non_island
(
    int             client_handle,
    qurt_qdi_obj_t *obj,
    int             method,
    qurt_qdi_arg_t  a1,
    qurt_qdi_arg_t  a2,
    qurt_qdi_arg_t  a3,
    qurt_qdi_arg_t  a4,
    qurt_qdi_arg_t  a5,
    qurt_qdi_arg_t  a6,
    qurt_qdi_arg_t  a7,
    qurt_qdi_arg_t  a8,
    qurt_qdi_arg_t  a9
);

void spi_notify_completion (uint32 status, void *context);

spi_qdi_ctxt *spi_qdi_root_get_q_ctxt(spi_qdi_opener *opener, boolean is_multi_desc);
void spi_qdi_root_put_q_ctxt(spi_qdi_opener *opener, spi_qdi_ctxt *q_ctxt, boolean is_multi_desc);

#define  SPI_QDI_ENCODE(index)   (index ^ SPI_QDI_MAGIC)

#define  SPI_QDI_DECODE(handle, index, status)                          \
    index = (uint32)handle ^ SPI_QDI_MAGIC;                             \
    if (index >= NUM_CLIENTS_PER_PD)                                    \
    {                                                                   \
        status = SPI_ERROR_QDI_MMAP_FAIL;                               \
        QUP_LOG(LEVEL_ERROR, "Invalid Handle 0x%x %d",handle,__LINE__); \
        break;                                                          \
    }


#endif //__SPI_QDI_H__
