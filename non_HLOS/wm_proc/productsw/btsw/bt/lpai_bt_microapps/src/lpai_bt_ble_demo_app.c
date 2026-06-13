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
    uint64_t socket_id;
    uint16_t remote_mtu;
    uint16_t max_sdu_len;
    uint32_t current_period_ms;
    uint32_t image_seq;
    uint32_t completed_image_count;
    const ble_demo_image_t *active_image;
    uint32_t active_image_len;
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

    g_ble_demo_ctx.bt_on = false;
    g_ble_demo_ctx.socket_open = false;
    g_ble_demo_ctx.tx_in_flight = false;
    g_ble_demo_ctx.image_active = false;
    g_ble_demo_ctx.send_b_next = false;
    g_ble_demo_ctx.socket_id = 0U;
    g_ble_demo_ctx.remote_mtu = 0U;
    g_ble_demo_ctx.max_sdu_len = 0U;
    g_ble_demo_ctx.current_period_ms = BLE_DEMO_TX_PERIOD_MS;
    g_ble_demo_ctx.image_seq = 0U;
    g_ble_demo_ctx.completed_image_count = 0U;
    g_ble_demo_ctx.active_image = NULL;
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

    g_ble_demo_ctx.socket_id = socketId;
    g_ble_demo_ctx.remote_mtu = remoteMtu;
    g_ble_demo_ctx.max_sdu_len = ble_demo_calc_max_sdu_len(remoteMtu);
    g_ble_demo_ctx.current_period_ms = BLE_DEMO_TX_PERIOD_MS;
    g_ble_demo_ctx.socket_open = true;
    g_ble_demo_ctx.tx_in_flight = false;
    g_ble_demo_ctx.image_active = false;
    g_ble_demo_ctx.send_b_next = false;
    g_ble_demo_ctx.image_seq = 0U;
    g_ble_demo_ctx.completed_image_count = 0U;
    g_ble_demo_ctx.active_image = NULL;
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

    g_ble_demo_ctx.socket_open = false;
    g_ble_demo_ctx.tx_in_flight = false;
    g_ble_demo_ctx.image_active = false;
    g_ble_demo_ctx.send_b_next = false;
    g_ble_demo_ctx.socket_id = 0U;
    g_ble_demo_ctx.remote_mtu = 0U;
    g_ble_demo_ctx.max_sdu_len = 0U;
    g_ble_demo_ctx.current_period_ms = BLE_DEMO_TX_PERIOD_MS;
    g_ble_demo_ctx.completed_image_count = 0U;
    g_ble_demo_ctx.active_image = NULL;
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

int ble_ori_imagesize = 0;
volatile uint8_t *const real_shared_buffer = (volatile uint8_t *)0x20488000;
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

    uint8_t ble_control = real_shared_buffer[0x1ff10];
    if(ble_control != 0x1)
    {
            //printk(BLE_DEMO_LOG_PREFIX "VGA Control OFF: %d\n", ble_control);
            ble_demo_schedule_next_image(g_ble_demo_ctx.current_period_ms);
            return;
    }

    jpgsize = jpgsize + 1; // Add 0x0F
    if(jpgsize == ble_ori_imagesize)
    {
	     //printk(BLE_DEMO_LOG_PREFIX " SAME IMAGE return!\n");
	     ble_demo_schedule_next_image(g_ble_demo_ctx.current_period_ms);
	     return;
    }
    ble_ori_imagesize = jpgsize;
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

    printk(BLE_DEMO_LOG_PREFIX " start image #%u %s len=%u fragPayload=%u fragCount=%u period=%ums\n",
           (uint32_t)g_ble_demo_ctx.image_seq,
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

    if (g_ble_demo_ctx.active_image == NULL)
    {
        g_ble_demo_ctx.image_active = false;
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

    g_ble_demo_ctx.image_active = false;
    g_ble_demo_ctx.active_image = NULL;
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
        g_ble_demo_ctx.image_active = false;
        g_ble_demo_ctx.active_image = NULL;
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
