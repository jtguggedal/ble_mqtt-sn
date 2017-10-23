/* Copyright (c) 2017 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include "mqttsn_transport.h"
#include "mqttsn_packet_internal.h"
#include "mem_manager.h"
#include "nrf_log.h"
#include "nrf_error.h"

#include <openthread/udp.h>
#include <openthread/openthread.h>

#include <stdint.h>

#define NULL_PARAM_CHECK(PARAM)                                                                    \
    if ((PARAM) == NULL)                                                                           \
    {                                                                                              \
        return (NRF_ERROR_NULL);                                                                   \
    }

/**@brief OpenThread transport instance. */
static otInstance * m_p_instance;
/**@brief OpenThread UDP socket. */
static otUdpSocket  m_socket;

/**@brief Callback from the OpenThread port. */
static void port_data_callback(void                * p_context,
                               otMessage           * p_message,
                               const otMessageInfo * p_message_info)
{
    const mqttsn_port_t port = p_message_info->mSockPort;
    mqttsn_remote_t     remote_endpoint;

    memcpy(remote_endpoint.addr,  p_message_info->mPeerAddr.mFields.m8, OT_IP6_ADDRESS_SIZE);
    remote_endpoint.port_number = MQTTSN_DEFAULT_GATEWAY_PORT;

    uint16_t  payload_size = otMessageGetLength(p_message) - otMessageGetOffset(p_message);
	uint8_t * p_msg        = nrf_malloc(payload_size);

    if (p_msg)
	{
	    if (otMessageRead(p_message, otMessageGetOffset(p_message), p_msg, payload_size) == payload_size)
	    {
            mqttsn_transport_read(p_context, &port, &remote_endpoint, p_msg, payload_size);
        }
        else
        {
            NRF_LOG_ERROR("Openthread message cannot be read.\r\n");
        }

	    nrf_free(p_msg);
	}
    else
    {
        NRF_LOG_ERROR("Openthread message buffer cannot be allocated (receive_data_callback)\r\n");
    }
}

/**@brief Creates OpenThread network port. */
static uint32_t port_create(mqttsn_client_t * p_client, uint16_t port)
{
    otError    err_code;
    otSockAddr addr;

    memset(&addr, 0, sizeof(addr));

    err_code = otUdpOpen(m_p_instance, &m_socket, port_data_callback, p_client);
    if (err_code == OT_ERROR_NONE)
    {
        addr.mPort = port;
        err_code   = otUdpBind(&m_socket, &addr);
    }
    else
    {
        NRF_LOG_ERROR("UDP Socket cannot be opened\r\n");
    }

    return (err_code == OT_ERROR_NONE) ? NRF_SUCCESS : NRF_ERROR_INTERNAL;
}

uint32_t mqttsn_transport_init(mqttsn_client_t * p_client, uint16_t port, const void * p_context)
{
    m_p_instance = (otInstance *)p_context;
    return port_create(p_client, port);
}

uint32_t mqttsn_transport_write(mqttsn_client_t       * p_client,
                                const mqttsn_remote_t * p_remote,
                                const uint8_t         * p_data,
                                uint16_t                datalen)
{
    NULL_PARAM_CHECK(p_remote);
    NULL_PARAM_CHECK(p_data);

    uint32_t      err_code = NRF_ERROR_INVALID_STATE;
    otMessage   * p_msg    = NULL;
    otUdpSocket * p_socket = &m_socket;

    do
    {
        p_msg = otUdpNewMessage(m_p_instance, true);
        if (p_msg == NULL)
        {
            NRF_LOG_ERROR("Failed to allocate OT message\r\n");
            err_code = NRF_ERROR_NO_MEM;
            break;
        }

        if (otMessageAppend(p_msg, p_data, datalen) != OT_ERROR_NONE)
        {
            NRF_LOG_ERROR("Failed to append message payload\r\n");
            err_code = NRF_ERROR_INTERNAL;
            break;
        }

        otMessageInfo msg_info;

		memset(&msg_info, 0, sizeof(msg_info));
		msg_info.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;
		msg_info.mPeerPort    = p_remote->port_number;

		memcpy(msg_info.mPeerAddr.mFields.m8, p_remote->addr, OT_IP6_ADDRESS_SIZE);
        msg_info.mSockAddr = *otThreadGetMeshLocalEid(m_p_instance);

        if (otUdpSend(p_socket, p_msg, &msg_info) != OT_ERROR_NONE)
        {
            NRF_LOG_ERROR("Failed to send message\r\n");
            err_code = NRF_ERROR_INTERNAL;
            break;
        }

        err_code = NRF_SUCCESS;

    } while(0);

    if ((p_msg != NULL) && (err_code != NRF_SUCCESS))
	{
		otMessageFree(p_msg);
	}

    return err_code;
}

uint32_t mqttsn_transport_read(void                   * p_context,
                               const mqttsn_port_t    * p_port,
                               const mqttsn_remote_t  * p_remote,
                               const uint8_t          * p_data,
                               uint16_t                 datalen)
{
    mqttsn_client_t * p_client = (mqttsn_client_t *)p_context;
    return mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
}

uint32_t mqttsn_transport_uninit(mqttsn_client_t * p_client)
{
    return otUdpClose(&m_socket) == OT_ERROR_NONE ? NRF_SUCCESS : NRF_ERROR_INTERNAL;
}