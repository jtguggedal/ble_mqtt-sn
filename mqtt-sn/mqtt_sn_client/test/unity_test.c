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

#include "unity.h"
#include "mock_mqttsn_transport.h"
#include "mock_mqttsn_platform.h"
#include "mock_mem_manager.h"
#include "mock_app_timer.h"
#include "sdk_config.h"
#include "mqttsn_client.c"
#include "mqttsn_packet_sender.c"
#include "mqttsn_packet_receiver.c"
#include "mqttsn_packet_fifo.c"

#define MQTTSN_BROADCAST_ADDR { 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }
#define MQTTSN_DEFAULT_CLIENT_PORT         47193

#define OPENTHREAD_TRANSPORT_ERROR         1
#define OPENTHREAD_MAX_TRANSPORT_ERROR     33
#define TRANSPORT_ERROR                    OPENTHREAD_TRANSPORT_ERROR
#define MAX_TRANSPORT_ERROR                OPENTHREAD_MAX_TRANSPORT_ERROR

#define CONNECT_WILL_ENABLED               1
#define CONNECT_WILL_DISABLED              0

#define MQTTSN_TEST_DEFAULT_ALIVE_DURATION 1
#define NRF_TIMER_RESOLUTION               0x1ffff

#define NRF_MALLOC_MEMORY_BLOCK            64
#define NRF_MALLOC_MEMORY_BLOCK_XL         1024

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint32_t         m_timer_val     = 0;
static uint32_t         m_timeout_val   = 1;
static uint32_t         m_instance      = 1;
static uint16_t         m_port          = 1;
static uint16_t         m_duration      = 1;
static uint16_t         m_polling_time  = 1;
static uint16_t         m_topic_id      = 1; 
static uint8_t          m_gateway_id    = 1;
static uint8_t          m_client_id[]   = "test_id";
static const uint8_t    m_topic_name[]  = { 0x74, 0x65, 0x73, 0x74, 0x5f, 0x74, 0x6f, 0x70, 0x69, 0x63 };
static const uint8_t    m_payload[]     = { 0x74, 0x65, 0x73, 0x74, 0x5f, 0x70, 0x61, 0x79, 0x6c, 0x6f, 0x61, 0x64 };
static const uint8_t    m_will_topic[]  = { 0x74, 0x65, 0x73, 0x74, 0x5f, 0x74, 0x6f, 0x70, 0x69, 0x63 };
static const uint8_t    m_will_msg[]    = { 0x74, 0x65, 0x73, 0x74, 0x5f, 0x70, 0x61, 0x79, 0x6c, 0x6f, 0x61, 0x64 };
static mqttsn_client_t  m_client;
static mqttsn_remote_t  m_remote        = 
{
    .addr        = {0xfd, 0xde, 0xad, 0x00, 0xbe, 0xef, 0x00, 0x00, 0x31, 0x01, 0x09, 0x69, 0x77, 0x46, 0xe6, 0x48},
    .port_number = 1,
};

static mqttsn_connect_opt_t m_connect_opt = 
{
    .alive_duration = MQTTSN_TEST_DEFAULT_ALIVE_DURATION,
    .clean_session  = MQTTSN_DEFAULT_CLEAN_SESSION_FLAG,
    .will_flag      = MQTTSN_DEFAULT_WILL_FLAG,
};

typedef struct expected_write_t
{
    mqttsn_client_t * p_client;
    mqttsn_remote_t * p_remote;
    uint8_t         * p_data;
    uint16_t          datalen;
} expected_write_t;

static expected_write_t m_expected_write_stub;
static uint32_t         m_expected_write_counter = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

                                            /* len | tp  | rad  */
static uint8_t m_test_searchgw_packet[]    = { 0x03, 0x01, 0x01 };

                                            /* len | tp  | flg | pid | duration  |  t     e     s     t     _     i     d   */
static uint8_t m_test_connect_packet[]     = { 0x0d, 0x04, 0x04, 0x01, 0x00, 0x01, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x69, 0x64 }; 

                                            /* len | tp   */
static uint8_t m_test_disconnect_packet[]  = { 0x02, 0x18 };

                                            /* len | tp  | poll_time  */
static uint8_t m_test_sleep_packet[]       = { 0x04, 0x18, 0x00, 0x19 };

                                            /* len | tp  | topic_id  |  msg_id   |  t     e     s     t     _     t     o     p     i     c   */
static uint8_t m_test_register_packet[]    = { 0x10, 0x0a, 0x00, 0x00, 0x00, 0x01, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x74, 0x6f, 0x70, 0x69, 0x63 };

                                            /* len | tp  | flg | topic_id  |  msg_id   |  t     e     s     t     _     p     a     y     l     o     a     d  */
static uint8_t m_test_publish_packet[]     = { 0x13, 0x0c, 0x20, 0x00, 0x01, 0x00, 0x01, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x70, 0x61, 0x79, 0x6c, 0x6f, 0x61, 0x64 };

                                            /* len | tp  | flg |  msg_id   |  t     e     s     t     _     t     o     p     i     c   */
static uint8_t m_test_subscribe_packet[]   = { 0x0f, 0x12, 0x20, 0x00, 0x01, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x74, 0x6f, 0x70, 0x69, 0x63 };

                                            /* len | tp  | flg |  msg_id   |  t     e     s     t     _     t     o     p     i     c   */
static uint8_t m_test_unsubscribe_packet[] = { 0x0f, 0x14, 0x00, 0x00, 0x01, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x74, 0x6f, 0x70, 0x69, 0x63 };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

                                               /* len | tp  | gwid */
static uint8_t m_test_gwinfo_packet[]         = { 0x03, 0x02, 0x01 };

                                               /* len | tp  | rc  */
static uint8_t m_test_connack_acc_packet[]    = { 0x03, 0x05, 0x00 }; // Return Code : Accepted

                                               /* len | tp  | rc  */
static uint8_t m_test_connack_rcg_packet[]    = { 0x03, 0x05, 0x01 }; // Return Code : Rejected (congestion)

                                               /* len | tp  | rc  */
static uint8_t m_test_connack_rid_packet[]    = { 0x03, 0x05, 0x02 }; // Return Code : Rejected (invalid topic ID)

                                               /* len | tp  | rc  */
static uint8_t m_test_connack_rns_packet[]    = { 0x03, 0x05, 0x03 }; // Return Code : Rejected (not supported)

                                               /* len | tp  |  rc  */
static uint8_t m_test_connack_bad_packet[]    = { 0x03, 0x05, 0xff }; // Incorrect Return Code : Reserved

                                               /* len | tp   */
static uint8_t m_test_willtopicreq_packet[]   = { 0x02, 0x06 };

                                               /* len | tp   */
static uint8_t m_test_willmsgreq_packet[]     = { 0x02, 0x08 };

                                               /* len | tp  | flg |  t     e     s     t     _     t     o     p     i     c   */
static uint8_t m_test_willtopicupd_packet[]   = { 0x0d, 0x1a, 0x20, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x74, 0x6f, 0x70, 0x69, 0x63 };

                                               /* len | tp  |  t     e     s     t     _     p     a     y     l     o     a     d  */
static uint8_t m_test_willmsgupd_packet[]     = { 0x0e, 0x1c, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x70, 0x61, 0x79, 0x6c, 0x6f, 0x61, 0x64 };

                                               /* len | tp  | topic_id  |  msg_id   |  t     e     s     t     _     t     o     p     i     c   */
static uint8_t m_test_register_rec_packet[]   = { 0x10, 0x0a, 0x00, 0x01, 0x00, 0x01, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x74, 0x6f, 0x70, 0x69, 0x63 };

                                               /* len | tp  | topic_id  |  msg_id   |  t     e     s     t     _     t     o     p     i     c   */
static uint8_t m_test_register_bad_packet[]   = { 0x20, 0x0a, 0x00, 0x01, 0x00, 0x01, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x74, 0x6f, 0x70, 0x69, 0x63 }; // Incorrect Length : Too Long

                                               /* len | tp  | topic_id  |  msg_id   | rc   */
static uint8_t m_test_regack_acc_packet[]     = { 0x07, 0x0b, 0x00, 0x01, 0x00, 0x01, 0x00 }; // Return Code : Accepted

                                               /* len | tp  | topic_id  |  msg_id   | rc   */
static uint8_t m_test_regack_rcg_packet[]     = { 0x07, 0x0b, 0x00, 0x01, 0x00, 0x01, 0x01 }; // Return Code : Rejected (congestion)

                                               /* len | tp  | topic_id  |  msg_id   | rc   */
static uint8_t m_test_regack_rid_packet[]     = { 0x07, 0x0b, 0x00, 0x01, 0x00, 0x01, 0x02 }; // Return Code : Rejected (invalid topic ID)

                                               /* len | tp  | topic_id  |  msg_id   | rc   */
static uint8_t m_test_regack_rns_packet[]     = { 0x07, 0x0b, 0x00, 0x01, 0x00, 0x01, 0x03 }; // Return Code : Rejected (not supported)

                                               /* len | tp  | topic_id  |  msg_id   | rc   */
static uint8_t m_test_regack_invid_packet[]   = { 0x07, 0x0b, 0x00, 0x01, 0x00, 0x02, 0x00 };

                                               /* len | tp  | topic_id  |  msg_id   | rc   */
static uint8_t m_test_regack_bad_packet[]     = { 0x07, 0xdd, 0x00, 0x01, 0x00, 0x01, 0x00 }; // Incorrect Message Type : Reserved

                                               /* len | tp  | flg | topic_id  |  msg_id   |  t     e     s     t     _     p     a     y     l     o     a     d  */
static uint8_t m_test_publish_rec_packet[]    = { 0x13, 0x0c, 0x20, 0x00, 0x01, 0x00, 0x01, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x70, 0x61, 0x79, 0x6c, 0x6f, 0x61, 0x64 };

                                               /* len | tp  | topic_id  |  msg_id   | rc   */
static uint8_t m_test_puback_acc_packet[]     = { 0x07, 0x0d, 0x00, 0x01, 0x00, 0x01, 0x00 }; // Return Code : Accepted

                                               /* len | tp  | topic_id  |  msg_id   | rc   */
static uint8_t m_test_puback_rcg_packet[]     = { 0x07, 0x0d, 0x00, 0x01, 0x00, 0x01, 0x01 }; // Return Code : Rejected (congestion)

                                               /* len | tp  | topic_id  |  msg_id   | rc   */
static uint8_t m_test_puback_rid_packet[]     = { 0x07, 0x0d, 0x00, 0x01, 0x00, 0x01, 0x02 }; // Return Code : Rejected (invalid topic ID)

                                               /* len | tp  | topic_id  |  msg_id   | rc   */
static uint8_t m_test_puback_rns_packet[]     = { 0x07, 0x0d, 0x00, 0x01, 0x00, 0x01, 0x03 }; // Return Code : Rejected (not supported)

                                               /* len | tp  | topic_id  |  msg_id   | rc   */
static uint8_t m_test_puback_invid_packet[]   = { 0x07, 0x0d, 0x00, 0x01, 0x00, 0x02, 0x00 };

                                               /* len | tp  | topic_id  |  msg_id   | rc   */
static uint8_t m_test_puback_bad_packet[]     = { 0x07, 0x0d, 0x00, 0x01, 0x00 }; // Incorrect Message : Two Last Bytes Lost

                                               /* len | tp  | flg | topic_id  |  msg_id   | rc   */
static uint8_t m_test_suback_acc_packet[]     = { 0x08, 0x13, 0x20, 0x00, 0x01, 0x00, 0x01, 0x00 }; // Return Code : Accepted

                                               /* len | tp  | flg | topic_id  |  msg_id   | rc   */
static uint8_t m_test_suback_rcg_packet[]     = { 0x08, 0x13, 0x20, 0x00, 0x01, 0x00, 0x01, 0x01 }; // Return Code : Rejected (congestion)

                                               /* len | tp  | flg | topic_id  |  msg_id   | rc   */
static uint8_t m_test_suback_rid_packet[]     = { 0x08, 0x13, 0x20, 0x00, 0x01, 0x00, 0x01, 0x02 }; // Return Code : Rejected (invalid topic ID)

                                               /* len | tp  | flg | topic_id  |  msg_id   | rc   */
static uint8_t m_test_suback_rns_packet[]     = { 0x08, 0x13, 0x20, 0x00, 0x01, 0x00, 0x01, 0x03 }; // Return Code : Rejected (not supported)

                                               /* len | tp  | flg | topic_id  |  msg_id   | rc   */
static uint8_t m_test_suback_invid_packet[]   = { 0x08, 0x13, 0x20, 0x00, 0x01, 0x00, 0x02, 0x00 };

                                               /* len | tp  | topic_id  |  msg_id   | rc   */
static uint8_t m_test_suback_bad_packet[]     = { 0x08, 0x13, 0x00, 0x01, 0x00, 0x02, 0x00 }; // Incorrect Message : Flag Byte Lost

                                               /* len | tp  |   msg_id   */
static uint8_t m_test_unsuback_acc_packet[]   = { 0x04, 0x15, 0x00, 0x01 };

                                               /* len | tp  |   msg_id   */
static uint8_t m_test_unsuback_invid_packet[] = { 0x04, 0x15, 0x00, 0x02 };

                                               /* len | tp  |   msg_id   */
static uint8_t m_test_unsuback_bad_packet[]   = { 0x00, 0x15 }; // Incorrect Message : Only Two First Bytes Left

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint32_t m_expected_evt_handler_calls = 0;
static uint32_t m_evt_handler_calls          = 0;

static uint32_t stub_transport_write(mqttsn_client_t * p_client, const mqttsn_remote_t * p_remote, const uint8_t * p_data, uint16_t datalen, int cmock_num_calls)
{
    TEST_ASSERT_EQUAL_UINT32(m_expected_write_stub.datalen, datalen);
    TEST_ASSERT_EQUAL_MEMORY(m_expected_write_stub.p_data, p_data, m_expected_write_stub.datalen);
}

static void _evt_handler(mqttsn_client_t * p_client, mqttsn_event_t * p_event)
{
    m_evt_handler_calls++;
    TEST_ASSERT_TRUE(m_evt_handler_calls <= m_expected_evt_handler_calls);
}

static void init_connect_opt()
{
    m_connect_opt.will_topic_len = strlen(m_will_topic);
    m_connect_opt.will_msg_len   = strlen(m_will_msg);
    m_connect_opt.client_id_len  = strlen(m_client_id);

    memcpy(m_connect_opt.p_will_topic, (unsigned char *)m_will_topic, m_connect_opt.will_topic_len);
    memcpy(m_connect_opt.p_will_msg,    &m_will_msg,                  m_connect_opt.will_msg_len);
    memcpy(m_connect_opt.p_client_id,  (unsigned char *)m_client_id,  m_connect_opt.client_id_len);
}

static void emulate_init_state(mqttsn_client_t * p_client)
{
                                  p_client            = &m_client;
    uint16_t                      port                = m_port;
    mqttsn_client_evt_handler_t   evt_handler         = _evt_handler;
    void                        * p_transport_context = &m_instance;

    nrf_mem_init_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_transport_init_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_platform_init_IgnoreAndReturn(NRF_SUCCESS);

    p_client->keep_alive.timeout                    = m_timer_val + m_timeout_val;
    p_client->keep_alive.duration                   = MQTTSN_DEFAULT_ALIVE_DURATION;
    p_client->keep_alive.response_arrived           = 0;
    p_client->keep_alive.message.retransmission_cnt = MQTTSN_DEFAULT_RETRANSMISSION_CNT + 1;

    mqttsn_client_init(p_client, port, evt_handler, p_transport_context);
}

static void emulate_connected_state(mqttsn_client_t * p_client)
{
    emulate_init_state(p_client);
    init_connect_opt();
    p_client->client_state = MQTTSN_CLIENT_CONNECTED;
}

static void emulate_gateway_found_state(mqttsn_client_t * p_client)
{
    emulate_init_state(p_client);
    p_client->client_state = MQTTSN_CLIENT_GATEWAY_FOUND;
}

static void emulate_timer_start()
{
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
}

static void emulate_sending_packet(void)
{
    static uint8_t buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(buf);
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    nrf_free_Ignore();
}

void setUp(void){}

void tearDown(void)
{
    memset(&m_client, 0, sizeof(m_client));
    memset(&m_expected_write_stub, 0, sizeof(m_expected_write_stub));
    memset(&m_expected_evt_handler_calls, 0, sizeof(m_expected_evt_handler_calls));
    memset(&m_evt_handler_calls, 0, sizeof(m_evt_handler_calls));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************************
 * @section INITIALIZE
 **************************************************************************************************/

void test_mqttsn_client_init_ShallReturnNullWhenNullParameter(void)
{
    uint32_t return_value = NRF_SUCCESS;

    mqttsn_client_t             * p_client_1            = NULL;
    uint16_t                      port_1                = m_port;
    mqttsn_client_evt_handler_t   evt_handler_1         = _evt_handler;
    void                        * p_transport_context_1 = &m_instance;

    mqttsn_client_t             * p_client_2            = &m_client;
    uint16_t                      port_2                = m_port;
    mqttsn_client_evt_handler_t   evt_handler_2         = NULL;
    void                        * p_transport_context_2 = &m_instance;

    mqttsn_client_t             * p_client_3            = &m_client;
    uint16_t                      port_3                = m_port;
    mqttsn_client_evt_handler_t   evt_handler_3         = _evt_handler;
    void                        * p_transport_context_3 = NULL;

    return_value = mqttsn_client_init(p_client_1, port_1, evt_handler_1, p_transport_context_1);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_init(p_client_2, port_2, evt_handler_2, p_transport_context_2);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_init(p_client_3, port_3, evt_handler_3, p_transport_context_3);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);
}

void test_mqttsn_client_init_ShallReturnErrorWhenInternalInitFails(void)
{
    uint32_t return_value = NRF_SUCCESS;

    mqttsn_client_t             * p_client_1            = &m_client;
    uint16_t                      port_1                = m_port;
    mqttsn_client_evt_handler_t   evt_handler_1         = _evt_handler;
    void                        * p_transport_context_1 = &m_instance;

    mqttsn_client_t             * p_client_2            = &m_client;
    uint16_t                      port_2                = m_port;
    mqttsn_client_evt_handler_t   evt_handler_2         = _evt_handler;
    void                        * p_transport_context_2 = &m_instance;

    nrf_mem_init_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_transport_init_IgnoreAndReturn(NRF_ERROR_INTERNAL);
    return_value = mqttsn_client_init(p_client_1, port_1, evt_handler_1, p_transport_context_1);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

/***************************************************************************************************
 * @section SEARCH GATEWAY
 **************************************************************************************************/

void test_mqttsn_client_search_gateway_ShallReturnErrorWhenNullClient(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = NULL;

    return_value = mqttsn_client_search_gateway(p_client);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);
}

void test_mqttsn_client_search_gateway_ShallReturnErrorWhenClientNotInitialized(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;

    return_value = mqttsn_client_search_gateway(p_client);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_search_gateway_ShallReturnErrorWhenClientConnected(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    emulate_connected_state(p_client);

    return_value = mqttsn_client_search_gateway(p_client);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INVALID_STATE, return_value);
}

void test_mqttsn_client_search_gateway_ShallReturnErrorWhenBufferAllocFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    emulate_init_state(p_client);

    mqttsn_platform_rand_IgnoreAndReturn(m_timeout_val);
    emulate_timer_start();
    mqttsn_client_search_gateway(p_client);

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timeout_val);
    nrf_malloc_IgnoreAndReturn(NULL);
    mqttsn_client_timer_timeout_handle(p_client);
}

void test_mqttsn_client_search_gateway_ShallReturnErrorWhenTransportFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    emulate_init_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    
    uint32_t possible_transport_error[] = {NRF_ERROR_INTERNAL,
                                           NRF_ERROR_NO_MEM};

    mqttsn_platform_rand_IgnoreAndReturn(m_timeout_val);
    emulate_timer_start();
    mqttsn_client_search_gateway(p_client);

    for ( int i = 0; i < sizeof (possible_transport_error) / sizeof (uint32_t); i++ )
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timeout_val);
        nrf_malloc_IgnoreAndReturn(p_buf);
        mqttsn_transport_write_IgnoreAndReturn(possible_transport_error[i]);
        nrf_free_Ignore();
        mqttsn_client_timer_timeout_handle(p_client);
    }
}

void test_mqttsn_client_search_gateway_CheckPacketParsing(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    emulate_init_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf);

    m_expected_write_stub.p_client = p_client;
    m_expected_write_stub.p_data   = m_test_searchgw_packet;
    m_expected_write_stub.datalen  = sizeof(m_test_searchgw_packet);
    mqttsn_transport_write_StubWithCallback(stub_transport_write);
    m_expected_write_counter++;

    nrf_free_Ignore();

    return_value = mqttsn_packet_sender_searchgw(p_client);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

/***************************************************************************************************
 * @section CONNECT
 **************************************************************************************************/

void test_mqttsn_client_connect_ShallReturnErrorWhenNullParameter(void)
{
    uint32_t             return_value  = NRF_SUCCESS;

    mqttsn_client_t    * p_client_1    = NULL;
    mqttsn_remote_t    * p_remote_1    = &m_remote;
    uint8_t              gateway_id_1  = m_gateway_id;
    mqttsn_connect_opt_t connect_opt_1 = 
    {
        .alive_duration = m_duration,
        .clean_session  = MQTTSN_DEFAULT_CLEAN_SESSION_FLAG,
        .will_flag      = CONNECT_WILL_DISABLED,
        .client_id_len  = strlen(m_client_id),
    };
    memcpy(connect_opt_1.p_client_id, (unsigned char *)m_client_id, connect_opt_1.client_id_len);

    mqttsn_client_t    * p_client_2    = &m_client;
    mqttsn_remote_t    * p_remote_2    = NULL;
    uint8_t              gateway_id_2  = m_gateway_id;
    mqttsn_connect_opt_t connect_opt_2 = 
    {
        .alive_duration = m_duration,
        .clean_session  = MQTTSN_DEFAULT_CLEAN_SESSION_FLAG,
        .will_flag      = CONNECT_WILL_DISABLED,
        .client_id_len  = strlen(m_client_id),
    };
    memcpy(connect_opt_2.p_client_id, (unsigned char *)m_client_id, connect_opt_2.client_id_len);

    mqttsn_client_t    * p_client_3    = &m_client;
    mqttsn_remote_t    * p_remote_3    = &m_remote;
    uint8_t              gateway_id_3  = 0;
    mqttsn_connect_opt_t connect_opt_3 = 
    {
        .alive_duration = m_duration,
        .clean_session  = MQTTSN_DEFAULT_CLEAN_SESSION_FLAG,
        .will_flag      = CONNECT_WILL_DISABLED,
        .client_id_len  = strlen(m_client_id),
    };
    memcpy(connect_opt_3.p_client_id, (unsigned char *)m_client_id, connect_opt_3.client_id_len);

    mqttsn_client_t    * p_client_4    = &m_client;
    mqttsn_remote_t    * p_remote_4    = &m_remote;
    uint8_t              gateway_id_4  = m_gateway_id;
    mqttsn_connect_opt_t connect_opt_4 = 
    {
        .alive_duration = 0,
        .clean_session  = MQTTSN_DEFAULT_CLEAN_SESSION_FLAG,
        .will_flag      = CONNECT_WILL_DISABLED,
        .client_id_len  = strlen(m_client_id),
    };
    memcpy(connect_opt_4.p_client_id, (unsigned char *)m_client_id, connect_opt_4.client_id_len);

    mqttsn_client_t    * p_client_5    = &m_client;
    mqttsn_remote_t    * p_remote_5    = &m_remote;
    uint8_t              gateway_id_5  = m_gateway_id;
    mqttsn_connect_opt_t connect_opt_5 = 
    {
        .alive_duration = m_duration,
        .clean_session  = MQTTSN_DEFAULT_CLEAN_SESSION_FLAG,
        .will_flag      = CONNECT_WILL_DISABLED,
        .client_id_len  = 0,
    };
    memcpy(connect_opt_5.p_client_id, (unsigned char *)m_client_id, connect_opt_5.client_id_len);

    mqttsn_client_t    * p_client_6    = &m_client;
    mqttsn_remote_t    * p_remote_6    = &m_remote;
    uint8_t              gateway_id_6  = m_gateway_id;
    mqttsn_connect_opt_t connect_opt_6 = 
    {
        .alive_duration = m_duration,
        .clean_session  = MQTTSN_DEFAULT_CLEAN_SESSION_FLAG,
        .will_flag      = CONNECT_WILL_ENABLED,
        .will_topic_len = 0,
        .will_msg_len   = sizeof(m_will_msg),
        .client_id_len  = strlen(m_client_id),
    };
    memcpy(connect_opt_6.p_will_topic, (unsigned char *)m_will_topic, connect_opt_6.will_topic_len);
    memcpy(connect_opt_6.p_will_msg,    &m_will_msg,                  connect_opt_6.will_msg_len);
    memcpy(connect_opt_6.p_client_id,  (unsigned char *)m_client_id,  connect_opt_6.client_id_len);

    mqttsn_client_t    * p_client_7    = &m_client;
    mqttsn_remote_t    * p_remote_7    = &m_remote;
    uint8_t              gateway_id_7  = m_gateway_id;
    mqttsn_connect_opt_t connect_opt_7 = 
    {
        .alive_duration = m_duration,
        .clean_session  = MQTTSN_DEFAULT_CLEAN_SESSION_FLAG,
        .will_flag      = CONNECT_WILL_ENABLED,
        .will_topic_len = strlen(m_will_topic),
        .will_msg_len   = 0,
        .client_id_len  = strlen(m_client_id),
    };
    memcpy(connect_opt_7.p_will_topic, (unsigned char *)m_will_topic, connect_opt_7.will_topic_len);
    memcpy(connect_opt_7.p_will_msg,    &m_will_msg,                  connect_opt_7.will_msg_len);
    memcpy(connect_opt_7.p_client_id,  (unsigned char *)m_client_id,  connect_opt_7.client_id_len);


    return_value = mqttsn_client_connect(p_client_1, p_remote_1, gateway_id_1, &connect_opt_1);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_connect(p_client_2, p_remote_2, gateway_id_2, &connect_opt_2);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_connect(p_client_3, p_remote_3, gateway_id_3, &connect_opt_3);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_connect(p_client_4, p_remote_4, gateway_id_4, &connect_opt_4);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_connect(p_client_5, p_remote_5, gateway_id_5, &connect_opt_5);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_connect(p_client_6, p_remote_6, gateway_id_6, &connect_opt_6);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_connect(p_client_7, p_remote_7, gateway_id_7, &connect_opt_7);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);
}

void test_mqttsn_client_connect_ShallReturnErrorWhenClientNotInitialized(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    mqttsn_remote_t * p_remote     = &m_remote;
    uint8_t           gateway_id   = m_gateway_id;

    return_value = mqttsn_client_connect(p_client, p_remote, gateway_id, &m_connect_opt);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_connect_ShallReturnErrorWhenClientNotEligibleForConnection(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    emulate_connected_state(p_client);

    mqttsn_remote_t * p_remote     = &m_remote;
    uint8_t           gateway_id   = m_gateway_id;
    
    return_value = mqttsn_client_connect(p_client, p_remote, gateway_id, &m_connect_opt);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INVALID_STATE, return_value);
}

void test_mqttsn_client_connect_ShallReturnErrorWhenMessageBufferAllocFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    emulate_gateway_found_state(p_client);

    mqttsn_remote_t * p_remote     = &m_remote;
    uint8_t           gateway_id   = m_gateway_id;

    nrf_malloc_IgnoreAndReturn(NULL);
    nrf_malloc_IgnoreAndReturn(NULL);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);

    return_value = mqttsn_client_connect(p_client, p_remote, gateway_id, &m_connect_opt);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_connect_ShallReturnErrorWhenMessageCopyBufferAllocFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    emulate_gateway_found_state(p_client);

    mqttsn_remote_t * p_remote     = &m_remote;
    uint8_t           gateway_id   = m_gateway_id;

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(NULL);  // message copy for retransmission
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();

    return_value = mqttsn_client_connect(p_client, p_remote, gateway_id, &m_connect_opt);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_connect_ShallReturnErrorWhenTransportFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;

    mqttsn_remote_t * p_remote     = &m_remote;
    uint8_t           gateway_id   = m_gateway_id;

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    
    uint32_t possible_transport_error[] = {NRF_ERROR_INTERNAL,
                                           NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_transport_error) / sizeof (uint32_t); i++ )
    {
        emulate_gateway_found_state(p_client);
        nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
        nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
        emulate_timer_start();
        mqttsn_transport_write_IgnoreAndReturn(possible_transport_error[i]);
        nrf_free_Ignore();

        return_value = mqttsn_client_connect(p_client, p_remote, gateway_id, &m_connect_opt);
        TEST_ASSERT_EQUAL_UINT32(possible_transport_error[i], return_value);
    }
}

void test_mqttsn_client_connect_CheckPacketParsing(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    emulate_gateway_found_state(p_client);

    mqttsn_remote_t * p_remote     = &m_remote;
    uint8_t           gateway_id   = m_gateway_id;

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    emulate_timer_start();

    m_expected_write_stub.p_client = p_client;
    m_expected_write_stub.p_data   = m_test_connect_packet;
    m_expected_write_stub.datalen  = sizeof(m_test_connect_packet);
    mqttsn_transport_write_StubWithCallback(stub_transport_write);
    m_expected_write_counter++;

    nrf_free_Ignore();

    return_value = mqttsn_client_connect(p_client, p_remote, gateway_id, &m_connect_opt);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_connect_ShallReturnErrorWhenTimerStartFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    mqttsn_remote_t * p_remote     = &m_remote;
    uint8_t           gateway_id   = m_gateway_id;

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    uint32_t possible_platform_error[] = {NRF_ERROR_INVALID_STATE,
                                          NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_platform_error) / sizeof (uint32_t); i++ )
    {
        emulate_gateway_found_state(p_client);
        nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
        nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
        nrf_free_Ignore();

        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
        mqttsn_platform_timer_start_IgnoreAndReturn(possible_platform_error[i]);

        return_value = mqttsn_client_connect(p_client, p_remote, gateway_id, &m_connect_opt);
        TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
    }
}

void test_mqttsn_client_connect_ShallRetransmitIfTimerTimeouts(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    mqttsn_remote_t * p_remote     = &m_remote;
    uint8_t           gateway_id   = m_gateway_id;

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    emulate_gateway_found_state(p_client);
    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_connect(p_client, p_remote, gateway_id, &m_connect_opt);

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + 2 * m_timeout_val);
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_timer_timeout_handle(p_client);
}

void test_mqttsn_client_connect_ShallHandleEventRetransmissionTimeout(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    mqttsn_remote_t * p_remote     = &m_remote;
    uint8_t           gateway_id   = m_gateway_id;

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    emulate_gateway_found_state(p_client);
    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_connect(p_client, p_remote, gateway_id, &m_connect_opt);

    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(p_client);
    }

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    nrf_free_Ignore();
    m_expected_evt_handler_calls = 1;
    mqttsn_client_timer_timeout_handle(p_client);
}

void test_mqttsn_client_connect_ShallIgnoreRetransmissionAfterLimitWasReached(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    mqttsn_remote_t * p_remote     = &m_remote;
    uint8_t           gateway_id   = m_gateway_id;

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    emulate_gateway_found_state(p_client);
    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_connect(p_client, p_remote, gateway_id, &m_connect_opt);

    // Run retransmission
    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(p_client);
    }

    // Event: retransmission limit reached. Packet destroyed
    m_expected_evt_handler_calls = 1;
    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    nrf_free_Ignore();
    mqttsn_client_timer_timeout_handle(p_client);

    // Ignored retransmission try
    mqttsn_client_timer_timeout_handle(p_client);
}

/***************************************************************************************************
 * @section WILLTOPIC
 **************************************************************************************************/

void test_mqttsn_client_willtopic_ShallReturnErrorWhenMessageBufferAllocFails(void)
{
    uint32_t return_value = NRF_SUCCESS;
    uint8_t  p_buf[NRF_MALLOC_MEMORY_BLOCK];

    emulate_gateway_found_state(&m_client);
    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_connect(&m_client, &m_remote, m_gateway_id, &m_connect_opt);

    const uint8_t * p_data  = m_test_willtopicreq_packet;
    uint16_t        datalen = sizeof(m_test_willtopicreq_packet);

    nrf_malloc_IgnoreAndReturn(NULL);

    return_value = mqttsn_packet_receiver(&m_client, &m_port, &m_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_willtopic_ShallReturnErrorWhenTransportFails(void)
{
    uint32_t return_value = NRF_SUCCESS;
    uint8_t  p_buf[NRF_MALLOC_MEMORY_BLOCK];

    emulate_gateway_found_state(&m_client);
    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_connect(&m_client, &m_remote, m_gateway_id, &m_connect_opt);

    const uint8_t * p_data  = m_test_willtopicreq_packet;
    uint16_t        datalen = sizeof(m_test_willtopicreq_packet);

    uint32_t possible_transport_error[] = {NRF_ERROR_INTERNAL,
                                           NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_transport_error) / sizeof (uint32_t); i++ )
    {
        nrf_malloc_IgnoreAndReturn(p_buf);
        mqttsn_transport_write_IgnoreAndReturn(possible_transport_error[i]);
        nrf_free_Ignore();

        return_value = mqttsn_packet_receiver(&m_client, &m_port, &m_remote, p_data, datalen);
        TEST_ASSERT_EQUAL_UINT32(possible_transport_error[i], return_value);
    }
}

void test_mqttsn_client_willtopic_CheckPacketParsing(void)
{
    uint32_t return_value = NRF_SUCCESS;
    uint8_t  p_buf[NRF_MALLOC_MEMORY_BLOCK];

    emulate_gateway_found_state(&m_client);
    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_connect(&m_client, &m_remote, m_gateway_id, &m_connect_opt);

    nrf_malloc_IgnoreAndReturn(p_buf);
    m_expected_write_stub.p_client = &m_client;
    m_expected_write_stub.p_data   = m_test_willtopicreq_packet;
    m_expected_write_stub.datalen  = sizeof(m_test_willtopicreq_packet);
    mqttsn_transport_write_StubWithCallback(stub_transport_write);
    m_expected_write_counter++;

    const uint8_t * p_data  = m_test_willtopicreq_packet;
    uint16_t        datalen = sizeof(m_test_willtopicreq_packet);
    return_value = mqttsn_packet_receiver(&m_client, &m_port, &m_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

/***************************************************************************************************
 * @section WILLMSG
 **************************************************************************************************/

void test_mqttsn_client_willmsg_ShallReturnErrorWhenMessageBufferAllocFails(void)
{
    uint32_t return_value = NRF_SUCCESS;
    uint8_t  p_buf[NRF_MALLOC_MEMORY_BLOCK];

    emulate_gateway_found_state(&m_client);
    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_connect(&m_client, &m_remote, m_gateway_id, &m_connect_opt);

    const uint8_t * p_data  = m_test_willmsgreq_packet;
    uint16_t        datalen = sizeof(m_test_willmsgreq_packet);

    nrf_malloc_IgnoreAndReturn(NULL);

    return_value = mqttsn_packet_receiver(&m_client, &m_port, &m_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_willmsg_ShallReturnErrorWhenTransportFails(void)
{
    uint32_t return_value = NRF_SUCCESS;
    uint8_t  p_buf[NRF_MALLOC_MEMORY_BLOCK];

    emulate_gateway_found_state(&m_client);
    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_connect(&m_client, &m_remote, m_gateway_id, &m_connect_opt);

    const uint8_t * p_data  = m_test_willmsgreq_packet;
    uint16_t        datalen = sizeof(m_test_willmsgreq_packet);

    uint32_t possible_transport_error[] = {NRF_ERROR_INTERNAL,
                                           NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_transport_error) / sizeof (uint32_t); i++ )
    {
        nrf_malloc_IgnoreAndReturn(p_buf);
        mqttsn_transport_write_IgnoreAndReturn(possible_transport_error[i]);
        nrf_free_Ignore();

        return_value = mqttsn_packet_receiver(&m_client, &m_port, &m_remote, p_data, datalen);
        TEST_ASSERT_EQUAL_UINT32(possible_transport_error[i], return_value);
    }
}

void test_mqttsn_client_willmsg_CheckPacketParsing(void)
{
    uint32_t return_value = NRF_SUCCESS;
    uint8_t  p_buf[NRF_MALLOC_MEMORY_BLOCK];

    emulate_gateway_found_state(&m_client);
    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_connect(&m_client, &m_remote, m_gateway_id, &m_connect_opt);

    nrf_malloc_IgnoreAndReturn(p_buf);
    m_expected_write_stub.p_client = &m_client;
    m_expected_write_stub.p_data   = m_test_willtopicreq_packet;
    m_expected_write_stub.datalen  = sizeof(m_test_willtopicreq_packet);
    mqttsn_transport_write_StubWithCallback(stub_transport_write);
    m_expected_write_counter++;

    const uint8_t * p_data  = m_test_willmsgreq_packet;
    uint16_t        datalen = sizeof(m_test_willmsgreq_packet);
    return_value = mqttsn_packet_receiver(&m_client, &m_port, &m_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

/***************************************************************************************************
 * @section WILLTOPICUPD
 **************************************************************************************************/

void test_mqttsn_client_willtopicupd_ShallReturnErrorWhenNullParameter(void)
{
    uint32_t                return_value     = NRF_SUCCESS;

    mqttsn_client_t       * p_client_1       = NULL;
    const uint8_t         * p_topic_name_1   = m_will_topic;
    uint16_t                topic_name_len_1 = sizeof(m_will_topic);

    mqttsn_client_t       * p_client_2       = &m_client;
    uint8_t               * p_topic_name_2   = NULL;
    uint16_t                topic_name_len_2 = sizeof(m_will_topic);

    mqttsn_client_t       * p_client_3       = &m_client;
    const uint8_t         * p_topic_name_3   = m_will_topic;
    uint16_t                topic_name_len_3 = 0;

    return_value = mqttsn_client_willtopicupd(p_client_1, p_topic_name_1, topic_name_len_1);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_willtopicupd(p_client_2, p_topic_name_2, topic_name_len_2);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_willtopicupd(p_client_3, p_topic_name_3, topic_name_len_3);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);
}


void test_mqttsn_client_willtopicupd_ShallReturnErrorWhenClientNotInitialized(void)
{
    uint32_t return_value = NRF_SUCCESS;

    return_value = mqttsn_client_willtopicupd(&m_client, m_will_topic, sizeof(m_will_topic));
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_willtopicupd_ShallReturnErrorWhenClientNotConnected(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_init_state(&m_client);
    return_value = mqttsn_client_willtopicupd(&m_client, m_will_topic, sizeof(m_will_topic));
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_willtopicupd_ShallReturnErrorWhenMessageBufferAllocFails(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(NULL);
    nrf_malloc_IgnoreAndReturn(p_buf);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();
    return_value = mqttsn_client_willtopicupd(&m_client, m_will_topic, sizeof(m_will_topic));
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}


void test_mqttsn_client_willtopicupd_ShallReturnErrorWhenMessageCopyBufferAllocFails(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf);
    nrf_malloc_IgnoreAndReturn(NULL);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();
    return_value = mqttsn_client_willtopicupd(&m_client, m_will_topic, sizeof(m_will_topic));
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_willtopicupd_ShallReturnErrorWhenTransportFails(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    uint32_t possible_transport_error[] = {NRF_ERROR_INTERNAL,
                                           NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_transport_error) / sizeof (uint32_t); i++ )
    {
        nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
        nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
        emulate_timer_start();
        mqttsn_transport_write_IgnoreAndReturn(possible_transport_error[i]);
        nrf_free_Ignore();

        return_value = mqttsn_client_willtopicupd(&m_client, m_will_topic, sizeof(m_will_topic));
        TEST_ASSERT_EQUAL_UINT32(possible_transport_error[i], return_value);
    }
}

void test_mqttsn_client_willtopicupd_ShallReturnErrorWhenPacketQueueOverflows(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    for (int i = 0; i < MQTTSN_PACKET_FIFO_MAX_LENGTH; i++)
    {
        nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
        nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
        emulate_timer_start();
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        nrf_free_Ignore();
        mqttsn_client_willtopicupd(&m_client, m_will_topic, sizeof(m_will_topic));
    }

    nrf_malloc_IgnoreAndReturn(p_buf);
    nrf_malloc_IgnoreAndReturn(p_buf);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();

    return_value = mqttsn_client_willtopicupd(&m_client, m_will_topic, sizeof(m_will_topic));

    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_willtopicupd_CheckPacketParsing(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    emulate_timer_start();
    nrf_free_Ignore();

    m_expected_write_stub.p_client = &m_client;
    m_expected_write_stub.p_data   = m_test_willtopicupd_packet;
    m_expected_write_stub.datalen  = sizeof(m_test_willtopicupd_packet);
    mqttsn_transport_write_StubWithCallback(stub_transport_write);
    m_expected_write_counter++;

    return_value = mqttsn_client_willtopicupd(&m_client, m_will_topic, sizeof(m_will_topic));
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_willtopicupd_ShallReturnErrorWhenTimerStartFails(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    uint32_t possible_platform_error[] = {NRF_ERROR_INVALID_STATE,
                                          NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_platform_error) / sizeof (uint32_t); i++ )
    {
        emulate_connected_state(&m_client);
        nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
        nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
        nrf_free_Ignore();

        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
        mqttsn_platform_timer_start_IgnoreAndReturn(possible_platform_error[i]);

        return_value = mqttsn_client_willtopicupd(&m_client, m_will_topic, sizeof(m_will_topic));
        TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
    }
}

void test_mqttsn_client_willtopicupd_ShallRetransmitIfTimerTimeouts(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    return_value = mqttsn_client_willtopicupd(&m_client, m_will_topic, sizeof(m_will_topic));

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + 2 * m_timeout_val);
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_timer_timeout_handle(&m_client);
}

void test_mqttsn_client_willtopicupd_ShallHandleEventRetransmissionTimeout(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_willtopicupd(&m_client, m_will_topic, sizeof(m_will_topic));

    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(&m_client);
    }

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    nrf_free_Ignore();
    m_expected_evt_handler_calls = 1;
    mqttsn_client_timer_timeout_handle(&m_client);
}

void test_mqttsn_client_willtopicupd_ShallIgnoreRetransmissionAfterLimitWasReached(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_willtopicupd(&m_client, m_will_topic, sizeof(m_will_topic));

    // Run retransmission
    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(&m_client);
    }

    // Event: retransmission limit reached. Packet destroyed
    m_expected_evt_handler_calls = 1;
    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    nrf_free_Ignore();
    mqttsn_client_timer_timeout_handle(&m_client);

    // Ignored retransmission try
    mqttsn_client_timer_timeout_handle(&m_client);
}


/***************************************************************************************************
 * @section WILLMSGUPD
 **************************************************************************************************/

void test_mqttsn_client_willmsgupd_ShallReturnErrorWhenNullParameter(void)
{
    uint32_t          return_value   = NRF_SUCCESS;

    mqttsn_client_t * p_client_1     = NULL;
    const uint8_t   * p_msg_name_1   = m_will_msg;
    uint16_t          msg_name_len_1 = sizeof(m_will_msg);

    mqttsn_client_t * p_client_2     = &m_client;
    const uint8_t   * p_msg_name_2   = NULL;
    uint16_t          msg_name_len_2 = sizeof(m_will_msg);

    mqttsn_client_t * p_client_3     = &m_client;
    const uint8_t   * p_msg_name_3   = m_will_msg;
    uint16_t          msg_name_len_3 = 0;

    return_value = mqttsn_client_willtopicupd(p_client_1, p_msg_name_1, msg_name_len_1);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_willtopicupd(p_client_2, p_msg_name_2, msg_name_len_2);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_willtopicupd(p_client_3, p_msg_name_3, msg_name_len_3);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);
}

void test_mqttsn_client_willmsgupd_ShallReturnErrorWhenClientNotInitialized(void)
{
    uint32_t return_value = NRF_SUCCESS;

    return_value = mqttsn_client_willmsgupd(&m_client, m_will_msg, sizeof(m_will_msg));
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_willmsgupd_ShallReturnErrorWhenClientNotConnected(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_init_state(&m_client);
    return_value = mqttsn_client_willmsgupd(&m_client, m_will_msg, sizeof(m_will_msg));
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_willmsgupd_ShallReturnErrorWhenMessageBufferAllocFails(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    nrf_malloc_IgnoreAndReturn(NULL);
    nrf_malloc_IgnoreAndReturn(NULL);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    return_value = mqttsn_client_willmsgupd(&m_client, m_will_msg, sizeof(m_will_msg));
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_willmsgupd_ShallReturnErrorWhenMessageCopyBufferAllocFails(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf);
    nrf_malloc_IgnoreAndReturn(NULL);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();
    return_value = mqttsn_client_willmsgupd(&m_client, m_will_msg, sizeof(m_will_msg));
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_willmsgupd_ShallReturnErrorWhenTransportFails(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    uint32_t possible_transport_error[] = {NRF_ERROR_INTERNAL,
                                           NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_transport_error) / sizeof (uint32_t); i++ )
    {
        nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
        nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
        emulate_timer_start();
        mqttsn_transport_write_IgnoreAndReturn(possible_transport_error[i]);
        nrf_free_Ignore();

        return_value = mqttsn_client_willmsgupd(&m_client, m_will_msg, sizeof(m_will_msg));
        TEST_ASSERT_EQUAL_UINT32(possible_transport_error[i], return_value);
    }
}

void test_mqttsn_client_willmsgupd_ShallReturnErrorWhenPacketQueueOverflows(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    for (int i = 0; i < MQTTSN_PACKET_FIFO_MAX_LENGTH; i++)
    {
        nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
        nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
        emulate_timer_start();
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        nrf_free_Ignore();
        mqttsn_client_willmsgupd(&m_client, m_will_msg, sizeof(m_will_msg));
    }

    nrf_malloc_IgnoreAndReturn(p_buf);
    nrf_malloc_IgnoreAndReturn(p_buf);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();

    return_value = mqttsn_client_willmsgupd(&m_client, m_will_msg, sizeof(m_will_msg));

    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_willmsgupd_CheckPacketParsing(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    emulate_timer_start();
    nrf_free_Ignore();

    m_expected_write_stub.p_client = &m_client;
    m_expected_write_stub.p_data   = m_test_willmsgupd_packet;
    m_expected_write_stub.datalen  = sizeof(m_test_willmsgupd_packet);
    mqttsn_transport_write_StubWithCallback(stub_transport_write);
    m_expected_write_counter++;

    return_value = mqttsn_client_willmsgupd(&m_client, m_will_msg, sizeof(m_will_msg));
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_willmsgupd_ShallReturnErrorWhenTimerStartFails(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    uint32_t possible_platform_error[] = {NRF_ERROR_INVALID_STATE,
                                          NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_platform_error) / sizeof (uint32_t); i++ )
    {
        emulate_connected_state(&m_client);
        nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
        nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
        nrf_free_Ignore();

        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
        mqttsn_platform_timer_start_IgnoreAndReturn(possible_platform_error[i]);

        return_value = mqttsn_client_willmsgupd(&m_client, m_will_msg, sizeof(m_will_msg));
        TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
    }
}

void test_mqttsn_client_willmsgupd_ShallRetransmitIfTimerTimeouts(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    return_value = mqttsn_client_willmsgupd(&m_client, m_will_msg, sizeof(m_will_msg));

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + 2 * m_timeout_val);
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_timer_timeout_handle(&m_client);
}

void test_mqttsn_client_willmsgupd_ShallHandleEventRetransmissionTimeout(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_willmsgupd(&m_client, m_will_msg, sizeof(m_will_msg));

    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(&m_client);
    }

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    nrf_free_Ignore();
    m_expected_evt_handler_calls = 1;
    mqttsn_client_timer_timeout_handle(&m_client);
}

void test_mqttsn_client_willmsgupd_ShallIgnoreRetransmissionAfterLimitWasReached(void)
{
    uint32_t return_value = NRF_SUCCESS;

    emulate_connected_state(&m_client);
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_willmsgupd(&m_client, m_will_msg, sizeof(m_will_msg));

    // Run retransmission
    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(&m_client);
    }

    // Event: retransmission limit reached. Packet destroyed
    m_expected_evt_handler_calls = 1;
    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    nrf_free_Ignore();
    mqttsn_client_timer_timeout_handle(&m_client);

    // Ignored retransmission try
    mqttsn_client_timer_timeout_handle(&m_client);
}

/***************************************************************************************************
 * @section DISCONNECT
 **************************************************************************************************/

void test_mqttsn_client_disconnect_ShallReturnErrorWhenNullClient(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = NULL;

    return_value = mqttsn_client_disconnect(p_client);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);
}

void test_mqttsn_client_disconnect_ShallReturnErrorWhenClientNotInitialized(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;

    return_value = mqttsn_client_disconnect(p_client);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_disconnect_ShallReturnErrorWhenClientNotConnected(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    emulate_init_state(p_client);

    return_value = mqttsn_client_disconnect(p_client);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_disconnect_ShallReturnErrorWhenBufferAllocFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    emulate_connected_state(p_client);

    nrf_malloc_IgnoreAndReturn(NULL);

    return_value = mqttsn_client_disconnect(p_client);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_disconnect_ShallReturnErrorWhenTransportFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    
    uint32_t possible_transport_error[] = {NRF_ERROR_INTERNAL,
                                           NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_transport_error) / sizeof (uint32_t); i++ )
    {
        nrf_malloc_IgnoreAndReturn(p_buf);
        mqttsn_transport_write_IgnoreAndReturn(possible_transport_error[i]);
        nrf_free_Ignore();

        return_value = mqttsn_client_disconnect(p_client);
        TEST_ASSERT_EQUAL_UINT32(possible_transport_error[i], return_value);
    }
}

void test_mqttsn_client_disconnect_CheckPacketParsing(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf);

    m_expected_write_stub.p_client = p_client;
    m_expected_write_stub.p_data   = m_test_disconnect_packet;
    m_expected_write_stub.datalen  = sizeof(m_test_disconnect_packet);
    mqttsn_transport_write_StubWithCallback(stub_transport_write);
    m_expected_write_counter++;

    nrf_free_Ignore();

    return_value = mqttsn_client_disconnect(p_client);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

/***************************************************************************************************
 * @section SLEEP
 **************************************************************************************************/

void test_mqttsn_client_sleep_ShallReturnErrorWhenNullParameter(void)
{
    uint32_t          return_value = NRF_SUCCESS;

    mqttsn_client_t * p_client     = NULL;
    uint16_t          polling_time = m_polling_time;

    return_value = mqttsn_client_sleep(p_client, polling_time);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);
}

void test_mqttsn_client_sleep_ShallReturnErrorWhenClientNotInitialized(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          polling_time = m_polling_time;

    return_value = mqttsn_client_sleep(p_client, polling_time);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_sleep_ShallReturnErrorWhenClientNotConnected(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          polling_time = m_polling_time;
    emulate_init_state(p_client);

    return_value = mqttsn_client_sleep(p_client, polling_time);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_sleep_ShallReturnErrorWhenBufferAllocFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          polling_time = m_polling_time;
    emulate_connected_state(p_client);

    nrf_malloc_IgnoreAndReturn(NULL);

    return_value = mqttsn_client_sleep(p_client, polling_time);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_sleep_ShallReturnErrorWhenTransportFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          polling_time = m_polling_time;
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    
    uint32_t possible_transport_error[] = {NRF_ERROR_INTERNAL,
                                           NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_transport_error) / sizeof (uint32_t); i++ )
    {
        nrf_malloc_IgnoreAndReturn(p_buf);
        mqttsn_transport_write_IgnoreAndReturn(possible_transport_error[i]);
        nrf_free_Ignore();

        return_value = mqttsn_client_sleep(p_client, polling_time);
        TEST_ASSERT_EQUAL_UINT32(possible_transport_error[i], return_value);
    }
}

void test_mqttsn_client_sleep_CheckPacketParsing(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          polling_time = m_polling_time;
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf);

    m_expected_write_stub.p_client = p_client;
    m_expected_write_stub.p_data   = m_test_sleep_packet;
    m_expected_write_stub.datalen  = sizeof(m_test_sleep_packet);
    mqttsn_transport_write_StubWithCallback(stub_transport_write);
    m_expected_write_counter++;

    nrf_free_Ignore();

    return_value = mqttsn_client_sleep(p_client, polling_time);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

/***************************************************************************************************
 * @section REGISTER
 **************************************************************************************************/

void test_mqttsn_client_topic_register_ShallReturnErrorWhenNullParameter(void)
{
    uint32_t          return_value     = NRF_SUCCESS;
    uint16_t          msg_id           = 0;

    mqttsn_client_t * p_client_1       = NULL;
    const uint8_t   * p_topic_name_1   = m_topic_name;
    uint16_t          topic_name_len_1 = sizeof(m_topic_name);

    mqttsn_client_t * p_client_2       = &m_client;
    const uint8_t   * p_topic_name_2   = NULL;
    uint16_t          topic_name_len_2 = sizeof(m_topic_name);

    mqttsn_client_t * p_client_3       = &m_client;
    const uint8_t   * p_topic_name_3   = m_topic_name;
    uint16_t          topic_name_len_3 = 0;

    return_value = mqttsn_client_topic_register(p_client_1, p_topic_name_1, topic_name_len_1, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_topic_register(p_client_2, p_topic_name_2, topic_name_len_2, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_topic_register(p_client_3, p_topic_name_3, topic_name_len_3, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);
}

void test_mqttsn_client_topic_register_ShallReturnErrorWhenClientNotInitialized(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);

    return_value = mqttsn_client_topic_register(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_topic_register_ShallReturnErrorWhenClientNotConnected(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_init_state(p_client);

    return_value = mqttsn_client_topic_register(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_topic_register_ShallReturnErrorWhenBufferAllocFails(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(NULL);
    nrf_malloc_IgnoreAndReturn(p_buf);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();

    return_value = mqttsn_client_topic_register(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_topic_register_ShallReturnErrorWhenTransportFails(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);
   
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    
    uint32_t possible_transport_error[] = {NRF_ERROR_INTERNAL,
                                           NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_transport_error) / sizeof (uint32_t); i++ )
    {
        nrf_malloc_IgnoreAndReturn(p_buf);
        nrf_malloc_IgnoreAndReturn(p_buf);
        emulate_timer_start();
        mqttsn_transport_write_IgnoreAndReturn(possible_transport_error[i]);
        nrf_free_Ignore();

        return_value = mqttsn_client_topic_register(p_client, p_topic_name, topic_name_len, &msg_id);
        TEST_ASSERT_EQUAL_UINT32(possible_transport_error[i], return_value);
    }
}

void test_mqttsn_client_topic_register_ShallReturnErrorWhenPacketQueueOverflows(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    for (int i = 0; i < MQTTSN_PACKET_FIFO_MAX_LENGTH; i++)
    {
        nrf_malloc_IgnoreAndReturn(p_buf);
        nrf_malloc_IgnoreAndReturn(p_buf);
        emulate_timer_start();
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        nrf_free_Ignore();
        mqttsn_client_topic_register(p_client, p_topic_name, topic_name_len, &msg_id);
    }

    nrf_malloc_IgnoreAndReturn(p_buf);
    nrf_malloc_IgnoreAndReturn(p_buf);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();

    return_value = mqttsn_client_topic_register(p_client, p_topic_name, topic_name_len, &msg_id);

    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_topic_register_CheckPacketParsing(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf);
    nrf_malloc_IgnoreAndReturn(p_buf);
    emulate_timer_start();

    m_expected_write_stub.p_client = p_client;
    m_expected_write_stub.p_data   = m_test_register_packet;
    m_expected_write_stub.datalen  = sizeof(m_test_register_packet);
    mqttsn_transport_write_StubWithCallback(stub_transport_write);
    m_expected_write_counter++;

    nrf_free_Ignore();

    return_value = mqttsn_client_topic_register(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_topic_register_ShallReturnErrorWhenTimerStartFails(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    uint32_t possible_platform_error[] = {NRF_ERROR_INVALID_STATE,
                                          NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_platform_error) / sizeof (uint32_t); i++ )
    {
        emulate_connected_state(p_client);
        nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
        nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
        nrf_free_Ignore();

        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
        mqttsn_platform_timer_start_IgnoreAndReturn(possible_platform_error[i]);

        return_value = mqttsn_client_topic_register(p_client, p_topic_name, topic_name_len, &msg_id);
        TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
    }
}

void test_mqttsn_client_topic_register_ShallRetransmitIfTimerTimeouts(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    return_value = mqttsn_client_topic_register(p_client, p_topic_name, topic_name_len, &msg_id);

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + 2 * m_timeout_val);
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_timer_timeout_handle(p_client);
}

void test_mqttsn_client_topic_register_ShallHandleEventRetransmissionTimeout(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_topic_register(p_client, p_topic_name, topic_name_len, &msg_id);

    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(p_client);
    }

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    nrf_free_Ignore();
    m_expected_evt_handler_calls = 1;
    mqttsn_client_timer_timeout_handle(p_client);
}

void test_mqttsn_client_topic_register_ShallIgnoreRetransmissionAfterLimitWasReached(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_topic_register(p_client, p_topic_name, topic_name_len, &msg_id);

    // Run retransmission
    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(p_client);
    }

    // Event: retransmission limit reached. Packet destroyed
    m_expected_evt_handler_calls = 1;
    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    nrf_free_Ignore();
    mqttsn_client_timer_timeout_handle(p_client);

    // Ignored retransmission try
    mqttsn_client_timer_timeout_handle(p_client);
}

/***************************************************************************************************
 * @section PUBLISH
 **************************************************************************************************/

void test_mqttsn_client_publish_ShallReturnErrorWhenNullParameter(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    uint16_t          msg_id       = 0;

    mqttsn_client_t * p_client_1   = NULL;
    uint16_t          topic_id_1   = m_topic_id;
    const uint8_t   * payload_1    = m_payload;
    uint16_t          payloadlen_1 = sizeof(m_payload)/sizeof(uint8_t);

    mqttsn_client_t * p_client_2   = &m_client;
    uint16_t          topic_id_2   = 0;
    const uint8_t   * payload_2    = m_payload;
    uint16_t          payloadlen_2 = sizeof(m_payload)/sizeof(uint8_t);

    mqttsn_client_t * p_client_3   = &m_client;
    uint16_t          topic_id_3   = m_topic_id;
    const uint8_t   * payload_3    = NULL;
    uint16_t          payloadlen_3 = sizeof(m_payload)/sizeof(uint8_t);

    mqttsn_client_t * p_client_4   = &m_client;
    uint16_t          topic_id_4   = m_topic_id;
    const uint8_t   * payload_4    = m_payload;
    uint16_t          payloadlen_4 = 0;

    return_value = mqttsn_client_publish(p_client_1, topic_id_1, payload_1, payloadlen_1, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_publish(p_client_2, topic_id_2, payload_2, payloadlen_2, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_publish(p_client_3, topic_id_3, payload_3, payloadlen_3, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_publish(p_client_4, topic_id_4, payload_4, payloadlen_4, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);
}

void test_mqttsn_client_publish_ShallReturnErrorWhenClientNotInitialized(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    uint16_t          msg_id       = 0;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          topic_id     = m_topic_id;
    const uint8_t   * payload      = m_payload;
    uint16_t          payloadlen   = sizeof(m_payload)/sizeof(uint8_t);

    return_value = mqttsn_client_publish(p_client, topic_id, payload, payloadlen, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_publish_ShallReturnErrorWhenClientNotConnected(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    uint16_t          msg_id       = 0;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          topic_id     = m_topic_id;
    const uint8_t   * payload      = m_payload;
    uint16_t          payloadlen   = sizeof(m_payload)/sizeof(uint8_t);
    emulate_init_state(p_client);

    return_value = mqttsn_client_publish(p_client, topic_id, payload, payloadlen, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_publish_ShallReturnErrorWhenBufferAllocFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    uint16_t          msg_id       = 0;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          topic_id     = m_topic_id;
    const uint8_t   * payload      = m_payload;
    uint16_t          payloadlen   = sizeof(m_payload)/sizeof(uint8_t);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(NULL);
    nrf_malloc_IgnoreAndReturn(p_buf);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();

    return_value = mqttsn_client_publish(p_client, topic_id, payload, payloadlen, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_publish_ShallReturnErrorWhenTransportFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    uint16_t          msg_id       = 0;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          topic_id     = m_topic_id;
    const uint8_t   * payload      = m_payload;
    uint16_t          payloadlen   = sizeof(m_payload)/sizeof(uint8_t);
    emulate_connected_state(p_client);
   
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf);
    emulate_timer_start();
    
    uint32_t possible_transport_error[] = {NRF_ERROR_INTERNAL,
                                           NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_transport_error) / sizeof (uint32_t); i++ )
    {
        nrf_malloc_IgnoreAndReturn(p_buf);
        mqttsn_transport_write_IgnoreAndReturn(possible_transport_error[i]);
        nrf_free_Ignore();

        return_value = mqttsn_client_publish(p_client, topic_id, payload, payloadlen, &msg_id);
        TEST_ASSERT_EQUAL_UINT32(possible_transport_error[i], return_value);
    }
}

void test_mqttsn_client_publish_ShallReturnErrorWhenPacketQueueOverflows(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    uint16_t          msg_id       = 0;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          topic_id     = m_topic_id;
    const uint8_t   * payload      = m_payload;
    uint16_t          payloadlen   = sizeof(m_payload);
    emulate_connected_state(p_client);
   
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    for (int i = 0; i < MQTTSN_PACKET_FIFO_MAX_LENGTH; i++)
    {
        nrf_malloc_IgnoreAndReturn(p_buf);
        nrf_malloc_IgnoreAndReturn(p_buf);
        emulate_timer_start();
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        nrf_free_Ignore();
        mqttsn_client_publish(p_client, topic_id, payload, payloadlen, &msg_id);
    }

    nrf_malloc_IgnoreAndReturn(p_buf);
    nrf_malloc_IgnoreAndReturn(p_buf);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();

    return_value = mqttsn_client_publish(p_client, topic_id, payload, payloadlen, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_publish_CheckPacketParsing(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    uint16_t          msg_id       = 0;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          topic_id     = m_topic_id;
    const uint8_t   * payload      = m_payload;
    uint16_t          payloadlen   = sizeof(m_payload);
    emulate_connected_state(p_client);

    uint16_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf);
    nrf_malloc_IgnoreAndReturn(p_buf);
    emulate_timer_start();

    m_expected_write_stub.p_client = p_client;
    m_expected_write_stub.p_data   = m_test_publish_packet;
    m_expected_write_stub.datalen  = sizeof(m_test_publish_packet);
    mqttsn_transport_write_StubWithCallback(stub_transport_write);
    m_expected_write_counter++;

    nrf_free_Ignore();

    return_value = mqttsn_client_publish(p_client, topic_id, payload, payloadlen, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_publish_ShallReturnErrorWhenTimerStartFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    uint16_t          msg_id       = 0;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          topic_id     = m_topic_id;
    const uint8_t   * payload      = m_payload;
    uint16_t          payloadlen   = sizeof(m_payload);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    uint32_t possible_platform_error[] = {NRF_ERROR_INVALID_STATE,
                                          NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_platform_error) / sizeof (uint32_t); i++ )
    {
        emulate_connected_state(p_client);
        nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
        nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
        nrf_free_Ignore();

        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
        mqttsn_platform_timer_start_IgnoreAndReturn(possible_platform_error[i]);

        return_value =  mqttsn_client_publish(p_client, topic_id, payload, payloadlen, &msg_id);
        TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
    }
}

void test_mqttsn_client_publish_ShallRetransmitIfTimerTimeouts(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    uint16_t          msg_id       = 0;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          topic_id     = m_topic_id;
    const uint8_t   * payload      = m_payload;
    uint16_t          payloadlen   = sizeof(m_payload);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    return_value =  mqttsn_client_publish(p_client, topic_id, payload, payloadlen, &msg_id);

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + 2 * m_timeout_val);
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_timer_timeout_handle(p_client);
}

void test_mqttsn_client_publish_ShallHandleEventRetransmissionTimeout(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    uint16_t          msg_id       = 0;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          topic_id     = m_topic_id;
    const uint8_t   * payload      = m_payload;
    uint16_t          payloadlen   = sizeof(m_payload);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_publish(p_client, topic_id, payload, payloadlen, &msg_id);

    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(p_client);
    }

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    nrf_free_Ignore();
    m_expected_evt_handler_calls = 1;
    mqttsn_client_timer_timeout_handle(p_client);
}

void test_mqttsn_client_publish_ShallIgnoreRetransmissionAfterLimitWasReached(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    uint16_t          msg_id       = 0;
    mqttsn_client_t * p_client     = &m_client;
    uint16_t          topic_id     = m_topic_id;
    const uint8_t   * payload      = m_payload;
    uint16_t          payloadlen   = sizeof(m_payload);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_publish(p_client, topic_id, payload, payloadlen, &msg_id);

    // Run retransmission
    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(p_client);
    }

    // Event: retransmission limit reached. Packet destroyed
    m_expected_evt_handler_calls = 1;
    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    nrf_free_Ignore();
    mqttsn_client_timer_timeout_handle(p_client);

    // Ignored retransmission try
    mqttsn_client_timer_timeout_handle(p_client);
}

/***************************************************************************************************
 * @section SUBSCRIBE
 **************************************************************************************************/

void test_mqttsn_client_subscribe_ShallReturnErrorWhenNullParameter(void)
{
    uint32_t          return_value     = NRF_SUCCESS;
    uint16_t          msg_id           = 0;

    mqttsn_client_t * p_client_1       = NULL;
    const uint8_t   * p_topic_name_1   = m_topic_name;
    uint16_t          topic_name_len_1 = sizeof(m_topic_name)/sizeof(uint8_t);

    mqttsn_client_t * p_client_2       = &m_client;
    const uint8_t   * p_topic_name_2   = NULL;
    uint16_t          topic_name_len_2 = sizeof(m_topic_name)/sizeof(uint8_t);

    mqttsn_client_t * p_client_3       = &m_client;
    const uint8_t   * p_topic_name_3   = m_topic_name;
    uint16_t          topic_name_len_3 = 0;

    return_value = mqttsn_client_subscribe(p_client_1, p_topic_name_1, topic_name_len_1, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_subscribe(p_client_2, p_topic_name_2, topic_name_len_2, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_subscribe(p_client_3, p_topic_name_3, topic_name_len_3, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);   
}

void test_mqttsn_client_subscribe_ShallReturnErrorWhenClientNotInitialized(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);

    return_value = mqttsn_client_subscribe(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_subscribe_ShallReturnErrorWhenClientNotConnected(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_init_state(p_client);

    return_value = mqttsn_client_subscribe(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_subscribe_ShallReturnErrorWhenBufferAllocFails(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name)/sizeof(uint8_t);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(NULL);
    nrf_malloc_IgnoreAndReturn(p_buf);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();

    return_value = mqttsn_client_subscribe(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_subscribe_ShallReturnErrorWhenTransportFails(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name)/sizeof(uint8_t);
    emulate_connected_state(p_client);
   
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf);
    emulate_timer_start();
    
    uint32_t possible_transport_error[] = {NRF_ERROR_INTERNAL,
                                           NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_transport_error) / sizeof (uint32_t); i++ )
    {
        nrf_malloc_IgnoreAndReturn(p_buf);
        mqttsn_transport_write_IgnoreAndReturn(possible_transport_error[i]);
        nrf_free_Ignore();

        return_value = mqttsn_client_subscribe(p_client, p_topic_name, topic_name_len, &msg_id);
        TEST_ASSERT_EQUAL_UINT32(possible_transport_error[i], return_value);
    }
}

void test_mqttsn_client_subscribe_ShallReturnErrorWhenPacketQueueOverflows(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);
   
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    for (int i = 0; i < MQTTSN_PACKET_FIFO_MAX_LENGTH; i++)
    {
        nrf_malloc_IgnoreAndReturn(p_buf);
        nrf_malloc_IgnoreAndReturn(p_buf);
        emulate_timer_start();
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        nrf_free_Ignore();
        mqttsn_client_subscribe(p_client, p_topic_name, topic_name_len, &msg_id);
    }

    nrf_malloc_IgnoreAndReturn(p_buf);
    nrf_malloc_IgnoreAndReturn(p_buf);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();

    return_value = mqttsn_client_subscribe(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_subscribe_CheckPacketParsing(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf);
    nrf_malloc_IgnoreAndReturn(p_buf);
    emulate_timer_start();

    m_expected_write_stub.p_client = p_client;
    m_expected_write_stub.p_data   = m_test_subscribe_packet;
    m_expected_write_stub.datalen  = sizeof(m_test_subscribe_packet);
    mqttsn_transport_write_StubWithCallback(stub_transport_write);
    m_expected_write_counter++;

    nrf_free_Ignore();

    return_value = mqttsn_client_subscribe(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_subscribe_ShallReturnErrorWhenTimerStartFails(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    uint32_t possible_platform_error[] = {NRF_ERROR_INVALID_STATE,
                                          NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_platform_error) / sizeof (uint32_t); i++ )
    {
        emulate_connected_state(p_client);
        nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
        nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
        nrf_free_Ignore();

        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
        mqttsn_platform_timer_start_IgnoreAndReturn(possible_platform_error[i]);

        return_value =  mqttsn_client_subscribe(p_client, p_topic_name, topic_name_len, &msg_id);
        TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
    }
}

void test_mqttsn_client_subscribe_ShallRetransmitIfTimerTimeouts(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    return_value =  mqttsn_client_subscribe(p_client, p_topic_name, topic_name_len, &msg_id);

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + 2 * m_timeout_val);
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_timer_timeout_handle(p_client);
}

void test_mqttsn_client_subscribe_ShallHandleEventRetransmissionTimeout(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_subscribe(p_client, p_topic_name, topic_name_len, &msg_id);

    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(p_client);
    }

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    nrf_free_Ignore();
    m_expected_evt_handler_calls = 1;
    mqttsn_client_timer_timeout_handle(p_client);
}

void test_mqttsn_client_subscribe_ShallIgnoreRetransmissionAfterLimitWasReached(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_subscribe(p_client, p_topic_name, topic_name_len, &msg_id);

    // Run retransmission
    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(p_client);
    }

    // Event: retransmission limit reached. Packet destroyed
    m_expected_evt_handler_calls = 1;
    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    nrf_free_Ignore();
    mqttsn_client_timer_timeout_handle(p_client);

    // Ignored retransmission try
    mqttsn_client_timer_timeout_handle(p_client);
}

/***************************************************************************************************
 * @section UNSUBSCRIBE
 **************************************************************************************************/

void test_mqttsn_client_unsubscribe_ShallReturnErrorWhenNullParameter(void)
{
    uint32_t          return_value     = NRF_SUCCESS;
    uint16_t          msg_id           = 0;

    mqttsn_client_t * p_client_1       = NULL;
    const uint8_t   * p_topic_name_1   = m_topic_name;
    uint16_t          topic_name_len_1 = sizeof(m_topic_name);

    mqttsn_client_t * p_client_2       = &m_client;
    const uint8_t   * p_topic_name_2   = NULL;
    uint16_t          topic_name_len_2 = sizeof(m_topic_name);

    mqttsn_client_t * p_client_3       = &m_client;
    const uint8_t   * p_topic_name_3   = m_topic_name;
    uint16_t          topic_name_len_3 = 0;

    return_value = mqttsn_client_unsubscribe(p_client_1, p_topic_name_1, topic_name_len_1, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_unsubscribe(p_client_2, p_topic_name_2, topic_name_len_2, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);

    return_value = mqttsn_client_unsubscribe(p_client_3, p_topic_name_3, topic_name_len_3, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);   
}

void test_mqttsn_client_unsubscribe_ShallReturnErrorWhenClientNotInitialized(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id           = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);

    return_value = mqttsn_client_unsubscribe(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_unsubscribe_ShallReturnErrorWhenClientNotConnected(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_init_state(p_client);

    return_value = mqttsn_client_unsubscribe(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_unsubscribe_ShallReturnErrorWhenBufferAllocFails(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name)/sizeof(uint8_t);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(NULL);
    nrf_malloc_IgnoreAndReturn(p_buf);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();

    return_value = mqttsn_client_unsubscribe(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_unsubscribe_ShallReturnErrorWhenTransportFails(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name)/sizeof(uint8_t);
    emulate_connected_state(p_client);
   
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf);
    emulate_timer_start();
    
    uint32_t possible_transport_error[] = {NRF_ERROR_INTERNAL,
                                           NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_transport_error) / sizeof (uint32_t); i++ )
    {
        nrf_malloc_IgnoreAndReturn(p_buf);
        mqttsn_transport_write_IgnoreAndReturn(possible_transport_error[i]);
        nrf_free_Ignore();

        return_value = mqttsn_client_unsubscribe(p_client, p_topic_name, topic_name_len, &msg_id);
        TEST_ASSERT_EQUAL_UINT32(possible_transport_error[i], return_value);
    }
}

void test_mqttsn_client_unsubscribe_ShallReturnErrorWhenPacketQueueOverflows(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);
   
    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    for (int i = 0; i < MQTTSN_PACKET_FIFO_MAX_LENGTH; i++)
    {
        nrf_malloc_IgnoreAndReturn(p_buf);
        nrf_malloc_IgnoreAndReturn(p_buf);
        emulate_timer_start();
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        nrf_free_Ignore();
        mqttsn_client_unsubscribe(p_client, p_topic_name, topic_name_len, &msg_id);
    }

    nrf_malloc_IgnoreAndReturn(p_buf);
    nrf_malloc_IgnoreAndReturn(p_buf);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
    nrf_free_Ignore();

    return_value = mqttsn_client_unsubscribe(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);
}

void test_mqttsn_client_unsubscribe_CheckPacketParsing(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(p_buf);
    nrf_malloc_IgnoreAndReturn(p_buf);
    emulate_timer_start();

    m_expected_write_stub.p_client = p_client;
    m_expected_write_stub.p_data   = m_test_unsubscribe_packet;
    m_expected_write_stub.datalen  = sizeof(m_test_unsubscribe_packet);
    mqttsn_transport_write_StubWithCallback(stub_transport_write);
    m_expected_write_counter++;

    nrf_free_Ignore();

    return_value = mqttsn_client_unsubscribe(p_client, p_topic_name, topic_name_len, &msg_id);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_unsubscribe_ShallReturnErrorWhenTimerStartFails(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    uint32_t possible_platform_error[] = {NRF_ERROR_INVALID_STATE,
                                          NRF_ERROR_NO_MEM};

    for ( int i = 0; i < sizeof (possible_platform_error) / sizeof (uint32_t); i++ )
    {
        emulate_connected_state(p_client);
        nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
        nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
        nrf_free_Ignore();

        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timeout_val);
        mqttsn_platform_timer_start_IgnoreAndReturn(possible_platform_error[i]);

        return_value =  mqttsn_client_unsubscribe(p_client, p_topic_name, topic_name_len, &msg_id);
        TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
    }
}

void test_mqttsn_client_unsubscribe_ShallRetransmitIfTimerTimeouts(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    return_value =  mqttsn_client_unsubscribe(p_client, p_topic_name, topic_name_len, &msg_id);

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + 2 * m_timeout_val);
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_timer_timeout_handle(p_client);
}

void test_mqttsn_client_unsubscribe_ShallHandleEventRetransmissionTimeout(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_unsubscribe(p_client, p_topic_name, topic_name_len, &msg_id);

    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(p_client);
    }

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    nrf_free_Ignore();
    m_expected_evt_handler_calls = 1;
    mqttsn_client_timer_timeout_handle(p_client);
}

void test_mqttsn_client_unsubscribe_ShallIgnoreRetransmissionAfterLimitWasReached(void)
{
    uint32_t          return_value   = NRF_SUCCESS;
    uint16_t          msg_id         = 0;
    mqttsn_client_t * p_client       = &m_client;
    const uint8_t   * p_topic_name   = m_topic_name;
    uint16_t          topic_name_len = sizeof(m_topic_name);
    emulate_connected_state(p_client);

    uint8_t p_buf[NRF_MALLOC_MEMORY_BLOCK];

    nrf_malloc_IgnoreAndReturn(p_buf); // message buffer
    nrf_malloc_IgnoreAndReturn(p_buf); // message copy for retransmission
    nrf_free_Ignore();

    emulate_timer_start();
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_unsubscribe(p_client, p_topic_name, topic_name_len, &msg_id);

    // Run retransmission
    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(p_client);
    }

    // Event: retransmission limit reached. Packet destroyed
    m_expected_evt_handler_calls = 1;
    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    nrf_free_Ignore();
    mqttsn_client_timer_timeout_handle(p_client);

    // Ignored retransmission try
    mqttsn_client_timer_timeout_handle(p_client);
}

/***************************************************************************************************
 * @section PINGREQ
 **************************************************************************************************/

void test_mqttsn_client_keepalive_ShallRetransmitIfTimerTimeouts(void)
{
    int32_t return_value = NRF_SUCCESS;

    emulate_gateway_found_state(&m_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_connect(&m_client, &m_remote, m_gateway_id, &m_connect_opt);

    m_expected_evt_handler_calls = 1;
    const uint8_t * p_data  = m_test_connack_acc_packet;
    uint16_t        datalen = sizeof(m_test_connack_acc_packet);
    return_value = mqttsn_packet_receiver(&m_client, &m_port, &m_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val);
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
    mqttsn_client_timer_timeout_handle(&m_client);
}

void test_mqttsn_client_keepalive_ShallHandleEventRetransmissionTimeout(void)
{
    int32_t return_value = NRF_SUCCESS;

    emulate_gateway_found_state(&m_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_connect(&m_client, &m_remote, m_gateway_id, &m_connect_opt);

    m_expected_evt_handler_calls = 1;
    const uint8_t * p_data  = m_test_connack_acc_packet;
    uint16_t        datalen = sizeof(m_test_connack_acc_packet);
    return_value = mqttsn_packet_receiver(&m_client, &m_port, &m_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);

    TEST_ASSERT_EQUAL_UINT32(0, m_client.packet_queue.num_of_elements);
    for (int i = 0; i < MQTTSN_DEFAULT_RETRANSMISSION_CNT + 1; i++)
    {
        mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val + m_timeout_val);
        mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
        mqttsn_platform_timer_set_in_ms_IgnoreAndReturn(m_timer_val + m_timeout_val + 1);
        mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_platform_timer_start_IgnoreAndReturn(NRF_SUCCESS);
        mqttsn_client_timer_timeout_handle(&m_client);
    }

    m_expected_evt_handler_calls++;
    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timer_val);
    mqttsn_platform_timer_resolution_get_IgnoreAndReturn(NRF_TIMER_RESOLUTION);
    mqttsn_client_timer_timeout_handle(&m_client);
}

/***************************************************************************************************
 * @section UNINITIALIZE
 **************************************************************************************************/

void test_mqttsn_client_uninit_ShallReturnErrorWhenNullClient(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = NULL;

    return_value = mqttsn_client_uninit(p_client);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NULL, return_value);
}

void test_mqttsn_client_uninit_ShallReturnErrorWhenClientNotInitialized(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;

    return_value = mqttsn_client_uninit(p_client);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_FORBIDDEN, return_value);
}

void test_mqttsn_client_uninit_ShallReturnErrorWhenTransportFails(void)
{
    uint32_t          return_value = NRF_SUCCESS;
    mqttsn_client_t * p_client     = &m_client;
    emulate_init_state(p_client);

    for (uint32_t i = TRANSPORT_ERROR; i < MAX_TRANSPORT_ERROR; i++)
    {
        mqttsn_transport_uninit_IgnoreAndReturn(i);
        return_value = mqttsn_client_uninit(p_client);
        TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
    }
}

/***************************************************************************************************
 * @section GATEWAY INFO
 **************************************************************************************************/

void test_mqttsn_client_receive_gwinfo_ShallDoNothingIfClientNotSearchingGateway(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_gwinfo_packet;
    uint16_t                datalen      = sizeof(m_test_gwinfo_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_receive_gwinfo_ShallHandleEventGatewayFound(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_gwinfo_packet;
    uint16_t                datalen      = sizeof(m_test_gwinfo_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_init_state(p_client);
    emulate_timer_start();
    mqttsn_platform_rand_IgnoreAndReturn(m_timeout_val);
    mqttsn_client_search_gateway(p_client);

    mqttsn_platform_timer_cnt_get_IgnoreAndReturn(m_timeout_val);
    emulate_sending_packet();
    mqttsn_client_timer_timeout_handle(p_client);

    m_expected_evt_handler_calls = 1;
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

/***************************************************************************************************
 * @section CONNACK
 **************************************************************************************************/

void test_mqttsn_client_receive_connack_ShallReturnErrorIfNotEstablishingConnection(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_connack_acc_packet;
    uint16_t                datalen      = sizeof(m_test_connack_acc_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

void test_mqttsn_client_receive_connack_ShallReturnErrorIfConnectRejected(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    mqttsn_client_t       * p_client     = &m_client;

    emulate_gateway_found_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_connect(p_client, &m_remote, m_gateway_id, &m_connect_opt);

    const uint8_t * p_data  = m_test_connack_rid_packet;
    uint16_t        datalen = sizeof(m_test_connack_rid_packet);
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);

    p_data  = m_test_connack_rns_packet;
    datalen = sizeof(m_test_connack_rns_packet);
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

void test_mqttsn_client_receive_connack_ShallHandleEventConnected(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_connack_acc_packet;
    uint16_t                datalen      = sizeof(m_test_connack_acc_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_gateway_found_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_connect(p_client, &m_remote, m_gateway_id, &m_connect_opt);

    m_expected_evt_handler_calls = 1;
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_receive_connack_ShallReturnErrorWhenMessageIncorrect(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_connack_bad_packet;
    uint16_t                datalen      = sizeof(m_test_connack_bad_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_gateway_found_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_connect(p_client, &m_remote, m_gateway_id, &m_connect_opt);

    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

void test_mqttsn_client_receive_connack_ShallHandleEventRejectedCongestion(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_connack_rcg_packet;
    uint16_t                datalen      = sizeof(m_test_connack_rcg_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_gateway_found_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_connect(p_client, &m_remote, m_gateway_id, &m_connect_opt);

    m_expected_evt_handler_calls = 1;
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

/***************************************************************************************************
 * @section REGISTER (received)
 **************************************************************************************************/

void test_mqttsn_client_receive_register_ShallReturnErrorIfRegackSendFails(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_register_rec_packet;
    uint16_t                datalen      = sizeof(m_test_register_rec_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    m_expected_evt_handler_calls = 2;

    nrf_malloc_IgnoreAndReturn(NULL);
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);

    uint8_t buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(buf);
    mqttsn_transport_write_IgnoreAndReturn(NRF_ERROR_INTERNAL);
    nrf_free_Ignore();
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

void test_mqttsn_client_receive_register_ShallHandleEventRegisterReceived(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_register_rec_packet;
    uint16_t                datalen      = sizeof(m_test_register_rec_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    m_expected_evt_handler_calls = 1;

    uint8_t buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(buf);
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    nrf_free_Ignore();
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_receive_register_ShallReturnErrorWhenMessageIncorrect(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_register_bad_packet;
    uint16_t                datalen      = sizeof(m_test_register_bad_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

/***************************************************************************************************
 * @section REGACK
 **************************************************************************************************/

void test_mqttsn_client_receive_regack_ShallReturnErrorIfRegisterRejected(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_topic_register(p_client, m_topic_name, sizeof(m_topic_name), &msg_id);

    const uint8_t * p_data  = m_test_regack_rid_packet;
    uint16_t        datalen = sizeof(m_test_regack_rid_packet);
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);

    p_data  = m_test_regack_rns_packet;
    datalen = sizeof(m_test_regack_rns_packet);
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

void test_mqttsn_client_receive_regack_ShallHandleEventRejectedCongestion(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_regack_rcg_packet;
    uint16_t                datalen      = sizeof(m_test_regack_rcg_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_topic_register(p_client, m_topic_name, sizeof(m_topic_name), &msg_id);

    m_expected_evt_handler_calls = 1;
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_receive_regack_ShallReturnErrorIfBadPacketID(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_regack_invid_packet;
    uint16_t                datalen      = sizeof(m_test_regack_invid_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_topic_register(p_client, m_topic_name, sizeof(m_topic_name), &msg_id);

    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

void test_mqttsn_client_receive_regack_ShallHandleEventRegisteredTopic(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_regack_acc_packet;
    uint16_t                datalen      = sizeof(m_test_regack_acc_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_topic_register(p_client, m_topic_name, sizeof(m_topic_name), &msg_id);

    m_expected_evt_handler_calls = 1;
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_receive_regack_ShallReturnErrorWhenMessageIncorrect(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_regack_bad_packet;
    uint16_t                datalen      = sizeof(m_test_regack_bad_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_topic_register(p_client, m_topic_name, sizeof(m_topic_name), &msg_id);

    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NOT_SUPPORTED, return_value);
}

/***************************************************************************************************
 * @section PUBLISH (received)
 **************************************************************************************************/

void test_mqttsn_client_receive_publish_ShallReturnErrorIfPubackSendFails(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_publish_rec_packet;
    uint16_t                datalen      = sizeof(m_test_publish_rec_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    m_expected_evt_handler_calls = 2;

    nrf_malloc_IgnoreAndReturn(NULL);
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_NO_MEM, return_value);

    uint8_t buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(buf);
    mqttsn_transport_write_IgnoreAndReturn(NRF_ERROR_INTERNAL);
    nrf_free_Ignore();
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

void test_mqttsn_client_receive_publish_ShallHandleEventPublishReceived(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_publish_rec_packet;
    uint16_t                datalen      = sizeof(m_test_publish_rec_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);

    uint8_t buf[NRF_MALLOC_MEMORY_BLOCK];
    nrf_malloc_IgnoreAndReturn(buf);
    mqttsn_transport_write_IgnoreAndReturn(NRF_SUCCESS);
    nrf_free_Ignore();
    m_expected_evt_handler_calls = 1;
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

/***************************************************************************************************
 * @section PUBACK 
 **************************************************************************************************/

void test_mqttsn_client_receive_puback_ShallReturnErrorIfPublishRejected(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_publish(p_client, m_topic_id, m_payload, sizeof(m_payload), &msg_id);

    const uint8_t * p_data  = m_test_puback_rid_packet;
    uint16_t        datalen = sizeof(m_test_puback_rid_packet);
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);

    p_data  = m_test_puback_rns_packet;
    datalen = sizeof(m_test_puback_rns_packet);
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

void test_mqttsn_client_receive_puback_ShallHandleEventRejectedCongestion(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_puback_rcg_packet;
    uint16_t                datalen      = sizeof(m_test_puback_rcg_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_publish(p_client, m_topic_id, m_payload, sizeof(m_payload), &msg_id);

    m_expected_evt_handler_calls = 1;
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_receive_puback_ShallReturnErrorIfBadPacketID(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_puback_invid_packet;
    uint16_t                datalen      = sizeof(m_test_puback_invid_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_publish(p_client, m_topic_id, m_payload, sizeof(m_payload), &msg_id);

    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

void test_mqttsn_client_receive_puback_ShallHandleEventPublished(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_puback_acc_packet;
    uint16_t                datalen      = sizeof(m_test_puback_acc_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_publish(p_client, m_topic_id, m_payload, sizeof(m_payload), &msg_id);

    m_expected_evt_handler_calls = 1;
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_receive_puback_ShallReturnErrorWhenMessageIncorrect(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    mqttsn_client_t       * p_client     = &m_client;
    const uint8_t         * p_data       = m_test_puback_bad_packet;
    uint16_t                datalen      = sizeof(m_test_puback_bad_packet);

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_publish(p_client, m_topic_id, m_payload, sizeof(m_payload), &msg_id);

    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

/***************************************************************************************************
 * @section SUBACK 
 **************************************************************************************************/

void test_mqttsn_client_receive_suback_ShallReturnErrorIfSubscribeRejected(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_subscribe(p_client, m_topic_name, sizeof(m_topic_name), &msg_id);

    const uint8_t * p_data  = m_test_suback_rid_packet;
    uint16_t        datalen = sizeof(m_test_suback_rid_packet);
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);

    p_data  = m_test_suback_rns_packet;
    datalen = sizeof(m_test_suback_rns_packet);
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

void test_mqttsn_client_receive_suback_ShallHandleEventRejectedCongestion(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_suback_rcg_packet;
    uint16_t                datalen      = sizeof(m_test_suback_rcg_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_subscribe(p_client, m_topic_name, sizeof(m_topic_name), &msg_id);

    m_expected_evt_handler_calls = 1;
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_receive_suback_ShallReturnErrorIfBadPacketID(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_suback_invid_packet;
    uint16_t                datalen      = sizeof(m_test_suback_invid_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_subscribe(p_client, m_topic_name, sizeof(m_topic_name), &msg_id);

    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

void test_mqttsn_client_receive_suback_ShallHandleEventSubscribed(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_suback_acc_packet;
    uint16_t                datalen      = sizeof(m_test_suback_acc_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_subscribe(p_client, m_topic_name, sizeof(m_topic_name), &msg_id);

    m_expected_evt_handler_calls = 1;
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_receive_suback_ShallReturnErrorWhenMessageIncorrect(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_suback_bad_packet;
    uint16_t                datalen      = sizeof(m_test_suback_bad_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_subscribe(p_client, m_topic_name, sizeof(m_topic_name), &msg_id);

    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

/***************************************************************************************************
 * @section UNSUBACK 
 **************************************************************************************************/

void test_mqttsn_client_receive_unsuback_ShallReturnErrorIfBadPacketID(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_unsuback_invid_packet;
    uint16_t                datalen      = sizeof(m_test_unsuback_invid_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_unsubscribe(p_client, m_topic_name, sizeof(m_topic_name), &msg_id);

    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}

void test_mqttsn_client_receive_unsuback_ShallHandleEventUnsubscribed(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_unsuback_acc_packet;
    uint16_t                datalen      = sizeof(m_test_unsuback_acc_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_unsubscribe(p_client, m_topic_name, sizeof(m_topic_name), &msg_id);

    m_expected_evt_handler_calls = 1;
    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_SUCCESS, return_value);
}

void test_mqttsn_client_receive_unsuback_ShallReturnErrorWhenMessageIncorrect(void)
{
    uint32_t                return_value = NRF_SUCCESS;
    uint16_t                msg_id       = 0;
    const mqttsn_port_t   * p_port       = &m_port;
    const mqttsn_remote_t * p_remote     = &m_remote;
    const uint8_t         * p_data       = m_test_unsuback_bad_packet;
    uint16_t                datalen      = sizeof(m_test_unsuback_bad_packet);
    mqttsn_client_t       * p_client     = &m_client;

    emulate_connected_state(p_client);
    emulate_sending_packet();
    emulate_timer_start();
    mqttsn_client_unsubscribe(p_client, m_topic_name, sizeof(m_topic_name), &msg_id);

    return_value = mqttsn_packet_receiver(p_client, p_port, p_remote, p_data, datalen);
    TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, return_value);
}
