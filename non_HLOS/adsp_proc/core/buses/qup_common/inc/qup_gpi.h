/**
    @file  qup_gpi.h
    @brief GPI interface
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __QUP_GPI_H__
#define __QUP_GPI_H__

/* *************************************************************** */
/*                          INCLUDE FILES                          */
/* *************************************************************** */
#include "qup_drv.h"
#include "qup_alloc.h"
#include "gpi.h"

/* *************************************************************** */
/*                 DATA STRUCTURES and MACRO DEFINITONS            */
/* *************************************************************** */
#define QUP_GPII_RING_BUFFER_ALIGN 1024
#define MAX_GPII_RING_ELEM         32

#define MAX_TX_TRE_LIST_SIZE_PER_CORE  (MAX_GPII_RING_ELEM/2)
#define MAX_RX_TRE_LIST_SIZE_PER_CORE  (MAX_GPII_RING_ELEM/4)
#define MAX_EVR_TRE_LIST_SIZE_PER_CORE (MAX_GPII_RING_ELEM)

#define MAX_UART_TX_TRE_LIST_SIZE_PER_CORE 16
#define MAX_UART_RX_TRE_LIST_SIZE_PER_CORE 8

/*****************************************************************
*                 GPI Clean Up Sequence and implementation
******************************************************************/

/*Flow Diagram for Possible GPII Channel States and Actions
 *In working Scenarios Both Tx and RX channels are in Running State
 *On any error the following Sequences can be followed to clean up
 *and return the channels to RUNNING state
 *
 *                    STOP TX               +------------+
 *          +-------------------------------+ TX STATE = |
 *          |                               |  RUNNING   +<------------------------------------------+
 *          v                               | RX STATE = |                                           |
 *   +------+-----+                   +-----+  RUNNING   +-----+                                     |
 *   |TX STATE =  |               STOP|TX   +-----+------+  BUS|ERROR                                |
 *   |STOP IN PROG|                   |           ^            |                                     |
 *   |RX STATE =  |                   v      START|TX          v                                START|RX
 *   |RUNNING     |            +------+-----+     |      +-----+------+                              |
 *   +------+-----+    +-------+ TX STATE = +-----+      |TX STATE =  |   STOP TX                    |
 *          |          |       | STOPPED    |            |ERROR       +--------------+               |
 *          |          |       | RX STATE = |            |RX STATE =  |              |         +-----+------+
 *          |          |    +--+ RUNNING    +<--+        |ERROR       |              v         |TX STATE =  |
 *     RESET|TX        |    |  +------------+   |        +-----+------+     +--------+---+     |RUNNING     |
 *          |          |    |                   |              |            |TX STATE =  |     |RX STATE =  |
 *          |          |    |                   |          STOP|TX          |STOP IN PROG|     |ALLOCATED   |
 *          |          |STOP|RX            START|RX            |            |RX STATE =  |     +-----+------+
 *          v          |    |                   |              |            |ERROR       |           ^
 *   +------+-----+    |    |                   |              v            +--------+---+           |
 *   |TX STATE =  |    |    |  +------------+   |        +-----+------+              |          START|TX
 *   |ALLOCATED   |    |    +->+TX STATE =  +-->+        |TX STATE =  |              |               |
 *   |RX STATE =  |    |       |STOPPED     |            |STOPPED     |         RESET|TX             |
 *   |RUNNING     |STOP|RX     |RX STATE =  |  STOP RX   |RX STATE =  |              |         +-----+------+
 *   +---+-----+--+    |       |STOPPED     +<-----------+ERROR       |              v         |TX STATE =  |
 *       |     |       |       +-----+------+            +---------+--+     +--------+---+     |ALLOCATED   |
 *       |     |       v             |     RESET TX(Force Reset)   |        |TX STATE =  |     |RX STATE =  |
 *       |     |    +--+---------+   +----------------------------------+   |ALLOCATED   |     |ALLOCATED   |
 *       |     |    |TX STATE =  |                                 |    |   |RX STATE =  |     +--+------+--+
 *       |     |    |STOPPED     |             STOP RX             |    |   |ERROR       |        ^      ^
 *       |     |    |RX STATE =  +<--------------------------------+    |   +-+------+---+        |      |
 *       |     |    |STOP IN PROG|                         STOP RX      |     |      |            |      |
 *       |     |    +--+---------+             +------------------------------+      |       RESET|RX    |
 *       |     |       |                       v                        |        STOP|RX          |      |
 *       | STOP|RX     |                 +-----+------+                 |            |            |      |
 *       |     |       |                 |TX STATE =  |                 |            |            |      |
 *       |     |       |                 |ALLOCATED   +-------------------------------------------+      |
 *       |     |       |   RESET TX      |RX STATE =  |                 v            |                   |
 *   STOP|RX   |       +---------------->+STOP IN PROG|            +----+-------+    |                   |
 *       |     |                         +-----+------+            |TX STATE =  |    |              RESET|RX
 *       |     |                               ^                   |ALLOCATED   +<---+                   |
 *       |     |                               |                   |RX STATE =  |                        |
 *       |     +-------------------------------+                   |STOPPED     |                        |
 *       |                                                         +--+----+----+                        |
 *       |                                                            ^    |                             |
 *       |                                                            |    |                             |
 *       +------------------------------------------------------------+    +-----------------------------+
 */

/* The following Structure is used to store present state in the sequence depicted above
 * Where in lower 3 bits are used to identify the channel state (refer to GPI_CHAN_STATE in gpi.h)
 * Following KEY can be derived for the same :
 * +------------------+-------+
 * |  Channel State   |  Key  |
 * +------------------+-------+
 * | Allocated        | 0 0 1 |
 * | Running          | 0 1 0 |
 * | Stopped          | 0 1 1 |
 * | Stop in Progress | 1 0 0 |
 * | Error            | 1 1 1 |
 * +------------------+-------+
 */
typedef union
{
    struct
    {
        uint8 prev_cmd_ch_id       : 1;
        uint8 rx_chan_state        : 3;
        uint8 tx_chan_state        : 3;
        uint8 force_reset          : 1;
    }data;
    uint8 raw;
}gpi_clean_state;


/* MACRO's used to modify each element of this structure*/
#define GPI_CHAN_STATE_MASK 0x7
#define SET_FORCE_RESET(var)  \
        { var.data.force_reset  = 1;}
#define CLEAR_CLEAN_STATE(var)  \
        { var.raw  = 0;}
#define SET_CURR_STATE(var,tx,rx)  \
    { var.data.tx_chan_state  = tx; var.data.rx_chan_state  = rx;}
#define SET_CURR_CMD_CHID(var,cmd_chid) \
    { var.data.prev_cmd_ch_id = (cmd_chid & 0x1) ;}


/* Using the sequence and states mentioned in the structure,
 * The following Truth Table can be derived considering the OUTPUT of
 * each state in the Sequence is the Next Command to be issued
 *  +----------------+-----------------+-----------------+------------------------------+-----------------+------+
 *  | Force Reset(f) | Tx State(x y z) | Rx State(a b c) | Channel of previous cmd (ch) | Command to Issue| Noop |
 *  +----------------+-----------------+-----------------+------------------------------+-----------------+------+
 *  | x              |           0 1 0 |           0 1 0 | x                            | Stop Tx         |    0 |
 *  | x              |           1 1 1 |           1 1 1 | x                            | Stop Tx         |    0 |
 *  | x              |           0 1 1 |           0 1 0 | 0                            | Stop Rx         |    0 |
 *  | 0              |           0 1 1 |           0 1 1 | x                            | Start Rx        |    1 |
 *  | x              |           0 1 1 |           0 1 0 | 1                            | Start Tx        |    0 |
 *  | x              |           1 0 0 |           0 1 0 | x                            | Reset Tx        |    0 |
 *  | x              |           0 0 1 |           0 1 0 | x                            | Stop Rx         |    0 |
 *  | x              |           0 0 1 |           1 0 0 | x                            | Reset Rx        |    0 |
 *  | x              |           0 0 1 |           0 1 1 | x                            | Reset Rx        |    0 |
 *  | x              |           0 0 1 |           0 0 1 | x                            | Start Tx        |    0 |
 *  | x              |           0 1 0 |           0 0 1 | x                            | Start Rx        |    0 |
 *  | x              |           0 1 1 |           1 0 0 | x                            | Reset Tx        |    0 |
 *  | x              |           0 1 1 |           1 1 1 | x                            | Stop Rx         |    0 |
 *  | x              |           1 0 0 |           1 1 1 | x                            | Reset Tx        |    0 |
 *  | x              |           0 0 1 |           1 1 1 | x                            | Stop Rx         |    0 |
 *  | 1              |           0 1 1 |           0 1 1 | x                            | Reset Tx        |    0 |
 *  +----------------+-----------------+-----------------+------------------------------+-----------------+------+
 *
 * Solving this Truth Table(using K-Maps), equtions for the outputs(i.e. Command to be issued) are:
 *
 *  +----------------------+---------------------------------------------------------+
 *  | Command to be issued |                        Functions                        |
 *  +----------------------+---------------------------------------------------------+
 *  | Start Tx             | f(f, x, y, z, a, b, c, ch) = y'b'c + yza'c'ch           |
 *  | Start Rx             | f(f, x, y, z, a, b, c, ch) = f'ya'c + fx'z'c            |
 *  | Stop Tx              | f(f, x, y, z, a, b, c, ch) = yz'c' + xz                 |
 *  | Stop Rx              | f(f, x, y, z, a, b, c, ch) = y'zbc' + x'zac + yza'c'ch' |
 *  | Reset Tx             | f(f, x, y, z, a, b, c, ch) = yb'c' + xy' + fya'bc       |
 *  | Reset Rx             | f(f, x, y, z, a, b, c, ch) = y'a'bc + y'zac'            |
 *  | Noop                 | f(f, x, y, z, a, b, c, ch) = f'ya'bc                    |
 *  +----------------------+---------------------------------------------------------+
 *
 *  f(f, x, y, z, a, b, c, ch) can be considered as a function of f(gpi_clean_state.raw)
 *  Rewriting the Equations in terms of values of gpi_clean_state.raw
 * +----------------------------+------------------+------------------+------------------+------------------+------------------+------------------+
 * |          Functions         |      Term 1      |      Term 1      |      Term 2      |      Term 2      |      Term 3      |      Term 3      |
 * +----------------------------+------------------+------------------+------------------+------------------+------------------+------------------+
 * |                            | Mask             | value            | Mask             | value            | Mask             | value            |
 * |                            | f x y z a b c ch | f x y z a b c ch | f x y z a b c ch | f x y z a b c ch | f x y z a b c ch | f x y z a b c ch |
 * | y'b'c + yza'c'ch           | 0 0 1 0 0 1 1 0  | 0 0 0 0 0 0 1 0  | 0 0 1 1 1 0 1 1  | 0 0 1 1 0 0 0 1  |                  |                  |
 * | f'ya'c + fx'z'c            | 1 0 1 0 1 0 1 0  | 0 0 1 0 0 0 1 0  | 1 1 0 1 0 0 1 0  | 1 0 0 0 0 0 1 0  |                  |                  |
 * | yz'c' + xz                 | 0 0 1 1 0 0 1 0  | 0 0 1 0 0 0 0 0  | 0 1 0 1 0 0 0 0  | 0 1 0 1 0 0 0 0  |                  |                  |
 * | y'zbc' + x'zac + yza'c'ch' | 0 0 1 1 0 1 1 0  | 0 0 0 1 0 1 0 0  | 0 1 0 1 1 0 1 0  | 0 0 0 1 1 0 1 0  | 0 0 1 1 1 0 1 1  | 0 0 1 1 0 0 0 0  |
 * | yb'c' + xy' + fya'bc       | 0 0 1 0 0 1 1 0  | 0 0 1 0 0 0 0 0  | 0 1 1 0 0 0 0 0  | 0 1 0 0 0 0 0 0  | 1 0 1 0 1 1 1 0  | 1 0 1 0 0 1 1 0  |
 * | y'a'bc + y'zac'            | 0 0 1 0 1 1 1 0  | 0 0 0 0 0 1 1 0  | 0 0 1 1 1 0 1 0  | 0 0 0 1 1 0 0 0  |                  |                  |
 * | f'ya'bc                    | 1 0 1 0 1 1 1 0  | 0 0 1 0 0 1 1 0  |                  |                  |                  |                  |
 * +----------------------------+------------------+------------------+------------------+------------------+------------------+------------------+
 *
 * Converting to Hex:
 * +--------+-------+--------+-------+---------+-------+
 * | Term 1 |       | Term 2 |       |  Term 3 |       |
 * +--------+-------+--------+-------+---------+-------+
 * | Mask   | value | Mask   | value | Mask    | value |
 * |        |       |        |       |         |       |
 * | 0x26   | 0x2   | 0x3B   | 0x31  |         |       |
 * | 0xAA   | 0x22  | 0xD2   | 0x82  |         |       |
 * | 0x32   | 0x20  | 0x50   | 0x50  |         |       |
 * | 0x36   | 0x14  | 0x5A   | 0x1A  | 0x3B    | 0x30  |
 * | 0x26   | 0x20  | 0x60   | 0x40  | 0xAE    | 0xA6  |
 * | 0x2E   | 0x6   | 0x3A   | 0x18  |         |       |
 * | 0xAE   | 0x26  |        |       |         |       |
 * +--------+-------+--------+-------+---------+-------+
 *
 */


/* MACRO's Representing the solution of the function of gpi_clean_state i.e. f(f,x,y,z,a,b,c,ch)*/
#define CHECK_CONDITON(var,mask,value)    (((var.raw) & mask) == value )
#define START_TX(var)   CHECK_CONDITON(var,0x26, 0x2)||CHECK_CONDITON(var,0x3B,0x31)
#define START_RX(var)   CHECK_CONDITON(var,0xAA,0x22)||CHECK_CONDITON(var,0xD2,0x82)
#define STOP_TX(var)    CHECK_CONDITON(var,0x32,0x20)||CHECK_CONDITON(var,0x50,0x50)
#define STOP_RX(var)    CHECK_CONDITON(var,0x36,0x14)||CHECK_CONDITON(var,0x5A,0x1A)||CHECK_CONDITON(var,0x3B,0x30)
#define RESET_TX(var)   CHECK_CONDITON(var,0x26,0x20)||CHECK_CONDITON(var,0x60,0x40)||CHECK_CONDITON(var,0xAE,0xA6)
#define RESET_RX(var)   CHECK_CONDITON(var,0x2E, 0x6)||CHECK_CONDITON(var,0x3A,0x18)
#define NOOP_TRE(var)   CHECK_CONDITON(var,0xAE,0x26)

/*****************************************************************
*                   GPI Transfer Related Structures
******************************************************************/

/* Flags to mark Statue of GPII in qup_gpi_core_ctxt */
typedef enum _gpi_interface_flags
{
    GPI_FLAGS_RESET            = 0,
    GPI_IFACE_REGISTERED       = (1 << 0),
    GPI_WAIT_FOR_CMD_COMP      = (1 << 1),

    GPI_IN_ERROR_PATH          = (1 << 2),
    GPI_IN_FAILURE_PATH        = (1 << 3),
    GPI_ERROR_FLAGS            = GPI_IN_ERROR_PATH | GPI_IN_FAILURE_PATH,

    /*Channel Related flags*/
    GPI_UNEXPECTED_EVT         = (1 << 4),

    /*Clean Related flags*/
    GPI_CLEAN_IN_FLIGHT        = (1 << 5),
    GPI_CLEAN_CH_RESTARTED     = (1 << 6),
    GPI_CLEAN_CH_RESET         = (1 << 7),
    GPI_CLEAN_COMPLETED        = (1 << 8),
    GPI_CLEAN_FLAGS            = GPI_CLEAN_IN_FLIGHT | GPI_CLEAN_CH_RESTARTED | GPI_CLEAN_CH_RESET | GPI_CLEAN_COMPLETED,
    GPI_DEINIT_INPROGRESS      = (1 << 9),
}gpi_interface_flags;

/* Flags to mark status of transfer pointed by qup_gpi_iface_ctxt */
typedef enum _gpi_io_flags
{
    GPI_IO_UNUSED              =  0,
    GPI_IO_IN_USE              = (1 << 0),
    GPI_TX_COMPLETED           = (1 << 1),
    GPI_RX_COMPLETED           = (1 << 2),
    GPI_WAIT_FOR_UNLOCK_TRE    = (1 << 3),
    GPI_WAIT_FOR_CLEAN         = (1 << 4),
    GPI_ALL_TRANSFER_FLAGS     = GPI_TX_COMPLETED | GPI_RX_COMPLETED | GPI_WAIT_FOR_UNLOCK_TRE | GPI_WAIT_FOR_CLEAN ,
}gpi_io_flags;

/* Client Callback type of GP Error from M_IRQ_STATUS and DMA_IRQ_STAT */
typedef boolean (*GP_err_cb_type)(qup_client_ctxt *,uint32);

/* Client Callback type for Transfer completion*/
typedef void    (*tf_completion_cb)(qup_client_ctxt *);

/* Client Callback type for IN-Band Interrupt*/
typedef void    (*gp_sync_cb)(qup_hw_ctxt *,gpi_result_type *);

typedef struct qup_gpi_core_ctxt qup_gpi_core_ctxt;

/* Structure held per core to bookmark client data / GPII status
 * Need to be filled as a part of Interface initalisation.
 */
typedef struct qup_gpi_core_ctxt
{
    GPI_CLIENT_HANDLE   gpi_handle;
    gpi_tre_ring        ring_info[GPI_CHAN_MAX];
    GPI_CHAN_STATE      chan_state[GPI_CHAN_MAX];
    uint32              gpi_err_log;
    tf_completion_cb    tf_comp_cb;
    GP_err_cb_type      gp_err_cb;
    gp_sync_cb          ibs_cb;
    void               *command_wait_signal;
    void               *client_to_be_cancelled;
    gpi_clean_state     clean_state;
    uint32              flags;
    boolean             in_use;
    qup_hw_ctxt        *h_ctxt;
    qup_gpi_core_ctxt  *next;
} qup_gpi_core_ctxt;

/* MACRO's used to attach Callbacks to qup_gpi_core_ctxt structure */
#define QUP_GPI_ATTACH_INBAND_CB(ctxt,cb)               ((qup_gpi_core_ctxt *)(ctxt))->ibs_cb = cb
#define QUP_GPI_ATTACH_TRANSFER_COMPLETION_CB(ctxt,cb)  ((qup_gpi_core_ctxt *)(ctxt))->tf_comp_cb = cb
#define QUP_GPI_ATTACH_GP_ERR_CB(ctxt,cb)               ((qup_gpi_core_ctxt *)(ctxt))->gp_err_cb = cb

/* Structure held per client to bookmark transfer status
 * Need to be filled as a part of transfer queue.
 */
typedef struct qup_gpi_iface_ctxt
{
    gpi_tre_req         tx_tre_req;
    gpi_tre_req         rx_tre_req;
    uint8               flags;
    uint8               recovery_attempts;
    qup_gpi_core_ctxt   *g_ctxt;
} qup_gpi_iface_ctxt;

typedef struct qup_gpi_ring_elem
{
    gpi_ring_elem tre_list[30];
    boolean in_use;
}qup_gpi_ring_elem;

typedef struct qup_gpi_ring_mem
{
    qup_mem_alloc_type mem_type;
    uint8 num_tre_elem;
}qup_gpi_ring_mem;

/* *************************************************************** */
/*                            FUNCTIONS                            */
/* *************************************************************** */

/* Initialize GPI interface */
qup_status qup_gpi_iface_init       (qup_hw_ctxt *h_ctxt);
qup_status qup_gpi_iface_init_ex    (se_dev_cfg *dcfg, qup_gpi_core_ctxt *gpi_ctxt, client_cb evr_cb, client_cb cmd_cb, void *h_ctxt, uint8 gpi_idx);

/* De - Initialize GPI interface */
qup_status qup_gpi_iface_de_init (qup_hw_ctxt *h_ctxt);
qup_status qup_gpi_iface_de_init_ex (se_dev_cfg *dcfg, qup_gpi_core_ctxt *g_ctxt, void *cb_data);

/* DE - Initialize GPI interface */
qup_status qup_gpi_cancel (qup_hw_ctxt *h_ctxt, qup_client_ctxt *c_ctxt,boolean force_reset);

/* Issue GSI command to GPII channels
 * Pass GPI_CHAN_STATE_NONE in gpi_exp_state incase the command does not effect channel state
 */
boolean    qup_gpi_issue_cmd (qup_hw_ctxt       *h_ctxt,
                              GPI_CHAN_CMD       gpi_chan_cmd,
                              uint32             cmd_param,
                              GPI_CHAN_STATE     gpi_exp_state,
                              GPI_CHAN_TYPE      gpi_chan_type,
                              boolean            asynchronous);

boolean qup_gpi_issue_cmd_ex (se_dev_cfg        *dcfg,
                              qup_gpi_core_ctxt *g_ctxt,
                              GPI_CHAN_CMD       gpi_chan_cmd,
                              uint32             cmd_param,
                              GPI_CHAN_STATE     gpi_exp_state,
                              GPI_CHAN_TYPE      gpi_chan_type,
                              boolean            asynchronous,
                              void              *user_data);

/* Used to find Space in GPII ring and Write Pointer*/
uint32  qup_gpi_tre_available   (qup_hw_ctxt *h_ctxt, GPI_CHAN_TYPE chan, gpi_ring_elem **next_available_tre, uint8 *wp_idx);
uint32  qup_gpi_tre_available_ex (se_dev_cfg *dcfg, qup_gpi_core_ctxt *g_ctxt, GPI_CHAN_TYPE chan, gpi_ring_elem **next_available_tre,uint8 *wp_idx);

/* Setup Lock TRE*/
void    qup_gpi_set_tre_lock     (gpi_ring_elem *tre);

/* Setup Unlock TRE*/
void    qup_gpi_set_tre_unlock   (gpi_ring_elem *tre);

/* Enable/Disable GPI interface*/
qup_status qup_gpi_enable        (qup_hw_ctxt *h_ctxt, boolean active);

/* Need to be implemented*/
qup_status qup_gpi_is_active    (qup_hw_ctxt *h_ctxt);

/* Timer based wait if GPII clean is progress */
qup_status qup_wait_for_gpii_clean (void* gpii_handle);

/*****************************************************************
* Internal Functions to be used across Island and Non-Island Files
******************************************************************/

/* Function used to get next commmand in GPI clean Sequence*/
boolean qup_gpi_clean_get_cmd(qup_gpi_core_ctxt        *g_ctxt,
                              GPI_CHAN_STATE            curr_tx_state,
                              GPI_CHAN_STATE            curr_rx_state,
                              GPI_CHAN_CMD             *next_cmd,
                              GPI_CHAN_TYPE            *next_cmd_ch,
                              boolean                  *noop);

/* Polling funtion for GPI ISR*/
void  qup_gpi_isr (void *int_handle);

/* GPI Event ring Callback Handler*/
void  qup_gpi_evr_callback (gpi_result_type *result);

/* GPI Command Completion and HW Error Notification Handler*/
void  qup_gpi_cmd_callback (gpi_result_type *result);

/* GPI IO context allocator*/
void *qup_gpi_get_io_ctxt (se_dev_cfg *dcfg);

/* Allocate GPI IO context*/
void  qup_gpi_release_io_ctxt (se_dev_cfg *dcfg, void *ptr);

/* Get GPI core context*/
void *qup_gpi_get_gpi_ctxt (se_dev_cfg *dcfg);

/* Free's GPI core context */
void qup_gpi_release_gpi_ctxt (se_dev_cfg *dcfg, void *ptr);

/* Free's GPI IO context */
void  qup_gpi_iface_clean(qup_client_ctxt *c_ctxt);

/* Handle GPI Clean from Timer Callback */
void  qup_gpi_handle_timer_cb(void *data);

#endif
