/**************************************************************************
 * @file     lpai_bt_state_mgr.c
 * @brief    LPAI BT State Manager Source file.
 *           It contains handling for BT ON and OFF Sequences
 *
 * Copyright (c) Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
******************************************************************************/

/*=========================================================================== 
                        INCLUDE FILES
===========================================================================*/
#include <stdbool.h>
#include "lpai_bt_state_mgr.h"
#include "lpai_bt_le_adv.h"
#include "lpai_bt_le_scan.h"
#include "lpai_bt_rfcomm_app.h"
#include "lpai_bt_ble_demo_app.h" /* add hyungchul : HLOS offload LECoC Hello World demo 상태 제어 */

extern appMgrContext_t appMgrCtx;

/**
 * @brief Method to perform unregistration of microapps once BT Off Status is received from ADSP
 * @param[in] None
 * @return None
 */
void handle_bt_off()
{
    printk("Deinit All Profiles \n");
    le_adv_deinit();
    le_scan_deinit();
    lecoc_app_deinit();
    rfcomm_app_deinit();
#ifdef CONFIG_LPAI_BTSW_ENABLE_GATT_OFFLOAD_GHEADER
    extern void gatt_app_deinit();
    gatt_app_deinit();
    extern void ancs_deinit();
    ancs_deinit();
#endif
}

bt_status_t lpai_bt_get_status(void)
{
    return appMgrCtx.btStatus;
}

void lpai_bt_status_evt(bt_status_t state)
{
    /*Store the Bt State for future use*/
    appMgrCtx.btStatus = state;
    printk("BT Status Received is %d\n", state);
    if(state == BT_STATUS_ON)
    {
        printk("BT Turned ON ! \n");

        /* add hyungchul :
         * Qualcomm Micro Stack ADV 문서 기준 AWM ADV는 connectable ADV를 지원하지 않는다.
         * 따라서 여기서는 AWM Advertising을 시작하지 않고, HLOS에서 offload된 LECoC socket open을 기다린다.
         */
        ble_demo_app_bt_on();

        #ifdef CONFIG_LPAI_BTSW_ENABLE_GATT_OFFLOAD_GHEADER
            extern void gatt_mgr_handle_bt_on_evt();
            gatt_mgr_handle_bt_on_evt();
        #endif
    }
    else
    {
        printk("BT Turned OFF ! \n");

        /* add hyungchul : BT OFF 시 Hello World 주기 송신을 중단한다. */
        ble_demo_app_bt_off();

        handle_bt_off();
    }

}
