/**************************************************************************
 * @file     lpai_bt_ble_demo_app.c
 * @brief    HLOS에서 offload된 LECoC socket을 이용해 wm_proc(AWM)에서
 *           1초마다 "Hello World\n"를 송신하는 데모 app 구현.
 *
 * 목적 및 기능:
 * - BT ON 시 AWM Advertising을 시작하지 않고 HLOS LECoC socket offload를 기다린다.
 * - ADSP가 UAPP_OPEN_SOCKET_REQ를 보내면 socketId를 저장한다.
 * - 저장된 socketId로 qapi_bt_lecoc_send_data()를 사용해 "Hello World\n"를 주기 송신한다.
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>


#include "lpai_bt_ble_demo_app.h"
#include "lpai_bt_lecoc_app.h"
#include "qapi_bt_lecoc_app.h"
#include "car_img.h"

#define BLE_DEMO_TX_PERIOD_MS              100U
#define BLE_DEMO_TX_SLOW_PERIOD_MS         100U
#define BLE_DEMO_TX_RETRY_MS               10U
#define BLE_DEMO_CAR_TX_MAX_SDU_LEN        960U
#define BLE_DEMO_LOG_PREFIX                "[BLE_DEMO]"
#define BLE_DEMO_FRAGMENT_LOG_INTERVAL      32U

/* change-20260615-hyungchul :
 * ADSP와 WM이 공유하는 TCM image control block 위치와 상태값을 정의한다.
 * ADSP가 JPG를 쓰는 중인지, WM이 BLE로 읽는 중인지 알 수 없어 단일 TCM buffer가 덮어써지는 문제를 막기 위해 추가한다. */
#define LGE_TCM_CTRL_OFFSET                0x1FF00
#define LGE_TCM_IMAGE_CTRL_MAGIC           0x4C474549U
#define LGE_TCM_IMAGE_STATE_IDLE           0U
#define LGE_TCM_IMAGE_STATE_WRITING        1U
#define LGE_TCM_IMAGE_STATE_READY          2U

typedef struct lge_tcm_image_ctrl
{
    uint32_t magic;
    uint32_t write_state;
    uint32_t jpg_len;
    uint8_t  reader_busy;
    uint8_t  reserved[3];
    /* change-20260615-hyungchul :
     * 기존 BLE control byte가 TCM offset 0x1FF10을 계속 사용할 수 있도록 4 byte를 예약하고, image_seq는 그 뒤 offset 0x1FF14에 배치한다.
     * image_seq 추가로 reader_busy나 BLE control offset이 바뀌면 ADSP/WM 기존 제어 경로가 깨질 수 있기 때문에 추가한다. */
    uint8_t  ble_control_reserved;
    uint8_t  reserved2[3];
    /* change-20260615-hyungchul :
     * ADSP가 READY image를 publish할 때마다 증가시키는 frame sequence를 WM도 같은 layout으로 읽는다.
     * JPG size가 같은 연속 이미지도 새 이미지로 구분하기 위해 추가한다. */
    uint32_t image_seq;
} lge_tcm_image_ctrl_t;

#ifndef BLE_DEMO_A_CAR_JPG_LEN
#define BLE_DEMO_A_CAR_JPG_LEN             ((uint32_t)sizeof(a_car_jpg))
#endif
#ifndef BLE_DEMO_B_CAR_JPG_LEN
#define BLE_DEMO_B_CAR_JPG_LEN             ((uint32_t)sizeof(b_car_jpg))
#endif

/*
 * CJPG fragment header format, little-endian host order.
 * Receiver can reassemble fragments by image_id/frag_idx and strip this header
 * before JPEG decode. Set BLE_DEMO_USE_CJPG_HEADER to 0 if the receiver expects
 * raw JPEG bytes only and already has another framing method.
 */
#define BLE_DEMO_USE_CJPG_HEADER           0U
#define BLE_DEMO_CJPG_MAGIC                0x47504A43U /* bytes: 'C' 'J' 'P' 'G' */
#define BLE_DEMO_CJPG_FLAG_FIRST           0x0001U
#define BLE_DEMO_CJPG_FLAG_LAST            0x0002U
#define BLE_DEMO_CJPG_FLAG_IMAGE_B         0x0004U

typedef struct __attribute__((packed)) ble_demo_cjpg_hdr
{
    uint32_t magic;
    uint32_t image_id;
    uint32_t total_len;
    uint32_t offset;
    uint16_t chunk_len;
    uint16_t frag_idx;
    uint16_t frag_count;
    uint16_t flags;
} ble_demo_cjpg_hdr_t;

typedef struct ble_demo_image
{
    const char *name;
    const uint8_t *data;
    uint32_t len;
    bool is_b_image;
} ble_demo_image_t;

typedef struct ble_demo_ctx
{
    bool initialized;
    bool bt_on;
    bool socket_open;
    bool tx_in_flight;
    bool image_active;
    bool send_b_next;
    /* change-20260615-hyungchul :
     * 현재 BLE demo app이 TCM image reader_busy를 획득했는지 기억한다.
     * 전송 완료, BT OFF, socket close, error path에서 reader_busy를 반드시 해제하기 위해 추가한다. */
    bool tcm_reader_busy;
    uint64_t socket_id;
    uint16_t remote_mtu;
    uint16_t max_sdu_len;
    uint32_t current_period_ms;
    uint32_t image_seq;
    /* change-20260615-hyungchul :
     * 마지막으로 BLE 전송을 시작한 ADSP TCM image_seq를 저장한다.
     * 기존 JPG size 비교 대신 sequence 비교로 새 이미지를 판단하기 위해 추가한다. */
    uint32_t last_tcm_image_seq;
    uint32_t completed_image_count;
    const ble_demo_image_t *active_image;
    uint32_t active_image_len;
    /*change-20260617-hyungchul
     * 수정한 이유 : BLE LECoC 전송 완료 시 실제 qapi_bt_lecoc_send_data()로 요청한 SDU frame byte 기준 전송 속도를 계산할 수 있는 누적값이 없었다.
     * 수정한 코드의 목적 : image 하나를 전송하는 동안 성공적으로 queue된 LE CoC frame 길이를 누적하여 JPG payload 기준 속도와 SDU frame 기준 속도를 함께 log로 출력한다. */
    uint32_t active_tx_frame_bytes_sent;
    uint32_t active_offset;
    uint16_t active_frag_idx;
    uint16_t active_frag_count;
    int64_t active_start_ms;
} ble_demo_ctx_t;

static ble_demo_ctx_t g_ble_demo_ctx;
static uint8_t g_ble_demo_tx_buf[BLE_DEMO_CAR_TX_MAX_SDU_LEN];

static void ble_demo_tx_work_handler(struct k_work *work);
static void ble_demo_schedule_next_image(uint32_t delay_ms);
static void ble_demo_start_next_image(void);
static void ble_demo_send_next_fragment_or_finish(void);
static void ble_demo_finish_active_image(void);
static uint16_t ble_demo_calc_max_sdu_len(uint16_t remote_mtu);
static uint16_t ble_demo_calc_payload_per_fragment(void);
static uint16_t ble_demo_calc_frag_count(uint32_t image_len, uint16_t payload_per_fragment);
static uint32_t ble_demo_update_period_from_elapsed(int64_t elapsed_ms);
/* change-20260615-hyungchul :
 * TCM reader_busy를 획득/해제할 때 사용할 helper prototype을 추가한다.
 * 여러 종료 경로에서 같은 해제 동작을 반복하지 않고 누락을 막기 위해 추가한다. */
static void ble_demo_shared_mem_barrier(void);
static void ble_demo_release_tcm_reader(void);

K_WORK_DELAYABLE_DEFINE(g_ble_demo_tx_work, ble_demo_tx_work_handler);

static const ble_demo_image_t g_ble_demo_images[2] =
{
    {
        .name = "a_car_jpg",
        .data = (const uint8_t *)a_car_jpg,
        .len = BLE_DEMO_A_CAR_JPG_LEN,
        .is_b_image = false,
    },
    {
        .name = "b_car_jpg",
        .data = (const uint8_t *)b_car_jpg,
        .len = BLE_DEMO_B_CAR_JPG_LEN,
        .is_b_image = true,
    },
};
void ble_demo_app_init(void)
{
    memset(&g_ble_demo_ctx, 0, sizeof(g_ble_demo_ctx));
    g_ble_demo_ctx.initialized = true;
    g_ble_demo_ctx.current_period_ms = BLE_DEMO_TX_PERIOD_MS;

    printk(BLE_DEMO_LOG_PREFIX " init complete, a_car_jpg=%u bytes, b_car_jpg=%u bytes\n",
           BLE_DEMO_A_CAR_JPG_LEN,
           BLE_DEMO_B_CAR_JPG_LEN);
}

void ble_demo_app_bt_on(void)
{
    if (g_ble_demo_ctx.initialized == false)
    {
        ble_demo_app_init();
    }

    g_ble_demo_ctx.bt_on = true;

    /* change(add)-hyungchul-20260511-0001:
     * Qualcomm Micro Stack ADV/SCAN API 문서상 connectable ADV는 지원하지 않는다.
     * 따라서 AWM ADV를 시작하지 않고 HLOS LECoC socket offload를 기다린다.
     */
    printk(BLE_DEMO_LOG_PREFIX " BT ON, wait HLOS LECoC socket offload\n");
}

void ble_demo_app_bt_off(void)
{
    (void)k_work_cancel_delayable(&g_ble_demo_tx_work);

    /* change-20260615-hyungchul :
     * BT OFF로 전송이 중단될 때 WM이 잡고 있던 TCM reader_busy를 해제한다.
     * reader_busy가 1로 남아 ADSP가 이후 JPG를 TCM에 쓰지 못하는 deadlock을 막기 위해 추가한다. */
    ble_demo_release_tcm_reader();

    g_ble_demo_ctx.bt_on = false;
    g_ble_demo_ctx.socket_open = false;
    g_ble_demo_ctx.tx_in_flight = false;
    g_ble_demo_ctx.image_active = false;
    g_ble_demo_ctx.send_b_next = false;
    g_ble_demo_ctx.tcm_reader_busy = false;
    g_ble_demo_ctx.socket_id = 0U;
    g_ble_demo_ctx.remote_mtu = 0U;
    g_ble_demo_ctx.max_sdu_len = 0U;
    g_ble_demo_ctx.current_period_ms = BLE_DEMO_TX_PERIOD_MS;
    g_ble_demo_ctx.image_seq = 0U;
    /* change-20260615-hyungchul :
     * BT OFF 시 마지막 ADSP image sequence 기록을 초기화한다.
     * 다음 BT ON/socket open 후 이전 sequence 때문에 새 JPG를 skip하지 않도록 하기 위해 추가한다. */
    g_ble_demo_ctx.last_tcm_image_seq = 0U;
    g_ble_demo_ctx.completed_image_count = 0U;
    g_ble_demo_ctx.active_image = NULL;
    /*change-20260617-hyungchul
     * 수정한 이유 : BT OFF로 전송 context를 초기화할 때 이전 image의 LE CoC frame 누적 byte가 남아 있으면 다음 속도 log가 잘못 계산될 수 있다.
     * 수정한 코드의 목적 : BT OFF 초기화 경로에서 전송 속도 계산용 frame byte 누적값을 0으로 정리한다. */
    g_ble_demo_ctx.active_tx_frame_bytes_sent = 0U;
    g_ble_demo_ctx.active_offset = 0U;
    g_ble_demo_ctx.active_frag_idx = 0U;
    g_ble_demo_ctx.active_frag_count = 0U;
    g_ble_demo_ctx.active_start_ms = 0;

    printk(BLE_DEMO_LOG_PREFIX " BT OFF, context cleared\n");
}

void ble_demo_app_on_lecoc_socket_opened(uint64_t socketId, uint16_t remoteMtu)
{
    if (g_ble_demo_ctx.initialized == false)
    {
        ble_demo_app_init();
    }

    /* change-20260615-hyungchul :
     * 새 LECoC socket이 열릴 때 이전 socket에서 남았을 수 있는 TCM reader_busy를 정리한다.
     * socket 재연결 후 ADSP writer가 reader_busy 때문에 TCM update를 계속 skip하는 상황을 방지하기 위해 추가한다. */
    ble_demo_release_tcm_reader();

    g_ble_demo_ctx.socket_id = socketId;
    g_ble_demo_ctx.remote_mtu = remoteMtu;
    g_ble_demo_ctx.max_sdu_len = ble_demo_calc_max_sdu_len(remoteMtu);
    g_ble_demo_ctx.current_period_ms = BLE_DEMO_TX_PERIOD_MS;
    g_ble_demo_ctx.socket_open = true;
    g_ble_demo_ctx.tx_in_flight = false;
    g_ble_demo_ctx.image_active = false;
    g_ble_demo_ctx.send_b_next = false;
    g_ble_demo_ctx.tcm_reader_busy = false;
    g_ble_demo_ctx.image_seq = 0U;
    /* change-20260615-hyungchul :
     * socket open 시 마지막 ADSP image sequence 기록을 초기화한다.
     * 새 LECoC 연결에서 이전 socket의 sequence 비교 결과가 남지 않도록 하기 위해 추가한다. */
    g_ble_demo_ctx.last_tcm_image_seq = 0U;
    g_ble_demo_ctx.completed_image_count = 0U;
    g_ble_demo_ctx.active_image = NULL;
    /*change-20260617-hyungchul
     * 수정한 이유 : 새 LECoC socket이 열릴 때 이전 socket의 frame byte 누적값이 남아 있으면 새 image 전송 속도 계산에 섞일 수 있다.
     * 수정한 코드의 목적 : socket open 초기화 경로에서 전송 속도 계산용 frame byte 누적값을 0으로 정리한다. */
    g_ble_demo_ctx.active_tx_frame_bytes_sent = 0U;
    g_ble_demo_ctx.active_offset = 0U;
    g_ble_demo_ctx.active_frag_idx = 0U;
    g_ble_demo_ctx.active_frag_count = 0U;
    g_ble_demo_ctx.active_start_ms = 0;

    (void)k_work_cancel_delayable(&g_ble_demo_tx_work);

    printk(BLE_DEMO_LOG_PREFIX " LECoC socket opened, socketId=%llu remoteMtu=%u maxSdu=%u period=%ums\n",
           (unsigned long long)socketId,
           (uint32_t)remoteMtu,
           (uint32_t)g_ble_demo_ctx.max_sdu_len,
           (uint32_t)g_ble_demo_ctx.current_period_ms);

    ble_demo_schedule_next_image(g_ble_demo_ctx.current_period_ms);
}

void ble_demo_app_on_lecoc_socket_closed(uint64_t socketId)
{
    if (g_ble_demo_ctx.socket_id != socketId)
    {
        printk(BLE_DEMO_LOG_PREFIX " ignore close for unknown socketId=%llu, current=%llu\n",
               (unsigned long long)socketId,
               (unsigned long long)g_ble_demo_ctx.socket_id);
        return;
    }

    (void)k_work_cancel_delayable(&g_ble_demo_tx_work);

    /* change-20260615-hyungchul :
     * socket close로 image 전송이 중간에 끊길 때 reader_busy를 해제한다.
     * WM reader가 더 이상 TCM을 읽지 않는데 ADSP writer가 계속 대기하는 문제를 막기 위해 추가한다. */
    ble_demo_release_tcm_reader();

    g_ble_demo_ctx.socket_open = false;
    g_ble_demo_ctx.tx_in_flight = false;
    g_ble_demo_ctx.image_active = false;
    g_ble_demo_ctx.send_b_next = false;
    g_ble_demo_ctx.tcm_reader_busy = false;
    g_ble_demo_ctx.socket_id = 0U;
    g_ble_demo_ctx.remote_mtu = 0U;
    g_ble_demo_ctx.max_sdu_len = 0U;
    g_ble_demo_ctx.current_period_ms = BLE_DEMO_TX_PERIOD_MS;
    /* change-20260615-hyungchul :
     * socket close 시 마지막 ADSP image sequence 기록을 초기화한다.
     * 재연결 후 같은 TCM READY image를 다시 확인할 수 있도록 하기 위해 추가한다. */
    g_ble_demo_ctx.last_tcm_image_seq = 0U;
    g_ble_demo_ctx.completed_image_count = 0U;
    g_ble_demo_ctx.active_image = NULL;
    /*change-20260617-hyungchul
     * 수정한 이유 : socket close로 전송이 중단될 때 누적된 frame byte가 남아 있으면 재연결 후 첫 속도 log가 부정확해질 수 있다.
     * 수정한 코드의 목적 : socket close 초기화 경로에서 전송 속도 계산용 frame byte 누적값을 0으로 정리한다. */
    g_ble_demo_ctx.active_tx_frame_bytes_sent = 0U;
    g_ble_demo_ctx.active_offset = 0U;
    g_ble_demo_ctx.active_frag_idx = 0U;
    g_ble_demo_ctx.active_frag_count = 0U;
    g_ble_demo_ctx.active_start_ms = 0;

    printk(BLE_DEMO_LOG_PREFIX " LECoC socket closed, socketId=%llu\n",
           (unsigned long long)socketId);
}

void ble_demo_app_on_lecoc_data_tx_cfm(uint64_t socketId, uint16_t status)
{
    if (g_ble_demo_ctx.socket_id != socketId)
    {
        printk(BLE_DEMO_LOG_PREFIX " ignore tx cfm for unknown socketId=%llu, current=%llu\n",
               (unsigned long long)socketId,
               (unsigned long long)g_ble_demo_ctx.socket_id);
        return;
    }

    g_ble_demo_ctx.tx_in_flight = false;

    /*
     * Some Qualcomm offload paths report a non-zero DATA_TX_CFM result even
     * when the LE CoC SDU has already reached the Android peer. If this value
     * is treated as a hard failure, the demo restarts the JPEG on every CFM and
     * Android receives only fragment #1 with a new image_id forever. Continue
     * the current image on every CFM; qapi_bt_lecoc_send_data() failures are
     * still handled in ble_demo_send_next_fragment_or_finish().
     */
    if ((status != 0U) &&
        ((g_ble_demo_ctx.active_frag_idx <= 1U) ||
         ((g_ble_demo_ctx.active_frag_idx % BLE_DEMO_FRAGMENT_LOG_INTERVAL) == 0U) ||
         (g_ble_demo_ctx.active_frag_idx >= g_ble_demo_ctx.active_frag_count)))
    {
        printk(BLE_DEMO_LOG_PREFIX " TX CFM socketId=%llu status=%u, keep current image TX image=%u fragDone=%u/%u\n",
               (unsigned long long)socketId,
               (uint32_t)status,
               (uint32_t)g_ble_demo_ctx.image_seq,
               (uint32_t)g_ble_demo_ctx.active_frag_idx,
               (uint32_t)g_ble_demo_ctx.active_frag_count);
    }

    if (g_ble_demo_ctx.image_active == true)
    {
        /*
         * qapi_bt_lecoc_send_data() cannot be called directly here because
         * lpai_bt_lecoc_app.c clears TX_ENABLE after this callback returns.
         * Schedule work instead so the next fragment is sent after TX_ENABLE
         * has been cleared.
         */
        ble_demo_schedule_next_image(0U);
    }
}

static void ble_demo_schedule_next_image(uint32_t delay_ms)
{
    if (g_ble_demo_ctx.socket_open == false)
    {
        return;
    }

    (void)k_work_schedule(&g_ble_demo_tx_work, K_MSEC(delay_ms));
}

static uint16_t ble_demo_calc_max_sdu_len(uint16_t remote_mtu)
{
    uint16_t max_sdu_len = BLE_DEMO_CAR_TX_MAX_SDU_LEN;

    if (remote_mtu <= 1U)
    {
        return 0U;
    }

    /* qapi_bt_lecoc_send_data() currently checks remoteMtu > dataLen. */
    if ((uint16_t)(remote_mtu - 1U) < max_sdu_len)
    {
        max_sdu_len = (uint16_t)(remote_mtu - 1U);
    }

    return max_sdu_len;
}

static uint16_t ble_demo_calc_payload_per_fragment(void)
{
#if BLE_DEMO_USE_CJPG_HEADER
    if (g_ble_demo_ctx.max_sdu_len <= sizeof(ble_demo_cjpg_hdr_t))
    {
        return 0U;
    }

    return (uint16_t)(g_ble_demo_ctx.max_sdu_len - sizeof(ble_demo_cjpg_hdr_t));
#else
    return g_ble_demo_ctx.max_sdu_len;
#endif
}

static uint16_t ble_demo_calc_frag_count(uint32_t image_len, uint16_t payload_per_fragment)
{
    if (payload_per_fragment == 0U)
    {
        return 0U;
    }

    return (uint16_t)((image_len + (uint32_t)payload_per_fragment - 1U) /
                      (uint32_t)payload_per_fragment);
}

static uint32_t ble_demo_update_period_from_elapsed(int64_t elapsed_ms)
{
    uint32_t new_period_ms = BLE_DEMO_TX_PERIOD_MS;

    if (elapsed_ms > (int64_t)BLE_DEMO_TX_PERIOD_MS)
    {
        new_period_ms = BLE_DEMO_TX_SLOW_PERIOD_MS;
    }

    if (g_ble_demo_ctx.current_period_ms != new_period_ms)
    {
        printk(BLE_DEMO_LOG_PREFIX " image TX elapsed=%lldms, change period %ums -> %ums\n",
               (long long)elapsed_ms,
               (uint32_t)g_ble_demo_ctx.current_period_ms,
               (uint32_t)new_period_ms);
        g_ble_demo_ctx.current_period_ms = new_period_ms;
    }

    return g_ble_demo_ctx.current_period_ms;
}

/* change-20260615-hyungchul :
 * 기존 ble_ori_imagesize 기반 중복 판단을 제거하고 g_ble_demo_ctx.last_tcm_image_seq로 새 이미지를 판단한다.
 * JPG 내용이 달라도 size가 같은 경우 전송이 skip되는 문제를 해결하기 위해 수정한다. */
volatile uint8_t *const real_shared_buffer = (volatile uint8_t *)0x20488000;
/* change-20260615-hyungchul :
 * TCM image data와 같은 shared buffer의 control 영역을 WM에서 직접 읽고 쓸 수 있도록 포인터를 추가한다.
 * ADSP writer의 write_state와 WM reader의 reader_busy를 같은 주소 기준으로 동기화하기 위해 추가한다. */
static volatile lge_tcm_image_ctrl_t *const real_shared_ctrl =
    (volatile lge_tcm_image_ctrl_t *)(0x20488000 + LGE_TCM_CTRL_OFFSET);

static void ble_demo_shared_mem_barrier(void)
{
    __asm__ volatile("" ::: "memory");
}

static void ble_demo_release_tcm_reader(void)
{
    /* change-20260615-hyungchul :
     * WM이 획득한 reader_busy를 해제하는 공통 helper이다.
     * 전송 완료, BT OFF, socket close, error path마다 동일한 해제 코드를 넣다가 누락되는 것을 방지하기 위해 추가한다. */
    if (g_ble_demo_ctx.tcm_reader_busy == true)
    {
        real_shared_ctrl->reader_busy = 0U;
        ble_demo_shared_mem_barrier();
        g_ble_demo_ctx.tcm_reader_busy = false;
    }
}

static void ble_demo_start_next_image(void)
{
    uint16_t payload_per_fragment = ble_demo_calc_payload_per_fragment();

    if (payload_per_fragment == 0U)
    {
        printk(BLE_DEMO_LOG_PREFIX " cannot start image TX, remoteMtu=%u maxSdu=%u header=%u\n",
               (uint32_t)g_ble_demo_ctx.remote_mtu,
               (uint32_t)g_ble_demo_ctx.max_sdu_len,
               (uint32_t)sizeof(ble_demo_cjpg_hdr_t));
        ble_demo_schedule_next_image(g_ble_demo_ctx.current_period_ms);
        return;
    }

    uint32_t jpgsize = (uint32_t)real_shared_buffer[3] |
                      ((uint32_t)real_shared_buffer[4] << 8) |
                      ((uint32_t)real_shared_buffer[5] << 16) |
                      ((uint32_t)real_shared_buffer[6] << 24);

    /* change-20260615-hyungchul :
     * JPG size를 사용하기 전에 ADSP가 TCM write를 완료했는지 control block의 magic/write_state/jpg_len으로 확인한다.
     * ADSP가 header만 쓰고 body를 아직 쓰는 중인 상태에서 WM이 BLE 전송을 시작하는 race condition을 막기 위해 추가한다. */
    if ((real_shared_ctrl->magic != LGE_TCM_IMAGE_CTRL_MAGIC) ||
        (real_shared_ctrl->write_state != LGE_TCM_IMAGE_STATE_READY) ||
        (real_shared_ctrl->jpg_len == 0U) ||
        (real_shared_ctrl->jpg_len != jpgsize))
    {
        ble_demo_schedule_next_image(g_ble_demo_ctx.current_period_ms);
        return;
    }

    /* change-20260615-hyungchul :
     * ADSP가 publish한 image_seq를 읽고, 마지막으로 전송한 sequence와 같으면 새 이미지가 아니므로 skip한다.
     * 기존 JPG size 비교는 같은 크기의 다른 JPG를 새 이미지로 인식하지 못했기 때문에 수정한다. */
    uint32_t tcm_image_seq = real_shared_ctrl->image_seq;
    if ((tcm_image_seq == 0U) || (tcm_image_seq == g_ble_demo_ctx.last_tcm_image_seq))
    {
        ble_demo_schedule_next_image(g_ble_demo_ctx.current_period_ms);
        return;
    }

    uint8_t ble_control = real_shared_buffer[0x1ff10];
    if(ble_control != 0x1)
    {
            //printk(BLE_DEMO_LOG_PREFIX "VGA Control OFF: %d\n", ble_control);
            ble_demo_schedule_next_image(g_ble_demo_ctx.current_period_ms);
            return;
    }

    /* change-20260615-hyungchul :
     * WM이 이번 READY image를 BLE로 읽기 전에 reader_busy를 1로 설정하고 상태를 다시 확인한다.
     * ADSP가 같은 TCM buffer를 덮어쓰는 것을 막고, reader_busy 설정 직전 ADSP가 WRITING으로 바꾼 경우에는 전송을 포기하기 위해 추가한다. */
    real_shared_ctrl->reader_busy = 1U;
    ble_demo_shared_mem_barrier();
    g_ble_demo_ctx.tcm_reader_busy = true;

    if ((real_shared_ctrl->magic != LGE_TCM_IMAGE_CTRL_MAGIC) ||
        (real_shared_ctrl->write_state != LGE_TCM_IMAGE_STATE_READY) ||
        (real_shared_ctrl->jpg_len != jpgsize) ||
        (real_shared_ctrl->image_seq != tcm_image_seq))
    {
        ble_demo_release_tcm_reader();
        ble_demo_schedule_next_image(g_ble_demo_ctx.current_period_ms);
        return;
    }

    jpgsize = jpgsize + 1; // Add 0x0F
    /* change-20260615-hyungchul :
     * BLE 전송을 시작하는 시점에 이번 ADSP image_seq를 마지막 처리 sequence로 저장한다.
     * 같은 JPG size의 새 frame도 image_seq가 다르면 정상 전송하고, 같은 image를 반복 전송하지 않기 위해 수정한다. */
    g_ble_demo_ctx.last_tcm_image_seq = tcm_image_seq;
    g_ble_demo_ctx.active_image = &g_ble_demo_images[g_ble_demo_ctx.send_b_next ? 1 : 0];
    g_ble_demo_ctx.send_b_next = !g_ble_demo_ctx.send_b_next;
    g_ble_demo_ctx.image_active = true;
    g_ble_demo_ctx.active_offset = 0U;
    g_ble_demo_ctx.active_frag_idx = 0U;
    //g_ble_demo_ctx.active_frag_count = ble_demo_calc_frag_count(g_ble_demo_ctx.active_image_len,
    //                                                            payload_per_fragment);
    g_ble_demo_ctx.active_start_ms = k_uptime_get();
    g_ble_demo_ctx.image_seq++;

    g_ble_demo_ctx.active_image_len = jpgsize;
    /*change-20260617-hyungchul
     * 수정한 이유 : image 전송 시작 시 이전 image에서 누적된 LE CoC frame byte가 남아 있으면 이번 image의 전송 속도 계산이 잘못된다.
     * 수정한 코드의 목적 : 새 image 전송을 시작할 때 qapi_bt_lecoc_send_data() 성공 frame byte 누적값을 0으로 초기화한다. */
    g_ble_demo_ctx.active_tx_frame_bytes_sent = 0U;

    printk(BLE_DEMO_LOG_PREFIX " #TCM image HEAD #%x %x %x %x %x %x %x %x\n",
           (uint8_t)real_shared_buffer[0],
           (uint8_t)real_shared_buffer[1],
           (uint8_t)real_shared_buffer[2],
           (uint8_t)real_shared_buffer[3],
           (uint8_t)real_shared_buffer[4],
           (uint8_t)real_shared_buffer[5],
           (uint8_t)real_shared_buffer[6],
           (uint8_t)real_shared_buffer[7]);
    uint32_t total_img_len = g_ble_demo_ctx.active_image_len + 129;
    uint32_t first_chunk_max = BLE_DEMO_CAR_TX_MAX_SDU_LEN;

    if (total_img_len <= first_chunk_max)
    {
        g_ble_demo_ctx.active_frag_count = 1U;
    }
    else
    {
        uint32_t remaining_img_len = total_img_len - first_chunk_max;

        g_ble_demo_ctx.active_frag_count = 1U + (uint16_t)((remaining_img_len + (uint32_t)payload_per_fragment - 1U) /
                                                          (uint32_t)payload_per_fragment);
    }

    printk(BLE_DEMO_LOG_PREFIX " start image #%u tcmSeq=%u %s len=%u fragPayload=%u fragCount=%u period=%ums\n",
           (uint32_t)g_ble_demo_ctx.image_seq,
           (uint32_t)tcm_image_seq,
           g_ble_demo_ctx.active_image->name,
           (uint32_t)g_ble_demo_ctx.active_image_len,
           (uint32_t)payload_per_fragment,
           (uint32_t)g_ble_demo_ctx.active_frag_count,
           (uint32_t)g_ble_demo_ctx.current_period_ms);
}

static void ble_demo_finish_active_image(void)
{
    int64_t now_ms;
    int64_t elapsed_ms;
    uint32_t period_ms;
    uint32_t next_delay_ms = 0U;
    /*change-20260617-hyungchul
     * 수정한 이유 : image 전송 완료 시 elapsed time과 전송 byte를 이용해 BLE LECoC throughput을 계산하기 위한 지역 변수가 필요하다.
     * 수정한 코드의 목적 : JPG payload byte 기준 속도와 qapi_bt_lecoc_send_data() frame byte 기준 속도를 정수 fixed-point 방식으로 계산한다. */
    uint32_t elapsed_for_calc_ms;
    uint32_t payload_kbps_x100;
    uint32_t payload_kBps_x100;
    uint32_t frame_kbps_x100;
    uint32_t frame_kBps_x100;

    if (g_ble_demo_ctx.active_image == NULL)
    {
        g_ble_demo_ctx.image_active = false;
        /*change-20260617-hyungchul
         * 수정한 이유 : active_image가 NULL인 비정상 종료 경로에서도 이전 frame byte 누적값이 다음 image 속도 계산에 남지 않도록 정리해야 한다.
         * 수정한 코드의 목적 : 비정상 종료 경로에서 전송 속도 계산용 frame byte 누적값을 0으로 초기화한다. */
        g_ble_demo_ctx.active_tx_frame_bytes_sent = 0U;
        /* change-20260615-hyungchul :
         * active_image가 NULL인 비정상 종료 경로에서도 reader_busy를 해제한다.
         * 전송 context가 이미 깨진 경우 ADSP writer가 계속 막히는 것을 방지하기 위해 추가한다. */
        ble_demo_release_tcm_reader();
        ble_demo_schedule_next_image(g_ble_demo_ctx.current_period_ms);
        return;
    }

    now_ms = k_uptime_get();
    elapsed_ms = now_ms - g_ble_demo_ctx.active_start_ms;
    period_ms = ble_demo_update_period_from_elapsed(elapsed_ms);
    g_ble_demo_ctx.completed_image_count++;

    if ((elapsed_ms >= 0) && (elapsed_ms < (int64_t)period_ms))
    {
        next_delay_ms = (uint32_t)((int64_t)period_ms - elapsed_ms);
    }

    printk(BLE_DEMO_LOG_PREFIX " complete image #%u %s bytes=%u fragments=%u elapsed=%lldms nextDelay=%ums\n",
           (uint32_t)g_ble_demo_ctx.image_seq,
           g_ble_demo_ctx.active_image->name,
           (uint32_t)g_ble_demo_ctx.active_image_len,
           (uint32_t)g_ble_demo_ctx.active_frag_count,
           (long long)elapsed_ms,
           (uint32_t)next_delay_ms);

    /*change-20260617-hyungchul
     * 수정한 이유 : BLE LECoC image 전송 시간이 실제로 얼마나 걸렸는지 숫자로 확인할 수 없어 성능 비교와 tuning이 어려웠다.
     * 수정한 코드의 목적 : image 전송 완료 시 JPG payload 기준 throughput과 qapi_bt_lecoc_send_data()로 queue한 SDU frame 기준 throughput을 KB/s와 kbps로 log에 출력한다. */
    elapsed_for_calc_ms = (elapsed_ms <= 0) ? 1U : (uint32_t)elapsed_ms;
    payload_kBps_x100 = (uint32_t)(((uint64_t)g_ble_demo_ctx.active_image_len * 100U) /
                                   (uint64_t)elapsed_for_calc_ms);
    payload_kbps_x100 = (uint32_t)(((uint64_t)g_ble_demo_ctx.active_image_len * 8U * 100U) /
                                   (uint64_t)elapsed_for_calc_ms);
    frame_kBps_x100 = (uint32_t)(((uint64_t)g_ble_demo_ctx.active_tx_frame_bytes_sent * 100U) /
                                 (uint64_t)elapsed_for_calc_ms);
    frame_kbps_x100 = (uint32_t)(((uint64_t)g_ble_demo_ctx.active_tx_frame_bytes_sent * 8U * 100U) /
                                 (uint64_t)elapsed_for_calc_ms);

    printk(BLE_DEMO_LOG_PREFIX " throughput image #%u tcmSeq=%u payload=%uB frame=%uB elapsed=%ums payload=%u.%02uKB/s(%u.%02ukbps) frame=%u.%02uKB/s(%u.%02ukbps)\n",
           (uint32_t)g_ble_demo_ctx.image_seq,
           (uint32_t)g_ble_demo_ctx.last_tcm_image_seq,
           (uint32_t)g_ble_demo_ctx.active_image_len,
           (uint32_t)g_ble_demo_ctx.active_tx_frame_bytes_sent,
           (uint32_t)elapsed_for_calc_ms,
           (uint32_t)(payload_kBps_x100 / 100U),
           (uint32_t)(payload_kBps_x100 % 100U),
           (uint32_t)(payload_kbps_x100 / 100U),
           (uint32_t)(payload_kbps_x100 % 100U),
           (uint32_t)(frame_kBps_x100 / 100U),
           (uint32_t)(frame_kBps_x100 % 100U),
           (uint32_t)(frame_kbps_x100 / 100U),
           (uint32_t)(frame_kbps_x100 % 100U));

    /* change-20260615-hyungchul :
     * 하나의 JPG image BLE 전송이 끝난 뒤 TCM reader_busy를 해제한다.
     * ADSP가 다음 interrupt에서 새 JPG를 TCM에 쓸 수 있도록 하기 위해 추가한다. */
    ble_demo_release_tcm_reader();

    g_ble_demo_ctx.image_active = false;
    g_ble_demo_ctx.active_image = NULL;
    /*change-20260617-hyungchul
     * 수정한 이유 : 완료된 image의 frame byte 누적값이 다음 image 전송 속도 계산에 포함되지 않도록 정리해야 한다.
     * 수정한 코드의 목적 : 정상 완료 경로에서 전송 속도 계산용 frame byte 누적값을 0으로 초기화한다. */
    g_ble_demo_ctx.active_tx_frame_bytes_sent = 0U;
    g_ble_demo_ctx.active_offset = 0U;
    g_ble_demo_ctx.active_frag_idx = 0U;
    g_ble_demo_ctx.active_frag_count = 0U;
    g_ble_demo_ctx.active_start_ms = 0;

    ble_demo_schedule_next_image(next_delay_ms);
}

static void ble_demo_send_next_fragment_or_finish(void)
{
    qapi_bt_lecoc_status_code_t result;
    uint16_t payload_per_fragment;
    uint32_t remain_len;
    uint16_t chunk_len;
    uint16_t frame_len;
    uint16_t flags = 0U;
    uint8_t *payload_dst = g_ble_demo_tx_buf;

    if (g_ble_demo_ctx.image_active == false)
    {
        ble_demo_start_next_image();
    }

    if ((g_ble_demo_ctx.image_active == false) || (g_ble_demo_ctx.active_image == NULL))
    {
        return;
    }

    if (g_ble_demo_ctx.active_offset >= g_ble_demo_ctx.active_image_len)
    {
        ble_demo_finish_active_image();
        return;
    }

    payload_per_fragment = ble_demo_calc_payload_per_fragment();
    if (payload_per_fragment == 0U)
    {
        printk(BLE_DEMO_LOG_PREFIX " invalid payload_per_fragment=0\n");
        /* change-20260615-hyungchul :
         * payload 계산 실패로 현재 image 전송을 중단할 때 reader_busy를 해제한다.
         * WM이 더 이상 TCM을 읽지 않는데 ADSP writer가 reader_busy 때문에 skip하는 문제를 막기 위해 추가한다. */
        ble_demo_release_tcm_reader();
        g_ble_demo_ctx.image_active = false;
        g_ble_demo_ctx.active_image = NULL;
        /*change-20260617-hyungchul
         * 수정한 이유 : payload 계산 실패로 image 전송을 중단할 때 일부 누적된 frame byte가 다음 속도 log에 섞일 수 있다.
         * 수정한 코드의 목적 : error path에서도 전송 속도 계산용 frame byte 누적값을 0으로 초기화한다. */
        g_ble_demo_ctx.active_tx_frame_bytes_sent = 0U;
        ble_demo_schedule_next_image(g_ble_demo_ctx.current_period_ms);
        return;
    }

    remain_len = g_ble_demo_ctx.active_image_len - g_ble_demo_ctx.active_offset;
    chunk_len = (remain_len > payload_per_fragment) ? payload_per_fragment : (uint16_t)remain_len;

#if BLE_DEMO_USE_CJPG_HEADER
    {
        ble_demo_cjpg_hdr_t *hdr = (ble_demo_cjpg_hdr_t *)g_ble_demo_tx_buf;

        if (g_ble_demo_ctx.active_offset == 0U)
        {
            flags |= BLE_DEMO_CJPG_FLAG_FIRST;
        }

        if ((g_ble_demo_ctx.active_offset + (uint32_t)chunk_len) >= g_ble_demo_ctx.active_image_len)
        {
            flags |= BLE_DEMO_CJPG_FLAG_LAST;
        }

        if (g_ble_demo_ctx.active_image->is_b_image == true)
        {
            flags |= BLE_DEMO_CJPG_FLAG_IMAGE_B;
        }

        hdr->magic = BLE_DEMO_CJPG_MAGIC;
        hdr->image_id = g_ble_demo_ctx.image_seq;
        hdr->total_len = g_ble_demo_ctx.active_image_len;
        hdr->offset = g_ble_demo_ctx.active_offset;
        hdr->chunk_len = chunk_len;
        hdr->frag_idx = g_ble_demo_ctx.active_frag_idx;
        hdr->frag_count = g_ble_demo_ctx.active_frag_count;
        hdr->flags = flags;

        payload_dst = &g_ble_demo_tx_buf[sizeof(ble_demo_cjpg_hdr_t)];
        frame_len = (uint16_t)(sizeof(ble_demo_cjpg_hdr_t) + chunk_len);
    }
#else
    frame_len = chunk_len;
#endif

    if(g_ble_demo_ctx.active_offset == 0U)
    {
        int idx = 0;
        // HEADER
        payload_dst[idx++] = 0xF0;
        payload_dst[idx++] = 0x01;

	uint32_t datatransfer_length = (uint32_t)g_ble_demo_ctx.active_image_len-1 + 4 + 86;
        uint32_t total_length = datatransfer_length + 4 + 24 + 4;
        payload_dst[idx++] = (total_length >> 24) & 0xFF;
        payload_dst[idx++] = (total_length >> 16) & 0xFF;
        payload_dst[idx++] = (total_length >> 8)  & 0xFF;
        payload_dst[idx++] = (total_length)       & 0xFF;

        payload_dst[idx++] = 0x0;        payload_dst[idx++] = 0x0;        payload_dst[idx++] = 0x0;        payload_dst[idx++] = 0x18;

        payload_dst[idx++] = 'c';        payload_dst[idx++] = 'o';        payload_dst[idx++] = 'm';        payload_dst[idx++] = '.';
        payload_dst[idx++] = 'l';        payload_dst[idx++] = 'g';        payload_dst[idx++] = 'e';        payload_dst[idx++] = '.';
        payload_dst[idx++] = 'w';        payload_dst[idx++] = 'e';        payload_dst[idx++] = 'a';        payload_dst[idx++] = 'r';
        payload_dst[idx++] = 'd';        payload_dst[idx++] = 'a';        payload_dst[idx++] = 't';        payload_dst[idx++] = 'a';
        payload_dst[idx++] = 't';        payload_dst[idx++] = 'r';        payload_dst[idx++] = 'a';        payload_dst[idx++] = 'n';
        payload_dst[idx++] = 's';        payload_dst[idx++] = 'f';        payload_dst[idx++] = 'e';        payload_dst[idx++] = 'r';

        payload_dst[idx++] = (datatransfer_length >> 24) & 0xFF;
        payload_dst[idx++] = (datatransfer_length >> 16) & 0xFF;
        payload_dst[idx++] = (datatransfer_length >> 8)  & 0xFF;
        payload_dst[idx++] = (datatransfer_length)       & 0xFF;

        payload_dst[idx++] = 0x0;        payload_dst[idx++] = 0x0;        payload_dst[idx++] = 0x0;        payload_dst[idx++] = 0x56;

	time_t timer = time(NULL);    
        struct tm *t = localtime(&timer);
        char json_output[150];
            
        int year  = t->tm_year + 1900;
        int month = t->tm_mon + 1;
        int day   = t->tm_mday;
        int hour  = t->tm_hour;
        int min   = t->tm_min;
        int sec   = t->tm_sec;
                                    
        int short_year = year % 100;
                                        
        snprintf(json_output, sizeof(json_output),
            "{\"filename\":\"%02d%02d%02d_%02d%02d%02d.jpg\","
            "\"system0_date\":\"%04d-%02d-%02d\","
            "\"system0_time\":\"%02d:%02d:%02d\"}",
            short_year, month, day, hour, min, sec,
            year, month, day, hour, min, sec);
        printk(BLE_DEMO_LOG_PREFIX "%s\n", json_output);

	int json_len = strlen(json_output);
        memcpy(&payload_dst[idx], json_output, json_len);
        idx += json_len;

        uint8_t* img_start_ptr = (uint8_t*)real_shared_buffer + 11;
        uint8_t* current_src_ptr = img_start_ptr + g_ble_demo_ctx.active_offset;
        memcpy((uint8_t*)&payload_dst[idx], current_src_ptr, BLE_DEMO_CAR_TX_MAX_SDU_LEN - 128);
	//printk(BLE_DEMO_LOG_PREFIX "%x %x\n", g_ble_demo_tx_buf[0], g_ble_demo_tx_buf[1]);
	//printk(BLE_DEMO_LOG_PREFIX "%x %x %x %x\n", g_ble_demo_tx_buf[2], g_ble_demo_tx_buf[3], g_ble_demo_tx_buf[4], g_ble_demo_tx_buf[5]);
    }
    else if((g_ble_demo_ctx.active_frag_idx + 1U) == g_ble_demo_ctx.active_frag_count)
    {
        uint8_t* img_start_ptr = (uint8_t*)real_shared_buffer + 11;
        uint8_t* current_src_ptr = img_start_ptr + g_ble_demo_ctx.active_offset;
        memcpy((uint8_t*)payload_dst, current_src_ptr, chunk_len);
    }
    else
    {
        uint8_t* img_start_ptr = (uint8_t*)real_shared_buffer + 11;
        uint8_t* current_src_ptr = img_start_ptr + g_ble_demo_ctx.active_offset;
        memcpy((uint8_t*)payload_dst, current_src_ptr, chunk_len);
    }
    
    result = qapi_bt_lecoc_send_data(LECOC_ENDPOINT_ID,
                                     g_ble_demo_ctx.socket_id,
                                     frame_len,
                                     g_ble_demo_tx_buf);


    if (result == QAPI_BT_LECOC_SUCCESS)

    {
        g_ble_demo_ctx.tx_in_flight = true;
        /*change-20260617-hyungchul
         * 수정한 이유 : image 전송 완료 시 LE CoC SDU frame 기준 속도를 계산하려면 각 fragment에서 실제 qapi_bt_lecoc_send_data()에 전달한 frame_len 누적이 필요하다.
         * 수정한 코드의 목적 : send_data가 성공한 fragment의 frame_len을 image 단위로 누적한다. */
        g_ble_demo_ctx.active_tx_frame_bytes_sent += (uint32_t)frame_len;

        if ((g_ble_demo_ctx.active_frag_idx == 0U) ||
            ((g_ble_demo_ctx.active_frag_idx + 1U) == g_ble_demo_ctx.active_frag_count) ||
            ((g_ble_demo_ctx.active_frag_idx % BLE_DEMO_FRAGMENT_LOG_INTERVAL) == 0U))
        {
            printk(BLE_DEMO_LOG_PREFIX " TX image #%u %s frag=%u/%u offset=%u chunk=%u frame=%u\n",
                   (uint32_t)g_ble_demo_ctx.image_seq,
                   g_ble_demo_ctx.active_image->name,
                   (uint32_t)(g_ble_demo_ctx.active_frag_idx + 1U),
                   (uint32_t)g_ble_demo_ctx.active_frag_count,
                   (uint32_t)g_ble_demo_ctx.active_offset,
                   (uint32_t)chunk_len,
                   (uint32_t)frame_len);
        }

	if(g_ble_demo_ctx.active_offset == 0U)  g_ble_demo_ctx.active_offset += BLE_DEMO_CAR_TX_MAX_SDU_LEN - 128;
	else g_ble_demo_ctx.active_offset += chunk_len;
        g_ble_demo_ctx.active_frag_idx++;
    }
    else
    {
        printk(BLE_DEMO_LOG_PREFIX " TX failed result=%u socketId=%llu image=%s offset=%u frame=%u retry=%ums\n",
               (uint32_t)result,
               (unsigned long long)g_ble_demo_ctx.socket_id,
               g_ble_demo_ctx.active_image->name,
               (uint32_t)g_ble_demo_ctx.active_offset,
               (uint32_t)frame_len,
               (uint32_t)BLE_DEMO_TX_RETRY_MS);

        g_ble_demo_ctx.tx_in_flight = false;
        ble_demo_schedule_next_image(BLE_DEMO_TX_RETRY_MS);
    }
}

static void ble_demo_tx_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    if (g_ble_demo_ctx.bt_on == false)
    {
        printk(BLE_DEMO_LOG_PREFIX " TX skipped, BT OFF\n");
        return;
    }

    if (g_ble_demo_ctx.socket_open == false)
    {
        printk(BLE_DEMO_LOG_PREFIX " TX skipped, socket not open\n");
        return;
    }

    if (g_ble_demo_ctx.tx_in_flight == true)
    {
        printk(BLE_DEMO_LOG_PREFIX " TX skipped, previous TX still in-flight\n");
        ble_demo_schedule_next_image(BLE_DEMO_TX_RETRY_MS);
        return;
    }

    ble_demo_send_next_fragment_or_finish();
}
