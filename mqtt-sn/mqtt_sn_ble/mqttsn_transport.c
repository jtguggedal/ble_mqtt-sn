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
#include "ble_nus.h"
#include "mqttsn_transport_ble.h"

#include <stdint.h>

#define NULL_PARAM_CHECK(PARAM)                                                                    \
    if ((PARAM) == NULL)                                                                           \
    {                                                                                              \
        return (NRF_ERROR_NULL);                                                                   \
    }



uint32_t mqttsn_transport_write(mqttsn_client_t       * p_client,
                                const mqttsn_remote_t * p_remote,
                                const uint8_t         * p_data,
                                uint16_t                datalen)
{
    NULL_PARAM_CHECK(p_remote);

    switch(p_client->transport.type)
    {
        case MQTTSN_CLIENT_TRANSPORT_THREAD:
            return mqttsn_transport_write(p_client, p_remote, p_data, datalen);
            break;
        case MQTTSN_CLIENT_TRANSPORT_BLE:
            return mqttsn_transport_write_ble(p_client, (uint8_t *)p_data, datalen);
            break;
    }
    return NRF_ERROR_INTERNAL;
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


uint32_t mqttsn_transport_init(mqttsn_client_t * p_client, uint16_t port, const void * p_context)
{

}




uint32_t mqttsn_transport_uninit(mqttsn_client_t * p_client)
{

}

