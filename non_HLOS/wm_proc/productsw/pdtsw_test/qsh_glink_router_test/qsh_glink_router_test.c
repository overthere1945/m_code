/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#include <stdlib.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include "qapi_qsh_glink.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "sns_client.pb.h"
#include "sns_std_type.pb.h"
#include "sns_std_sensor.pb.h"
#include "sns_suid.pb.h"
#include "pdtsw_heap.h"

static qapi_qsh_glink_hndl_t glink_handle;
sns_std_suid suid = {12370169555311111083ull, 12370169555311111083ull};
sns_std_suid decoded_suid = sns_std_suid_init_default;
typedef struct sns_buffer_arg
{
    /* Buffer to be written, or reference to read buffer */
    void const *buf;
    /* Length of buf */
    size_t buf_len;
} sns_buffer_arg;

typedef struct {
  char data_type[64];
  uint64_t suid_low;
  uint64_t suid_high;
} DecodedEvent;

DecodedEvent decoded_event;

bool test_decode_data_type_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    char *data_type = (char *)*arg;
    int data_type_sz = stream->bytes_left;
    if(!pb_read(stream, (pb_byte_t *)data_type, stream->bytes_left))
    {
      printk("decode_data_type_callback failed\n");
        return false;
    }
    data_type[data_type_sz] = '\0'; // Null-terminate the string
    printk("decode_data_type_callback done\n");
    return true;
}

bool test_decode_suid_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    DecodedEvent *decoded_event = (DecodedEvent *)*arg;
    sns_std_suid suid = sns_std_suid_init_zero;

    if(!pb_decode_noinit(stream, sns_std_suid_fields, &suid))
    {
        printk("decode_suid_callback failed\n");
        return false;
    }

    decoded_event->suid_low = suid.suid_low;
    decoded_event->suid_high = suid.suid_high;
    printk("decode_suid_callback done. SUID Low: %llu, SUID High: %llu\n",
        decoded_event->suid_low, decoded_event->suid_high);
    return true;
}

bool test_decode_sns_suid_event(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    printk("decodeing sns_suid_event\n");
    sns_suid_event suid_event = sns_suid_event_init_zero;

    suid_event.data_type.funcs.decode = test_decode_data_type_callback;
    suid_event.data_type.arg = decoded_event.data_type;
    suid_event.suid.funcs.decode = test_decode_suid_callback;
    suid_event.suid.arg = &decoded_event;

    if (pb_decode_noinit(stream, sns_suid_event_fields, &suid_event))
    {
        printk("Data Type: %s\n", decoded_event.data_type);
        printk("SUID Low: %llu, SUID High: %llu\n", decoded_event.suid_low, decoded_event.suid_high);
        return true;
    }
    else
    {
        printk("Failed to decode sns_suid_event message. err = %s\n", PB_GET_ERROR(stream));
        return false;
    }
}

void handle_specific_suid(pb_istream_t *stream, size_t byte_left)
{
  printk("decodeing handle_specific_suid\n");
  sns_client_event_msg event_msg = sns_client_event_msg_init_zero;
  event_msg.events.funcs.decode = test_decode_sns_suid_event;

  if (!pb_decode(stream, sns_client_event_msg_fields, &event_msg))
  {
      printk("Failed to decode sns_client_event_msg. err = %s\n", PB_GET_ERROR(stream));
  }
}

static bool test_qsh_decode_event_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
  sns_client_event_msg_sns_client_event event = sns_client_event_msg_sns_client_event_init_zero;
    printk("decodeing sns_client_event\n");
    event.payload.arg = NULL;
    event.payload.funcs.decode = &test_decode_sns_suid_event;
    if (!pb_decode(stream, sns_client_event_msg_sns_client_event_fields, &event))
    {
      printk("Failed to decode sns_client_event. err = %s\n", PB_GET_ERROR(stream));
      return false;
    }

  return true;
}

void test_handle_data_evt(pb_istream_t* stream, uint32_t size, const void *priv)
{
    printk("decodeing sns_client_event_msg\n");
    sns_client_event_msg event_msg = sns_client_event_msg_init_zero;
    event_msg.events.funcs.decode = test_qsh_decode_event_callback;

    if (!pb_decode(stream, sns_client_event_msg_fields, &event_msg))
    {
      printk("Failed to decode sns_client_event_msg. err = %s\n", PB_GET_ERROR(stream));
    }
    printk("sns_client_event_msg: SUID Low: %llu, SUID High: %llu\n", event_msg.suid.suid_low, event_msg.suid.suid_high);
}

static void conn_callback(const qapi_qsh_glink_conn_evt_t *evt, const void *priv)
{
    switch (evt->evt_type) {
        case QSH_GLINK_EVT_CONN_ALLOC_RESP:
            shell_print(priv, "Connection allocated: %d", evt->conn_resp);
            break;
        case QSH_GLINK_EVT_CONN_FREE_RESP:
            shell_print(priv, "Connection freed: %d", evt->conn_resp);
            break;
        case QSH_GLINK_EVT_CONN_HNDL_ERR:
            shell_print(priv, "Connection error: %d", evt->conn_err);
            break;
        case QSH_GLINK_EVT_SEND_RESP:
            shell_print(priv, "Send response: %d", evt->send_resp);
            break;
        case QSH_GLINK_EVT_DATA:
            shell_print(priv, "Data received");
            /* Handle data processing here*/
            test_handle_data_evt(evt->data, evt->data_sz, priv);
            break;
        default:
            shell_print(priv, "Unknown event type");
            break;
    }
}

static int cmd_allocate_conn(const struct shell *shell, size_t argc, char **argv) {
    int ret = qapi_qsh_glink_allocate_conn(&glink_handle, conn_callback, shell);
    if (ret == 0) {
        shell_print(shell, "Connection allocated successfully");
    } else {
        shell_print(shell, "Failed to allocate connection: %d", ret);
    }
    return ret;
}

static int cmd_free_conn(const struct shell *shell, size_t argc, char **argv) {
    int ret = qapi_qsh_glink_free_conn(glink_handle);
    if (ret == 0) {
        shell_print(shell, "Connection freed successfully");
    } else {
        shell_print(shell, "Failed to free connection: %d", ret);
    }
    return ret;
}

static int cmd_send_suid_req(const struct shell *shell, size_t argc, char **argv) {
  if (argc < 2) {
      shell_print(shell, "Usage: send_data 1=mag suid req, 2=mag data register");
      return -EINVAL;
  }

  int ret = send_suid_request(shell);

  return ret;
}

static int cmd_send_data(const struct shell *shell, size_t argc, char **argv) {
    if (argc < 2) {
        shell_print(shell, "Usage: send_data 1=mag suid req, 2=mag data register");
        return -EINVAL;
    }

    int ret = send_register_request(shell);

    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_qsh_glink,
    SHELL_CMD(allocate_conn, NULL, "Allocate connection", cmd_allocate_conn),
    SHELL_CMD(free_conn, NULL, "Free connection", cmd_free_conn),
    SHELL_CMD(send_suid_req, NULL, "Send data over connection", cmd_send_suid_req),
    SHELL_CMD(register_for_events, NULL, "Send data over connection", cmd_send_data),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(qsh_glink, &sub_qsh_glink, "QSH GLink commands", NULL);

/** Utility functions */

typedef struct
{
  /* Buffer to be written, or reference to read buffer */
  void const *buf;
  /* Length of buf */
  size_t buf_len;
} slim_sns_buffer_arg;
slim_sns_buffer_arg slimz_arg_in_suid_payload, slimz_arg_in_config_payload;

bool encodePayload
(
  pb_ostream_t *pz_stream,
  const pb_field_t *pz_field,
  void *const *pz_arg
)
{
  slim_sns_buffer_arg *pz_info;
  printk("**encodePayload called\n");

  if((NULL == pz_arg) || (NULL == pz_stream) || (NULL == pz_field) ||
      (NULL == (pz_info = (slim_sns_buffer_arg*)*pz_arg)))
  {
    return false;
  }

  if(pb_encode_tag_for_field(pz_stream, pz_field) &&
      pb_encode_string(pz_stream, pz_info->buf, pz_info->buf_len))
  {
    return true;
  }

  return true;
}

static size_t PBGetEncodedSuidReq
(
  char const *pc_data_type,
  void **ppz_encoded_req
)
{
  uint32_t q_encoded_req_size;
  sns_suid_req z_suid_req = sns_suid_req_init_default;

  if( (NULL == pc_data_type) ||
      (NULL == ppz_encoded_req) )
  {
    return 0;
  }

  *ppz_encoded_req = NULL;

  z_suid_req.data_type.funcs.encode = encodePayload;
  slimz_arg_in_suid_payload.buf = pc_data_type;
  slimz_arg_in_suid_payload.buf_len = strlen(pc_data_type);
  z_suid_req.data_type.arg = &slimz_arg_in_suid_payload;

  z_suid_req.has_register_updates = true;
  z_suid_req.register_updates = true;

  if(!pb_get_encoded_size((size_t *)&q_encoded_req_size, sns_suid_req_fields, &z_suid_req))
  {
    //error
  }
  else 
  {
    void *encoded_buffer = pdtsw_heap_alloc(PDTSW_COMMON_HEAP, q_encoded_req_size);
    if(NULL != encoded_buffer)
    {
      pb_ostream_t pz_stream = pb_ostream_from_buffer(encoded_buffer, q_encoded_req_size);

      if(!pb_encode(&pz_stream, sns_suid_req_fields, &z_suid_req))
      {
        pdtsw_heap_free(PDTSW_COMMON_HEAP, encoded_buffer);
      }
      else
      {
        *ppz_encoded_req = encoded_buffer;
      }
    }
  }
  return NULL == *ppz_encoded_req ? 0 : q_encoded_req_size;
}

int send_suid_request(const struct shell *shell)
{
  sns_client_request_msg* request = (sns_client_request_msg*)pdtsw_heap_alloc(PDTSW_COMMON_HEAP, sizeof(sns_client_request_msg));
  *request = (sns_client_request_msg)sns_client_request_msg_init_default;
  request->suid = suid;
  request->msg_id = SNS_SUID_MSGID_SNS_SUID_REQ;
  request->susp_config.client_proc_type = SNS_STD_CLIENT_PROCESSOR_APSS;
  request->susp_config.delivery_type = SNS_CLIENT_DELIVERY_WAKEUP;
  request->has_client_tech = true;
  request->client_tech = SNS_TECH_SENSORS;
  request->request.payload.funcs.encode = encodePayload;

  size_t len = PBGetEncodedSuidReq("mag", (void**)&(slimz_arg_in_suid_payload.buf));
  slimz_arg_in_suid_payload.buf_len = len;

  request->request.payload.arg = &slimz_arg_in_suid_payload;

  size_t encoded_len = 0;
  if(!pb_get_encoded_size((size_t *)&encoded_len, sns_client_request_msg_fields, request))
  {
    //error
    shell_print(shell, "get encoded size error");
  }
  else
  {
    int ret = qapi_qsh_glink_send(glink_handle, request, encoded_len);
    if (ret == 0) {
        shell_print(shell, "Data sent successfully");
    } else {
        shell_print(shell, "Failed to send data: %d", ret);
    }
  }

  return 0;
}

int send_register_request(const struct shell *shell)
{
  sns_client_request_msg* request = (sns_client_request_msg*)pdtsw_heap_alloc(PDTSW_COMMON_HEAP, sizeof(sns_client_request_msg));
  *request = (sns_client_request_msg)sns_client_request_msg_init_default;
  request->suid.suid_low = decoded_event.suid_low;
  request->suid.suid_high = decoded_event.suid_high;
  request->msg_id = SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG;
  request->susp_config.client_proc_type = SNS_STD_CLIENT_PROCESSOR_APSS;
  request->susp_config.delivery_type = SNS_CLIENT_DELIVERY_WAKEUP;
  request->has_client_tech = true;
  request->client_tech = SNS_TECH_SENSORS;
  request->request.payload.funcs.encode = encodePayload;

  sns_std_sensor_config config = sns_std_sensor_config_init_default;
  config.sample_rate = 0.02f;
  
  size_t config_encoded_len = 0;
  if(!pb_get_encoded_size(&config_encoded_len, sns_std_sensor_config_fields, &config))
  {
    //error
    shell_print(shell, "sns_std_sensor_config get encoded size error");
    return 0;
  }
  shell_print(shell, "config_encoded_len = %d", config_encoded_len);
  void* config_buff = pdtsw_heap_alloc(PDTSW_COMMON_HEAP, config_encoded_len);

  pb_ostream_t temp_stream = pb_ostream_from_buffer(config_buff, config_encoded_len);
  if (!pb_encode(&temp_stream, sns_std_sensor_config_fields, &config))
  {
    shell_print(shell, "sns_std_sensor_config encode error");
    return 0;
  }
  shell_print(shell, "sns_std_sensor_config encoded");

  slimz_arg_in_config_payload.buf = config_buff;
  slimz_arg_in_config_payload.buf_len = config_encoded_len;

  request->request.payload.arg = &slimz_arg_in_config_payload;

  size_t req_encoded_len = 0;
  if(!pb_get_encoded_size((size_t *)&req_encoded_len, sns_client_request_msg_fields, request))
  {
    //error
    shell_print(shell, "get encoded size error");
  }
  else
  {
    int ret = qapi_qsh_glink_send(glink_handle, request, req_encoded_len);
    if (ret == 0) {
        shell_print(shell, "Data sent successfully");
    } else {
        shell_print(shell, "Failed to send data: %d", ret);
    }
  }

  return 0;
}