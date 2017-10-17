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

#include "mqttsn_platform.h"
#include "mqttsn_packet_internal.h"
#include "app_timer.h"
//#include "openthread/platform/random.h"

/* Available timer is 17-bit. 1FFFF is the biggest 17-bit long number. */
#define MQTTSN_PLATFORM_TIMER_MAX_MS 0x1ffff

APP_TIMER_DEF(m_timer_id);

typedef app_timer_event_t mqttsn_timer_event_t;

static void timer_timeout_handler(void * p_context)
{
    mqttsn_client_t * p_client = (mqttsn_client_t *)p_context;
    mqttsn_client_timer_timeout_handle(p_client);    
}

uint32_t mqttsn_platform_init()
{
    return mqttsn_platform_timer_init();
}

uint32_t mqttsn_platform_timer_init()
{
    app_timer_init();
    return app_timer_create(&m_timer_id, APP_TIMER_MODE_SINGLE_SHOT, &timer_timeout_handler);
}

uint32_t mqttsn_platform_timer_start(mqttsn_client_t * p_client, uint32_t timeout_ms)
{
    uint32_t timeout_ticks = APP_TIMER_TICKS(timeout_ms);
    return app_timer_start(m_timer_id, timeout_ticks, p_client);
}

uint32_t mqttsn_platform_timer_stop()
{
    return app_timer_stop(m_timer_id);
}

uint32_t mqttsn_platform_timer_cnt_get()
{
    uint32_t timeout_ticks = app_timer_cnt_get();
    uint32_t timeout_ms =
        ((uint32_t)ROUNDED_DIV(timeout_ticks * 1000 * (APP_TIMER_CONFIG_RTC_FREQUENCY + 1),
                               (uint32_t)APP_TIMER_CLOCK_FREQ));

    return timeout_ms;
}

uint32_t mqttsn_platform_timer_resolution_get()
{
    return MQTTSN_PLATFORM_TIMER_MAX_MS;
}

uint32_t mqttsn_platform_timer_ms_to_ticks(uint32_t timeout_ms)
{
    return APP_TIMER_TICKS(timeout_ms);
}

uint32_t mqttsn_platform_timer_set_in_ms(uint32_t timeout_ms)
{
    return mqttsn_platform_timer_cnt_get() + timeout_ms;
}

uint16_t mqttsn_platform_rand(uint16_t max_val)
{
    return 1;//otPlatRandomGet() % max_val;
}