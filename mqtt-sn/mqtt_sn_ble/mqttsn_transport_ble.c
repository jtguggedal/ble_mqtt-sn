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

#include "mqttsn_transport_ble.h"
#include "mqttsn_packet_internal.h"
#include "mem_manager.h"
#include "nrf_log.h"
#include "nrf_error.h"
#include "ble_nus.h"

#include <stdint.h>

#define NULL_PARAM_CHECK(PARAM)                                                                    \
    if ((PARAM) == NULL)                                                                           \
    {                                                                                              \
        return (NRF_ERROR_NULL);                                                                   \
    }

mqttsn_client_t * ptr_client;

uint32_t mqttsn_transport_write_ble(  mqttsn_client_t     * p_client,
                                      uint8_t             * p_data,
                                      uint16_t              datalen)
{
    return ble_nus_string_send(p_client->transport.handle.p_nus, p_data, &datalen);
}


/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_nus    Nordic UART Service structure.
 * @param[in] p_data   Data to be send to UART module.
 * @param[in] length   Length of the data.
 */
/**@snippet [Handling the data received over BLE] */
static void ble_data_handler(ble_nus_evt_t * p_evt)
{
    if (p_evt->type == BLE_NUS_EVT_RX_DATA)
    {
        mqttsn_packet_receiver(ptr_client, NULL, NULL, p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
    }
}


// Init

uint32_t mqttsn_transport_ble_init(mqttsn_client_t * p_client) 
{
    NULL_PARAM_CHECK(p_client);

    ptr_client = p_client;

    ble_nus_t * p_nus_temp = (ble_nus_t *)p_client->transport.handle.p_nus;

    p_nus_temp->data_handler = ble_data_handler;

    return NRF_SUCCESS;
}

