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

#ifndef MQTTSN_PLATFORM_H
#define MQTTSN_PLATFORM_H

#include <stdint.h>

#include "mqttsn_client.h"

typedef void (*timer_timeout_handler_t)(void * p_context);


/**@brief Initializes the MQTT-SN client's platform.  
 *
 * @return NRF_SUCCESS if the initialization has been successful. Otherwise error code is returned.
 */
uint32_t mqttsn_platform_init(void);


/**@brief Initializes the MQTT-SN platform's timer.  
 *
 * @return NRF_SUCCESS if the initialization has been successful. Otherwise error code is returned.
 */
uint32_t mqttsn_platform_timer_init(void);


/**@brief Starts the MQTT-SN platform's timer. 
 *
 * @note Calling this function on a running timer should be ignored.
 *
 * @param[in]    p_client            Pointer to MQTT-SN client instance.
 * @param[in]    timeout_ms          Timeout in milliseconds.
 *
 * @return NRF_SUCCESS if the start operation has been successful. Otherwise error code is returned.
 */
uint32_t mqttsn_platform_timer_start(mqttsn_client_t * p_client, uint32_t timeout_ms);


/**@brief Stops the MQTT-SN platform's timer.  
 *
 * @return NRF_SUCCESS if the stop operation has been successful. Otherwise error code is returned.
 */
uint32_t mqttsn_platform_timer_stop(void);


/**@brief Gets the current MQTT-SN platform's timer value.  
 *
 * @return       Current timer value in milliseconds.
 */
uint32_t mqttsn_platform_timer_cnt_get(void);


/**@brief Gets the MQTT-SN platform's timer max value in milliseconds.  
 *
 * @return       Maximum timer value in milliseconds.
 */
uint32_t mqttsn_platform_timer_resolution_get(void);


/**@brief Converts milliseconds to the MQTT-SN platform's timer ticks.  
 *
 * @param[in]    timeout_ms Time in milliseconds to convert to timer ticks.
 *
 * @return       Calculated number of ticks.
 */
uint32_t mqttsn_platform_timer_ms_to_ticks(uint32_t timeout_ms);


/**@brief Sets the MQTT-SN platform's timer timeout.  
 *
 * @param[in]    timeout_ms Timeout in milliseconds, calculated with current timer value.
 *
 * @return       Calculated timeout in ticks.
 */
uint32_t mqttsn_platform_timer_set_in_ms(uint32_t timeout_ms);


/**@brief MQTT-SN platform's random number generator.  
 *
 * @param[in]    max_val  The maximum value of the generated random number.
 *
 * @return       Calculated random number. 
 */
uint16_t mqttsn_platform_rand(uint16_t max_val);

#endif // MQTTSN_PLATFORM_H