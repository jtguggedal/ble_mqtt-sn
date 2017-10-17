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

void mqttsn_packet_fifo_init(mqttsn_client_t * p_client)
{
    memset(p_client->packet_queue.packet, 0, sizeof(mqttsn_packet_queue_t));
}

void mqttsn_packet_fifo_uninit(mqttsn_client_t * p_client)
{
    memset(p_client->packet_queue.packet, 0, sizeof(mqttsn_packet_queue_t));
}

uint32_t mqttsn_packet_fifo_elem_add(mqttsn_client_t * p_client, mqttsn_packet_t * packet)
{
    if (p_client->packet_queue.num_of_elements == MQTTSN_PACKET_FIFO_MAX_LENGTH)
    {
        NRF_LOG_ERROR("Packet ID fifo capacity exceeded\r\n");
        return NRF_ERROR_NO_MEM;
    }
    else
    {
        p_client->packet_queue.packet[p_client->packet_queue.num_of_elements] = *packet;

        p_client->packet_queue.num_of_elements++;
        return NRF_SUCCESS;
    }
}

uint32_t mqttsn_packet_fifo_elem_dequeue(mqttsn_client_t * p_client, uint16_t msg_to_dequeue, mqttsn_packet_dequeue_t mode)
{
    uint32_t elem_to_dequeue = mqttsn_packet_fifo_elem_find(p_client, msg_to_dequeue, mode);
    if (elem_to_dequeue == MQTTSN_PACKET_FIFO_MAX_LENGTH)
    {
        NRF_LOG_ERROR("Cannot dequeue packet. Packet does not exist\r\n");
        return NRF_ERROR_NOT_FOUND;
    }

    nrf_free(p_client->packet_queue.packet[elem_to_dequeue].p_data);

    for (int i = elem_to_dequeue; i < p_client->packet_queue.num_of_elements; i++)
    {
        p_client->packet_queue.packet[i] = p_client->packet_queue.packet[i+1];
    }

    p_client->packet_queue.num_of_elements--;

    return NRF_SUCCESS;
}

uint32_t mqttsn_packet_fifo_elem_find(mqttsn_client_t * p_client, uint16_t msg_to_dequeue, mqttsn_packet_dequeue_t mode)
{
    uint32_t index       = MQTTSN_PACKET_FIFO_MAX_LENGTH;
    uint32_t type_offset = 0;

    for (int i = 0; i < p_client->packet_queue.num_of_elements; i++)
    {
        switch (mode)
        {
            case MQTTSN_MESSAGE_ID:
                if (p_client->packet_queue.packet[i].id == msg_to_dequeue)
                {
                    return i;
                }
                break;

            case MQTTSN_MESSAGE_TYPE:

                type_offset = p_client->packet_queue.packet[i].p_data[0] == MQTTSN_TWO_BYTE_LENGTH_CODE ? 
                              MQTTSN_OFFSET_TWO_BYTE_LENGTH : MQTTSN_OFFSET_ONE_BYTE_LENGTH;
                
                if (p_client->packet_queue.packet[i].p_data[type_offset] == msg_to_dequeue)
                {
                    return i;
                }
                break;

            default:
                break;
        }
    }
    return index;
}