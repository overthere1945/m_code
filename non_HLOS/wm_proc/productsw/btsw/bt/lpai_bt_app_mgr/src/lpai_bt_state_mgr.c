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
#include <zephyr/kernel.h>
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

static struct k_work_delayable highcam_work2;
static bool is_highcam_work_initialized2 = false;


typedef enum
{
     PMIC_PON_BUTTON,         /**< PM_PON_Button */
     PMIC_PON_RESIN_BUTTON,   /**< PON resin Button*/
     PMIC_BUTTON_1,           /**< PM button1*/
     PMIC_BUTTON_2,           /**< PM button2*/
     PMIC_BUTTON_3,           /**< PM button3*/
     PMIC_BUTTON_MAX          /**< PM button Max*/
 } api_PM_Button_Instance_t;
 typedef enum
 {
     PMIC_BUTTON_PRESS = 1,   /**< PM button Press */
     PMIC_BUTTON_RELEASE,     /**< PM button Release*/
     PMIC_BUTTON_INVALID_EVENT,/** PM Event notification for invalid event */
 } api_PM_Button_Event_t;
 
extern void disp_button_cb(api_PM_Button_Instance_t btn_instance,
                           api_PM_Button_Event_t event_type,
                           uint64_t evt_timestamp_ms);

/**
 * @brief 가상으로 파워 버튼 누름 이벤트를 생성하여 던지는 함수
 */
void trigger_virtual_power_button_press2(void)
{
    // 1. 버튼 인스턴스 지정 (보통 파워 버튼은 0번이거나 고정된 enum 값이 있습니다)
    // 예: API_PM_BUTTON_PWR_BTN 등 프로젝트 헤더에 맞는 값 대입
    api_PM_Button_Instance_t fake_btn = (api_PM_Button_Instance_t)PMIC_BUTTON_2;

    // 2. 이벤트 타입 지정 (단순 누름: PRESS / RELEASE 등 헤더 확인 필요)
    api_PM_Button_Event_t fake_event = (api_PM_Button_Event_t)PMIC_BUTTON_PRESS;

    // 3. 현재 타임스탬프 획득 (Zephyr 커널 시간 활용)
    // k_uptime_get()은 ms 단위의 현재 업타임을 반환합니다.
    uint64_t current_ts_ms = (uint64_t)k_uptime_get();

    printk("[PERIODIC] Input Highcam Key\n");

    // 4. 원래의 콜백 함수를 강제로 호출하여 디스플레이 매니저 쓰레드로 전달
    disp_button_cb(fake_btn, fake_event, current_ts_ms);
}

/**
 * @brief 10초마다 주기적으로 현재 BT Status를 printk로 출력하는 워크 핸들러
 */
static void highcam_work_handler2(struct k_work *work)
{
    //printk("[PERIODIC] highcam_work_handler\n");
    //disp_button_notify_to_apps(0x2);
    trigger_virtual_power_button_press2();

    // 10초(K_MSEC(10000)) 후에 다시 자기 자신을 스케줄링 (무한 반복)
    k_work_reschedule(&highcam_work2, K_MSEC(10000));
}

void start_periodic_highcam2(void)
{
    if (!is_highcam_work_initialized2) {
        k_work_init_delayable(&highcam_work2, highcam_work_handler2);
        is_highcam_work_initialized2 = true;
    }
    // 호출 즉시 10초 뒤로 스케줄링 시작
    k_work_reschedule(&highcam_work2, K_MSEC(10000));
    printk("Periodic highcam_work Started!!\n");
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

        start_periodic_highcam2();
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
