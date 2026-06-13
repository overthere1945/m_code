/** 
    @file  qup_plat.h
    @brief platform interface
 */

/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
10/15/25   MG      Added QUP PIPE API support for compilation and updated QUP PIPE Macro size.
10/07/25   SS      Added changes for Alana to by pass SWA for SID HW bug.
09/23/25   MG      Added QUP PIPE MACRO Support
05/13/25   RK      Added SWA for CESTA bug on aspen
03/17/25   SS      Added SWA for AXI QUP HW bug.
06/26/24   GKR     Added new API to convert PA of HW block to Image specific VA
*/
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __QUP_PLAT_H__
#define __QUP_PLAT_H__

/* *************************************************************** */
/*                          INCLUDE FILES                          */
/* *************************************************************** */

#include "qup_devcfg.h"
#include "qup_log.h"
#include "ClockDefs.h"
#include "qup_drv.h"
#include "ChipInfo.h"
#include "busywait.h"

#define QUP_PIPE_NUM_PARAMETERS                     1
#define QUP_PIPE_MAX_CONCURRENT_CLIENTS             20
/* *************************************************************** */
/*                         DATA STRUCTURES                         */
/* *************************************************************** */

/* Chip version type*/
typedef enum qup_chip_version_type
{
    CHIP_VERSION_R1,
    CHIP_VERSION_R2,
	CHIP_VERSION_R3,
	CHIP_VERSION_MAX,
} qup_chip_version_type;

/* IRQ trigger type*/
typedef enum qup_irq_trigger_type
{
    DEFAULT_TRIGGER   = 0,
    LEVEL_HIGH_TRIGGER,
    LEVEL_LOW_TRIGGR,
    RISING_EDGE_TRIGGER,
    FALLING_EDGE_TRIGGER,
    DUAL_EDGE_TRIGGER,
    TRIGGER_MAX
} qup_irq_trigger_type;

/* Structure to hold timer related callback data*/
typedef struct qup_timer_callback_data
{
    void      *cb_data;
    void     (*cb_func)(void *);
} qup_timer_callback_data;

/* Structure for Qup Type Specific functions*/
typedef struct QupPlatFcnTable
{
   boolean   (*QupClockEnable)                (se_dev_cfg *config);
   boolean   (*QupClockDisable)               (se_dev_cfg *config);
   boolean   (*QupGpioEnableIO)               (se_dev_cfg *config, uint8 io_idx);
   boolean   (*QupGpioEnable)                 (se_dev_cfg *config);
   boolean   (*QupGpioDisable)                (se_dev_cfg *config);
   void      (*QupGpioDump)                   (se_dev_cfg *config);
   void*     (*QupTimerInit)                  (se_dev_cfg *dcfg, void (*timer_cb) (void *), void *data);
   boolean   (*QupTimerSet)                   (se_dev_cfg *dcfg, void *handle, uint32 time_out_msec);
   boolean   (*QupTimerClear)                 (se_dev_cfg *dcfg, void *handle);
   boolean   (*QupTimerDeInit)                (se_dev_cfg *dcfg, void *handle);
} QupPlatFcnTable;

extern const QupPlatFcnTable top_qup_functions;

#ifdef USES_SSC_QUP
extern const QupPlatFcnTable ssc_qup_functions;
#endif

/* *************************************************************** */
/*                            FUNCTIONS                            */
/* *************************************************************** */

/* Vote for Non-Island SE Clock Resources */
boolean   qup_setup_se_clock (se_dev_cfg *config, uint32 frequency_khz);
boolean   qup_unset_se_clock (se_dev_cfg *config, uint32 frequency_khz);

/* Vote for Non-Island Core Clock Resources */
boolean   qup_setup_core_clock (se_dev_cfg *config, uint32 frequency_khz);
boolean   qup_unset_core_clock (se_dev_cfg *config);
/**
 * sets common clock ids in devcfg strucutre .
 *
 * @param[in]  se_dev_cfg
 *
 * @return   TRUE for SUCCESS, FALSE for FAILURE.
 */

boolean   qup_setup_common_clock_id (se_dev_cfg *config);

/* Enable/Disable Clock Resources */
static __inline
boolean qup_clock_enable(se_dev_cfg *config)
{
   return ((QupPlatFcnTable *)config->qup_plat_fcn_table)->QupClockEnable(config);
}

static __inline
boolean   qup_clock_disable (se_dev_cfg *config)
{
   return ((QupPlatFcnTable *)config->qup_plat_fcn_table)->QupClockDisable(config);
}

/* Get SE Clock Plan w.r.t DFS index */
uint32    qup_get_se_clock_plan_copy(se_dev_cfg *config, uint32 size);

/* Register GPIO Pinctrls  */
boolean qup_gpio_register_io (se_dev_cfg *config);

/* Enable/Disable GPIO Resources */
static __inline
boolean   qup_gpio_enable_io (se_dev_cfg *config, uint8 io_idx)
{
   return ((QupPlatFcnTable *)config->qup_plat_fcn_table)->QupGpioEnableIO(config,io_idx);
}

static __inline
boolean   qup_gpio_enable (se_dev_cfg *config)
{
   return ((QupPlatFcnTable *)config->qup_plat_fcn_table)->QupGpioEnable(config);
}

static __inline
boolean   qup_gpio_disable (se_dev_cfg *config)
{
   return ((QupPlatFcnTable *)config->qup_plat_fcn_table)->QupGpioDisable(config);
}

/* Print GPIO Status and Config */
static __inline
void      qup_gpio_dump_stat (se_dev_cfg *config)
{
   ((QupPlatFcnTable *)config->qup_plat_fcn_table)->QupGpioDump(config);
}

/* Qup Power Domain*/
boolean   qup_power_domain_enable (se_dev_cfg *config);
boolean   qup_power_domain_disable (se_dev_cfg *config);
boolean   qup_is_power_domain_on (se_dev_cfg *config);

/* Interupt Management API's */
boolean   qup_interrupt_register_ex(uint32 irq_num,qup_irq_trigger_type trigger, void (*isr) (void *), void *handle, uint16 island_spec, uint8 is_island);
#define   qup_interrupt_register(irq_num,trigger,isr,handle,island_spec) qup_interrupt_register_ex(irq_num,trigger,isr,handle,island_spec,1)
boolean   qup_interrupt_enable (uint32 irq_num);
boolean   qup_interrupt_disable  (uint32 irq_num);
boolean   qup_interrupt_clear  (uint32 irq_num);
boolean   qup_interrupt_deregister_ex (uint32 irq_num, uint8 is_island);
#define   qup_interrupt_deregister(irq_num) qup_interrupt_deregister_ex(irq_num, 1)

boolean   qup_gpio_int_register_ex (se_dev_cfg *dcfg , qup_irq_trigger_type trigger,
                                    void (*isr) (void *), void *handle,uint8 is_island);
boolean   qup_gpio_int_deregister_ex (se_dev_cfg *dcfg, uint8 is_island);
boolean   qup_manage_island_interrupts (uint32 irq_num, uint16 island_spec, boolean isIsland);
/* Configure TCSR (To Be implemented)*/
boolean   qup_internal_tcsr_config (se_dev_cfg *config);
boolean   qup_internal_tcsr_deconfig (se_dev_cfg *config);

/* Qup Polling Delay */
void      qup_delay_us (uint32 us);

/* Qup Timer handling API's*/
static __inline
void*     qup_timer_init(se_dev_cfg *dcfg,void (*timer_cb) (void *), void *data)
{
    return ((QupPlatFcnTable *)dcfg->qup_plat_fcn_table)->QupTimerInit(dcfg,timer_cb,data);
}

static __inline
boolean   qup_timer_clear(se_dev_cfg *dcfg,void *handle)
{
    return ((QupPlatFcnTable *)dcfg->qup_plat_fcn_table)->QupTimerClear(dcfg,handle);
}

static __inline
boolean   qup_timer_set(se_dev_cfg *dcfg,void *handle, uint32 time_out_msec)
{
    return ((QupPlatFcnTable *)dcfg->qup_plat_fcn_table)->QupTimerSet(dcfg,handle,time_out_msec);
}

static __inline
boolean   qup_timer_deinit(se_dev_cfg *dcfg,void *handle)
{
    return ((QupPlatFcnTable *)dcfg->qup_plat_fcn_table)->QupTimerDeInit(dcfg,handle);
}

/* Attach function table for QUP */
void      qup_attach_platform_functions(se_dev_cfg *dcfg);

/* Internal timer expity api */
void      internal_qup_timer_expiry(int data);

/* Returns True if RUMI platform  */
boolean   qup_detect_rumi_platform(void);

/* Initilaise Global Platform Resources*/
void      qup_plat_init(void);

/* Enable/Disable SSC GDSC */
boolean qup_enable_power_domain(boolean enable);

/* Return current chip version*/
qup_chip_version_type qup_get_chip_version(void);

/**
 * @brief Registers an NPA client for a QUP SE.
 * @param se_dev_cfg Pointer to the QUP SE hardware context.
 * @return TRUE if the NPA client was successfully registered, FALSE otherwise.
 *
 * This function attempts to register a synchronous NPA (Node Power Architecture) client
 * for a QUP (Qualcomm Universal Peripheral) SE device. It constructs the client name based
 * on the QUP type, ID, and SE (Serial Engine) ID, and then creates the client. If the
 * client creation fails, it logs an error and returns FALSE.
 */
boolean qup_register_npa_client(se_dev_cfg* dcfg);

/**
 * @brief vote against big image sleep for QUP usecases.
 * @param h_ctxt Pointer to the QUP SE hardware context.
 * @param sleep_disable Flag  TRUE to vote agaisnt sleep.
 * @return None.
 */
void qup_set_sleep_votes(qup_hw_ctxt *h_ctxt,boolean sleep_enable);

/* Return current chip version*/
qup_chip_version_type qup_get_chip_version(void);

/* Return current chip family*/
ChipInfoFamilyType qup_get_chip_family(void);

/* Returns Virtual address mapping for the HW Phy_addr */
void* qup_convert_hw_phy_to_vir_addr (void* phy_addr);

/* Retrieves the QUP pipe handle. */
qurt_pipe_t *qup_get_pipe_handle(void);

/* Initializes the QUP pipe */
int qup_pipe_init (void);

/* Deinitializes the QUP pipe */
void qup_pipe_de_init (void);

#ifdef QUP_AXI_HW_BUG
void qup_smem_init(void);
#endif

#endif
