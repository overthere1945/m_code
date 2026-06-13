/**
    @file  qup_config.c
    @brief Device Tree implementaion for Qup configs
 */
/*
===============================================================================

                               Edit History

$Header:

when       who     what, where, why
--------   ---     ------------------------------------------------------------
04/07/25   GKR     Changed EXCLUSIVE_SE to SHARED_SE
02/03/25   GKR     I2C HS mode support
12/10/24   JAY     Changes to Read is_pipeline_enable DT property in SE CFG
10/07/24   GKR     Changes to Read log level DT property in SE CFG
09/09/24   MG      Added Changes to support and read TOP QUP I3C Configuration
07/29/24   GKR     Added Changes to vote against big image sleep
06/26/24   GKR     CLean up & Multi SSC QUP Support
                   breakdown of qup_get_hw_cfg() api to smaller apis
                   Qupv3 TCSR migration from devcfg to dtsi
*/
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/


/*==================================================================================================
                                           INCLUDE FILES
==================================================================================================*/

#include <stdio.h>
#include <stringl/stringl.h>

#include "qup_alloc.h"
#include "qup_devcfg.h"
#include "qup_os.h"
#include "qup_ssc_mem.h"

#include "DTBExtnLib.h"
#include "ClockImage.h"

/*==================================================================================================
                                             CONSTANTS
==================================================================================================*/
#define NODE_NAME       "/soc/"

/*==================================================================================================
                                              TYPEDEFS
==================================================================================================*/

typedef struct se_cfg_lookup
{
    se_dev_cfg  *cfg_pointer;
}se_cfg_lookup;

#define DT_CHECK_ERROR(x)                                             \
{                                                                     \
    if (ret_value) {                                                  \
      QUP_LOG(LEVEL_ERROR, "%s, ret Value = %d\n", x, ret_value);     \
      goto ON_EXIT;                                                   \
    }                                                                 \
}

/*==================================================================================================
                                          LOCAL VARIABLES
==================================================================================================*/
ibi_dev_cfg*              ibi_dev_cfg_list[QUP_MAJOR_MAX][MAX_IBI_INSTANCE];
qup_dev_cfg*              qup_cfg_list[QUP_MAX] = {NULL};
static se_cfg_lookup      devcfg_lookup[QUP_MAX][MAX_SE_PER_QUP] = {0};

uint8                     qup_num_se[QUP_MAX] = {0};

#ifdef QUP_SDC_PROC_SUPPORT

/* stores IBI dev cfg list being used by SDC PROC for PDR & SSR Cleanup */
ibi_dev_cfg*              ssc_sdc_ibi_dcfg_list[MAX_IBI_INSTANCE];
uint32                    ssc_sdc_ibi_dcfg_list_size = 0;

#endif
/*==================================================================================================
                                         EXTERNAL VARIABLES
==================================================================================================*/
extern ClockHandle           qup_clock_handle;
extern GPIOClientHandleType  qup_gpio_handle[];
/*==================================================================================================
                                          GLOBAL FUNCTIONS
==================================================================================================*/

qup_dev_cfg* qup_get_common_dev_cfg (QUP_TYPE qup_type)
{
    qup_dev_cfg* qcfg = NULL;
    char qup_name_str[30] = {0};
    volatile uint64_t phy_address = 0x0;
    uint64_t reg_size=0;
    uint32 offset = 0;
    int ret_value = 0;

    qcfg = qup_cfg_list[qup_idx_offset[GET_QUP_MAJOR(qup_type)]+GET_QUP_MINOR(qup_type)];

    if (qcfg) goto ON_EXIT;

    qcfg = (qup_dev_cfg *)qup_se_dev_cfg_mem_alloc(qup_type,sizeof(qup_dev_cfg),QUP_CONFIG_ISLAND);

    if (qcfg == NULL)
    {
        QUP_LOG(LEVEL_ERROR, "qup_se_dev_cfg_mem_alloc returned NULL for qcfg !!!");
        goto ON_EXIT;
    }

    snprintf(qup_name_str, 30, "%s%s_QUP_%d",NODE_NAME,QUP_name[GET_QUP_MAJOR(qup_type)],GET_QUP_MINOR(qup_type));
    ret_value = fdt_get_node_handle(&(qcfg->qup_node), NULL, qup_name_str);
    DT_CHECK_ERROR("Unable to get QUP device config handle");

    /* Get handles for the core and ahb clocks - type ClockIdType. */
    ret_value = Clock_GetIdDT(qup_clock_handle, &(qcfg->qup_node), "core2x", &qcfg->common_clk_id[QUP_CORE_2X_CLOCK_IDX]);
    DT_CHECK_ERROR("Unable to get clock ID for core 2x");
    ret_value = Clock_GetIdDT(qup_clock_handle, &(qcfg->qup_node), "core", &qcfg->common_clk_id[QUP_CORE_CLOCK_IDX]);
    DT_CHECK_ERROR("Unable to get clock ID for core x");
    ret_value = Clock_GetIdDT(qup_clock_handle, &(qcfg->qup_node), "s-ahb", &qcfg->common_clk_id[QUP_S_AHB_CLOCK_IDX]);
    DT_CHECK_ERROR("Unable to get clock ID for s-ahb");
    ret_value = Clock_GetIdDT(qup_clock_handle, &(qcfg->qup_node), "m-ahb", &qcfg->common_clk_id[QUP_M_AHB_CLOCK_IDX]);
    DT_CHECK_ERROR("Unable to get clock ID for m-ahb");

    ret_value = fdt_get_uint8_prop(&(qcfg->qup_node), "qup_id", (uint8 *)&qcfg->qup_id);
    DT_CHECK_ERROR("Unable to get qup_id");

    ret_value = fdt_get_reg(&(qcfg->qup_node), NULL, 0,SIZE_32,SIZE_32,(uint64_t *)&phy_address,&reg_size);
    DT_CHECK_ERROR("Unable to get reg address");

    qcfg->core_base_addr = (uint8 *)qup_convert_hw_phy_to_vir_addr((void*)phy_address);
    if (qcfg->core_base_addr == NULL)
    {
        ret_value = -1;
        DT_CHECK_ERROR("qcfg->core_base_addr is NULL");
    }

    ret_value = fdt_get_uint32_prop(&(qcfg->qup_node), "qup_common_offset", (uint32 *)&offset);
    DT_CHECK_ERROR("Unable to get qup_common_offset");

    qcfg->common_base_addr = qcfg->core_base_addr + offset;

    ret_value = fdt_get_uint32_prop(&(qcfg->qup_node), "se_wrapper_base_offset", (uint32 *)&offset);
    DT_CHECK_ERROR("Unable to get se_wrapper_base_offset");

    qcfg->se_wrapper_base_addr = qcfg->core_base_addr+offset;

    ret_value = fdt_get_uint32_prop(&(qcfg->qup_node), "core_frequency", (uint32 *)&qcfg->core_freq_hz);
    DT_CHECK_ERROR("Unable to get core_frequency");

    ret_value = fdt_get_uint32_prop(&(qcfg->qup_node), "qup_flags", (uint32 *)&qcfg->flags);
    DT_CHECK_ERROR("Unable to get qup_flags");

    ret_value = fdt_get_uint8_prop(&(qcfg->qup_node), "num_se", &qcfg->num_se);
    DT_CHECK_ERROR("Unable to get num_se");

    // Same Qup Common Cfg can be reused for all the SEs
    qup_cfg_list[qup_idx_offset[GET_QUP_MAJOR(qup_type)]+GET_QUP_MINOR(qup_type)] = qcfg;

ON_EXIT:
    if (ret_value)
    {
        if (qcfg != NULL)
        {
            qup_se_dev_cfg_mem_free(qcfg, qup_type,sizeof(qup_dev_cfg), QUP_CONFIG_ISLAND);
        }
    }
    return qcfg;
}

static boolean qup_read_se_dt_cfg (se_dev_cfg *cfg)
{
    fdt_node_handle hNode = cfg->se_node;
    uint32 offset = 0;
    int ret_value = 0;
    uint8_t shared_se = 0;

    ret_value = fdt_get_uint32_prop(&hNode, "se_flags", (uint32 *)&cfg->flags);
    DT_CHECK_ERROR("Unable to get se_flags");

    ret_value = fdt_get_uint8_prop(&hNode, "se_index", (uint8 *)&cfg->se_id);
    DT_CHECK_ERROR("Unable to get se_index");

    ret_value = fdt_get_uint8_prop(&hNode, "protocol_supported", (uint8 *)&cfg->protocols_supported);
    DT_CHECK_ERROR("Unable to get protocol_supported");

    ret_value = fdt_get_uint8_prop(&hNode, "interface_supported", (uint8 *)&cfg->iface_data.interface_supported);
    DT_CHECK_ERROR("Unable to get interface_supported");

    ret_value = fdt_get_uint16_prop(&hNode, "core_irq", (uint16 *)&cfg->iface_data.core_irq);
    DT_CHECK_ERROR("Unable to get core_irq");

    ret_value = fdt_get_uint32_prop(&hNode, "pdc_irq", (uint32 *)&cfg->iface_data.pdc_irq);
    DT_CHECK_ERROR("Unable to get pdc_irq");

    ret_value = fdt_get_uint32_prop(&hNode, "parent_wakeup_gpio", (uint32 *)&cfg->iface_data.wakeup_gpio);
    DT_CHECK_ERROR("Unable to get parent_wakeup_gpio");

    ret_value = fdt_get_uint8_prop(&hNode, "num_gpiis", (uint8 *)&cfg->gpi_data.num_gpi_idx);
    DT_CHECK_ERROR("Unable to get num_gpiis");

    ret_value = fdt_get_uint8_prop_list(&hNode, "gpii_list", (void *)&(cfg->gpi_data.gpi_idx[0]), MAX_GPI_IDX);
    DT_CHECK_ERROR("Unable to get gpii_list");

    ret_value = fdt_get_uint8_prop(&hNode, "ring_size_multiplier", (uint8 *)&cfg->gpi_data.ring_size_multiplier);
    DT_CHECK_ERROR("Unable to get ring_size_multiplier");

    /* Get handles for the SE - type ClockIdType. */
    ret_value = Clock_GetIdDT(qup_clock_handle, &hNode, "se-clk", &cfg->se_clk_id);
    DT_CHECK_ERROR("Unable to get clock ID for se-clk");

    ret_value = fdt_get_uint32_prop(&hNode, "core_offset", (uint32 *)&offset);
    DT_CHECK_ERROR("Unable to get core_offset");

    if(qup_detect_rumi_platform() == FALSE)
    {
        ret_value = fdt_get_uint32_prop(&hNode, "i2c_hs_i3c_src_freq", (uint32 *)&cfg->se_src_frequency);
        DT_CHECK_ERROR("Unable to get i2c_hs_i3c_src_freq");

        ret_value = fdt_get_uint32_prop(&hNode, "od_frequency", (uint32 *)&cfg->od_freq);
        DT_CHECK_ERROR("Unable to get od_frequency");
    }
    else
    {
        cfg->se_src_frequency = I2C_HS_I3C_SE_RUMI_SRC_FREQ;
        cfg->od_freq = I3C_RUMI_OD_FREQ;
    }

    cfg->se_base_addr = cfg->qup_data->se_wrapper_base_addr + offset;

    ret_value = fdt_get_uint8_prop(&hNode, "log_level", (uint8 *)&cfg->log_level);
    if(ret_value || cfg->log_level > QUP_ALL_LOG_LEVELS)
    {
        // Enable Default log levels incase of DT error / node not found
        cfg->log_level = QUP_DEFAULT_LOG_LEVEL;
    }

    /* Check if SE shared accross EEs & Do the necessary */
    ret_value = fdt_get_uint8_prop(&hNode, "shared_se", (uint8 *)&shared_se);
    DT_CHECK_ERROR("Unable to get shared_se flag");

    if(shared_se)
    {
        QUP_FLAG_SET(cfg->flags,SHARED_SE);
    }

    ret_value = fdt_get_boolean_prop(&hNode, "is_pipeline_enable", (uint32 *)&cfg->is_pipeline_enable);
    DT_CHECK_ERROR("Unable to get is_pipeline_enable flag");

ON_EXIT:
    if (ret_value)
    {
        return FALSE;
    }

    return TRUE;
}

ibi_dev_cfg* qup_get_ibi_cfg(QUP_TYPE qup_type, uint8 ibi_instance)
{
    uint8 i = ibi_instance;
    int ret_value = 0;
    char str[30] = {'\0'};
    fdt_node_handle hNode;
    ibi_dev_cfg * ibi_dcfg = NULL;
    volatile uint64_t phy_address = 0x0;
    uint64_t reg_size=0;

    if(ibi_dev_cfg_list[GET_QUP_MAJOR(qup_type)][i] == NULL)   // check if ibi list is already populated
    {
        ibi_dcfg = (ibi_dev_cfg *) qup_se_dev_cfg_mem_alloc(qup_type, sizeof(ibi_dev_cfg),QUP_IBI_CONFIG_ISLAND);

        if (ibi_dcfg == NULL)
        {
            ret_value = -1;
            DT_CHECK_ERROR(" ERROR ibi_dcfg is NULL !!");
        }
        if(GET_QUP_MAJOR(qup_type) == SSC_QUP)
        {
            snprintf(str, 30, "/soc/ibi_ssc_%d_cfg",i);
        }
        else if(GET_QUP_MAJOR(qup_type)  == TOP_QUP)
        {
            snprintf(str, 30, "/soc/ibi_top_%d_cfg",i);
        }
        else
        {
            ret_value = -1;
            DT_CHECK_ERROR(" ERROR qup_type of ibi_dcfg is incorrect !!");
            goto ON_EXIT;
        }
        ret_value = fdt_get_node_handle(&hNode, NULL, str);
        DT_CHECK_ERROR("Unable to get QUP device config handle for getting ibi instance");

        ret_value = fdt_get_reg(&hNode, NULL, 0,SIZE_32,SIZE_32,(uint64_t *)&phy_address,&reg_size);
        DT_CHECK_ERROR("Unable to get IBI reg address");

        ibi_dcfg->core_base_addr = (uint8 *)qup_convert_hw_phy_to_vir_addr((void*)phy_address);
        if (ibi_dcfg->core_base_addr == NULL)
        {
            ret_value = -1;
            DT_CHECK_ERROR(" ERROR ibi_dcfg->core_base_addr is NULL !!");
        }

        ret_value = fdt_get_uint8_prop(&hNode, "ibi_id", (uint8 *)&(ibi_dcfg->ibi_id));
        DT_CHECK_ERROR("Unable to get ibi_id");

        ret_value = fdt_get_uint8_prop(&hNode, "flags", (uint8 *)&(ibi_dcfg->flags));
        DT_CHECK_ERROR("Unable to get ibi flags");

        ret_value = fdt_get_uint8_prop(&hNode, "gpii", (uint8 *)&(ibi_dcfg->gpii));
        DT_CHECK_ERROR("Unable to get ibi gpii");

        ret_value = fdt_get_uint32_prop(&hNode, "gpii_irq", (uint32 *)&(ibi_dcfg->gpii_irq));
        DT_CHECK_ERROR("Unable to get ibi gpii_irq");

        ret_value = fdt_get_uint32_prop(&hNode, "mgr_irq", (uint32 *)&(ibi_dcfg->mgr_irq));
        DT_CHECK_ERROR("Unable to get ibi mgr_irq");

        ibi_dev_cfg_list[GET_QUP_MAJOR(qup_type)][i] = ibi_dcfg;

    }

ON_EXIT:
    if (ret_value)
    {
        if (ibi_dcfg)
        {
            qup_se_dev_cfg_mem_free (ibi_dcfg, qup_type, sizeof(ibi_dev_cfg),QUP_IBI_CONFIG_ISLAND);
        }
        return NULL;
    }
    return ibi_dev_cfg_list[GET_QUP_MAJOR(qup_type)][i];
}

se_dev_cfg* qup_get_hw_cfg(QUP_TYPE qup_type, uint32 se_idx)
{
    char                 qup_name_str[30] = {'\0'};
    char                 se_name_str[100] = {'\0'};
    se_dev_cfg*          cfg=NULL;
    int                  ret_value = 0;
    fdt_node_handle      hNode;
    uint8_t              qup_index = 0;
    uint8_t              fifo_mode = 0;
    uint8_t              ibi_instance = 0xFF;

    if( GET_QUP_MAJOR(qup_type) >= QUP_MAJOR_MAX ||
        GET_QUP_MINOR(qup_type) >= qup_minor_max[GET_QUP_MAJOR(qup_type)] ||
        se_idx >= qup_num_se[qup_idx_offset[GET_QUP_MAJOR(qup_type)]+GET_QUP_MINOR(qup_type)])
    {
        QUP_LOG(LEVEL_ERROR," Not a Valid QUP : %x or SE :%d", qup_type, se_idx);
        return NULL;
    }

    qup_index =  qup_idx_offset[GET_QUP_MAJOR(qup_type)] + GET_QUP_MINOR(qup_type);

    if(devcfg_lookup[qup_index][se_idx].cfg_pointer == NULL)
    {
        /* Allocates memory for SE Dev Cfg */
        cfg = (se_dev_cfg *)qup_se_dev_cfg_mem_alloc(qup_type, sizeof(se_dev_cfg),QUP_SE_CONFIG_ISLAND);
        if(!cfg)
        {
            ret_value = -1;
            DT_CHECK_ERROR("Unable alloc memory for qup se cfg");
        }

        /* Allocates memory and parse QUP COMMON CFG */
        cfg->qup_data = qup_get_common_dev_cfg(qup_type);
        if(!cfg->qup_data)
        {
            ret_value = -1;
            DT_CHECK_ERROR("Unable alloc memory for qup common cfg");
        }


        /* Prepare Node name for SE DT Node */
        snprintf(qup_name_str, 30, "%s_QUP_%d",QUP_name[GET_QUP_MAJOR(qup_type)],GET_QUP_MINOR(qup_type));
        snprintf(se_name_str, 100, "%s%s/%s_SE_%lu",NODE_NAME,qup_name_str,qup_name_str,se_idx);

        ret_value = fdt_get_node_handle(&hNode, NULL, se_name_str);
        DT_CHECK_ERROR("Unable to get QUP_SE device config handle");

        cfg->se_node = hNode;

        /* Parse SE CFG from DT */
        if (FALSE == qup_read_se_dt_cfg(cfg))
        {
            ret_value = -1;
            DT_CHECK_ERROR("Reading DT Failed for SE");
        }



        qup_attach_platform_functions(cfg);
        cfg->protocol_in_use     = qup_common_get_fw_loaded(cfg);
        cfg->island_spec_in_use  = qup_common_get_island_spec(cfg);

        if((GET_PROTOCOL_MAJOR(cfg->protocol_in_use) == SE_PROTOCOL_INVALID) ||
           (GET_PROTOCOL_MAJOR(cfg->protocol_in_use) >= PROTOCOL_MAJOR_MAX))
        {
            QUP_ASSERT(cfg,QUP_FATAL_TYPE);        // ASSERT IF PROTOCOL is NOT VALID
        }
        /* Allocate & Parse IBI CTRL Cfg */
        if(cfg->protocol_in_use == SE_PROTOCOL_I3C_IBI)
        {
            ret_value = fdt_get_uint8_prop(&hNode, "ibi_instance", &ibi_instance);
            DT_CHECK_ERROR("Unable to get ibi_instance");
            if (ibi_instance != 0xFF)
            {
                cfg->ibi_data = qup_get_ibi_cfg(qup_type, ibi_instance);
                if (!cfg->ibi_data)
                {
                    ret_value = -1;
                    DT_CHECK_ERROR("Unable to get ibi devcfg");
                }
            }
        }
        /* GPIO Pinctrl Registrations */
        if(TRUE != qup_gpio_register_io(cfg))     //register gpio's
        {
            ret_value = -1;
            DT_CHECK_ERROR("GPIO registration failed");
        }

        if (QUP_IS_SET(cfg->flags, ENABLE_VOTE_AGAINST_ALL_SLEEP))
        {
            if (FALSE == qup_register_npa_client(cfg))
            {
                ret_value = QUP_ERROR_PLATFORM_RESOURCE_SETUP_FAIL;
                DT_CHECK_ERROR("qup npa registration failed");
            }
        }

        if(QUP_IS_NOT_SET(cfg->flags,SHARED_SE))  /* if not shared SE optimize TREs */
        {
            if((GET_PROTOCOL_MAJOR(cfg->protocol_in_use) != SE_PROTOCOL_I3C))
            {
                QUP_FLAG_SET(cfg->flags,OPTIMISE_TRANSFERS);
            }
        }
        else
        {
#ifdef QUP_SDC_PROC_SUPPORT
            if (GET_QUP_MAJOR(cfg->qup_data->qup_id) == SSC_QUP && cfg->protocol_in_use == SE_PROTOCOL_I3C_IBI)
            {
                ssc_sdc_ibi_dcfg_list[ssc_sdc_ibi_dcfg_list_size] = cfg->ibi_data;
                ssc_sdc_ibi_dcfg_list_size++;
            }
#endif
        }

        ret_value = fdt_get_uint8_prop(&hNode, "FIFO_MODE", (uint8 *)&fifo_mode);
        DT_CHECK_ERROR("Unable to get FIFO_MODE flag");

        if(fifo_mode && GET_PROTOCOL_MAJOR(cfg->protocol_in_use) == SE_PROTOCOL_I2C)
        {
            QUP_FLAG_SET(cfg->flags,ENABLE_FIFO_MODE);
        }

/*      Below flags settings are redudant for IBI CTRL only required for legacy IBI
        if(GET_PROTOCOL_MAJOR(cfg->protocol_in_use) == SE_PROTOCOL_I2C || GET_PROTOCOL_MAJOR(cfg->protocol_in_use) == SE_PROTOCOL_I3C )
        {
            if(qup_detect_rumi_platform())
            {
                QUP_FLAG_UNSET(cfg->qup_data->flags,IBI_AUTO_PROCESS_WA_ENABLE);
                QUP_FLAG_SET(cfg->flags,STRETCH_CLK_POST_IBI);
            }
        }
*/
        if(GET_PROTOCOL_MAJOR(cfg->protocol_in_use) == SE_PROTOCOL_I3C)
        {
            cfg->force_default_reg_write =  TRUE;
        }

        devcfg_lookup[qup_index][se_idx].cfg_pointer = cfg;

    }

ON_EXIT:
    if (ret_value)
    {
        if(cfg)
        {
            qup_se_dev_cfg_mem_free(cfg, qup_type, sizeof(se_dev_cfg),QUP_SE_CONFIG_ISLAND);

        }
    }
    return (se_dev_cfg *)devcfg_lookup[qup_index][se_idx].cfg_pointer;
}

qup_tcsr_config* qup_get_tcsr_config(void)
{
    static qup_tcsr_config* tcsr_cfg_ptr = NULL;
    static qup_tcsr_config qup_tcsr_cfgs[MAX_QUP_TCSR_CFGS];

    fdt_node_handle hNode;
    uint8 i,j, num_top_qups;
    uint32 size_in_bytes;
    int ret_value = 0;
    char tcsr_cfg_name[35] = "/soc/qup_tcsr_info/";
    char data_pattern[25] = {'\0'};

    if (tcsr_cfg_ptr) goto ON_EXIT;

    ret_value = fdt_get_node_handle(&hNode, NULL, tcsr_cfg_name);
    DT_CHECK_ERROR("Unable to get qup_tcsr_info cfg handle");

    ret_value = fdt_get_uint8_prop(&hNode, "num_top_qups", (uint8*)&num_top_qups);
    DT_CHECK_ERROR("Unable to get num_top_qups");

    for ( i = 0; i < MAX_QUP_TCSR_CFGS; i++)
    {
        snprintf(tcsr_cfg_name, 35, "/soc/qup_tcsr_info/tcsr_cfg%hhu",i);
        ret_value = fdt_get_node_handle(&hNode, NULL, tcsr_cfg_name);
        DT_CHECK_ERROR("Unable to get qup tcsr cfg handle");

        ret_value = fdt_get_prop_values_size_of_node(&hNode, &size_in_bytes);
        DT_CHECK_ERROR("Unable to get qup tcsr cfg node size");

        data_pattern[0] = 'w'; // one word for irq_num

        qup_memset(&data_pattern[1],'w', num_top_qups);

        ret_value = fdt_get_prop_values_of_node(&hNode, data_pattern, (void *)&qup_tcsr_cfgs[i], size_in_bytes);
        DT_CHECK_ERROR("Unable to Read qup tcsr data");

        for ( j = 0; j < num_top_qups; j++)
        {
            if (!qup_tcsr_cfgs[i].reg_info[j].addr) continue;
            qup_tcsr_cfgs[i].reg_info[j].addr = QUP_TCSR_PA_TO_VA(qup_tcsr_cfgs[i].reg_info[j].addr);
        }
    }

    tcsr_cfg_ptr = &qup_tcsr_cfgs[0];

ON_EXIT:
    return tcsr_cfg_ptr;
}

int qup_config_tcsr(QUP_TYPE qup, TCSR_IRQ_TYPE type, uint8 idx)
{
    qup_tcsr_config                *cfg_ptr=NULL;
    uint8                           qup_index = qup_idx_offset[GET_QUP_MAJOR(qup)] + GET_QUP_MINOR(qup);

    cfg_ptr = qup_get_tcsr_config();

    if(cfg_ptr != NULL)
    {

        while(cfg_ptr->irq_num)
        {
            if(cfg_ptr->used == FALSE)
            {
                if(cfg_ptr->reg_info[qup_index].addr)
                {
                    if(type == QUP_SE_IRQ)
                    {
                        out_dword(cfg_ptr->reg_info[qup_index].addr,(1 << (cfg_ptr->reg_info[qup_index].se_bit_offset + idx)));
                    }
                    else
                    {
                        return -1;
                    }
                    cfg_ptr->used = TRUE;
                    return cfg_ptr->irq_num;
                }
            }
            cfg_ptr++;
        }
    }
    return -1;
}

boolean config_qup_io(se_dev_cfg  *cfg, uint8 io_idx)
{
    return TRUE;
}

boolean qup_config_get_info(uint32 instance, QUP_TYPE *qup_type, uint32 *se_idx)
{
    uint8 core_idx,i,j,se_offset,curr_idx = 0,curr_delta = 0;

    if (instance > 0)
    {
        core_idx = instance - 1;
        for (i = 0; i < QUP_MAJOR_MAX; i++)
        {
            for (j = 0; j < qup_minor_max[i]; j++)
            {
                se_offset = qup_num_se[qup_idx_offset[i] + j];
                curr_idx += se_offset;
                if(core_idx < curr_idx)
                {
                    *qup_type = SET_QUP_TYPE(i,j);
                    *se_idx = core_idx - curr_delta;
                    return TRUE;
                }
                curr_delta += se_offset;
            }
        }
    }
    return FALSE;
}


/*** This API is invoked from RC-INIT   ***/
void qup_config_init(void)
{

    uint8                           i,j;
    char                            str[30];
    fdt_node_handle                 hNode;
    int ret_value =  -1;

    for (i = 0; i < QUP_MAJOR_MAX; i++)
    {
        for (j = 0; j < qup_minor_max[i]; j++)
        {
             snprintf(str, 30, "/soc/%s_QUP_%d",QUP_name[i],j);
             ret_value = fdt_get_node_handle(&hNode, NULL, str);

             if (!ret_value)
             {
                 ret_value = fdt_get_uint8_prop(&hNode, "num_se", (uint8 *)&qup_num_se[qup_idx_offset[i] + j]);
             }
        }
    }
}

boolean qup_get_qup_se_instance(void *handle, QUP_TYPE *qup_type, uint32 *se_idx)
{

    qup_client_ctxt  *c_ctxt = (qup_client_ctxt *) handle;
    qup_hw_ctxt      *h_ctxt = NULL;
    se_dev_cfg       *dcfg = NULL;

    if (c_ctxt ==  NULL) return FALSE;

    h_ctxt = c_ctxt->h_ctxt;
    if (h_ctxt == NULL) return FALSE;

    dcfg   = (se_dev_cfg *) h_ctxt->core_config;

    *qup_type = dcfg->qup_data->qup_id;
    *se_idx = dcfg->se_id;

    return TRUE;
}
