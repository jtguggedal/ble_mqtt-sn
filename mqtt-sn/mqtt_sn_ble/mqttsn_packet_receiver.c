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

#include <string.h>
#include "mqttsn_packet_internal.h"

#define MQTTSN_PACKET_PINGREQ_LENGTH 2

/**@brief PINGREQ message buffer. */
static uint8_t mp_pingreq_msg[MQTTSN_PACKET_PINGREQ_LENGTH + MQTTSN_CLIENT_ID_MAX_LENGTH];

uint32_t mqttsn_packet_msgtype_index_get(const uint8_t * p_buffer)
{
    if (p_buffer[0] == MQTTSN_TWO_BYTE_LENGTH_CODE)
    {
        return MQTTSN_OFFSET_TWO_BYTE_LENGTH;
    }
    else
    {
        return MQTTSN_OFFSET_ONE_BYTE_LENGTH;
    }
}

mqttsn_ack_error_t mqttsn_packet_msgtype_error_get(const uint8_t * p_buffer)
{
    uint32_t msg_type = p_buffer[mqttsn_packet_msgtype_index_get(p_buffer)];
    
    switch (msg_type)
    {
        case 0x04:
            return MQTTSN_PACKET_CONNACK;

        case 0x0a:
            return MQTTSN_PACKET_REGACK;

        case 0x0c:
            return MQTTSN_PACKET_PUBACK;

        case 0x12:
            return MQTTSN_PACKET_SUBACK;

        case 0x14:
            return MQTTSN_PACKET_UNSUBACK;

        case 0x16:
            return MQTTSN_PACKET_PINGREQ;

        case 0x1a:
            return MQTTSN_PACKET_WILLTOPICUPD;

        case 0x1c:
            return MQTTSN_PACKET_WILLMSGUPD;

        default:
            return MQTTSN_PACKET_INCORRECT;
    }
}

/**@brief Creates keep-alive PINGREQ packet.  
 *
 * @param[inout] p_client Pointer to an MQTT-SN client instance. 
 */
static void pingreq_packet_create(mqttsn_client_t * p_client)
{
    MQTTSNString client_id = MQTTSNString_initializer;
    client_id.cstring      = nrf_malloc(MQTTSN_CLIENT_ID_MAX_LENGTH);
    if (client_id.cstring == NULL)
    {
        return;
    }

    memset(client_id.cstring, 0, MQTTSN_CLIENT_ID_MAX_LENGTH);
    memcpy(client_id.cstring, p_client->connect_info.p_client_id, p_client->connect_info.client_id_len);
    uint16_t pingreq_len = MQTTSN_PACKET_PINGREQ_LENGTH + p_client->connect_info.client_id_len;
    uint16_t datalen     = MQTTSNSerialize_pingreq(mp_pingreq_msg, pingreq_len, client_id);
    if (datalen == 0)
    {
        nrf_free(client_id.cstring);
        return;
    }
    
    p_client->keep_alive.message.retransmission_cnt = MQTTSN_DEFAULT_RETRANSMISSION_CNT + 1;
    p_client->keep_alive.message.p_data             = mp_pingreq_msg;
    p_client->keep_alive.message.len                = datalen;

    nrf_free(client_id.cstring);
}

/**@brief Handles sleep permission received from the gateway. 
 *
 * @param[inout] p_client Pointer to an MQTT-SN client instance. 
 */
static void sleep_handle(mqttsn_client_t * p_client)
{
    mqttsn_event_t evt = { .event_id = MQTTSN_EVENT_SLEEP_PERMIT };
    p_client->evt_handler(p_client, &evt);
}

/**@brief Handles GWINFO message received from the gateway. 
 *
 * @note Unless client is searching for gateway, GWINFO message is ignored.
 *
 * @param[inout] p_client    Pointer to an MQTT-SN client instance.
 * @param[in]    p_remote    Pointer to remote endpoint - the gateway's address.
 * @param[in]    p_data      Received data.
 * @param[in]    datalen     Length of the received data.
 *
 * @retval       NRF_SUCCESS In all cases.
 */
static uint32_t gwinfo_handle(mqttsn_client_t        * p_client,
                              const mqttsn_remote_t  * p_remote,
                              const uint8_t          * p_data,
                              uint16_t                 datalen)
{
    if (p_client->client_state == MQTTSN_CLIENT_SEARCHING_GATEWAY)
    {
        mqttsn_remote_t temp_remote;
        memset(&temp_remote, 0,        sizeof(mqttsn_remote_t));
        memcpy(&temp_remote, p_remote, sizeof(mqttsn_remote_t));

        p_client->client_state = MQTTSN_CLIENT_GATEWAY_FOUND;

        mqttsn_event_t evt =
        {
            .event_id             = MQTTSN_EVENT_GATEWAY_FOUND,
            .event_data.connected = 
            {
                .gateway_id     = p_data[MQTTSN_OFFSET_GATEWAY_INFO_ID],
                .p_gateway_addr = &temp_remote,
            },
        };

        p_client->evt_handler(p_client, &evt);
    }

    return NRF_SUCCESS;
}

/**@brief Handles WILLTOPICREQ message received from the gateway.  
 *
 * @param[inout] p_client Pointer to an MQTT-SN client instance. 
 */
static uint32_t willtopicreq_handle(mqttsn_client_t * p_client)
{
    return mqttsn_packet_sender_willtopic(p_client);
}

/**@brief Handles WILLMSGREQ message received from the gateway.  
 *
 * @param[inout] p_client Pointer to an MQTT-SN client instance. 
 */
static uint32_t willmsgreq_handle(mqttsn_client_t * p_client)
{
    return mqttsn_packet_sender_willmsg(p_client);
}

/**@brief Handles CONNACK message received from the gateway.  
 *
 * @param[inout] p_client    Pointer to an MQTT-SN client instance. 
 * @param[in]    p_data      Received data.
 * @param[in]    datalen     Length of the received data.
 *
 * @retval       NRF_SUCCESS        If CONNECT message was accepted or rejected due to congestion
 *                                  and processed successfully.
 * @retval       NRF_ERROR_INTERNAL Otherwise.
 */
static uint32_t connack_handle(mqttsn_client_t * p_client,
                               const uint8_t   * p_data,
                               uint16_t          datalen)
{
    uint32_t return_code = 0;
    if (p_client->client_state != MQTTSN_CLIENT_ESTABLISHING_CONNECTION)
    {
        NRF_LOG_ERROR("CONNACK packet was not expected.\r\n");
        return NRF_ERROR_INTERNAL;
    }

    if (MQTTSNDeserialize_connack((int *)(&return_code), (unsigned char *)(p_data), datalen) == 0)
    {
        NRF_LOG_ERROR("CONNACK packet cannot be deserialized.\r\n");
        return NRF_ERROR_INTERNAL;
    }
    
    // Change: Moved declarations out of switch to avoid compile warnings
    mqttsn_event_t evt_rc;
    mqttsn_event_t evt_acc;

    switch(return_code)
    {
        case MQTTSN_RC_ACCEPTED:
            mqttsn_packet_fifo_elem_dequeue(p_client, MQTTSN_MSGTYPE_CONNECT, MQTTSN_MESSAGE_TYPE);
        
            pingreq_packet_create(p_client);
            p_client->keep_alive.duration = p_client->connect_info.alive_duration * 1000;
            p_client->keep_alive.timeout  = mqttsn_platform_timer_set_in_ms(p_client->keep_alive.duration);
            mqttsn_platform_timer_start(p_client, p_client->keep_alive.timeout);
        
            p_client->client_state = MQTTSN_CLIENT_CONNECTED;
            evt_rc.event_id = MQTTSN_EVENT_CONNECTED;
            p_client->evt_handler(p_client, &evt_rc);
            return NRF_SUCCESS;

        case MQTTSN_RC_REJECTED_CONGESTED:
            NRF_LOG_INFO("Connect message was rejected. Reason: congestion.\r\n");
            evt_acc.event_id = MQTTSN_EVENT_TIMEOUT,
            evt_acc.event_data.error.error    = MQTTSN_ERROR_REJECTED_CONGESTION;
            evt_acc.event_data.error.msg_type = mqttsn_packet_msgtype_error_get(p_data);
            evt_acc.event_data.error.msg_id   = 0;
        
            mqttsn_packet_fifo_elem_dequeue(p_client, MQTTSN_MSGTYPE_CONNECT, MQTTSN_MESSAGE_TYPE);
            p_client->evt_handler(p_client, &evt_acc);
            return NRF_SUCCESS;

        default:
            NRF_LOG_ERROR("Connect message was rejected. Reason: %d\r\n", return_code);
            return NRF_ERROR_INTERNAL;
    }
}

/**@brief Handles REGISTER message received from the gateway.  
 *
 * @param[inout] p_client    Pointer to an MQTT-SN client instance. 
 * @param[in]    p_data      Received data.
 * @param[in]    datalen     Length of the received data.
 *
 * @retval       NRF_SUCCESS        If REGACK message was sent successfully in response.
 * @retval       NRF_ERROR_INTERNAL Otherwise.
 */
static uint32_t register_handle(mqttsn_client_t * p_client,
                                const uint8_t   * p_data,
                                uint16_t          datalen)
{
    uint32_t err_code  = NRF_ERROR_INVALID_STATE;
    uint16_t packet_id = 0;
    uint16_t topic_id  = 0;
    MQTTSNString ret_topic;

    if (MQTTSNDeserialize_register(&topic_id, &packet_id, &ret_topic, (unsigned char *)p_data, datalen) == 0)
    {
        NRF_LOG_ERROR("REGISTER packet cannot be deserialized\r\n");
        return NRF_ERROR_INTERNAL;
    }

    err_code = mqttsn_packet_sender_regack(p_client, topic_id, packet_id, MQTTSN_RC_ACCEPTED);

    mqttsn_topic_t topic =
    {
        .topic_id = topic_id,
        .p_topic_name = (unsigned char *)ret_topic.cstring,
    };
    mqttsn_event_t evt =
    {
        .event_id = MQTTSN_EVENT_REGISTER_RECEIVED,
        .event_data.registered =
            {
                .packet = { .id    = packet_id,
                            .topic = topic },
            },
    };
    p_client->evt_handler(p_client, &evt);

    return err_code;
}

/**@brief Handles REGACK message received from the gateway.  
 *
 * @param[inout] p_client    Pointer to an MQTT-SN client instance. 
 * @param[in]    p_data      Received data.
 * @param[in]    datalen     Length of the received data.
 *
 * @retval       NRF_SUCCESS        If REGISTER message was accepted or rejected due to congestion
 *                                  and processed successfully.
 * @retval       NRF_ERROR_INTERNAL Otherwise.
 */
static uint32_t regack_handle(mqttsn_client_t * p_client,
                              const uint8_t   * p_data,
                              uint16_t          datalen)
{
    uint16_t topic_id    = 0;
    uint16_t packet_id   = 0;
    uint8_t  return_code = 0;
    uint32_t index       = 0;
        
    // Fix: Moved declarations out of switch to avoid warnings
    mqttsn_event_t evt_rc;
    mqttsn_event_t evt_acc;
    mqttsn_topic_t topic;

    if (MQTTSNDeserialize_regack(&topic_id, &packet_id, (unsigned char *)(&return_code), (unsigned char *)p_data, datalen) == 0)
    {
        NRF_LOG_ERROR("REGACK packet cannot be deserialized.\r\n");
        return NRF_ERROR_INTERNAL;
    }

    
    switch (return_code)
    {
        case MQTTSN_RC_REJECTED_CONGESTED:
            NRF_LOG_INFO("Register message was rejected. Reason: congestion.\r\n");
            evt_rc.event_id = MQTTSN_EVENT_TIMEOUT;
            evt_rc.event_data.error.error    = MQTTSN_ERROR_REJECTED_CONGESTION;
            evt_rc.event_data.error.msg_type = mqttsn_packet_msgtype_error_get(p_data);
            evt_rc.event_data.error.msg_id   = 0;
        
            mqttsn_packet_fifo_elem_dequeue(p_client, packet_id, MQTTSN_MESSAGE_ID);
            p_client->evt_handler(p_client, &evt_rc);
            return NRF_SUCCESS;

        case MQTTSN_RC_ACCEPTED:
            index = mqttsn_packet_fifo_elem_find(p_client, packet_id, MQTTSN_MESSAGE_ID);
            if (index == MQTTSN_PACKET_FIFO_MAX_LENGTH)
            {
                NRF_LOG_ERROR("REGACK packet ID had unexpected value.\r\n");
                return NRF_ERROR_INTERNAL;
            }

            mqttsn_packet_fifo_elem_dequeue(p_client, packet_id, MQTTSN_MESSAGE_ID);

            topic.topic_id     = topic_id;
            topic.p_topic_name = p_client->packet_queue.packet[index].topic.p_topic_name;
            
            evt_acc.event_id = MQTTSN_EVENT_REGISTERED,
            evt_acc.event_data.registered.packet.id = packet_id; 
            evt_acc.event_data.registered.packet.topic = topic;
            
            p_client->evt_handler(p_client, &evt_acc);
            return NRF_SUCCESS;

        default:
            NRF_LOG_ERROR("Register message was rejected. Reason: %d\r\n", return_code);
            return NRF_ERROR_INTERNAL;
    }
}

/**@brief Handles PUBLISH message received from the gateway.  
 *
 * @param[inout] p_client    Pointer to an MQTT-SN client instance. 
 * @param[in]    p_data      Received data.
 * @param[in]    datalen     Length of the received data.
 *
 * @retval       NRF_SUCCESS        If the message is processed successfully and (valid for QoS > 0)
 *                                  PUBACK message was sent successfully in response.
 * @retval       NRF_ERROR_INTERNAL Otherwise.
 */
static uint32_t publish_handle(mqttsn_client_t * p_client,
                               const uint8_t   * p_data,
                               uint16_t          datalen)
{
    uint32_t  err_code    = NRF_SUCCESS;
    uint16_t  packet_id   = 0;
    uint32_t  payload_len = 0;
    uint8_t   qos         = 0;
    uint8_t   dup         = 0;
    uint8_t   retained    = 0;
    uint8_t * p_payload;
    MQTTSN_topicid ret_topic;

    if (MQTTSNDeserialize_publish(&dup,
                                  (int *)(&qos),
                                  &retained,
                                  (short unsigned int *)(&packet_id),
                                  &ret_topic,
                                  &p_payload,
                                  (int *)(&payload_len),
                                  (unsigned char *)p_data,
                                  datalen) != 1)
    {
        NRF_LOG_ERROR("PUBLISH packet cannot be deserialized.\r\n");
        return NRF_ERROR_INTERNAL;
    }

    if (qos > 0)
    {
        err_code = mqttsn_packet_sender_puback(p_client, ret_topic.data.id, packet_id, MQTTSN_RC_ACCEPTED);
    }

    mqttsn_topic_t topic = { .topic_id = ret_topic.data.id };
    mqttsn_event_t evt =
    {
        .event_id = MQTTSN_EVENT_RECEIVED,
        .event_data.published =
            {
                .packet    = { .id    = packet_id,
                               .topic = topic,
                               .len   = (uint16_t)payload_len},
                .p_payload = p_payload,
            },
    };
    p_client->evt_handler(p_client, &evt);
    return err_code;
}

/**@brief Handles PUBACK message received from the gateway.  
 *
 * @param[inout] p_client    Pointer to an MQTT-SN client instance. 
 * @param[in]    p_data      Received data.
 * @param[in]    datalen     Length of the received data.
 *
 * @retval       NRF_SUCCESS        If PUBLISH message was accepted or rejected due to congestion
 *                                  and processed successfully.
 * @retval       NRF_ERROR_INTERNAL Otherwise.
 */
static uint32_t puback_handle(mqttsn_client_t * p_client,
                              const uint8_t   * p_data,
                              uint16_t          datalen)
{
    uint16_t topic_id    = 0;
    uint16_t packet_id   = 0;
    uint8_t  return_code = 0;
    
    // Fix: Moved declarations out of switch to avoid warnings
    mqttsn_event_t evt_rc;
    mqttsn_event_t evt_acc;

    if (MQTTSNDeserialize_puback(&topic_id, &packet_id, (unsigned char *)(&return_code), (unsigned char *)(p_data), datalen) == 0)
    {
        NRF_LOG_ERROR("PUBACK packet cannot be deserialized.\r\n");
        return NRF_ERROR_INTERNAL;
    }

    switch (return_code)
    {
        case MQTTSN_RC_REJECTED_CONGESTED:
            NRF_LOG_INFO("Register message was rejected. Reason: congestion.\r\n");
            evt_rc.event_id         = MQTTSN_EVENT_TIMEOUT;
            evt_rc.event_data.error.error    = MQTTSN_ERROR_REJECTED_CONGESTION;
            evt_rc.event_data.error.msg_type = mqttsn_packet_msgtype_error_get(p_data);
            evt_rc.event_data.error.msg_id   = 0;
        
            mqttsn_packet_fifo_elem_dequeue(p_client, packet_id, MQTTSN_MESSAGE_ID);
            p_client->evt_handler(p_client, &evt_rc);
            return NRF_SUCCESS;

        case MQTTSN_RC_ACCEPTED:
            if (mqttsn_packet_fifo_elem_find(p_client, packet_id, MQTTSN_MESSAGE_ID) == MQTTSN_PACKET_FIFO_MAX_LENGTH)
            {
                NRF_LOG_ERROR("PUBACK packet ID has unexpected value.\r\n");
                return NRF_ERROR_INTERNAL;
            }
    
            mqttsn_packet_fifo_elem_dequeue(p_client, packet_id, MQTTSN_MESSAGE_ID);
            evt_acc.event_id = MQTTSN_EVENT_PUBLISHED;
            p_client->evt_handler(p_client, &evt_acc);
            return NRF_SUCCESS;

        default:
            NRF_LOG_ERROR("Publish message was rejected. Reason: %d\r\n", return_code);
            return NRF_ERROR_INTERNAL;
    }
}

/**@brief Handles SUBACK message received from the gateway.  
 *
 * @param[inout] p_client    Pointer to an MQTT-SN client instance. 
 * @param[in]    p_data      Received data.
 * @param[in]    datalen     Length of the received data.
 *
 * @retval       NRF_SUCCESS        If SUBSCRIBE message was accepted or rejected due to congestion
 *                                  and processed successfully.
 * @retval       NRF_ERROR_INTERNAL Otherwise.
 */
static uint32_t suback_handle(mqttsn_client_t * p_client,
                              const uint8_t   * p_data,
                              uint16_t          datalen)
{
    uint16_t topic_id    = 0;
    uint16_t packet_id   = 0;
    uint8_t  return_code = 0;
    uint8_t  qos         = 0;
    
    // Fix: Moved declarations out of switch to avoid warnings
    mqttsn_event_t evt_rc;
    mqttsn_event_t evt_acc;

    if (MQTTSNDeserialize_suback((int *)(&qos),
                                 &topic_id,
                                 &packet_id,
                                 (unsigned char *)(&return_code),
                                 (unsigned char *)p_data,
                                 datalen) == 0)
    {
        NRF_LOG_ERROR("SUBACK packet cannot be deserialized.\r\n");
        return NRF_ERROR_INTERNAL;
    }

    switch (return_code)
    {
        case MQTTSN_RC_REJECTED_CONGESTED:
            NRF_LOG_INFO("Register message was rejected. Reason: congestion.\r\n");
            evt_rc.event_id         = MQTTSN_EVENT_TIMEOUT;
            evt_rc.event_data.error.error    = MQTTSN_ERROR_REJECTED_CONGESTION;
            evt_rc.event_data.error.msg_type = mqttsn_packet_msgtype_error_get(p_data);
            evt_rc.event_data.error.msg_id   = 0;
        
            mqttsn_packet_fifo_elem_dequeue(p_client, packet_id, MQTTSN_MESSAGE_ID);
            p_client->evt_handler(p_client, &evt_rc);
            return NRF_SUCCESS;

        case MQTTSN_RC_ACCEPTED:
            if (mqttsn_packet_fifo_elem_find(p_client, packet_id, MQTTSN_MESSAGE_ID) == MQTTSN_PACKET_FIFO_MAX_LENGTH)
            {
                NRF_LOG_ERROR("SUBACK packet ID has unexpected value.\r\n");
                return NRF_ERROR_INTERNAL;
            }
    
            mqttsn_packet_fifo_elem_dequeue(p_client, packet_id, MQTTSN_MESSAGE_ID);
            evt_acc.event_id = MQTTSN_EVENT_SUBSCRIBED;
            p_client->evt_handler(p_client, &evt_acc);
            return NRF_SUCCESS;

        default:
            NRF_LOG_ERROR("Subscribe message was rejected. Reason: %d\r\n", return_code);
            return NRF_ERROR_INTERNAL;
            
    }
}

/**@brief Handles UNSUBACK message received from the gateway.  
 *
 * @param[inout] p_client    Pointer to an MQTT-SN client instance. 
 * @param[in]    p_data      Received data.
 * @param[in]    datalen     Length of the received data.
 *
 * @retval       NRF_SUCCESS        If UNSUBACK message was processed successfully.
 * @retval       NRF_ERROR_INTERNAL Otherwise.
 */
static uint32_t unsuback_handle(mqttsn_client_t * p_client,
                                const uint8_t   * p_data,
                                uint16_t          datalen)
{
    uint16_t packet_id = 0;

    if (MQTTSNDeserialize_unsuback(&packet_id, (unsigned char *)p_data, datalen) == 0)
    {
        NRF_LOG_ERROR("UNSUBACK packet cannot be deserialized.\r\n");
        return NRF_ERROR_INTERNAL;
    }

    if (mqttsn_packet_fifo_elem_find(p_client, packet_id, MQTTSN_MESSAGE_ID) == MQTTSN_PACKET_FIFO_MAX_LENGTH)
    {
        NRF_LOG_ERROR("UNSUBACK packet ID has unexpected value\r\n");
        return NRF_ERROR_INTERNAL;
    }

    mqttsn_packet_fifo_elem_dequeue(p_client, packet_id, MQTTSN_MESSAGE_ID);
    mqttsn_event_t evt = {.event_id = MQTTSN_EVENT_UNSUBSCRIBED};
    p_client->evt_handler(p_client, &evt);
    return NRF_SUCCESS;
}

/**@brief Handles PINGRESP message received from the gateway. 
 *
 * @param[inout] p_client    Pointer to an MQTT-SN client instance.
 *
 * @retval       NRF_SUCCESS In all cases.
 */
static uint32_t pingresp_handle(mqttsn_client_t * p_client)
{
    p_client->keep_alive.timeout = mqttsn_platform_timer_set_in_ms(p_client->keep_alive.duration);
    p_client->keep_alive.response_arrived = 1;
    p_client->keep_alive.message.retransmission_cnt = MQTTSN_DEFAULT_RETRANSMISSION_CNT + 1;
    if (p_client->client_state == MQTTSN_CLIENT_ASLEEP)
    {
        sleep_handle(p_client);
    }
    return NRF_SUCCESS;
}

/**@brief Handles DISCONNECT message received from the gateway. 
 *
 * @param[inout] p_client    Pointer to an MQTT-SN client instance.
 *
 * @retval       NRF_SUCCESS If the message is a response to a DISCONNECT message sent by the
 *                           gateway or if CONNECT message is sent successfully in the opposite case.
 * @retval       Otherwise, error code is returned.
 */
static uint32_t disconnect_handle(mqttsn_client_t * p_client)
{
    if (p_client->client_state == MQTTSN_CLIENT_WAITING_FOR_SLEEP)
    {
        p_client->keep_alive.timeout = mqttsn_platform_timer_set_in_ms(p_client->keep_alive.duration);
        sleep_handle(p_client);
        return NRF_SUCCESS;
    }
    else if (p_client->client_state == MQTTSN_CLIENT_WAITING_FOR_DISCONNECT)
    {
        p_client->client_state = MQTTSN_CLIENT_DISCONNECTED;
        mqttsn_event_t evt = { .event_id = MQTTSN_EVENT_DISCONNECTED };
        p_client->evt_handler(p_client, &evt);
        return NRF_SUCCESS;
    }
    else
    {
        p_client->client_state = MQTTSN_CLIENT_ESTABLISHING_CONNECTION;
        return mqttsn_packet_sender_connect(p_client);
    }
}

/**@brief Handles WILLTOPICRESP message received from the gateway.  
 *
 * @param[inout] p_client    Pointer to an MQTT-SN client instance. 
 * @param[in]    p_data      Received data.
 * @param[in]    datalen     Length of the received data.
 *
 * @retval       NRF_SUCCESS        If WILLTOPICRESP message was accepted or rejected due to congestion
 *                                  and processed successfully.
 * @retval       NRF_ERROR_INTERNAL Otherwise.
 */
static uint32_t willtopicresp_handle(mqttsn_client_t * p_client,
                                     const uint8_t   * p_data,
                                     uint16_t          datalen)
{
    uint32_t return_code = 0;
    
    // Fix: Moved declarations out of switch to avoid warnings
    mqttsn_event_t evt_rc;
    mqttsn_event_t evt_acc;
    
    if (MQTTSNDeserialize_willtopicresp((int *)(&return_code), (unsigned char *)p_data, datalen) == 0)
    {
        NRF_LOG_ERROR("WILLTOPICRESP packet cannot be deserialized.\r\n");
        return NRF_ERROR_INTERNAL;
    }

    switch (return_code)
    {
        case MQTTSN_RC_REJECTED_CONGESTED:
            NRF_LOG_INFO("WILLTOPICUPD message was rejected. Reason: congestion.\r\n");
            evt_rc.event_id = MQTTSN_EVENT_TIMEOUT;
            evt_rc.event_data.error.error    = MQTTSN_ERROR_REJECTED_CONGESTION;
            evt_rc.event_data.error.msg_type = mqttsn_packet_msgtype_error_get(p_data);
            evt_rc.event_data.error.msg_id   = 0;

            mqttsn_packet_fifo_elem_dequeue(p_client, MQTTSN_MSGTYPE_WILLTOPICUPD, MQTTSN_MESSAGE_TYPE);
            p_client->evt_handler(p_client, &evt_rc);
            return NRF_SUCCESS;

        case MQTTSN_RC_ACCEPTED:
            mqttsn_packet_fifo_elem_dequeue(p_client, MQTTSN_MSGTYPE_WILLTOPICUPD, MQTTSN_MESSAGE_TYPE);
            evt_acc.event_id = MQTTSN_EVENT_WILL_TOPIC_UPD;
            p_client->evt_handler(p_client, &evt_acc);
            return NRF_SUCCESS;

        default:
            NRF_LOG_ERROR("WILLTOPICUPD message was rejected. Reason: %d\r\n", return_code);
            return NRF_ERROR_INTERNAL;
    }
}

/**@brief Handles WILLMSGRESP message received from the gateway.  
 *
 * @param[inout] p_client    Pointer to an MQTT-SN client instance. 
 * @param[in]    p_data      Received data.
 * @param[in]    datalen     Length of the received data.
 *
 * @retval       NRF_SUCCESS        If WILLMSGRESP message was accepted or rejected due to congestion
 *                                  and processed successfully.
 * @retval       NRF_ERROR_INTERNAL Otherwise.
 */
static uint32_t willmsgresp_handle(mqttsn_client_t * p_client,
                                   const uint8_t   * p_data,
                                   uint16_t          datalen)
{
    uint32_t return_code = 0;
    
    // Fix: Moved declarations out of switch to avoid warnings
    mqttsn_event_t evt_rc;
    mqttsn_event_t evt_acc;
    
    if (MQTTSNDeserialize_willmsgresp((int *)(&return_code), (unsigned char *)p_data, datalen) == 0)
    {
        NRF_LOG_ERROR("WILLTOPICRESP packet cannot be deserialized.\r\n");
        return NRF_ERROR_INTERNAL;
    }

    switch (return_code)
    {
        case MQTTSN_RC_REJECTED_CONGESTED:
            NRF_LOG_INFO("WILLTOPICUPD message was rejected. Reason: congestion.\r\n");
            evt_rc.event_id                     = MQTTSN_EVENT_TIMEOUT;
            evt_rc.event_data.error.error       = MQTTSN_ERROR_REJECTED_CONGESTION;
            evt_rc.event_data.error.msg_type    = mqttsn_packet_msgtype_error_get(p_data);
            evt_rc.event_data.error.msg_id      = 0;
        
            mqttsn_packet_fifo_elem_dequeue(p_client, MQTTSN_MSGTYPE_WILLMSGUPD, MQTTSN_MESSAGE_TYPE);
            p_client->evt_handler(p_client, &evt_rc);
            return NRF_SUCCESS;

        case MQTTSN_RC_ACCEPTED:
            mqttsn_packet_fifo_elem_dequeue(p_client, MQTTSN_MSGTYPE_WILLMSGUPD, MQTTSN_MESSAGE_TYPE);
            evt_acc.event_id = MQTTSN_EVENT_WILL_MSG_UPD;
            p_client->evt_handler(p_client, &evt_acc); 
            return NRF_SUCCESS;

        default:
            NRF_LOG_ERROR("WILLTOPICUPD message was rejected. Reason: %d\r\n", return_code);
            return NRF_ERROR_INTERNAL;
    }
}

/**@brief Handles message received from the gateway.  
 *
 * @param[inout] p_client     Pointer to an MQTT-SN client instance.
 * @param[in]    p_port       Pointer to local port that received the message.
 * @param[in]    p_remote     Pointer to remote endpoint.
 * @param[in]    p_data       Received data.
 * @param[in]    datalen      Length of the received data.
 * @param[in]    message_type Message Type field value.
 *
 * @retval       NRF_SUCCESS             If message is processed successfully.
 * @retval       NRF_ERROR_NOT_SUPPORTED If message of unsupported type has been received.
 * @retval       Otherwise, appropriate error code is returned.
 */
static uint32_t message_handle(mqttsn_client_t        * p_client,
                               const mqttsn_port_t    * p_port,
                               const mqttsn_remote_t  * p_remote,
                               const uint8_t          * p_data,
                               uint16_t                 datalen,
                               MQTTSN_msgTypes          message_type)
{
    uint32_t err_code = NRF_ERROR_INVALID_STATE;

    switch(message_type)
    {
        case MQTTSN_ADVERTISE:
            break;

        case MQTTSN_GWINFO:
            err_code = gwinfo_handle(p_client, p_remote, p_data, datalen);
            break;

        case MQTTSN_WILLTOPICREQ:
            err_code = willtopicreq_handle(p_client);
            break;

        case MQTTSN_WILLMSGREQ:
            err_code = willmsgreq_handle(p_client);
            break;

        case MQTTSN_CONNACK:
            err_code = connack_handle(p_client, p_data, datalen);
            break;

        case MQTTSN_REGISTER:
            err_code = register_handle(p_client, p_data, datalen);
            break;

        case MQTTSN_REGACK:
            err_code = regack_handle(p_client, p_data, datalen);
            break;

        case MQTTSN_PUBLISH:
            err_code = publish_handle(p_client, p_data, datalen);
            break;

        case MQTTSN_PUBACK:
            err_code = puback_handle(p_client, p_data, datalen);
            break;

        case MQTTSN_SUBACK:
            err_code = suback_handle(p_client, p_data, datalen);
            break;
        
        case MQTTSN_UNSUBACK:
            err_code = unsuback_handle(p_client, p_data, datalen);
            break;

        case MQTTSN_PINGRESP:
            err_code = pingresp_handle(p_client);
            break;

        case MQTTSN_DISCONNECT:
            err_code = disconnect_handle(p_client);
            break;

        case MQTTSN_WILLTOPICRESP:
            err_code = willtopicresp_handle(p_client, p_data, datalen);
            break;

        case MQTTSN_WILLMSGRESP:
            err_code = willmsgresp_handle(p_client, p_data, datalen);
            break;

        default:
            err_code = NRF_ERROR_NOT_SUPPORTED;
            NRF_LOG_ERROR("Message of unsupported type has been received.\r\n");
            break;
    }

    return err_code;
}

uint32_t mqttsn_packet_receiver(mqttsn_client_t        * p_client,
                                const mqttsn_port_t    * p_port,
                                const mqttsn_remote_t  * p_remote,
                                const uint8_t          * p_data,
                                uint16_t                datalen)
{
    MQTTSN_msgTypes msg_type = (MQTTSN_msgTypes)p_data[mqttsn_packet_msgtype_index_get(p_data)];
    return message_handle(p_client, p_port, p_remote, p_data, datalen, msg_type);
}
