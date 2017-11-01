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

#ifndef MQTTSN_TRANSPORT_BLE_H
#define MQTTSN_TRANSPORT_BLE_H

#include "mqttsn_client.h"


/**@brief Sends MQTT-SN message over BLE.  
 *
 * @param[inout] p_client    Pointer to initialized and connected client. 
 * @param[in]    p_data      Buffered data to send.
 * @param[in]    datalen     Length of the buffered data.
 *
 * @return       NRF_SUCCESS if the message has been sent successfully.
 *               Otherwise error code is returned.
 */

uint32_t mqttsn_transport_write_ble(  mqttsn_client_t     * p_client,
                                      uint8_t             * p_data,
                                      uint16_t              datalen);

uint32_t mqttsn_transport_ble_init(mqttsn_client_t * p_client);

#endif // MQTTSN_TRANSPORT_BLE_H
