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

#ifndef MQTTSN_TRANSPORT_H
#define MQTTSN_TRANSPORT_H

#include "mqttsn_client.h"


/**@brief Initializes the MQTT-SN client's transport.  
 *
 * @param[inout] p_client            Pointer to uninitialized client. 
 * @param[in]    port                Number of the port the client will be bound to.
 * @param[in]    p_context           Pointer to transport layer specific context.
 *
 * @retval       NRF_SUCCESS         If the initialization has been successful.
 * @retval       NRF_ERROR_INTERNAL  Otherwise.
 */
uint32_t mqttsn_transport_init(mqttsn_client_t * p_client, uint16_t port, const void * p_context);


/**@brief Sends message.  
 *
 * @param[inout] p_client    Pointer to initialized and connected client. 
 * @param[in]    p_remote    Pointer to remote endpoint.
 * @param[in]    p_data      Buffered data to send.
 * @param[in]    datalen     Length of the buffered data.
 *
 * @return       NRF_SUCCESS if the message has been sent successfully.
 *               Otherwise error code is returned.
 */
uint32_t mqttsn_transport_write(mqttsn_client_t       * p_client,
                                const mqttsn_remote_t * p_remote,
                                const uint8_t         * p_data,
                                uint16_t                datalen);


/**@brief Receives message.  
 *
 * @param[inout] p_context          Pointer to transport layer specific context. 
 * @param[in]    p_port             Pointer to local port number.
 * @param[in]    p_remote           Pointer to remote endpoint.
 * @param[in]    p_data             Buffer for received data.
 * @param[in]    datalen            Length of the buffered data.
 *
 * @return       NRF_SUCCESS if the message has been processed successfully.
 *               Otherwise error code is returned.
 */
uint32_t mqttsn_transport_read(void                   * p_context,
                               const mqttsn_port_t    * p_port,
                               const mqttsn_remote_t  * p_remote,
                               const uint8_t          * p_data,
                               uint16_t                 datalen);


/**@brief Unitializes the MQTT-SN client's transport.  
 *
 * @param[inout] p_client        Pointer to initialized and connected client.
 *
 * @retval    NRF_SUCCESS        If the uninitialization has been successful.
 * @retval    NRF_ERROR_INTERNAL Otherwise.
 */
uint32_t mqttsn_transport_uninit(mqttsn_client_t * p_client);

#endif // MQTTSN_TRANSPORT_H

