/*************************************************************************
 * @file     lpai_bt_lecoc_app.c
 * @brief    LPAI BT LECOC App source file.
 *
 * 목적 및 기능:
 * - ADSP Offload Manager에서 전달되는 LECoC socket open/close/data event를 처리한다.
 * - HLOS에서 offload된 socketId를 저장한다.
 * - qapi_bt_lecoc_send_data()를 통해 AWM/wm_proc에서 Mobile로 데이터를 송신한다.
 * - Qualcomm LECoC Micro App API 문서 기준 DATA_RX_IND 수신 후 DATA_RX_RES를 ADSP로 응답한다.
 *
 * Copyright (c) Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 ******************************************************************************/

/*===========================================================================
                        INCLUDE FILES
===========================================================================*/
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>				/* add hyungchul */
#include <zephyr/sys/printk.h>
#include "lpai_bt_lecoc_app.h"
#include <zephyr/kernel.h>
#include "lpai_bt_ble_demo_app.h" /* add hyungchul : HLOS offload socket으로 Hello World demo hook 추가 */
#if CONFIG_QC_M55_DISPLAY_ENABLE
#include "disp_qapi.h"
#endif


lecocApp_t lecocAppInfo;
static uint64_t startTime;
static uint64_t endTime;

static uint64_t startRxTime;
static uint64_t endRxTime;
qapiAudioToneEnable_t audioToneEnable;

#if CONFIG_QC_M55_DISPLAY_ENABLE
qapiDisplayEnable_t displayEnable;
#endif

void qapi_lecoc_evt_handler(uint16_t opcode, uint16_t appDataLen, void *appData);	/* add hyungchul */
void lecoc_app_add_end_point_details()
{
	/*Add End Point Details*/
	lecocAppInfo.endPointDetails.endPointId.hubId = LECOC_ENDPOINT_HUB_ID;
	lecocAppInfo.endPointDetails.endPointId.epId = LECOC_ENDPOINT_ID;
	memscpy(lecocAppInfo.endPointDetails.name,sizeof("LECOC"),"LECOC",sizeof("LECOC"));
	lecocAppInfo.endPointDetails.endPointService.majorVersion = 0x01;
	lecocAppInfo.endPointDetails.endPointService.minorVersion = 0x01;
	memscpy(lecocAppInfo.endPointDetails.endPointService.serviceDescriptor,sizeof("LECOC_SERVICE"),"LECOC_SERVICE",sizeof("LECOC_SERVICE"));

	lecocAppInfo.protocolType = SOCKET_TYPE_LECOC;
	lecocAppInfo.socketsInUse = 0x0;
	for(uint8_t idx = 0; idx < MAX_SOCKET_SUPPORTED; idx++)
	{
		memset((lecocAppInfo.socketDetails + idx), 0 , sizeof(socketDetails_t));
	}
}

/*
 * Register a lecoc app.
 */
void lecoc_app_init()
{
	lecoc_app_add_end_point_details();
	/*Register End Point with BT App Manager*/
	if(lpai_bt_app_mgr_register_endpt_client(lecoc_app_cb,lecocAppInfo.endPointDetails)!= APP_REGISTRATION_SUCCESS)
    {
		printk("Lecoc App callback registration Failed");
    }
}

/**
 * @brief DeInitializes the LECOC microApp.
 *
 * This function resets necessary configurations, data structures, and
 * resources required to start the LECOC MicroApp.
 *
 * @note This function does not take any parameters and does not return a value.
 */
void lecoc_app_deinit()
{
    for(uint8_t idx = 0; idx < MAX_SOCKET_SUPPORTED; idx++)
	{
		memset((lecocAppInfo.socketDetails + idx), 0 , sizeof(socketDetails_t));
	}
    lecocAppInfo.socketsInUse = 0;	/* add hyungchul */
}

/**
 * @brief  Method to Check if the recieved BT Protocol Type matches with the protocol for a given End Point.
 * @param[in]   protocol_type   Protocol Type to be Validated for an End Point
 * @param[out]  None
 * @return      bool            True if protocol matches , false otherwise
 */
static bool lpai_bt_app_mgr_validate_protocol_type(uint8_t protocolType )
{
    return ( lecocAppInfo.protocolType == protocolType);
}

/*
 * Open a lecoc socket.
 */
void lecoc_socket_opened(void *appData)
{
	uint8_t protocolType = 0xFF ;
	uapp_socket_open_rsp_t socketOpenRsp;
	protocolType = ((uapp_socket_open_req_t*)appData)->socket_type;
	socketOpenRsp.socket_id = ((uapp_socket_open_req_t*)appData)->socket_id;
	if(lpai_bt_app_mgr_validate_protocol_type(protocolType))
	{
		if(lecocAppInfo.socketsInUse < MAX_SOCKET_SUPPORTED)
		{
			lecocAppInfo.socketsInUse++;
			socketOpenRsp.status = SOCKET_OPEN_SUCCESS;
			for(uint8_t idx = 0; idx < MAX_SOCKET_SUPPORTED; idx++)
			{
				if(lecocAppInfo.socketDetails[idx].idxInUse == false)
				{
					lecocAppInfo.socketDetails[idx].idxInUse = true;
					lecocAppInfo.socketDetails[idx].socketId = ((uapp_socket_open_req_t*)appData)->socket_id;
					lecocAppInfo.socketDetails[idx].remoteMtu = ((uapp_socket_open_req_t*)appData)->socket_info.lecoc_socket_info.remotemtu;
					lecocAppInfo.socketDetails[idx].pendingOperations = 0;
					lpai_bt_appmgr_send_endpt_msg_adsp(lecocAppInfo.endPointDetails.endPointId.epId,UAPP_OPEN_SOCKET_RES,sizeof(socketOpenRsp),&socketOpenRsp,false);
					break;
				}
			}
		}
		else
		{
			printk("Maximum Number of Sockets Supported Exhausted\n");
			socketOpenRsp.status = SOCKET_OPEN_FAILURE;
			lpai_bt_appmgr_send_endpt_msg_adsp(lecocAppInfo.endPointDetails.endPointId.epId,UAPP_OPEN_SOCKET_RES,sizeof(socketOpenRsp),&socketOpenRsp,false);
		}
	}
	else
	{
		printk("Open Socket Request Does not match with Protocol Type\n");
		socketOpenRsp.status = SOCKET_OPEN_PROTOCOL_MISMATCH;
		lpai_bt_appmgr_send_endpt_msg_adsp(lecocAppInfo.endPointDetails.endPointId.epId,UAPP_OPEN_SOCKET_RES,sizeof(socketOpenRsp),&socketOpenRsp,false);
	}

}

/*
 * Close a lecoc socket.
 */
void lecoc_socket_closed(uint64_t socketId)
{
	for(uint8_t idx = 0; idx < MAX_SOCKET_SUPPORTED; idx++)
	{
		if(lecocAppInfo.socketDetails[idx].socketId == socketId)
		{
            if(lecocAppInfo.socketsInUse > 0)	/* add hyungchul */
            {
                lecocAppInfo.socketsInUse--;
            }
			memset((lecocAppInfo.socketDetails + idx), 0 , sizeof(socketDetails_t));
			return;
		}
	}
	printk("Close Socket Request Does not match with Socket id\n");
}
/*
 * lecoc data Rx ind.
 */
void lecoc_socket_data_rx_ind(uint64_t socketId, uint16_t appDataLen,void *appData)
{
	// for(int i=0;i<appDataLen;i++)
	// {
	// 	printk("%02x ",((uint8_t*)appData)[i]);
	// }
	// printk("\n");
	//printk("lecoc_rx_ind[%d]\n", appDataLen);
	uapp_data_rx_rsp_t rxRsp;
    ARG_UNUSED(appDataLen);		/* add hyungchul */
    ARG_UNUSED(appData);		/* add hyungchul */
	rxRsp.socket_id = socketId;
    lpai_bt_appmgr_send_endpt_msg_adsp(lecocAppInfo.endPointDetails.endPointId.epId, UAPP_DATA_RX_RES, sizeof(rxRsp), &rxRsp, false);
}

/*
 * lecoc data Tx req.
 */
void lecoc_socket_data_tx_req(uint64_t socketId, uint16_t dataLen,void *data)
{
		uapp_data_tx_req_t *tx_req = malloc(sizeof(uapp_data_tx_req_t) + dataLen);
		if(tx_req != NULL)
		{
			tx_req->socket_id = socketId;
			tx_req->data_len =  dataLen;
			memscpy(tx_req->data,dataLen,data,dataLen);
			lpai_bt_appmgr_send_endpt_msg_adsp(lecocAppInfo.endPointDetails.endPointId.epId, UAPP_DATA_TX_REQ, (sizeof(uapp_data_tx_req_t) + dataLen), tx_req,false);
			free(tx_req);
		}
}

/*
* data frame to be send for Tx
*/
static void send_data_frame(uint16_t packetSize, uint8_t *data)
{
	  for(int i=0;i<packetSize;i++)
        {
            data[i] = 0x61;
    	}

}

/*
QAPI for through put testing
*/
qapi_bt_lecoc_status_code_t qapi_bt_lecoc_test_tput_tx(uint64_t EndpointId, uint64_t socketId, uint16_t packetSize, uint16_t numPackets)
{
    ARG_UNUSED(EndpointId);		/* add hyungchul */

	if(packetSize == 0)
	{
		printk("Error: packetSize should be greater than 0\n");
		return QAPI_BT_LECOC_INVALID_OPERATION;
	}
	for(uint8_t idx = 0; idx < MAX_SOCKET_SUPPORTED; idx++)
	{
		if(lecocAppInfo.socketDetails[idx].socketId == socketId)
		{
			 if(lecocAppInfo.socketDetails[idx].remoteMtu > packetSize) 
			 {
				/* change hyungchul */
                if(((lecocAppInfo.socketDetails[idx].pendingOperations & TX_ENABLE) == 0) &&
                   ((lecocAppInfo.socketDetails[idx].pendingOperations & TX_TPUT_ENABLE) == 0))
                {
					/* change hyungchul */
//					lecocAppInfo.socketDetails[idx].pendingOperations = lecocAppInfo.socketDetails[idx].pendingOperations | TX_TPUT_ENABLE;
                    lecocAppInfo.socketDetails[idx].pendingOperations |= TX_TPUT_ENABLE;
					if(lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.totalCnt == 0)
					{
						lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.totalCnt = numPackets;
						lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.txPacketSize = packetSize;
                        lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.curCnt = 0;					/* add hyungchul */

						uint8_t *data = malloc(packetSize);
						if(data != NULL)
						{
							send_data_frame(packetSize, data);
							startTime = k_uptime_get();
							lecoc_socket_data_tx_req(lecocAppInfo.socketDetails[idx].socketId, packetSize, data);
							free(data);
						}
						else
						{
							printk("Failed to allocate memory for TX throughput test\n");
    						lecocAppInfo.socketDetails[idx].pendingOperations &= ~TX_TPUT_ENABLE;
    						lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.totalCnt = 0;
							lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.txPacketSize = 0;
    						return QAPI_BT_LECOC_FAILURE;
						}
					}
				}
				else
				{
					printk("Another Tx Tput or Tx Operation Already in Progress or Enabled\n");
					return QAPI_BT_LECOC_INVALID_OPERATION;
				}
			 }
			 else
			 {
				printk("MTU Size exceed\n");  
				return QAPI_BT_LECOC_MTU_EXCEEDED;
			 }
			 return QAPI_BT_LECOC_SUCCESS;
		}
	}
	return QAPI_BT_LECOC_FAILURE;
}

qapi_bt_lecoc_status_code_t qapi_bt_lecoc_test_tput_rx(uint64_t EndpointId, uint64_t socketId, uint16_t packetSize, uint16_t numPackets)
{
    ARG_UNUSED(EndpointId);		/* add hyungchul */

		if(packetSize == 0)
		{
				printk("Error:packetSize should be greater than 0\n");
				return QAPI_BT_LECOC_INVALID_OPERATION;
		}
        for(uint8_t idx = 0; idx < MAX_SOCKET_SUPPORTED; idx++)
        {
                if(lecocAppInfo.socketDetails[idx].socketId == socketId)
                {
                        if(lecocAppInfo.socketDetails[idx].remoteMtu > packetSize) 
                        {      
							if(((lecocAppInfo.socketDetails[idx].pendingOperations & RX_TPUT_ENABLE) == 0) && ((lecocAppInfo.socketDetails[idx].pendingOperations & LOOPBACK_ENABLE) == 0))
							{
									/* change hyungchul */
//									lecocAppInfo.socketDetails[idx].pendingOperations = lecocAppInfo.socketDetails[idx].pendingOperations | RX_TPUT_ENABLE;
									lecocAppInfo.socketDetails[idx].pendingOperations |= RX_TPUT_ENABLE;
									lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.totalCnt = numPackets;
									lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.txPacketSize = packetSize;
									lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.curCnt = 0;
							}
							else
							{
									printk("Another Rx Tput or Loopback Operation Already in Progress or Enabled\n");
									return QAPI_BT_LECOC_INVALID_OPERATION;
							}
						}
						else
						{
							printk("MTU Size exceed\n");  
							return QAPI_BT_LECOC_MTU_EXCEEDED;  
						}
						return QAPI_BT_LECOC_SUCCESS;
				}
		}
		return QAPI_BT_LECOC_FAILURE;
}


/*
QAPI for send data
*/
qapi_bt_lecoc_status_code_t qapi_bt_lecoc_send_data(uint64_t EndpointId, uint64_t socketId, uint16_t dataLen, uint8_t* data)
{
    ARG_UNUSED(EndpointId);			/* add hyungchul */

	for(uint8_t idx = 0; idx < MAX_SOCKET_SUPPORTED; idx++)
	{
		if(lecocAppInfo.socketDetails[idx].socketId == socketId)
		{
			if(lecocAppInfo.socketDetails[idx].remoteMtu > dataLen) 
			{
				if(((lecocAppInfo.socketDetails[idx].pendingOperations & TX_ENABLE) == 0) && ((lecocAppInfo.socketDetails[idx].pendingOperations & TX_TPUT_ENABLE) == 0))
				{
					/* change hyungchul */
//					lecocAppInfo.socketDetails[idx].pendingOperations =  lecocAppInfo.socketDetails[idx].pendingOperations | TX_ENABLE;
                    lecocAppInfo.socketDetails[idx].pendingOperations |= TX_ENABLE;
					uint8_t *userData = malloc(dataLen);
					if(userData != NULL)
					{
						memscpy(userData, dataLen, data, dataLen);
						lecoc_socket_data_tx_req(lecocAppInfo.socketDetails[idx].socketId, dataLen, userData);
						free(userData);
                        return QAPI_BT_LECOC_SUCCESS;
					}
                    else
                    {
                        printk("Failed to allocate memory for sending LECOC data\n");
						lecocAppInfo.socketDetails[idx].pendingOperations &= ~TX_ENABLE;
                        return QAPI_BT_LECOC_FAILURE;
                    }
				}
				else
				{
					printk("Another Tx Tput or Tx Operation Already in Progress or Enabled\n");
					return QAPI_BT_LECOC_INVALID_OPERATION;
				}
			}
			else
			{
				printk("MTU Size exceed\n");
				return QAPI_BT_LECOC_MTU_EXCEEDED; 
			}
		}
	}
	return QAPI_BT_LECOC_FAILURE;
}

/*
QAPI for loopback test
*/
qapi_bt_lecoc_status_code_t qapi_lecoc_test_loopback(uint64_t EndpointId, uint64_t socketId)
{
    ARG_UNUSED(EndpointId);			/* add hyungchul */

	for(uint8_t idx = 0; idx < MAX_SOCKET_SUPPORTED; idx++)
	{
		if(lecocAppInfo.socketDetails[idx].socketId == socketId )
		{
			if(((lecocAppInfo.socketDetails[idx].pendingOperations & LOOPBACK_ENABLE) == 0) && ((lecocAppInfo.socketDetails[idx].pendingOperations & RX_TPUT_ENABLE) == 0))
			{
				/* change hyungchul */
//				lecocAppInfo.socketDetails[idx].pendingOperations = lecocAppInfo.socketDetails[idx].pendingOperations | LOOPBACK_ENABLE;
                lecocAppInfo.socketDetails[idx].pendingOperations |= LOOPBACK_ENABLE;
			}
			else
			{
				printk("Another Rx Tput or Loopback Operation Already in Progress or Enabled\n");
				return QAPI_BT_LECOC_INVALID_OPERATION;
			}
			return QAPI_BT_LECOC_SUCCESS;
		}
	}
	return QAPI_BT_LECOC_FAILURE;
}

/*
QAPI to request socket close through shell
*/
qapi_bt_lecoc_status_code_t qapi_bt_lecoc_close_socket(uint64_t EndpointId, uint64_t socketId)
{
	qapiLecocSocketCloseReq_t socketCloseReq;
    ARG_UNUSED(EndpointId);						/* add hyungchul */

	socketCloseReq.socketId = socketId;
	socketCloseReq.reason = 0x00;
	for(uint8_t idx = 0; idx < MAX_SOCKET_SUPPORTED; idx++)
	{
		if(lecocAppInfo.socketDetails[idx].socketId == socketId )
		{
			lpai_bt_appmgr_send_endpt_msg_adsp(lecocAppInfo.endPointDetails.endPointId.epId,UAPP_SOCKET_CLOSE_IND,sizeof(socketCloseReq),&socketCloseReq,false);
			return QAPI_BT_LECOC_SUCCESS;
		}
	}
	return QAPI_BT_LECOC_FAILURE;
}

/*
QAPI to fetch the offloaded socket details
*/
qapi_bt_lecoc_status_code_t qapi_bt_lecoc_get_socketdetails()
{
	if(lecocAppInfo.socketsInUse>0)
	{
		for(uint8_t idx = 0; idx < MAX_SOCKET_SUPPORTED; idx++)
		{
            if(lecocAppInfo.socketDetails[idx].idxInUse)	/* add hyungchul */
            {
                printk("Socket ID = %" PRIu64 "\n", lecocAppInfo.socketDetails[idx].socketId);
            }
        }
    }
	else
	{
		return QAPI_BT_LECOC_NO_SOCKET_DATA;
	}
	return QAPI_BT_LECOC_SUCCESS;
}

#if defined(CONFIG_PDTSW_AUDIO_SERVICE_ENABLE)
void qapi_bt_lecoc_audio_tone_enable(bool enable, uint16_t filterDataLen, uint8_t* filterData)
{
	printk("qapi_bt_lecoc_audio_tone_enable\n");
	if(enable == false)
	{
		if(audioToneEnable.filterData != NULL)
		{
			free(audioToneEnable.filterData);
		}
		memset(&audioToneEnable, 0, sizeof(audioToneEnable));
	}
	else
	{
		if(audioToneEnable.filterData != NULL)
        {
            free(audioToneEnable.filterData);
        }
		audioToneEnable.audioEnable = true;
		audioToneEnable.filterDataLen = filterDataLen;
		audioToneEnable.filterData = malloc(filterDataLen);
		if(audioToneEnable.filterData != NULL)
		{
			memscpy(audioToneEnable.filterData, filterDataLen, filterData, filterDataLen);
		}
		else
        {
            printk("Failed to allocate memory for filter data\n");
            audioToneEnable.audioEnable = false;
            audioToneEnable.filterDataLen = 0;
        }
	}
}
#endif

#if CONFIG_QC_M55_DISPLAY_ENABLE
void qapi_bt_lecoc_display_enable(bool enable, uint16_t filterDataLen, uint8_t* filterData)
{
	if(enable == false)
	{
		if(displayEnable.filterData != NULL)
		{
			free(displayEnable.filterData);
		}
		memset(&displayEnable, 0, sizeof(displayEnable));
	}
	else
	{
		if(displayEnable.filterData != NULL)
        {
            free(displayEnable.filterData);
        }
		displayEnable.displayEnable = true;
		displayEnable.filterDataLen = filterDataLen;
		displayEnable.filterData = malloc(filterDataLen);
		if(displayEnable.filterData != NULL)
		{
			memscpy(displayEnable.filterData, filterDataLen, filterData, filterDataLen);
		}
		else
        {
            printk("qapi_bt_lecoc_display_enable: Failed to allocate memory for filter data\n");
            displayEnable.displayEnable = false;
            displayEnable.filterDataLen = 0;
        }
	}
}
#endif

/*
 * Handle event received from BT app mgr
 */
void lecoc_app_cb(uint64_t endPointId, uint16_t eventId , uint16_t appDataLen , void *appData , bool proto_encoded)
{
    ARG_UNUSED(proto_encoded);		/* add hyungchul */

	switch(eventId)
	{
		case UAPP_OPEN_SOCKET_REQ:
		{
			qapiLecocSocketOpened_t socketOpened;
            uapp_socket_open_req_t *req = (uapp_socket_open_req_t*)appData;
            socketOpened.endPointId.epId = endPointId;
            socketOpened.endPointId.hubId = LECOC_ENDPOINT_HUB_ID;
            socketOpened.socketId = req->socket_id;
            socketOpened.remoteMtu = (uint16_t)req->socket_info.lecoc_socket_info.remotemtu;
			lecoc_socket_opened(appData);
            qapi_lecoc_evt_handler(QAPI_BT_LECOC_SOCKET_OPENED, sizeof(socketOpened),&socketOpened);

            /* add hyungchul : JPEG demo에 socket open event 전달 */
            ble_demo_app_on_lecoc_socket_opened(socketOpened.socketId, socketOpened.remoteMtu);
			break;
		}
		case UAPP_SOCKET_CLOSE_CMD:
		{
			socketClosed_t socketClosed;
            socketClosed.socketId = ((uapp_socket_close_cmd_t*)appData)->socket_id;
            lecoc_socket_closed(socketClosed.socketId);

            qapi_lecoc_evt_handler(QAPI_BT_LECOC_SOCKET_CLOSED, sizeof(socketClosed),&socketClosed);

            /* add hyungchul : Hello World demo에 socket close event 전달 */
            ble_demo_app_on_lecoc_socket_closed(socketClosed.socketId);
			break;
		}
		case UAPP_DATA_RX_IND:
		{
			qapiLecocDataRxInd_t *evt = (qapiLecocDataRxInd_t*)appData;
            qapiLecocDataTxCfm_t dataTxCfm = {.socketId=evt->socketId};
			//printk("Rx Indication Received for LECOC APP\n");

			for(uint8_t idx = 0; idx < MAX_SOCKET_SUPPORTED; idx++)
			{
				if(lecocAppInfo.socketDetails[idx].socketId == evt->socketId)
				{
					lecoc_socket_data_rx_ind(evt->socketId, evt->dataLen, evt->data);
					qapi_lecoc_evt_handler(QAPI_BT_LECOC_SOCKET_DATA_IND, appDataLen, appData);
					if((lecocAppInfo.socketDetails[idx].pendingOperations & LOOPBACK_ENABLE) == LOOPBACK_ENABLE)
					{
						printk("Rx Indication Received for lecoc loopback\n");
						lecoc_socket_data_tx_req(lecocAppInfo.socketDetails[idx].socketId, evt->dataLen, evt->data);
						printk("Complete Data Tx Transfer for LECOC loopback Done\n");
						lecocAppInfo.socketDetails[idx].pendingOperations &= ~LOOPBACK_ENABLE;
						qapi_lecoc_evt_handler(QAPI_BT_LECOC_SOCKET_DATA_LOOPBACK, sizeof(*(evt)),evt);
					}
                    else if ((lecocAppInfo.socketDetails[idx].pendingOperations & RX_TPUT_ENABLE) == RX_TPUT_ENABLE)
                    {
                        //start the timer for 1st received packet
                        if (lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.curCnt == 0)
                        {
                            //printk("start timer\n");
                            startRxTime = k_uptime_get();
                        }
                        if(lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.curCnt < lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.totalCnt)
                        {
                            //printk("pkt_cnt %d %d\n", lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.curCnt, evt->dataLen);
                            lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.curCnt++;
                        }
                        if (lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.curCnt == lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.totalCnt)
                        {
                            //printk("last pkt\n");
                            endRxTime = k_uptime_get();
                            double tput = (lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.totalCnt *
                                           lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.txPacketSize * 8) /
                                          ((double)(endRxTime - startRxTime));
							//printk("Complete Data Rx Transfer for LECOC throughput Done\n");
                            printk("LECOC RX throught =  %.2f kbps\n", tput);
							/* change hyungchul */
                            lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.curCnt = 0;
                            lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.txPacketSize = 0;	/* add hyungchul */
                            lecocAppInfo.socketDetails[idx].rxOperations.tputPktDetail.totalCnt = 0;		/* add hyungchul */
                            lecocAppInfo.socketDetails[idx].pendingOperations &= ~RX_TPUT_ENABLE;
                            dataTxCfm.status = QAPI_BT_LECOC_SUCCESS;
                            qapi_lecoc_evt_handler(QAPI_BT_LECOC_SOCKET_DATA_THROUGHPUT, sizeof(dataTxCfm),&dataTxCfm);
                        }
                    }

				}
			}
			break;
		}
		case UAPP_DATA_TX_RES:
		{
			qapiLecocDataTxCfm_t dataTxCfm;
			dataTxCfm.socketId = ((qapiLecocDataTxCfm_t*)appData)->socketId;
            dataTxCfm.status = ((qapiLecocDataTxCfm_t*)appData)->status;			/* add hyungchul */


			for(uint8_t idx = 0; idx < MAX_SOCKET_SUPPORTED; idx++)
			{
				if(lecocAppInfo.socketDetails[idx].socketId == dataTxCfm.socketId)
				{
					if((lecocAppInfo.socketDetails[idx].pendingOperations & TX_TPUT_ENABLE) == TX_TPUT_ENABLE)
					{

                        lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.curCnt++;
						if(lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.curCnt < lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.totalCnt)
						{
							uint8_t *data = malloc(lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.txPacketSize);
							if(data != NULL)
							{
								send_data_frame(lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.txPacketSize, data);
								lecoc_socket_data_tx_req(lecocAppInfo.socketDetails[idx].socketId, lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.txPacketSize, data);
								free(data);
							}
							else
							{
								printk("Failed to allocate memory during TX throughput test\n");
        						lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.curCnt = 0;
								lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.totalCnt = 0;
								lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.txPacketSize = 0;
								lecocAppInfo.socketDetails[idx].pendingOperations &= ~TX_TPUT_ENABLE;
							}
						}
						if(lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.curCnt == lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.totalCnt)
						{
							//printk("Complete Data Tx Transfer for LECOC throughput Done\n");
							endTime = k_uptime_get();
                            double tput = (lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.totalCnt * lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.txPacketSize *  8)/((double)(endTime - startTime));
                            printk("LECOC TX throught =  %.2f kbps\n", tput);
							/* change hyungchul */
//							lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.curCnt = lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.totalCnt = lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.txPacketSize = 0;
                            lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.curCnt = 0;
                            lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.totalCnt = 0;		/* add hyungchul */
                            lecocAppInfo.socketDetails[idx].txOperations.tputPktDetail.txPacketSize = 0;	/* add hyungchul */
							lecocAppInfo.socketDetails[idx].pendingOperations &= ~TX_TPUT_ENABLE;
							qapi_lecoc_evt_handler(QAPI_BT_LECOC_SOCKET_DATA_THROUGHPUT, sizeof(dataTxCfm),&dataTxCfm);
						}
					}
/* change hyungchul					
					else if((lecocAppInfo.socketDetails[idx].pendingOperations & LOOPBACK_ENABLE) == LOOPBACK_ENABLE)
					{
						// printk("Complete Data Tx Transfer for LECOC loopback Done\n");
						// lecocAppInfo.socketDetails[idx].pendingOperations = 0;
						// qapi_lecoc_evt_handler(QAPI_BT_LECOC_SOCKET_DATA_LOOPBACK, sizeof(dataTxCfm),&dataTxCfm);
					} */					
					else if((lecocAppInfo.socketDetails[idx].pendingOperations & TX_ENABLE)  == TX_ENABLE)
					{
						qapi_lecoc_evt_handler(QAPI_BT_LECOC_SOCKET_DATA_TX_CFM, sizeof(dataTxCfm),&dataTxCfm);
						lecocAppInfo.socketDetails[idx].pendingOperations &= ~TX_ENABLE;

                        /* add hyungchul : JPEG demo에 TX confirm 전달.
                         * TX_ENABLE을 먼저 clear한 뒤 다음 fragment work를 schedule해야
                         * qapi_bt_lecoc_send_data()가 INVALID_OPERATION으로 막히지 않는다. */
                        ble_demo_app_on_lecoc_data_tx_cfm(dataTxCfm.socketId, dataTxCfm.status);
					}
				}
			}
            break;
        }

		default:
		{
			printk("Undefined event\n");
		}
	}
}

#if CONFIG_QC_M55_DISPLAY_ENABLE
static disp_qapi_ret_t display_notification(char *notify_str, int data_len) {

    disp_event_notify_type_t notify_type = DISP_EVENT_NOTIFY_BT;
    disp_qapi_ret_t rc = DISP_QAPI_RET_SUCCESS;
    disp_bt_notify_t event = {
    		DISP_EVENT_BT_RAW_NOTIFICATION,
    		(void*)notify_str,
    		data_len
    };

    /* send notification to display */
    rc = disp_event_notify(notify_type, (void *) (&event));
	return rc;
}
#endif




static struct k_work_delayable highcam_work;
static bool is_highcam_work_initialized = false;


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

int highcam_delay = 60000;
/**
 * @brief 가상으로 파워 버튼 누름 이벤트를 생성하여 던지는 함수
 */
volatile uint8_t *const ble_shared_buffer = (volatile uint8_t *)0x20488000;
void trigger_virtual_power_button_press(void)
{
   uint8_t ble_control = ble_shared_buffer[0x1ff10];

   if(ble_control == 0x1)
   {
    // 1. 버튼 인스턴스 지정 (보통 파워 버튼은 0번이거나 고정된 enum 값이 있습니다)
    // 예: API_PM_BUTTON_PWR_BTN 등 프로젝트 헤더에 맞는 값 대입
    api_PM_Button_Instance_t fake_btn = (api_PM_Button_Instance_t)PMIC_BUTTON_1;

    // 2. 이벤트 타입 지정 (단순 누름: PRESS / RELEASE 등 헤더 확인 필요)
    api_PM_Button_Event_t fake_event = (api_PM_Button_Event_t)PMIC_BUTTON_PRESS;

    // 3. 현재 타임스탬프 획득 (Zephyr 커널 시간 활용)
    // k_uptime_get()은 ms 단위의 현재 업타임을 반환합니다.
    uint64_t current_ts_ms = (uint64_t)k_uptime_get();

    printk("[PERIODIC] Input Highcam Key\n");

    // 4. 원래의 콜백 함수를 강제로 호출하여 디스플레이 매니저 쓰레드로 전달
    disp_button_cb(fake_btn, fake_event, current_ts_ms);

   }
   else
   {
    printk("HIGH CAM OFF!!!!!!\n");
   }
}

static void highcam_work_handler(struct k_work *work)
{
    //printk("[PERIODIC] highcam_work_handler\n");
    //disp_button_notify_to_apps(0x2);
    trigger_virtual_power_button_press();

    // 10초(K_MSEC(10000)) 후에 다시 자기 자신을 스케줄링 (무한 반복)
    k_work_reschedule(&highcam_work, K_MSEC(highcam_delay));
}

void start_periodic_highcam(void)
{
    if (!is_highcam_work_initialized) {
        k_work_init_delayable(&highcam_work, highcam_work_handler);
        is_highcam_work_initialized = true;
    }
    trigger_virtual_power_button_press();
    // 호출 즉시 10초 뒤로 스케줄링 시작
    k_work_reschedule(&highcam_work, K_MSEC(highcam_delay));
    printk("Periodic highcam_work Started!! %d\n", highcam_delay);
}

void stop_periodic_highcam(void)
{
    if (!is_highcam_work_initialized) {
        k_work_init_delayable(&highcam_work, highcam_work_handler);
        is_highcam_work_initialized = true;
    }
    // 호출 즉시 10초 뒤로 스케줄링 시작
    k_work_cancel_delayable(&highcam_work);
    printk("Periodic highcam_work End!!\n");
}
/**
 * @brief 10초마다 주기적으로 현재 BT Status를 printk로 출력하는 워크 핸들러
 */
void qapi_lecoc_evt_handler(uint16_t opcode,uint16_t appDataLen , void *appData)
{
    ARG_UNUSED(appDataLen);		/* add hyungchul */

        switch(opcode)
        {
			case QAPI_BT_LECOC_SOCKET_OPENED:
			{
                qapiLecocSocketOpened_t* ind = (qapiLecocSocketOpened_t*)appData;
				printk("QAPI_BT_LECOC_SOCKET_OPENED\n");
                printk("socketId : %" PRIu64 ", remoteMtu : %u\n", ind->socketId, ind->remoteMtu);
				break;
			}
			case QAPI_BT_LECOC_SOCKET_CLOSED:
			{
				printk("QAPI_BT_LECOC_SOCKET_CLOSED\n");
				break;
			}
			case QAPI_BT_LECOC_SOCKET_DATA_TX_CFM:
			{
			/* add hyungchul */
            qapiLecocDataTxCfm_t* cfm = (qapiLecocDataTxCfm_t*)appData;
			/* change hyungchul */
            //printk("QAPI_BT_LECOC_SOCKET_DATA_TX_CFM socketId=%" PRIu64 " status=%u\n",
             //      cfm->socketId,
             //      cfm->status);
				break;
			}
			case QAPI_BT_LECOC_SOCKET_DATA_LOOPBACK:
			{
				printk("QAPI_BT_LECOC_SOCKET_DATA_LOOPBACK\n");
				break;
			}
			case QAPI_BT_LECOC_SOCKET_DATA_THROUGHPUT:
			{
				printk("QAPI_BT_LECOC_SOCKET_DATA_THROUGHPUT\n");
				break;
			}
			case QAPI_BT_LECOC_SOCKET_DATA_IND:
			{
				//printk("QAPI_BT_LECOC_SOCKET_DATA_IND\n");
				/* TBD will enable after testing audio tone*/
				qapiLecocDataRxInd_t *ind = (qapiLecocDataRxInd_t *)appData;
				/* add hyungchul */
	            printk("QAPI_BT_LECOC_SOCKET_DATA_IND socketId=%" PRIu64 " dataLen=%d\n",
	                   ind->socketId,
	                   ind->dataLen);

		            if((ind->dataLen > 200) && (ind->dataLen < 400))
			    {
				char buffer[10];
				int t_len = 10;

				int state_on = -1;

				char* target[] = {"PUSH_ACTION", "img-high","state","delay"};
                                int found_index = -1;
				printk("HYL#0 %d, %c%c%c)\n", found_index, ind->data[238], ind->data[239], ind->data[240]);
                                for (int j = 0; j <= ind->dataLen - t_len; j++) {

                                    for (int k = 0; k < t_len; k++) {
                                        buffer[k] = ind->data[j + k];
                                    }
                                  
				    int match_PUSH_ACTION = 1; // 일치한다고 가정
                                    for (int k = 0; k < t_len; k++) {
                                        if (buffer[k] != target[0][k]) {
                                            match_PUSH_ACTION = 0; // 한 글자라도 다르면 탈락
                                            break;
                                        }
                                    }

                                    if (match_PUSH_ACTION == 1) {
                                       found_index = j;

				       printk("HYL#1 PUSH_ACTION: %d)\n", found_index);
			 	 
				       //============== PUSH_ACTION OK
		 		       char buffer2[8];
			  	       int t_len2 = 8;
                                       found_index = -1;
                                       for (int j = 0; j <= ind->dataLen - t_len2; j++) {
 
                                          for (int k = 0; k < t_len2; k++) {
                                             buffer2[k] = ind->data[j + k];
                                          }
                                  
				        int match_imghigh = 1; // 일치한다고 가정
                                        for (int k = 0; k < t_len2; k++) {
                                            if (buffer2[k] != target[1][k]) {
                                                match_imghigh = 0; // 한 글자라도 다르면 탈락
                                                break;
                                            }
                                         }

                                        if (match_imghigh == 1) {
                                            found_index = j;

				            printk("HYL#2 img-high: %d)\n", found_index);

				  	    //===================== img-high OK
			 	            char buffer3[5];
			  	            int t_len3 = 5;
                                            found_index = -1;
                                            for (int j = 0; j <= ind->dataLen - t_len3; j++) {
 
                                                for (int k = 0; k < t_len3; k++) {
                                                   buffer3[k] = ind->data[j + k];
                                                }
                                  
				                int match_state = 1; // 일치한다고 가정
                                                for (int k = 0; k < t_len3; k++) {
                                                    if (buffer3[k] != target[2][k]) {
                                                        match_state = 0; // 한 글자라도 다르면 탈락
                                                        break;
                                                    }
                                                }

                                                if (match_state == 1) {
                                                    found_index = j;

				                    printk("HYL#3 state: %d %c%c%c)\n", found_index, ind->data[found_index+9],ind->data[found_index+10],ind->data[found_index+11]);
						    //================== state OK
						    if(ind->data[found_index+9] == 'n')
						    {
				                       printk("HYL#3 state: ON!\n");
						       state_on = found_index;
						    }
						    else if(ind->data[found_index+9] == 'f')
						    {
				                       printk("HYL#3 state: OFF!\n");
						       stop_periodic_highcam();
						    }
                                                    break;//if (match_state == 1) 
                                                }
                                            }
				  	    break; //if (mmatch_imghigh == 1)

                                         }
                                      }
                                      break; //if (match_PUSH_ACTION == 1)
                                  }

			        }

			        if(state_on != -1)
				{
				    printk("HYL4 find Delay\n");


			 	            char buffer4[5];
			  	            int t_len4 = 5;
                                            found_index = -1;
                                            for (int j = 0; j <= ind->dataLen - t_len4; j++) {
 
                                                for (int k = 0; k < t_len4; k++) {
                                                   buffer4[k] = ind->data[j + k];
                                                }
                                  
				                int match_delay = 1; // 일치한다고 가정
                                                for (int k = 0; k < t_len4; k++) {
                                                    if (buffer4[k] != target[3][k]) {
                                                        match_delay = 0; // 한 글자라도 다르면 탈락
                                                        break;
                                                    }
                                                }

                                                if (match_delay == 1) {
                                                    found_index = j;

				                    printk("HYL#4 delay: %d)\n", found_index);
						    //================== state OK
                                                    break;
                                                }
                                            }

                                    // delay가 발견되었으면 뒤쪽 숫자 ASCII를 int로 변환
                                    if (found_index >= 0) {

                                      int pos = found_index + 5;
                                      int delay_value = -1;

                                      /*
                                       * ':' 위치 찾기
                                       * 예: "delay":300
                                       */
                                        while (pos < ind->dataLen && ind->data[pos] != ':') {
                                              pos++;
                                        }

                                        if (pos < ind->dataLen && ind->data[pos] == ':') {

                                          pos++;

                                          // 공백 skip
                                          while (pos < ind->dataLen &&
                                              (ind->data[pos] == ' ' ||
                                               ind->data[pos] == '\t' ||
                                               ind->data[pos] == '\r' ||
                                               ind->data[pos] == '\n')) {
                                               pos++;
                                          }

                                          int value = 0;
                                          int digit_found = 0;

                                          while (pos < ind->dataLen &&
                                              ind->data[pos] >= '0' &&
                                              ind->data[pos] <= '9') {

                                              value = value * 10 + (ind->data[pos] - '0');
                                              digit_found = 1;
                                              pos++;
                                          }

                                          if (digit_found) {
                                              delay_value = value;

                                              printk("HYL delay_value=%d\n", delay_value);

					      highcam_delay = delay_value * 1000;
                                              start_periodic_highcam();
                                          } else {
                                              printk("HYL delay value digit not found\n");
                                          }
                                       } else {
                                        printk("HYL delay ':' not found\n");
                                       }
                                   }

				}

			    }
					#if defined(CONFIG_PDTSW_AUDIO_SERVICE_ENABLE)
				/* change hyungchul */
	            if(audioToneEnable.audioEnable && audioToneEnable.filterData != NULL &&
	               audioToneEnable.filterDataLen <= ind->dataLen &&
	               memcmp(ind->data, audioToneEnable.filterData, audioToneEnable.filterDataLen) == 0)
	            {
	                printk("LECOC audio tone filter matched\n");
	            }
				#endif

				#if CONFIG_QC_M55_DISPLAY_ENABLE
				/* change hyungchul */
	            if(displayEnable.displayEnable && displayEnable.filterData != NULL &&
	               displayEnable.filterDataLen <= ind->dataLen &&
	               memcmp(ind->data, displayEnable.filterData, displayEnable.filterDataLen) == 0)
	            {
					/* show the notification if the prefix matches */
					disp_qapi_ret_t result = display_notification((char *)ind->data, ind->dataLen);
					if(result != 0)
					{
						printk("Error: Failed to display the notification %d\n", result);
						return;
					}
	            }
				#endif
            break;
        }
		
		/* add hyungchul */
        default:											
        {
            printk("QAPI LECOC undefined opcode = 0x%x\n", opcode);
            break;
        }
    }
}
