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

#ifndef MQTTSN_CLIENT_H
#define MQTTSN_CLIENT_H

#include <stdint.h>
#include <stdbool.h>


/***************************************************************************************************
 * @section DEFINES
 **************************************************************************************************/

/**@brief Default maximum number of elements in packet queue. */
#define MQTTSN_PACKET_FIFO_MAX_LENGTH            4

/**@brief Maximum length of Client ID according to the protocol spec in bytes. */
#define MQTTSN_CLIENT_ID_MAX_LENGTH              23

/**@brief Default time in seconds for which MQTT-SN Client is considered alive by the MQTT-SN Gateway. */
#define MQTTSN_DEFAULT_ALIVE_DURATION            60

/**@brief Default time in seconds for which MQTT-SN Client is considered asleep by the MQTT-SN Gateway. */
#define MQTTSN_DEFAULT_SLEEP_DURATION            30

/**@brief Default value of the clean session flag in CONNECT message. */
#define MQTTSN_DEFAULT_CLEAN_SESSION_FLAG        1

/**@brief Default value of the will flag in CONNECT message. */
#define MQTTSN_DEFAULT_WILL_FLAG                 0

/**@brief Default port MQTT-SN client binds to. */
#define MQTTSN_DEFAULT_CLIENT_PORT               47193

/**@brief Default retransmission time in milliseconds. */
#define MQTTSN_DEFAULT_RETRANSMISSION_TIME_IN_MS 8000

/**@brief Default number of retransmission retries. */
#define MQTTSN_DEFAULT_RETRANSMISSION_CNT        2

/**@brief Length of an IPv6 address in bytes. For internal use only */
#define IPV6_ADDR_BYTE_LENGTH                    16

/**@brief Maximum length of will feature buffers. For internal use only */
#define MQTTSN_WILL_TOPIC_MAX_LENGTH             32
#define MQTTSN_WILL_MSG_MAX_LENGTH               32


/***************************************************************************************************
 * @section TYPES
 **************************************************************************************************/

/**@brief Port identification information. */
typedef uint16_t mqttsn_port_t;

/**@brief Remote endpoint. */
typedef struct mqttsn_remote_t
{
    uint8_t  addr[IPV6_ADDR_BYTE_LENGTH]; /**< IPv6 address. */
    uint16_t port_number;                 /**< Port number. */
} mqttsn_remote_t;

/**@brief Regular topic information. */
typedef struct mqttsn_topic_t
{
    const uint8_t * p_topic_name; /**< Topic name. */
    uint16_t        topic_id;     /**< Topic ID. */
} mqttsn_topic_t;

/**@brief Packet information. */
typedef struct mqttsn_packet_t
{
    uint8_t        retransmission_cnt; /**< Number of retransmissions to attempt if necessary. */
    uint8_t      * p_data;             /**< Message content. */
    uint16_t       len;                /**< Length of the message. */
    uint16_t       id;                 /**< Message ID. */
    uint32_t       timeout;            /**< Time of the next retransmissions in ms (if necessary). */
    mqttsn_topic_t topic;              /**< Topic of the message. */
} mqttsn_packet_t;

/**@brief Packet queueing data available for client. For internal use only */
typedef struct mqttsn_packet_queue_t
{
    mqttsn_packet_t packet[MQTTSN_PACKET_FIFO_MAX_LENGTH]; /**< Array of packets. */
    uint8_t         num_of_elements;                       /**< Current number of elements in the queue. */
} mqttsn_packet_queue_t;

/**@brief State of client. For internal use only */
typedef enum mqttsn_client_state_t
{
    MQTTSN_CLIENT_IDLE = 0,                /**< Client idle. */
    MQTTSN_CLIENT_SEARCHING_GATEWAY,       /**< Client is searching for a gateway. */
    MQTTSN_CLIENT_GATEWAY_FOUND,           /**< Client has found a gateway. */
    MQTTSN_CLIENT_ESTABLISHING_CONNECTION, /**< Client is attempting to connect. */
    MQTTSN_CLIENT_CONNECTED,               /**< Client is connected. */
    MQTTSN_CLIENT_DISCONNECTED,            /**< Client is disconnected. */
    MQTTSN_CLIENT_WAITING_FOR_SLEEP,       /**< Client is waiting for permission to sleep. */
    MQTTSN_CLIENT_WAITING_FOR_DISCONNECT,  /**< Client is waiting for permission to disconnect. */
    MQTTSN_CLIENT_ASLEEP,                  /**< Client is in sleep mode. */
} mqttsn_client_state_t;

/**@brief MQTT-SN gateway information. */
typedef struct mqttsn_gw_info_t
{
    uint8_t         id;   /**< Gateway ID. */
    mqttsn_remote_t addr; /**< Address and port number of the gateway. */
} mqttsn_gw_info_t;

/**@brief MQTT-SN client connect options. */
typedef struct mqttsn_connect_opt_t
{
    uint16_t  alive_duration;                             /**< Keep alive timer duration in seconds. */
    uint8_t   clean_session;                              /**< Clean session flag. */
    uint8_t   will_flag;                                  /**< Will feature flag. */
    uint8_t   p_will_topic[MQTTSN_WILL_TOPIC_MAX_LENGTH]; /**< Will topic (optional). */
    uint16_t  will_topic_len;                             /**< Length of the will topic. */
    uint8_t   p_will_msg[MQTTSN_WILL_MSG_MAX_LENGTH];     /**< Will message (optional). */
    uint16_t  will_msg_len;                               /**< Length of the will message. */
    uint8_t   p_client_id[MQTTSN_CLIENT_ID_MAX_LENGTH];   /**< Unique client ID. */
    uint16_t  client_id_len;                              /**< Client ID length. */
} mqttsn_connect_opt_t;

/**@brief MQTT-SN event identifier. */
typedef enum mqttsn_event_id_t
{
    MQTTSN_EVENT_GATEWAY_FOUND,     /**< Client has found a gateway. */
    MQTTSN_EVENT_CONNECTED,         /**< Client has connected successfully. */
    MQTTSN_EVENT_DISCONNECTED,      /**< Client has disconnected. */
    MQTTSN_EVENT_REGISTERED,        /**< Client has registered a topic. */
    MQTTSN_EVENT_REGISTER_RECEIVED, /**< Client has received a topic to register (wildcard topic only) */
    MQTTSN_EVENT_PUBLISHED,         /**< Client has published data successfully. */
    MQTTSN_EVENT_SUBSCRIBED,        /**< Client has subscribed successfully. */
    MQTTSN_EVENT_UNSUBSCRIBED,      /**< Client has unsubscribed successfully. */
    MQTTSN_EVENT_WILL_TOPIC_UPD,    /**< Client has updated the will topic successfully. */
    MQTTSN_EVENT_WILL_MSG_UPD,      /**< Client has updated the will message successfully. */
    MQTTSN_EVENT_RECEIVED,          /**< Client has received data to subscribed topic. */
    MQTTSN_EVENT_SLEEP_PERMIT,      /**< Client is allowed to sleep. */
    MQTTSN_EVENT_SLEEP_STOP,        /**< Client should wake up. */
    MQTTSN_EVENT_TIMEOUT            /**< Message hasn't been delivered successfully. Reason: mqttsn_error_t */
} mqttsn_event_id_t;

/**@brief MQTT-SN sending error the application shall handle. */
typedef enum mqttsn_error_t
{
    MQTTSN_ERROR_REJECTED_CONGESTION, /**< Message has been rejected due to network congestion. */
    MQTTSN_ERROR_TIMEOUT              /**< Retransmission limit has been reached. */
} mqttsn_error_t;

/**@brief MQTT-SN ACK message error. Is forwarded to the application when MQTTSN_EVENT_TIMEOUT occurs. */
typedef enum mqttsn_ack_error_t 
{
    MQTTSN_PACKET_CONNACK,      /**< CONNACK message has not been received. */
    MQTTSN_PACKET_REGACK,       /**< REGACK message has not been received. */
    MQTTSN_PACKET_PUBACK,       /**< PUBACK message has not been received. */
    MQTTSN_PACKET_SUBACK,       /**< SUBACK message has not been received. */
    MQTTSN_PACKET_UNSUBACK,     /**< UNSUBACK message has not been received. */
    MQTTSN_PACKET_PINGREQ,      /**< PINGREQ message has not been received. */
    MQTTSN_PACKET_WILLTOPICUPD, /**< WILLTOPICUPD message has not been received. */
    MQTTSN_PACKET_WILLMSGUPD,   /**< WILLMSGUPD message has not been received. */
    MQTTSN_PACKET_INCORRECT     /**< Unknown error. */
} mqttsn_ack_error_t;

/**@brief MQTT-SN event data when client received GWINFO message. */
typedef struct mqttsn_event_gwinfo_t
{
    uint8_t           gateway_id;     /**< Gateway ID. */
    mqttsn_remote_t * p_gateway_addr; /**< Pointer to gateway address. */
} mqttsn_event_gwinfo_t;

/**@brief MQTT-SN event data when client received REGISTER or REGACK message. */
typedef struct mqttsn_event_register_t
{
    mqttsn_packet_t packet; /**< Packet forwarded to the application. */
} mqttsn_event_register_t;

/**@brief MQTT-SN event data when client received PUBLISH message. */
typedef struct mqttsn_event_publish_t
{
    mqttsn_packet_t packet;    /**< Packet forwarded to the application. */
    uint8_t       * p_payload; /**< Payload of the message. */
} mqttsn_event_publish_t;

/**@brief MQTT-SN event data when a retransmission error occurred. */
typedef struct mqttsn_event_error_t
{
    mqttsn_error_t     error;    /**< Type of error that occurred. */
    mqttsn_ack_error_t msg_type; /**< Type of ACK that has not been received. */
    uint16_t           msg_id;   /**< Message ID. If it is not specified for given message type, coded 0. */
} mqttsn_event_error_t;

/**@brief MQTT-SN event data for specific events. */
typedef union mqttsn_event_data_t
{
    mqttsn_event_gwinfo_t   connected;  /**< Data forwarded to the application when a gateway is found. */
    mqttsn_event_register_t registered; /**< Data forwarded to the application when a topic is registered. */
    mqttsn_event_publish_t  published;  /**< Data forwarded to the application when PUBLISH message is received. */
    mqttsn_event_error_t    error;      /**< Data forwarded to the application when a retransmission error occurred. */
} mqttsn_event_data_t;

/**@brief MQTT-SN event. */
typedef struct mqttsn_event_t
{
    mqttsn_event_id_t   event_id;   /**< Event identifier. */
    mqttsn_event_data_t event_data; /**< Event data. */
} mqttsn_event_t;

/**@brief MQTT-SN keep-alive procedure. */
typedef struct mqttsn_keep_alive_t
{
    uint32_t        timeout;          /**< Time of the next keep alive message in milliseconds. */
    uint32_t        duration;         /**< Keep alive duration in milliseconds. */
    uint8_t         response_arrived; /**< 1 when gateway has responded to previous keep alive, 0 otherwise. */
    mqttsn_packet_t message;          /**< Keep alive message (PINGREQ). */
} mqttsn_keep_alive_t;

/**@brief Forward declaration of MQTT-SN client. */
typedef struct mqttsn_client_t mqttsn_client_t;

/**@brief MQTT-SN event handler.
 * 
 * @param[inout]  p_client Pointer to initialized client.
 * @param[in]     p_event  Pointer to MQTT-SN event.
 */
typedef void (*mqttsn_client_evt_handler_t)(mqttsn_client_t * p_client, mqttsn_event_t * p_event);

/**@brief MQTT-SN Client.
 *
 * @details Below parameters are for internal use only.
 */
struct mqttsn_client_t
{
    uint16_t                    message_id;   /**< Current message ID. */
    mqttsn_keep_alive_t         keep_alive;   /**< Keep alive data. */
    mqttsn_client_state_t       client_state; /**< Current state of the client. */
    mqttsn_gw_info_t            gateway_info; /**< Gateway information. */
    mqttsn_connect_opt_t        connect_info; /**< Connect options. */
    mqttsn_packet_queue_t       packet_queue; /**< Packet queue. */
    mqttsn_client_evt_handler_t evt_handler;  /**< Event handler. */
};


/***************************************************************************************************
 * @section API
 **************************************************************************************************/

/**@brief Initializes the MQTT-SN client.  
 * 
 * @param[out] p_client            Pointer to initialized client.
 * @param[in]  port                Number of the port the client will be bound to.
 * @param[in]  evt_handler         Pointer to function handling MQTT-SN client events.
 * @param[in]  p_transport_context Pointer to context specific for transport layer.
 *
 * @retval     NRF_SUCCESS         If the initialization has been successful.
 * @retval     NRF_ERROR_INTERNAL  Otherwise.
 */
uint32_t mqttsn_client_init(mqttsn_client_t             * p_client,
                            uint16_t                      port,
                            mqttsn_client_evt_handler_t   evt_handler
                            /*const void                  * p_transport_context*/);

/**@brief Searches gateway.  
 *
 * @param[inout] p_client    Pointer to initialized client.
 *
 * @return       NRF_SUCCESS if the gateway info request has been sent successfully.
 *               Otherwise error code is returned.
 */
uint32_t mqttsn_client_search_gateway(mqttsn_client_t * p_client);


/**@brief Connects the MQTT-SN client to gateway.  
 *
 * @param[inout] p_client    Pointer to initialized client.
 * @param[in]    p_remote    Pointer to the gateway endpoint.
 * @param[in]    gateway_id  Gateway ID as received from the gateway in GWINFO message.
 * @param[in]    p_options   Connect options.
 *
 * @return       NRF_SUCCESS if the connection request has been sent successfully.
 *               Otherwise error code is returned.
 */
uint32_t mqttsn_client_connect(mqttsn_client_t      * p_client,
                               mqttsn_remote_t      * p_remote,
                               uint8_t                gateway_id,
                               mqttsn_connect_opt_t * p_options);


/**@brief Disconnects the MQTT-SN client.  
 *
 * @param[inout] p_client     Pointer to initialized and connected client.
 *
 * @return       NRF_SUCCESS if the disconnection request has been sent successfully.
 *               Otherwise error code is returned.
 */
uint32_t mqttsn_client_disconnect(mqttsn_client_t * p_client);


/**@brief Puts the MQTT-SN client in asleep state.  
 *
 * @details MQTT-SN Disconnect message with appropriate duration field is sent. 
 *
 * @param[inout] p_client     Pointer to initialized and connected client.
 * @param[in]    polling_time Value of time in seconds for which the client is considered asleep by the gateway.
 *
 * @return       NRF_SUCCESS if the message has been sent successfully.
 *               Otherwise error code is returned.
 */
uint32_t mqttsn_client_sleep(mqttsn_client_t * p_client, uint16_t polling_time);


/**@brief Registers topic.  
 *
 * @details Topic name with assigned topic ID can be accessed in MQTT-SN_EVENT_REGISTERED callback.  
 *
 * @param[inout] p_client       Pointer to initialized and connected client.
 * @param[in]    p_topic_name   String buffer containing the topic name.
 * @param[in]    topic_name_len Topic name length.
 * @param[out]   msg_id         (optional) Pointer to message ID assigned to the message by client. 
 *
 * @return       NRF_SUCCESS if the registration request has been sent successfully.
 *               Otherwise error code is returned.
 */
uint32_t mqttsn_client_topic_register(mqttsn_client_t * p_client,
                                      const uint8_t   * p_topic_name,
                                      uint16_t          topic_name_len,
                                      uint16_t        * msg_id);


/**@brief Publishes data to given topic.  
 *
 * @param[inout] p_client       Pointer to initialized and connected client.
 * @param[in]    topic_id       Value of previously registered topic ID. 
 * @param[in]    p_payload      Data to be published.
 * @param[in]    payload_len    Length of data to be published.
 * @param[out]   msg_id         (optional) Pointer to message ID assigned to the message by client. 
 *
 * @return       NRF_SUCCESS if the publish request has been sent successfully.
 *               Otherwise error code is returned.
 */
uint32_t mqttsn_client_publish(mqttsn_client_t * p_client,
                               uint16_t          topic_id,
                               const uint8_t   * p_payload,
                               uint16_t          payload_len,
                               uint16_t        * msg_id);


/**@brief Subscribes to given topic.  
 *
 * @param[inout] p_client       Pointer to initialized and connected client.
 * @param[in]    p_topic_name   String buffer containing the topic name.
 * @param[in]    topic_name_len Topic name length.
 * @param[out]   msg_id         (optional) Pointer to message ID assigned to the message by client.
 *
 * @return       NRF_SUCCESS if the subscribe request has been sent successfully.
 *               Otherwise error code is returned.
 */
uint32_t mqttsn_client_subscribe(mqttsn_client_t * p_client,
                                 const uint8_t   * p_topic_name,
                                 uint16_t          topic_name_len,
                                 uint16_t        * msg_id);


/**@brief Unsubscribes given topic.  
 *
 * @param[inout] p_client       Pointer to initialized and connected client.
 * @param[in]    p_topic_name   String buffer containing the topic name.
 * @param[in]    topic_name_len Topic name length.
 * @param[out]   msg_id         (optional) Pointer to message ID assigned to the message by client.
 *
 * @return       NRF_SUCCESS if the subscribe request has been sent successfully.
 *               Otherwise error code is returned.
 */
uint32_t mqttsn_client_unsubscribe(mqttsn_client_t * p_client,
                                   const uint8_t   * p_topic_name,
                                   uint16_t          topic_name_len,
                                   uint16_t        * msg_id);


/**@brief Updates MQTT-SN client's will topic.  
 *
 * @details This feature is not supported by the Paho MQTT-SN Gateway yet.
 *          Therefore it has not been tested completely.
 *
 * @param[inout] p_client       Pointer to initialized and connected client.
 * @param[in]    p_will_topic   String buffer containing the new will topic name.
 * @param[in]    will_topic_len Will topic name length.
 *
 * @return       NRF_SUCCESS if the update request has been sent successfully.
 *               Otherwise error code is returned.
 */
uint32_t mqttsn_client_willtopicupd(mqttsn_client_t * p_client,
                                    const uint8_t   * p_will_topic,
                                    uint16_t          will_topic_len);


/**@brief Updates MQTT-SN client's will message.  
 *
 * @details This feature is not supported by the Paho MQTT-SN Gateway yet.
 *          Therefore it has not been tested completely.
 *
 * @param[inout] p_client     Pointer to initialized and connected client.
 * @param[in]    p_will_msg   String buffer containing the new will message name.
 * @param[in]    will_msg_len Will message name length.
 *
 * @return       NRF_SUCCESS if the update request has been sent successfully.
 *               Otherwise error code is returned.
 */
uint32_t mqttsn_client_willmsgupd(mqttsn_client_t * p_client,
                                  const uint8_t   * p_will_msg,
                                  uint16_t          will_msg_len);


/**@brief Unitializes the MQTT-SN client.  
 *
 * @details Unitializes transport layer and packet queue.
 *
 * @param[inout] p_client Pointer to initialized and connected client.
 *
 * @return    NRF_SUCCESS if the uninitialization has been successful.
 *               Otherwise error code is returned.
 */
uint32_t mqttsn_client_uninit(mqttsn_client_t * p_client);

#endif // MQTTSN_CLIENT_H

