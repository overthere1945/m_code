/**
    @file  spi_plat_island.c
    @brief platform image specific implementation
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#include "spi_plat.h"
#include "qup_common_internal.h"
#include "ClockDefs.h"
#include "qup_alloc.h"
#include "qup_os.h"
#include "qup_plat.h"

// static uint32 num_entries;
// static ClockFreqPlanType  freq_list_local[MAX_FREQ_PLAN_TABLE_SIZE] ={0};

boolean spi_plat_get_div_dfs (se_dev_cfg  *cfg, uint32 frequency_hz, uint32* dfs_index, uint32* div)
{
    uint32 min_error=0xFFFFFFFF;
    uint8 min_index=0xFF;
    uint32 mod = 0,i = 0;
    uint32 max_div_val = 4095;
    
    
    if(!cfg->freq_plan->num_entries)
    {
        QUP_LOG(LEVEL_ERROR,"spi_plat_get_div_dfs : local freq list unpopulated, call setup_lpi_resource");
        return FALSE; 
    }
    
    for (i = 0; i < cfg->freq_plan->num_entries; i++)
    {
    mod = cfg->freq_plan->freq_list_local[i].nFreqHz % frequency_hz;
    if (mod != 0)
    {
        mod = frequency_hz - mod;
    }
    if (mod < min_error)
    {
        min_error = mod;
        min_index = i;
    }
    }
    
    if(min_index > MAX_FREQ_PLAN_TABLE_SIZE)
    {
        QUP_LOG(LEVEL_ERROR,"spi_plat_get_div_dfs : Calc value for DFS is more than no of DFS entries");
        return FALSE;
    }
    
    *dfs_index = min_index;
    *div = (cfg->freq_plan->freq_list_local[min_index].nFreqHz / frequency_hz);
    *div += (min_error == 0) ? 0 : 1;
    
    /*IN GSI the CLK divider value is only 12 bits, 
    so forcing max value as divider so that zero is not configured*/
    if((*div) > max_div_val )
    {
        *div = max_div_val;
    }
    return TRUE;
}


uint32 spi_plat_clock_get_frequency 
(
   se_dev_cfg *config,
   uint32 frequency_hz
)
{

    uint32 min_error=0xFFFFFFFF;
    uint8 min_index=0xFF;
    uint32 mod = 0,i = 0;
    
	if(!config->freq_plan)
	{
	    config->freq_plan = qup_mem_alloc(config, sizeof(se_clk_dfs_config), QUP_SE_DFS_CONFIG_ISLAND);
		if(config->freq_plan == NULL)
	    {
			QUP_LOG(LEVEL_ERROR,"spi_plat_clock_get_frequency : FALSE 1");
	    	return 0;
	    }
	}
	
	if(config->freq_plan->num_entries == 0)
	{
        config->freq_plan->num_entries = qup_get_se_clock_plan_copy(config, MAX_FREQ_PLAN_TABLE_SIZE);
    }
	
    
    if(config->freq_plan->num_entries == 0)
    {
        return 0;
    }
    
    
    for (i = 0; i < config->freq_plan->num_entries; i++)
    {
    
    mod = config->freq_plan->freq_list_local[i].nFreqHz % frequency_hz;
    if (mod != 0)
    {
        mod = frequency_hz - mod;
    }
    if (mod < min_error)
    {
        min_error = mod;
        min_index = i;
    }
    }
    
    return config->freq_plan->freq_list_local[min_index].nFreqHz;
}

spi_timestamp_type spi_plat_get_timestamp_supported(void)
{
    if (qup_common_get_hw_version() >= 0x30000000)
    {
        return SPI_TIMESTAMP_64_BIT;
    }
    else
    {
        return SPI_TIMESTAMP_32_BIT_LEGACY;
    }
}

