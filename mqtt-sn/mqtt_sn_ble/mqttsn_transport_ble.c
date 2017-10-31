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



uint32_t mqttsn_transport_write_ble(  mqttsn_client_t     * p_client,
                                             uint8_t             * p_data,
                                             uint16_t              datalen)
{
    return ble_nus_string_send(p_client->transport.handle, p_data, &datalen);
}

/**@brief Creates OpenThread network port. */
static uint32_t port_create(mqttsn_client_t * p_client, uint16_t port)
{
    
}

