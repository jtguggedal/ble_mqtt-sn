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

#include "mqttsn_packet_internal.h"

#define MQTTSN_PACKET_SEARCHGW_LENGTH     3
#define MQTTSN_PACKET_CONNECT_LENGTH     (6 + MQTTSN_CLIENT_ID_MAX_LENGTH)
#define MQTTSN_PACKET_WILLTOPIC_LENGTH    3
#define MQTTSN_PACKET_WILLMSG_LENGTH      2
#define MQTTSN_PACKET_REGISTER_LENGTH     6
#define MQTTSN_PACKET_REGACK_LENGTH       7
#define MQTTSN_PACKET_PUBLISH_LENGTH      7
#define MQTTSN_PACKET_PUBACK_LENGTH       7
#define MQTTSN_PACKET_SUBSCRIBE_LENGTH    5
#define MQTTSN_PACKET_UNSUBSCRIBE_LENGTH  5
#define MQTTSN_PACKET_DISCONNECT_LENGTH   2
#define MQTTSN_PACKET_WILLTOPICUPD_LENGTH 4
#define MQTTSN_PACKET_WILLMSGUPD_LENGTH   4
#define MQTTSN_PACKET_DISCONNECT_DURATION -1

/**@brief Calculates next message ID. 
 *
 * @param[in]    p_client    Pointer to MQTT-SN client instance.
 *
 * @return       Next packet ID to use.
 */
static uint16_t next_packet_id_get(mqttsn_client_t * p_client)
{
    return (p_client->message_id == MQTTSN_MAX_PACKET_ID) ? 1 : (++(p_client->message_id));
}

/**@brief Sends MQTT-SN message.  
 *
 * @param[inout] p_client    Pointer to initialized and connected client. 
 * @param[in]    p_remote    Pointer to remote endpoint.
 * @param[in]    p_data      Buffered data to send.
 * @param[in]    datalen     Length of the buffered data.
 *
 * @return       NRF_SUCCESS if the message has been sent successfully.
 *               Otherwise error code is returned.
 */
static uint32_t mqttsn_packet_sender_send(mqttsn_client_t       * p_client,
                                          const mqttsn_remote_t * p_remote,
                                          const uint8_t         * p_data,
                                          uint16_t                datalen)
{
    // TODO: must include some sort of generic function to allow sending over different transport layers
    return NRF_SUCCESS; //mqttsn_transport_write(p_client, p_remote, p_data, datalen);
}

uint32_t mqttsn_packet_sender_retransmit(mqttsn_client_t       * p_client,
                                         const mqttsn_remote_t * p_remote,
                                         const uint8_t         * p_data,
                                         uint16_t                datalen)
{
    return mqttsn_packet_sender_send(p_client, p_remote, p_data, datalen);
}

uint32_t mqttsn_packet_sender_searchgw(mqttsn_client_t * p_client)
{
    unsigned char radius = 1;
    uint32_t err_code = NRF_SUCCESS;
    uint8_t * p_data  = nrf_malloc(MQTTSN_PACKET_SEARCHGW_LENGTH);
    uint16_t datalen  = 0;

    if (p_data == NULL)
    {
        err_code = NRF_ERROR_NO_MEM;
        NRF_LOG_ERROR("SEARCHGW message cannot be allocated\r\n");
    }

    if (err_code == NRF_SUCCESS)
    {
        datalen = MQTTSNSerialize_searchgw(p_data, MQTTSN_PACKET_SEARCHGW_LENGTH, radius);
        if (datalen == 0)
        {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        const mqttsn_remote_t broadcast_search =
        {
            .addr        = MQTTSN_BROADCAST_ADDR,
            .port_number = MQTTSN_DEFAULT_CLIENT_PORT,
        };
        err_code = mqttsn_packet_sender_send(p_client, &broadcast_search, p_data, datalen);
    }

    if (p_data)
    {
        nrf_free(p_data);
    }

    return err_code;
}

uint32_t mqttsn_packet_sender_willtopic(mqttsn_client_t * p_client)
{
    uint8_t will_qos     = 0;
    uint8_t will_retain  = 0;

    uint32_t  err_code   = NRF_SUCCESS;
    uint16_t  packet_len = MQTTSN_PACKET_WILLTOPIC_LENGTH + p_client->connect_info.will_topic_len;
    uint8_t * p_data     = nrf_malloc(packet_len);
    uint16_t datalen     = 0;

    if (p_data == NULL)
    {
        err_code = NRF_ERROR_NO_MEM;
        NRF_LOG_ERROR("WILLTOPIC message cannot be allocated\r\n");
    }

    if (err_code == NRF_SUCCESS)
    {
        MQTTSNString will_topic = 
        {
            .lenstring = { .data = (char *)p_client->connect_info.p_will_topic,
                           .len  = p_client->connect_info.will_topic_len, },
        };

        datalen = MQTTSNSerialize_willtopic(p_data, packet_len, will_qos, will_retain, will_topic);
        if (datalen == 0)
        {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = mqttsn_packet_sender_send(p_client, &(p_client->gateway_info.addr), p_data, datalen);
    }

    if (p_data)
    {
        nrf_free(p_data);
    }

    return err_code;
}

uint32_t mqttsn_packet_sender_willmsg(mqttsn_client_t * p_client)
{
    uint32_t  err_code   = NRF_SUCCESS;
    uint16_t  packet_len = MQTTSN_PACKET_WILLMSG_LENGTH + p_client->connect_info.will_msg_len;
    uint8_t * p_data     = nrf_malloc(packet_len);
    uint16_t datalen     = 0;

    if (p_data == NULL)
    {
        err_code = NRF_ERROR_NO_MEM;
        NRF_LOG_ERROR("WILLMSG message cannot be allocated\r\n");
    }

    if (err_code == NRF_SUCCESS)
    {
        MQTTSNString will_msg = 
        {
            .lenstring = { .data = (char *)p_client->connect_info.p_will_msg,
                           .len  = p_client->connect_info.will_msg_len, },
        };
        datalen = MQTTSNSerialize_willmsg(p_data, packet_len, will_msg);
        if (datalen == 0)
        {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = mqttsn_packet_sender_send(p_client, &(p_client->gateway_info.addr), p_data, datalen);
    }

    if (p_data)
    {
        nrf_free(p_data);
    }

    return err_code;
}

uint32_t mqttsn_packet_sender_connect(mqttsn_client_t * p_client)
{
    uint32_t                 err_code = NRF_SUCCESS;
    char                     p_buf[MQTTSN_CLIENT_ID_MAX_LENGTH];
    MQTTSNPacket_connectData options  = MQTTSNPacket_connectData_initializer;
    options.clientID.cstring          = p_buf;

    memset(options.clientID.cstring, 0, MQTTSN_CLIENT_ID_MAX_LENGTH);
    memcpy(options.clientID.cstring,
           p_client->connect_info.p_client_id,
           p_client->connect_info.client_id_len);
    options.willFlag     = p_client->connect_info.will_flag;
    options.duration     = p_client->connect_info.alive_duration;
    options.cleansession = p_client->connect_info.clean_session;

    uint8_t * p_data = nrf_malloc(MQTTSN_PACKET_CONNECT_LENGTH);
    if (p_data == NULL)
    {
        err_code = NRF_ERROR_NO_MEM;
        NRF_LOG_ERROR("CONNECT message cannot be allocated\r\n");
    }

    uint16_t datalen = 0;
    if (err_code == NRF_SUCCESS)
    {
        datalen = MQTTSNSerialize_connect(p_data, MQTTSN_PACKET_CONNECT_LENGTH, &options);
        if (datalen == 0)
        {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    uint8_t * packet_copy = nrf_malloc(datalen);
    if (err_code == NRF_SUCCESS)
    {
        if (packet_copy == NULL)
        {
            err_code = NRF_ERROR_NO_MEM;
        }
        else
        {
            memcpy(packet_copy, p_data, datalen);
        }
    }

    
    mqttsn_packet_t retransmission_packet = 
    {
        .retransmission_cnt = MQTTSN_DEFAULT_RETRANSMISSION_CNT,
        .p_data             = packet_copy,
        .len                = datalen,
        .timeout            = mqttsn_platform_timer_set_in_ms(MQTTSN_DEFAULT_RETRANSMISSION_TIME_IN_MS),
    };
    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_packet_fifo_elem_add(p_client, &retransmission_packet) != NRF_SUCCESS)
        {
            err_code = NRF_ERROR_NO_MEM;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_platform_timer_start(p_client, retransmission_packet.timeout) != NRF_SUCCESS)
        {
            mqttsn_packet_fifo_elem_dequeue(p_client, MQTTSN_MSGTYPE_CONNECT, MQTTSN_MESSAGE_TYPE);
            err_code = NRF_ERROR_INTERNAL;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = mqttsn_packet_sender_send(p_client, &(p_client->gateway_info.addr), p_data, datalen);
    }

    if (p_data)
    {
        nrf_free(p_data);
    }

    if (packet_copy && err_code != NRF_SUCCESS)
    {
        nrf_free(packet_copy);
    }

    return err_code;
}

uint32_t mqttsn_packet_sender_register(mqttsn_client_t * p_client,
                                       mqttsn_topic_t  * p_topic,
                                       uint16_t          topic_name_len)
{
    uint32_t  err_code   = NRF_SUCCESS;
    uint16_t  packet_len = MQTTSN_PACKET_REGISTER_LENGTH + topic_name_len;
    uint8_t * p_data     = nrf_malloc(packet_len);

    if (p_data == NULL)
    {
        err_code = NRF_ERROR_NO_MEM;
        NRF_LOG_ERROR("REGISTER message cannot be allocated\r\n");
    }

    uint16_t datalen = 0;
    if (err_code == NRF_SUCCESS)
    {
        MQTTSNString topic_name =
        {
            .lenstring =
            {
                .data = (char *)(p_topic->p_topic_name),
                .len  = topic_name_len,
            },
        };
        datalen = MQTTSNSerialize_register(p_data,
                                           packet_len,
                                           0,
                                           next_packet_id_get(p_client),
                                           &topic_name);
        if (datalen == 0)
        {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    uint8_t * packet_copy = nrf_malloc(datalen);
    if (err_code == NRF_SUCCESS)
    {
        if (packet_copy == NULL)
        {
            err_code = NRF_ERROR_NO_MEM;
        
        }
        else
        {
            memcpy(packet_copy, p_data, datalen);
        }
    }

    mqttsn_packet_t retransmission_packet = 
    {
        .retransmission_cnt = MQTTSN_DEFAULT_RETRANSMISSION_CNT,
        .p_data             = packet_copy,
        .id                 = p_client->message_id,
        .topic              = *p_topic, 
        .len                = datalen,
        .timeout            = mqttsn_platform_timer_set_in_ms(MQTTSN_DEFAULT_RETRANSMISSION_TIME_IN_MS),
    };
    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_packet_fifo_elem_add(p_client, &retransmission_packet) != NRF_SUCCESS)
        {
            err_code = NRF_ERROR_NO_MEM;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_platform_timer_start(p_client, retransmission_packet.timeout) != NRF_SUCCESS)
        {
            mqttsn_packet_fifo_elem_dequeue(p_client, p_client->message_id, MQTTSN_MESSAGE_ID);
            err_code = NRF_ERROR_INTERNAL;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = mqttsn_packet_sender_send(p_client, &(p_client->gateway_info.addr), p_data, datalen);
    }

    if (p_data)
    {
        nrf_free(p_data);
    }

    if (packet_copy && err_code != NRF_SUCCESS)
    {
        nrf_free(packet_copy);
    }

    return err_code;
}

uint32_t mqttsn_packet_sender_regack(mqttsn_client_t * p_client,
                                     uint16_t          topic_id,
                                     uint16_t          packet_id,
                                     uint8_t           ret_code)
{
    uint32_t err_code = NRF_SUCCESS;
    uint8_t * p_data  = nrf_malloc(MQTTSN_PACKET_REGACK_LENGTH);

    if (p_data == NULL)
    {
        err_code = NRF_ERROR_NO_MEM;
        NRF_LOG_ERROR("REGACK message cannot be allocated\r\n");
    }

    uint16_t datalen = 0;
    if (err_code == NRF_SUCCESS)
    {
        datalen = MQTTSNSerialize_regack(p_data,
                                         MQTTSN_PACKET_REGACK_LENGTH,
                                         topic_id,
                                         packet_id,
                                         ret_code);
        if (datalen == 0)
        {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = mqttsn_packet_sender_send(p_client, &(p_client->gateway_info.addr), p_data, datalen);
    }

    if (p_data)
    {
        nrf_free(p_data);
    }

    return err_code;
}

uint32_t mqttsn_packet_sender_publish(mqttsn_client_t * p_client,
                                      mqttsn_topic_t  * p_topic,
                                      const uint8_t   * payload,
                                      uint16_t          payload_len)
{
    uint32_t err_code       = NRF_SUCCESS;

    unsigned char dup       = 0;
    unsigned char retained  = 0;
    uint8_t  qos            = 1;

    uint32_t  packet_len    = MQTTSN_PACKET_PUBLISH_LENGTH + payload_len;
    uint8_t * p_data        = nrf_malloc(packet_len);

    if (p_data == NULL)
    {
        err_code = NRF_ERROR_NO_MEM;
        NRF_LOG_ERROR("PUBLISH message cannot be allocated\r\n");
    }
    
    uint16_t datalen = 0;
    if (err_code == NRF_SUCCESS)
    {
        MQTTSN_topicid topic = 
        {
            .type = MQTTSN_TOPIC_TYPE_NORMAL,
            .data.id = p_topic->topic_id,
        };
        datalen = MQTTSNSerialize_publish(p_data,
                                          packet_len,
                                          dup,
                                          qos,
                                          retained,
                                          next_packet_id_get(p_client),
                                          topic,
                                          (uint8_t *)payload,
                                          payload_len);
        if (datalen == 0)
        {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    uint8_t * packet_copy = nrf_malloc(datalen);
    if (err_code == NRF_SUCCESS)
    {
        if (packet_copy == NULL)
        {
            err_code = NRF_ERROR_NO_MEM;
        }
        else
        {
            memcpy(packet_copy, p_data, datalen);
        }
    }

    mqttsn_packet_t retransmission_packet = 
    {
        .retransmission_cnt = MQTTSN_DEFAULT_RETRANSMISSION_CNT,
        .p_data             = packet_copy,
        .id                 = p_client->message_id,
        .topic              = *p_topic, 
        .len                = datalen,
        .timeout            = mqttsn_platform_timer_set_in_ms(MQTTSN_DEFAULT_RETRANSMISSION_TIME_IN_MS),
    };
    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_packet_fifo_elem_add(p_client, &retransmission_packet) != NRF_SUCCESS)
        {
            err_code = NRF_ERROR_NO_MEM;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_platform_timer_start(p_client, retransmission_packet.timeout) != NRF_SUCCESS)
        {
            mqttsn_packet_fifo_elem_dequeue(p_client, p_client->message_id, MQTTSN_MESSAGE_ID);
            err_code = NRF_ERROR_INTERNAL;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = mqttsn_packet_sender_send(p_client, &(p_client->gateway_info.addr), p_data, datalen);
    }

    if (p_data)
    {
        nrf_free(p_data);
    }

    if (packet_copy && err_code != NRF_SUCCESS)
    {
        nrf_free(packet_copy);
    }

    return err_code;
}

uint32_t mqttsn_packet_sender_puback(mqttsn_client_t * p_client,
                                     uint16_t          topic_id,
                                     uint16_t          packet_id,
                                     uint8_t           ret_code)
{
    uint32_t err_code = NRF_SUCCESS;
    uint8_t * p_data  = nrf_malloc(MQTTSN_PACKET_PUBACK_LENGTH);

    if (p_data == NULL)
    {
        err_code = NRF_ERROR_NO_MEM;
        NRF_LOG_ERROR("PUBACK message cannot be allocated\r\n");
    }

    uint16_t datalen = 0;
    if (err_code == NRF_SUCCESS)
    {
        datalen = MQTTSNSerialize_puback(p_data, MQTTSN_PACKET_PUBACK_LENGTH, topic_id, packet_id, ret_code);
        if (datalen == 0)
        {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = mqttsn_packet_sender_send(p_client, &(p_client->gateway_info.addr), p_data, datalen);
    }

    if (p_data)
    {
        nrf_free(p_data);
    }

    return err_code;
}

uint32_t mqttsn_packet_sender_subscribe(mqttsn_client_t * p_client,
                                        mqttsn_topic_t * p_topic,
                                        uint16_t topic_name_len)
{
    uint32_t  err_code   = NRF_SUCCESS;
    uint16_t  packet_len = MQTTSN_PACKET_SUBSCRIBE_LENGTH + topic_name_len;
    uint8_t   qos        = 1;
    uint8_t   dup        = 0;
    uint8_t * p_data     = nrf_malloc(packet_len);

    if (p_data == NULL)
    {
        err_code = NRF_ERROR_NO_MEM;
        NRF_LOG_ERROR("SUBSCRIBE message cannot be allocated\r\n");
    }

    uint16_t datalen = 0;
    if (err_code == NRF_SUCCESS)
    {
        MQTTSN_topicid topic =
        {
            .type = MQTTSN_TOPIC_TYPE_NORMAL,
            .data.long_.name = (char *)(p_topic->p_topic_name),
            .data.long_.len  = (int)topic_name_len,
        };

        datalen = MQTTSNSerialize_subscribe(p_data,
                                            packet_len,
                                            dup,
                                            qos,
                                            next_packet_id_get(p_client),
                                            &topic);
        if (datalen == 0)
        {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    uint8_t * packet_copy = nrf_malloc(datalen);
    if (err_code == NRF_SUCCESS)
    {
        if (packet_copy == NULL)
        {
            err_code = NRF_ERROR_NO_MEM;
        }
        else
        {
            memcpy(packet_copy, p_data, datalen);
        }
    }

    mqttsn_packet_t retransmission_packet = 
    {
        .retransmission_cnt = MQTTSN_DEFAULT_RETRANSMISSION_CNT,
        .p_data             = packet_copy,
        .id                 = p_client->message_id,
        .topic              = *p_topic, 
        .len                = datalen,
        .timeout            = mqttsn_platform_timer_set_in_ms(MQTTSN_DEFAULT_RETRANSMISSION_TIME_IN_MS),
    };
    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_packet_fifo_elem_add(p_client, &retransmission_packet) != NRF_SUCCESS)
        {
            err_code = NRF_ERROR_NO_MEM;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_platform_timer_start(p_client, retransmission_packet.timeout) != NRF_SUCCESS)
        {
            mqttsn_packet_fifo_elem_dequeue(p_client, p_client->message_id, MQTTSN_MESSAGE_ID);
            err_code = NRF_ERROR_INTERNAL;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = mqttsn_packet_sender_send(p_client, &(p_client->gateway_info.addr), p_data, datalen);
    }

    if (p_data)
    {
        nrf_free(p_data);
    }

    if (packet_copy && err_code != NRF_SUCCESS)
    {
        nrf_free(packet_copy);
    }

    return err_code;
}

uint32_t mqttsn_packet_sender_unsubscribe(mqttsn_client_t * p_client,
                                          mqttsn_topic_t  * p_topic,
                                          uint16_t          topic_name_len)
{
    uint32_t  err_code   = NRF_SUCCESS;
    uint16_t  packet_len = MQTTSN_PACKET_UNSUBSCRIBE_LENGTH + topic_name_len;
    uint8_t * p_data     = nrf_malloc(packet_len);

    if (p_data == NULL)
    {
        err_code = NRF_ERROR_NO_MEM;
        NRF_LOG_ERROR("UNSUBSCRIBE message cannot be allocated\r\n");
    }

    uint16_t datalen = 0;
    if (err_code == NRF_SUCCESS)
    {
        MQTTSN_topicid topic =
        {
            .type = MQTTSN_TOPIC_TYPE_NORMAL,
            .data.long_.name = (char *)(p_topic->p_topic_name),
            .data.long_.len  = (int)topic_name_len,
        };

        datalen = MQTTSNSerialize_unsubscribe(p_data,
                                              packet_len,
                                              next_packet_id_get(p_client),
                                              &topic);
        if (datalen == 0)
        {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    uint8_t * packet_copy = nrf_malloc(datalen);
    if (err_code == NRF_SUCCESS)
    {
        if (packet_copy == NULL)
        {
            err_code = NRF_ERROR_NO_MEM;
        }
        else
        {
            memcpy(packet_copy, p_data, datalen);
        }
    }

    mqttsn_packet_t retransmission_packet = 
    {
        .retransmission_cnt = MQTTSN_DEFAULT_RETRANSMISSION_CNT,
        .p_data             = packet_copy,
        .id                 = p_client->message_id,
        .topic              = *p_topic, 
        .len                = datalen,
        .timeout            = mqttsn_platform_timer_set_in_ms(MQTTSN_DEFAULT_RETRANSMISSION_TIME_IN_MS),
    };
    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_packet_fifo_elem_add(p_client, &retransmission_packet) != NRF_SUCCESS)
        {
            err_code = NRF_ERROR_NO_MEM;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_platform_timer_start(p_client, retransmission_packet.timeout) != NRF_SUCCESS)
        {
            mqttsn_packet_fifo_elem_dequeue(p_client, p_client->message_id, MQTTSN_MESSAGE_ID);
            err_code = NRF_ERROR_INTERNAL;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = mqttsn_packet_sender_send(p_client, &(p_client->gateway_info.addr), p_data, datalen);
    }

    if (p_data)
    {
        nrf_free(p_data);
    }

    if (packet_copy && err_code != NRF_SUCCESS)
    {
        nrf_free(packet_copy);
    }

    return err_code;
}

uint32_t mqttsn_packet_sender_disconnect(mqttsn_client_t * p_client, uint16_t duration)
{
    uint32_t err_code = NRF_SUCCESS;
    uint16_t packet_len;
    int16_t  disconnect_duration;
    mqttsn_client_state_t next_state;

    if (duration > 0)
    {
        disconnect_duration = duration + (MQTTSN_DEFAULT_RETRANSMISSION_CNT + 1) *
                                          MQTTSN_DEFAULT_RETRANSMISSION_TIME_IN_MS / 1000;
        next_state          = MQTTSN_CLIENT_WAITING_FOR_SLEEP;
        packet_len          = MQTTSN_PACKET_DISCONNECT_LENGTH + sizeof(duration);
        p_client->keep_alive.duration = duration * 1000;
    }
    else
    {
        disconnect_duration = MQTTSN_PACKET_DISCONNECT_DURATION;
        next_state          = MQTTSN_CLIENT_WAITING_FOR_DISCONNECT;
        packet_len          = MQTTSN_PACKET_DISCONNECT_LENGTH;
    }

    uint8_t * p_data = nrf_malloc(packet_len);

    if (p_data == NULL)
    {
        err_code = NRF_ERROR_NO_MEM;
        NRF_LOG_ERROR("DISCONNECT message cannot be allocated\r\n");
    }

    uint16_t datalen = 0;
    if (err_code == NRF_SUCCESS)
    {
        datalen = MQTTSNSerialize_disconnect(p_data, packet_len, disconnect_duration);
        if (datalen == 0)
        {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = mqttsn_packet_sender_send(p_client, &(p_client->gateway_info.addr), p_data, datalen);
    }

    if (err_code == NRF_SUCCESS)
    {
        p_client->client_state = next_state;
    }

    if (p_data)
    {
        nrf_free(p_data);
    }

    return err_code;
}

uint32_t mqttsn_packet_sender_willtopicupd(mqttsn_client_t * p_client)
{
    uint8_t will_qos     = 1;
    uint8_t will_retain  = 0;

    uint32_t  err_code   = NRF_SUCCESS;
    uint16_t  packet_len = MQTTSN_PACKET_WILLTOPIC_LENGTH + p_client->connect_info.will_topic_len;
    uint8_t * p_data     = nrf_malloc(packet_len);

    if (p_data == NULL)
    {
        err_code = NRF_ERROR_NO_MEM;
        NRF_LOG_ERROR("WILLTOPICUPD message cannot be allocated\r\n");
    }

    uint16_t datalen = 0;
    if (err_code == NRF_SUCCESS)
    {
        MQTTSNString will_topic = 
        {
            .lenstring = { .data = (char *)p_client->connect_info.p_will_topic,
                           .len  = p_client->connect_info.will_topic_len, },
        };

        datalen = MQTTSNSerialize_willtopicupd(p_data, packet_len, will_qos, will_retain, will_topic);
        if (datalen == 0)
        {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    uint8_t * packet_copy = nrf_malloc(datalen);
    if (err_code == NRF_SUCCESS)
    {
        if (packet_copy == NULL)
        {
            err_code = NRF_ERROR_NO_MEM;
        }
        else
        {
            memcpy(packet_copy, p_data, datalen);
        }
    }

    
    mqttsn_packet_t retransmission_packet = 
    {
        .retransmission_cnt = MQTTSN_DEFAULT_RETRANSMISSION_CNT,
        .p_data             = packet_copy,
        .len                = datalen,
        .timeout            = mqttsn_platform_timer_set_in_ms(MQTTSN_DEFAULT_RETRANSMISSION_TIME_IN_MS),
    };
    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_packet_fifo_elem_add(p_client, &retransmission_packet) != NRF_SUCCESS)
        {
            err_code = NRF_ERROR_NO_MEM;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_platform_timer_start(p_client, retransmission_packet.timeout) != NRF_SUCCESS)
        {
            mqttsn_packet_fifo_elem_dequeue(p_client, MQTTSN_MSGTYPE_WILLTOPICUPD, MQTTSN_MESSAGE_TYPE);
            err_code = NRF_ERROR_INTERNAL;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = mqttsn_packet_sender_send(p_client, &(p_client->gateway_info.addr), p_data, datalen);
    }

    if (p_data)
    {
        nrf_free(p_data);
    }

    if (packet_copy && err_code != NRF_SUCCESS)
    {
        nrf_free(packet_copy);
    }

    return err_code;
}

uint32_t mqttsn_packet_sender_willmsgupd(mqttsn_client_t * p_client)
{
    uint32_t  err_code   = NRF_SUCCESS;
    uint16_t  packet_len = MQTTSN_PACKET_WILLMSG_LENGTH + p_client->connect_info.will_msg_len;
    uint8_t * p_data     = nrf_malloc(packet_len);

    if (p_data == NULL)
    {
        err_code = NRF_ERROR_NO_MEM;
        NRF_LOG_ERROR("WILLMSGUPD message cannot be allocated\r\n");
    }

    uint16_t datalen = 0;
    if (err_code == NRF_SUCCESS)
    {
        MQTTSNString will_msg = 
        {
            .lenstring = { .data = (char *)p_client->connect_info.p_will_msg,
                           .len  = p_client->connect_info.will_msg_len, },
        };
        datalen = MQTTSNSerialize_willmsgupd(p_data, packet_len, will_msg);
        if (datalen == 0)
        {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    uint8_t * packet_copy = nrf_malloc(datalen);
    if (err_code == NRF_SUCCESS)
    {
        if (packet_copy == NULL)
        {
            err_code = NRF_ERROR_NO_MEM;
        }
        else
        {
            memcpy(packet_copy, p_data, datalen);
        }
    }

    mqttsn_packet_t retransmission_packet = 
    {
        .retransmission_cnt = MQTTSN_DEFAULT_RETRANSMISSION_CNT,
        .p_data             = packet_copy,
        .len                = datalen,
        .timeout            = mqttsn_platform_timer_set_in_ms(MQTTSN_DEFAULT_RETRANSMISSION_TIME_IN_MS),
    };
    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_packet_fifo_elem_add(p_client, &retransmission_packet) != NRF_SUCCESS)
        {
            err_code = NRF_ERROR_NO_MEM;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        if (mqttsn_platform_timer_start(p_client, retransmission_packet.timeout) != NRF_SUCCESS)
        {
            mqttsn_packet_fifo_elem_dequeue(p_client, MQTTSN_MSGTYPE_WILLMSGUPD, MQTTSN_MESSAGE_TYPE);
            err_code = NRF_ERROR_INTERNAL;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = mqttsn_packet_sender_send(p_client, &(p_client->gateway_info.addr), p_data, datalen);
    }

    if (p_data)
    {
        nrf_free(p_data);
    }

    if (packet_copy && err_code != NRF_SUCCESS)
    {
        nrf_free(packet_copy);
    }

    return err_code;
}
