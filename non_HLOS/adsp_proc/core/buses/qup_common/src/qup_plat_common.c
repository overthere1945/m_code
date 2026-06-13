/**
    @file  qup_plat.c
    @brief platform implementation
 */

/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
10/15/25   MG      Moved QUP PIPE API support to ISLAND.
10/07/25   SS      Added changes for Alana to by pass SWA for SID HW bug.
09/23/25   MG      Added qup pipe api support
07/17/25   GKR     Fix Bug in Previous promotion
06/16/25   SS      Added changes for SE clock voting for TOP QUP.
05/27/25   GKR     Migrated SSC GDSC to DT
05/13/25   RK      Added SWA for CESTA bug on aspen
03/17/25   SS      SWA for Kaanapali QUP AXI HW Bug
10/01/24   KSN     Updated changes for SPI_3W support
08/16/24   GKR     Moved qup_enable_power_domain() to non island file
07/29/24   GKR     Added apis to create npa handle per SE & vote against big image sleep
06/26/24   GKR     Added New API qup_convert_hw_phy_to_vir_addr() to convert HW PHY address to Image specific VA
                   logging correction
*/
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/


/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/
/** STANDARD INCLUDES **/
#include "stdlib.h"
#include <stringl/stringl.h>

/** QURT INCLUDES **/
#include "qurt.h"
#include "qurt_island.h"
#include "qurt_isr.h"

/** SOCINFRA INCLUDES **/
#include "Clock.h"
#include "ClockDefs.h"
#include "ClockImage.h"
#include "GPIO.h"
#include "uGPIOInt.h"
#include "GPIOIntDefs.h"
#include "GPIOImage.h"

/** PLATFORM INCLUDES **/
#include "DALDeviceId.h"
#include "err.h"
#include "DDIPlatformInfo.h"
#include "DDIHWIO.h"

/** QUP INCLUDES **/
#include "qup_alloc.h"
#include "qup_os.h"
#include "qup_plat.h"
#include "qup_common_internal.h"

#ifdef QUP_ENABLE_ISLAND
#include "island_user.h"
#include "uSleep_islands.h"
#endif


#ifdef QUP_IN_ADSP_PROC
#include "npa.h"
#include "sleep_npa.h"
#endif

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/
#define ISR_THREAD_STACK_SIZE 2*1024
#define ISR_THREAD_PRIORITY   4

/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/


/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/
extern ClockHandle           qup_clock_handle;
extern ClockIdType           gdsc_pwr_domain_id;
extern GPIOClientHandleType  qup_gpio_handle[];

static boolean               is_rumi          = FALSE;
static qup_chip_version_type chip_version     = CHIP_VERSION_MAX;


const unsigned char irq_trigger_map[TRIGGER_MAX] = { QURT_INT_TRIGGER_USE_DEFAULT,
                                                     QURT_INT_TRIGGER_LEVEL_HIGH,
                                                     QURT_INT_TRIGGER_LEVEL_LOW,
                                                     QURT_INT_TRIGGER_RISING_EDGE,
                                                     QURT_INT_TRIGGER_FALLING_EDGE,
                                                     QURT_INT_TRIGGER_DUAL_EDGE };

const uGPIOIntTriggerType ugpioint_trigger[TRIGGER_MAX] = { 0,   /* Making this 0 as there is not default TRIGGER in UGPIO */
                                                            UGPIOINT_TRIGGER_HIGH,
                                                            UGPIOINT_TRIGGER_LOW,
                                                            UGPIOINT_TRIGGER_RISING,
                                                            UGPIOINT_TRIGGER_FALLING,
                                                            UGPIOINT_TRIGGER_DUAL_EDGE};
/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/

/*Clocks*/
boolean qup_setup_se_clock (se_dev_cfg *config, uint32 req_frequency_khz)
{
    ClockResult res;
    uint32 flags = 0;
    int req_freq_idx = 0;
    //const char *se_src_clock = (const char *) (*(config->se_clock));

    if( QUP_IS_NOT_SET(config->qup_data->flags,VOTE_FOR_SRC_CLOCK) &&
        (config->protocol_in_use != SE_PROTOCOL_UART) &&
        (config->protocol_in_use != SE_PROTOCOL_UFCS)
    )
    {
        return TRUE;
    }

    if (GET_QUP_MAJOR(config->qup_data->qup_id) == SSC_QUP)
    {
         flags = CLOCK_FLAG_FORCE_DOMAIN;
    }

    if (qup_clock_handle == NULL) { return FALSE; }

    if (config->freq_plan)  // Check to handle UART as UART don't configure from freq_plan
    {
        for (req_freq_idx = 0; req_freq_idx < config->freq_plan->num_entries; req_freq_idx++)
        {
            if(config->freq_plan->freq_list_local[req_freq_idx].nFreqHz == req_frequency_khz)
            {
                break;
            }
        }

        if (req_freq_idx == config->freq_plan->num_entries)
        {
            QUP_SE_LOG(config,LEVEL_ERROR, "qup_setup_se_clock:req freq not found in dfs plan");
            return FALSE;
        }
        config->freq_plan->freq_ref_count[req_freq_idx]++;
    }

    if (req_frequency_khz > config->current_se_freq)
    {

        res = Clock_SetFrequencyEx(qup_clock_handle, config->se_clk_id, req_frequency_khz, CLOCK_FREQUENCY_HZ_EXACT, NULL, flags, 0);
        if (CLOCK_SUCCESS != res)
        {
            QUP_SE_LOG(config,LEVEL_ERROR, "Clock_SetFrequency failed for se src clk res = %d",res);
            return FALSE;
        }

        QUP_SE_LOG(config,LEVEL_INFO, "qup_setup_se_clock: Src Freq Change from %d to %d ",
                                        config->current_se_freq, req_frequency_khz);

        /* if transistion is from non zero frequency need to call Clock_SetFrequencyEx()
           with zero freq after transistion
           to avoid stale votes in clock driver */
        if (config->current_se_freq )
        {
            res = Clock_SetFrequencyEx(qup_clock_handle, config->se_clk_id, 0, CLOCK_FREQUENCY_HZ_EXACT, NULL, flags, 0);
            if (CLOCK_SUCCESS != res)
            {
                QUP_SE_LOG(config,LEVEL_ERROR, "Clock_SetFrequency Removing stale vote  failed");
                return FALSE;
            }
        }
        config->current_se_freq = req_frequency_khz;

        // if(QUP_IS_SET(config->qup_data->flags,VOTE_FOR_SRC_CLOCK))
        // {
            // res = Clock_Enable(qup_clock_handle, config->se_clk_id);
            // if (CLOCK_SUCCESS != res) { return FALSE; }
        // }
    }

    QUP_SE_LOG(config,LEVEL_VERBOSE, "req se src freq = %d Hz", config->current_se_freq);

    return TRUE;
}
boolean qup_setup_common_clock_id (se_dev_cfg *config)
{
    return TRUE;
}

boolean qup_setup_core_clock (se_dev_cfg *config, uint32 frequency_khz)
{
    ClockResult res;
    ClockIdType clk_id = config->qup_data->common_clk_id[QUP_CORE_2X_CLOCK_IDX];
    uint32 flags = CLOCK_FLAG_FORCE_DOMAIN_ENABLE;

    if(! (config->qup_data->flags & VOTE_FOR_CORE2X_CLOCK))
    {
        return TRUE;
    }

    if (qup_clock_handle == NULL) { return FALSE; }

    if (clk_id == 0)
    {
        QUP_LOG(LEVEL_ERROR, "Core Clock Id not intialzed");
        return FALSE;
    }

    res = Clock_SetFrequencyEx(qup_clock_handle, clk_id, frequency_khz, CLOCK_FREQUENCY_HZ_AT_LEAST, NULL, flags, 0);
    if (CLOCK_SUCCESS != res)
    {
        QUP_LOG(LEVEL_ERROR, "plat_setup_core_clock : Clock_SetFrequency failed %d", res);
        return FALSE;
    }

    // res = Clock_Enable(qup_clock_handle, clk_id);
    // if (CLOCK_SUCCESS != res)
    // {
        // QUP_LOG(LEVEL_ERROR, "plat_setup_core_clock : Clock_Enable failed %d", res);
        // return FALSE;
    // }

    QUP_SE_LOG(config,LEVEL_VERBOSE, "req core2x freq = %d Hz", frequency_khz);

    return TRUE;
}

boolean qup_unset_core_clock (se_dev_cfg *config)
{
    ClockResult res;
    ClockIdType clk_id = config->qup_data->common_clk_id[QUP_CORE_2X_CLOCK_IDX];
    uint32 flags = CLOCK_FLAG_FORCE_DOMAIN;

    if (qup_clock_handle == NULL) { return FALSE; }

    if(! (config->qup_data->flags & VOTE_FOR_CORE2X_CLOCK))
    {
          return TRUE;
    }

    if (clk_id == 0)
    {
        QUP_LOG(LEVEL_ERROR, "qup_unset_core_clock: invalid clk_id %d", clk_id);
        return FALSE;
    }
    if(qup_get_chip_family() != CHIPINFO_FAMILY_ASPEN)
    {
        /* calling with 0kHz to reset RCG to safe MUX value */
        res = Clock_SetFrequencyEx(qup_clock_handle, clk_id, 0, CLOCK_FREQUENCY_HZ_EXACT, NULL, flags, 0);

        if (CLOCK_SUCCESS != res)
        {
            QUP_LOG(LEVEL_ERROR, "plat_unset_core_clock: Clock_Disable failed %d", res);
            return FALSE;
        }
    }

    QUP_SE_LOG(config,LEVEL_VERBOSE, "qup clock unset %s", clock_str);

    return TRUE;
}

boolean qup_unset_se_clock (se_dev_cfg *config, uint32 req_frequency_khz)
{
    ClockResult res;
    //const char *se_src_clock = (const char *) (*(config->se_clock));
    uint32 req_freq_idx = 0;
    uint32 set_se_freq_level = 0;
    ClockFrequencyType freqreqType = CLOCK_FREQUENCY_HZ_EXACT;
    uint32 flags = 0;
    boolean isFreqReConfig = FALSE;
    uint32 i = 0;

    if(!(config->qup_data->flags & VOTE_FOR_SRC_CLOCK))
    {
          return TRUE;
    }

    if (GET_QUP_MAJOR(config->qup_data->qup_id) == SSC_QUP)
    {
         flags = CLOCK_FLAG_FORCE_DOMAIN;
    }

    if (qup_clock_handle == NULL) { return FALSE; }

    /* Check to handle UART as UART don't configure from freq_plan
       and if client doesn't pass frequency for i2c and spi case */
    if(req_frequency_khz && config->freq_plan)
    {
        for (req_freq_idx = 0; req_freq_idx < config->freq_plan->num_entries; req_freq_idx++)
        {
            if(config->freq_plan->freq_list_local[req_freq_idx].nFreqHz == req_frequency_khz)
            {
                break;
            }
        }

        if (req_frequency_khz == config->current_se_freq &&
             (config->freq_plan->freq_ref_count[req_freq_idx] == 1))
        {
             for (i = req_freq_idx; i > 0; i --)
             {
                 if(config->freq_plan->freq_ref_count[i-1])
                 {
                     set_se_freq_level = config->freq_plan->freq_list_local[i-1].nFreqHz;
                     break;
                 }
             }
             isFreqReConfig = TRUE;
        }

        if(config->freq_plan->freq_ref_count[req_freq_idx])
        {
            config->freq_plan->freq_ref_count[req_freq_idx]--;
        }

    }
    else if (config->resources.island_setup_votes == 1)
    {
        isFreqReConfig = TRUE;
    }

    if (isFreqReConfig)
    {
        if (set_se_freq_level == 0)
        {
            freqreqType = CLOCK_FREQUENCY_HZ_AT_LEAST;
        }
        res = Clock_SetFrequencyEx(qup_clock_handle, config->se_clk_id, set_se_freq_level, freqreqType, NULL, flags, 0);
        if (CLOCK_SUCCESS != res)
        {
            QUP_SE_LOG(config,LEVEL_ERROR, "Clock_SetFrequencyEx failed with :%d, for frequency :%d", res, set_se_freq_level);
            return FALSE;
        }

        /* if transistion is to non zero frequency need to call Clock_SetFrequencyEx()
           with zero freq first before actual transistion
           to avoid stale votes in clock driver */
        if (set_se_freq_level && flags == CLOCK_FLAG_FORCE_DOMAIN)
        {
            res = Clock_SetFrequencyEx(qup_clock_handle, config->se_clk_id, 0, CLOCK_FREQUENCY_HZ_AT_LEAST, NULL, flags, 0);
            if (CLOCK_SUCCESS != res)
            {
                QUP_SE_LOG(config,LEVEL_ERROR, "Clock_SetFrequencyEx failed with :%d, for frequency zero", res);
                return FALSE;
            }
        }
        QUP_SE_LOG(config,LEVEL_INFO, "qup_unset_se_clock: Src Freq Change from %d to %d ",
                                        config->current_se_freq, set_se_freq_level);

        config->current_se_freq = set_se_freq_level;
    }

    /* This to remove all votes per freq in case client using old i2c_reset_lpi_resource()
       or during PDR */

    if (config->resources.island_setup_votes == 1 && config->freq_plan)
    {
         qup_memset(config->freq_plan->freq_ref_count, 0, MAX_FREQ_PLAN_TABLE_SIZE*sizeof(uint8));
    }

    QUP_SE_LOG(config,LEVEL_VERBOSE, "se clock unset %s", req_frequency_khz);

    return TRUE;
}

uint32 qup_get_se_clock_plan_copy(se_dev_cfg *config, uint32 size)
{
    ClockResult res;
    //ClockIdType se_id = 0;
    size_t copied = 0;
    ClockFreqPlanType   freq_list[MAX_FREQ_PLAN_TABLE_SIZE] = {0};
    //const char *se_src_clock = (const char *) (*(config->se_clock));
    uint8 num_entries = config->freq_plan->num_entries;
    uint32 dfs_perf_levels = 0, i = 0;
    ClockQueryResultType  query_result;


    if (qurt_island_get_status ())
    {
        QUP_LOG(LEVEL_ERROR, "ERROR: setup clock called in island %d", config->se_id);
        return FALSE;
    }

    if (qup_clock_handle == NULL) { return FALSE; }

    if(config->freq_plan->num_entries == 0)
    {
        //res = Clock_Query(qup_clock_handle, se_id, CLOCK_QUERY_NUM_PERF_LEVELS, 0, &query_result);
        res = Clock_Query(qup_clock_handle, config->se_clk_id, CLOCK_QUERY_NUM_PERF_LEVELS, 0, &query_result);
        if (CLOCK_SUCCESS != res)
        {
            QUP_LOG(LEVEL_ERROR, "ERROR: sClock_Query per_level failed res : %d", res);
            return FALSE;
        }

        dfs_perf_levels = query_result.Data.nNumPerfLevels;
        num_entries = dfs_perf_levels;


        for (i = 0; (i<dfs_perf_levels) && (i<size); i++)
        {
            //res = Clock_Query(qup_clock_handle, se_id, CLOCK_QUERY_PERF_LEVEL_FREQ_HZ, i, &query_result);
            res = Clock_Query(qup_clock_handle, config->se_clk_id, CLOCK_QUERY_PERF_LEVEL_FREQ_HZ, i, &query_result);
            if (CLOCK_SUCCESS != res) {
            QUP_LOG(LEVEL_ERROR, "ERROR: sClock_Query CLOCK_QUERY_PERF_LEVEL_FREQ_HZ failed res : %d", res);
            return FALSE; }
            freq_list[i].nFreqHz = query_result.Data.nPerfLevelFreqHz;
            //res = Clock_Query(qup_clock_handle, se_id, CLOCK_QUERY_PERF_LEVEL_CORNER, i, &query_result);
            res = Clock_Query(qup_clock_handle, config->se_clk_id, CLOCK_QUERY_PERF_LEVEL_CORNER, i, &query_result);
            if (CLOCK_SUCCESS != res) {
            QUP_LOG(LEVEL_ERROR, "ERROR: sClock_Query CLOCK_QUERY_PERF_LEVEL_CORNER failed res : %d", res);
            return FALSE; }
            freq_list[i].eVRegLevel = query_result.Data.ePerfLevelCorner;
        }

        if(num_entries > size){QUP_LOG(LEVEL_ERROR, "ERROR:qup_get_se_clock_plan_copy:insuficient size "); return FALSE;}

        copied = memscpy(config->freq_plan->freq_list_local, size * sizeof(ClockFreqPlanType), freq_list, num_entries * sizeof(ClockFreqPlanType));
        if (copied != num_entries * sizeof(ClockFreqPlanType)) {
        QUP_LOG(LEVEL_ERROR, "ERROR: copied = %d, num_entries = %d",copied,num_entries);
        return FALSE; }
    }
    return num_entries;
}


// interrupt

 /**
    qup_manage_island_interrupts() -  QUP API to Add/Remove Interrupts to Island interrupts list

    @param[in]  irq_num        -  Interrupt num to be added/removed from Island list
    @param[in]  island_spec    -  Type of island spec LLC,STD....
    @param[in]  add_to_island  -  if TRUE add to island list, if FALSE remove from island list
    @return
    TRUE -- Function was successful
    FALSE -- In case of failures

  */
boolean qup_manage_island_interrupts (uint32 irq_num, uint16 island_spec, boolean add_to_island)
{

#ifdef QUP_ENABLE_ISLAND
    int island_mgr_status = ISLAND_MGR_EOK;
    if (add_to_island && island_spec != ISLAND_CONFIG_INVALID)
    {
        island_mgr_status = island_mgr_add_interrupt_island(island_spec, irq_num);

        if (ISLAND_MGR_EOK != island_mgr_status)
        {
            QUP_LOG(LEVEL_ERROR, "qup_manage_island_interrupts: Adding Int to island list failed ! : %d",island_mgr_status );
            return FALSE;
        }
    }
    else
    {
        if(island_spec != ISLAND_CONFIG_INVALID)
        {
            island_mgr_status = island_mgr_remove_interrupt_island(island_spec, irq_num);
        }

        if (ISLAND_MGR_EOK != island_mgr_status)
        {
            QUP_LOG(LEVEL_ERROR, "qup_manage_island_interrupts: Removing Int from island list failed ! : %d",island_mgr_status );
            return FALSE;
        }
    }
#endif

    return TRUE;
}

boolean qup_interrupt_register_ex (uint32 irq_num,qup_irq_trigger_type trigger, void (*isr) (void *), void *handle,uint16 island_spec, uint8 is_island)
{
    qurt_thread_attr_t thread_attr;
    qurt_thread_t thread_id;
    char t_name[QURT_THREAD_ATTR_NAME_MAXLEN] = "";
    int err = 0;

    if(is_island && island_spec != ISLAND_CONFIG_INVALID)
    {
        qup_manage_island_interrupts(irq_num, island_spec, TRUE);
        qurt_thread_get_thread_id(&thread_id, "UIST");
    }
    else
    {
        snprintf(t_name, QURT_THREAD_ATTR_NAME_MAXLEN, "qup_irq%d", (int)irq_num);
        qurt_thread_attr_init (&thread_attr);
        qurt_thread_attr_set_stack_size (&thread_attr, ISR_THREAD_STACK_SIZE);
        qurt_thread_attr_set_stack_addr (&thread_attr, malloc(ISR_THREAD_STACK_SIZE));
        qurt_thread_attr_set_name (&thread_attr, t_name);
        qurt_thread_attr_set_priority (&thread_attr, ISR_THREAD_PRIORITY);
        qurt_thread_attr_set_detachstate(&thread_attr, QURT_THREAD_ATTR_CREATE_JOINABLE);
        qurt_isr_create(&thread_id, &thread_attr);
    }

    err = qurt_isr_register2(thread_id,
                             irq_num,
                             4,
                             QURT_INT_ACK_DEFAULT,
                             irq_trigger_map[trigger],
                             (void(*)(void *,int))isr,
                             (void *)handle);
    if(QURT_EOK != err)
    {
        QUP_LOG(LEVEL_ERROR, "ERROR: qurt_isr_register2 :IRQ %d register failed ",irq_num);
        return FALSE;
    }
    return TRUE;

}

boolean qup_interrupt_deregister_ex (uint32 irq_num, uint8 is_island)
{
    char t_name[QURT_THREAD_ATTR_NAME_MAXLEN] = "";
    qurt_thread_attr_t thread_attr;
    qurt_thread_t thread_id;

    if(QURT_EOK != qurt_isr_deregister2(irq_num))
    {
        return FALSE;
    }

    if(!is_island)
    {
        snprintf(t_name, QURT_THREAD_ATTR_NAME_MAXLEN, "qup_irq%d", (int)irq_num);
        qurt_thread_get_thread_id(&thread_id, t_name);
        qurt_thread_attr_get(thread_id, &thread_attr);
        qurt_isr_delete(thread_id);
        free(thread_attr.stack_addr);

    }

    return TRUE;
}

 /**
    qup_gpio_int_register_ex() -  QUP API to register GPIO Pin Interrupts

    @param[in]  gpioIntPinNum  -  GPIO pin num to resigster interrupt
    @param[in]  trigger        -  Trigger type to Generate the interrupt
    @param[in]  isr            -  Callback for Interrupt
    @param[in]  handle         -  handle to be passed to Isr callback
    @param[in]  is_island      -  island mode supported or not
    @param[out] pIrq_num       -  Saves the dynamically assigned Irq num
    @return
    TRUE -- Function was successful
    FALSE -- In case of failures

  */

boolean qup_gpio_int_register_ex (se_dev_cfg *dcfg , qup_irq_trigger_type trigger, void (*isr) (void *), void *handle, boolean add_to_island)
{
    GPIOIntResult gpioIntResult = GPIOINT_SUCCESS;
    uint32 flags = UGPIOINTF_ISLAND;

    gpioIntResult = uGPIOInt_RegisterInterrupt(dcfg->iface_data.wakeup_gpio,
                                               ugpioint_trigger[trigger],
                                               (GPIOINTISR)isr,
                                               (uGPIOINTISRCtx)handle,
                                               flags);

    if (gpioIntResult != GPIOINT_SUCCESS)
    {
        QUP_SE_LOG(dcfg, LEVEL_ERROR, "QUP uGPIOInt_RegisterInterrupt for pin %d failed with error : %d",
                dcfg->iface_data.wakeup_gpio,
                gpioIntResult);
        return FALSE;
    }

    gpioIntResult = uGPIOInt_GetInterruptID(dcfg->iface_data.wakeup_gpio,
                                                  &(dcfg->iface_data.pdc_irq));
    if (gpioIntResult != GPIOINT_SUCCESS)
    {
        QUP_SE_LOG(dcfg, LEVEL_ERROR, "QUP uGPIOInt_GetInterruptID for pin %d failed with error : %d",
                dcfg->iface_data.wakeup_gpio,
                gpioIntResult);
        return FALSE;
    }

    if (add_to_island)
    {
        if (!qup_manage_island_interrupts( dcfg->iface_data.pdc_irq,dcfg->island_spec_in_use, TRUE))
        {
            return FALSE;
        }
    }

    return TRUE;
}

 /**
    qup_gpio_int_deregister_ex() -  QUP API to register GPIO Pin Interrupts

    @param[in]  gpioIntPinNum  -  GPIO pin num to resigster interrupt
    @param[in]  is_island      -  island mode supported or not
    @return
    TRUE -- Function was successful
    FALSE -- In case of failures

  */

boolean qup_gpio_int_deregister_ex (se_dev_cfg *dcfg, uint8 remove_island)
{
    GPIOIntResult gpioIntResult = GPIOINT_SUCCESS;

    if (remove_island)
    {
        if (!qup_manage_island_interrupts(dcfg->iface_data.pdc_irq,dcfg->island_spec_in_use, FALSE))
        {
            return FALSE;
        }
    }

    gpioIntResult = uGPIOInt_DeregisterInterrupt(dcfg->iface_data.wakeup_gpio);
    if (GPIOINT_SUCCESS != gpioIntResult)
    {
        QUP_SE_LOG(dcfg, LEVEL_ERROR, "QUP uGPIOInt_DeregisterInterrupt for pin %d failed with error : %d",
                dcfg->iface_data.wakeup_gpio,
                gpioIntResult);
        return FALSE;
    }

    return TRUE;
}

boolean qup_gpio_register_io (se_dev_cfg *config)
{
    char pin_ctrl_name[20] = {'\0'};
    char protocol_name[10] = {'\0'};
    uint8 qup_type = GET_QUP_MAJOR(config->qup_data->qup_id);
    GPIOResult gpio_status = GPIO_SUCCESS;

    switch (config->protocol_in_use)
    {
        case SE_PROTOCOL_I2C:
            snprintf(protocol_name, 10, "i2c");
            break;

        case SE_PROTOCOL_I3C:
            snprintf(protocol_name, 10, "i3c");
            break;

        case SE_PROTOCOL_I3C_IBI:
            snprintf(protocol_name, 10, "i3c_ibi");
            break;

        case SE_PROTOCOL_SPI:
            snprintf(protocol_name, 10, "spi");
            break;

        case SE_PROTOCOL_SPI_3W:
            snprintf(protocol_name, 10, "spi-3w");
            break;

        case SE_PROTOCOL_UART:
        case SE_PROTOCOL_UFCS:
            snprintf(protocol_name, 10, "uart");
            break;
        default:
            return FALSE;
    }

    snprintf (pin_ctrl_name, 20, "%s-default", protocol_name);
    gpio_status = GPIO_RegisterDeviceConfig(qup_gpio_handle[qup_type], &config->se_node,
                                            (char *)pin_ctrl_name, &config->se_gpio_enable_key);

    if (GPIO_SUCCESS != gpio_status || config->se_gpio_enable_key == 0)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "Gpio Registration failed gpio status : %d gpio_enable_key : %d", gpio_status, config->se_gpio_enable_key);
        return FALSE;
    }

    snprintf (pin_ctrl_name, 20, "%s-sleep", protocol_name);

    gpio_status = GPIO_RegisterDeviceConfig(qup_gpio_handle[qup_type], &config->se_node,
                                            (char *)pin_ctrl_name, &config->se_gpio_disable_key);
    if (GPIO_SUCCESS != gpio_status || config->se_gpio_disable_key == 0)
    {
        QUP_SE_LOG(config,LEVEL_ERROR, "Gpio Registration failed gpio status : %d se_gpio_disable_key : %d", gpio_status, config->se_gpio_disable_key);
        return FALSE;
    }

    return TRUE;
}

// tcsr
boolean qup_internal_tcsr_config (se_dev_cfg *config)
{
    return TRUE;
}
boolean qup_internal_tcsr_deconfig (se_dev_cfg *config)
{
    return TRUE;
}

//Platform Function table attach
void      qup_attach_platform_functions(se_dev_cfg *dcfg)
{
    if(GET_QUP_MAJOR(dcfg->qup_data->qup_id) == SSC_QUP)
    {
#ifdef USES_SSC_QUP
        dcfg->qup_plat_fcn_table = (void *)&ssc_qup_functions;
#else
        QUP_ASSERT(NULL,QUP_FATAL_TYPE);
#endif
    }
    else if(GET_QUP_MAJOR(dcfg->qup_data->qup_id) == TOP_QUP)
    {
        dcfg->qup_plat_fcn_table = (void *)&top_qup_functions;
    }
    else
    {
        QUP_ASSERT(dcfg,QUP_FATAL_TYPE);
    }
}

static void get_rumi_platform(void)
{
    DALResult                       result = DAL_SUCCESS;
    DalDeviceHandle                *pDevice =  NULL;

    result = DAL_DeviceAttach(DALDEVICEID_PLATFORMINFO, &pDevice);
    if(result != DAL_SUCCESS)
    {
        QUP_LOG(LEVEL_ERROR, "get_rumi_platform: DAL_PlatformInfo Attach Failed :0x%x",result);
        return;
    }

    //GET PLATFORM
    DalPlatformInfoPlatformInfoType plat = { 0 };
    DalPlatformInfo_GetPlatformInfo(pDevice,&plat);
    if(plat.platform ==  DALPLATFORMINFO_TYPE_RUMI )
    {
        is_rumi = TRUE;
    }
}

ChipInfoFamilyType qup_get_chip_family(void)
{
    return ChipInfo_GetChipFamily();
}

boolean  qup_detect_rumi_platform(void)
{
    return is_rumi;
}

static void qup_detect_chip_version(void)
{
    ChipInfoVersionType Chip_Info_Version_Type;

    Chip_Info_Version_Type = ChipInfo_GetChipVersion();

    if (Chip_Info_Version_Type == CHIPINFO_VERSION(1,0))
    {
        chip_version =  CHIP_VERSION_R1;
    }
    else if (Chip_Info_Version_Type == CHIPINFO_VERSION(2,0))
    {
        chip_version =  CHIP_VERSION_R2;
    }
	else if (Chip_Info_Version_Type == CHIPINFO_VERSION(3,0))
	{
		chip_version = CHIP_VERSION_R3;
	}
}

qup_chip_version_type qup_get_chip_version(void)
{
    return chip_version;
}
/**
 * @brief Enable or disable the SSC GDSC power domain.
 *
 * This function enables or disables the SSC GDSC power domain based on the input parameter.
 * It checks if the power domain ID is valid and then either enables or disables the clock.
 * On RUMI platforms, retention is not supported, so GDSC DEVOTE will not be disabled.
 *
 * @param enable Boolean value to enable (TRUE) or disable (FALSE) the power domain.
 * @return TRUE if the operation is successful, FALSE otherwise.
 */
boolean qup_enable_power_domain(boolean enable)
{
#ifdef SSC_QUP_IN_ADSP
    if (!gdsc_pwr_domain_id)
    {
        return FALSE;
    }

    if (enable)
    {
        if (CLOCK_SUCCESS != Clock_Enable(qup_clock_handle,
                                          gdsc_pwr_domain_id))
        {
            return FALSE;
        }
    }
    else
    {
        /* On RUMI QUP Register value retention is not supported
           if we disable GDSC Vote QUP register values will be reset
           Hence Avoid GDSC Devoting on RUMI               */
        if (qup_detect_rumi_platform() == FALSE)
        {
            if (CLOCK_SUCCESS != Clock_Disable(qup_clock_handle,
                                               gdsc_pwr_domain_id))
            {
                return FALSE;
            }
        }
    }

#endif
    return TRUE;
}

#ifdef QUP_IN_ADSP_PROC

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
boolean qup_register_npa_client(se_dev_cfg* dcfg)
{

    // Retrieve the major and minor QUP types.
    QUP_MAJOR_TYPE qup_type = GET_QUP_MAJOR(dcfg->qup_data->qup_id);
    uint8 qup_id = GET_QUP_MINOR(dcfg->qup_data->qup_id);
    char se_npa_name_str[30] = {'\0'};

    // Check if the NPA handle is not already created.
    if (dcfg->npa_handle == NULL)
    {
        // Format the NPA client name string.
        snprintf(se_npa_name_str, sizeof(se_npa_name_str), "npa_%s_qup_%d_se_%hhu",
                 QUP_name[qup_type], qup_id, dcfg->se_id);

        // Create the NPA client.
        dcfg->npa_handle = (npa_client_handle) npa_create_sync_client(SLEEP_CPUVDD_LPR_NODE_NAME,
                                                  se_npa_name_str, NPA_CLIENT_REQUIRED);

        // Check if the NPA client creation failed.
        if (dcfg->npa_handle == NULL)
        {
            // Log an error and return FALSE.
            QUP_SE_LOG(dcfg, LEVEL_ERROR,
                       "qup_register_npa_client: npa_create_sync_client returned NULL !!");
            return FALSE;
        }
    }

    // Return TRUE if the NPA client was successfully created.
    return TRUE;
}


/**
 * @brief vote against big image sleep for QUP usecases.
 * @param h_ctxt Pointer to the QUP SE hardware context.
 * @param sleep_disable Flag  TRUE to vote agaisnt sleep.
 * @return None.
 */
void qup_set_sleep_votes(qup_hw_ctxt *h_ctxt, boolean sleep_disable)
{
    se_dev_cfg *dcfg = (se_dev_cfg *) h_ctxt->core_config;

    if (GET_QUP_MAJOR(dcfg->qup_data->qup_id) != TOP_QUP)
    {
        return;
    }

    if(sleep_disable)
    {
        npa_issue_required_request(dcfg->npa_handle, SLEEP_CPUVDD_LPR_CLOCKGATE_ONLY);
    }
    else
    {
         // Complete the NPA request to allow sleep.
         npa_complete_request(dcfg->npa_handle);
    }
}

#endif

void* qup_convert_hw_phy_to_vir_addr (void* phy_address)
{
    DALResult dal_res;
    static DalDeviceHandle * qup_dal_hwio_handle = NULL;
    uint8* virt_address = NULL;

    if (qup_dal_hwio_handle == NULL)
    {
        dal_res = DAL_DeviceAttach(DALDEVICEID_HWIO, &qup_dal_hwio_handle);
        if (DAL_SUCCESS != dal_res)
        {
            QUP_LOG(LEVEL_ERROR, "DAL_DeviceAttach failed status : %d",dal_res);
            return NULL;
        }
    }

    dal_res = DalHWIO_MapRegionByAddress(
                         qup_dal_hwio_handle, (uint8 *)phy_address,
                      (uint8 **)&virt_address);

    if (dal_res != DAL_SUCCESS || virt_address == NULL)
    {
        QUP_LOG(LEVEL_ERROR, "Dal Hwio Map failed status : %d, vir_address : 0x%x",dal_res, virt_address);
    }

    return virt_address;
}

void qup_plat_init(void)
{

    ClockResult clock_res;
    GPIOResult  gpio_res;
    fdt_node_handle node;
    int ret_value = 0;

    qup_comm_ulog_init();

    clock_res = Clock_Attach(&qup_clock_handle, "qup_plat_common");
    if (CLOCK_SUCCESS != clock_res) { QUP_LOG(LEVEL_ERROR, "qup_plat_init: Clock_Attach Failed :0x%x",clock_res); }

    ret_value = fdt_get_node_handle(&node, NULL, "/soc/ssc_pwr_domains");
    if (ret_value) { QUP_LOG(LEVEL_ERROR, "qup_plat_init: get node failed for ssc_pwr_domains :0x%x",ret_value); }

    clock_res = Clock_GetIdDT(qup_clock_handle, &node, "ssc_gdsc", &gdsc_pwr_domain_id);
    if (CLOCK_SUCCESS != clock_res) { QUP_LOG(LEVEL_ERROR, "qup_plat_init: Clock_GetId for GDSC Pwr Domain Failed :0x%x",clock_res); }

    gpio_res = GPIO_Attach(GPIO_DEVICE_TLMM, &qup_gpio_handle[TOP_QUP]);
    if (GPIO_SUCCESS != gpio_res) {  QUP_LOG(LEVEL_ERROR, "qup_plat_init: GPIO_Attach Failed for top qup :0x%x",gpio_res);}

    gpio_res = GPIO_Attach(GPIO_DEVICE_SSC_LPI, &qup_gpio_handle[SSC_QUP]);
    if (GPIO_SUCCESS != gpio_res) {  QUP_LOG(LEVEL_ERROR, "qup_plat_init: GPIO_Attach Failed for ssc qup :0x%x",gpio_res);}

    get_rumi_platform();

    qup_detect_chip_version();

    ret_value = qup_pipe_init();
    if(QURT_EOK != ret_value)
    {
        QUP_LOG(LEVEL_ERROR, "qup_plat_init qup_pipe_init failed error %d", ret_value);
    }
#ifdef QUP_AXI_HW_BUG
    if (qup_get_chip_version() == CHIP_VERSION_R1)
    {
        qup_smem_init();
    }
#endif

}

