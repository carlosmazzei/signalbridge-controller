#include "main.h"

/** @brief Decode reception task
 *
 * Process bytes received from UART.
 *
 */
static void decode_reception_task()
{
    uint8_t data;
    if (tud_cdc_n_available(0))
    {
        uint32_t count = tud_cdc_n_read(0, receive_buffer, sizeof(receive_buffer));
        for (size_t i = 0; i < count; i++)
        {
            data = receive_buffer[i];
            printf("Received data (encoded): %d", data);
            if (data == PACKET_MARKER)
            {
                uint8_t decode_buffer[receive_buffer_index];
                size_t num_decoded = cobs_decode(receive_buffer, receive_buffer_index, decode_buffer);
                printf("Decoded %d bytes", num_decoded);

                receive_buffer_index = 0;
                receive_buffer_overflow = false;

                process_inbound_data(decode_buffer);
            }
            else
            {
                if ((receive_buffer_index + 1) < MAX_ENCODED_BUFFER_SIZE)
                    receive_buffer[receive_buffer_index++] = data;
                else
                    receive_buffer_overflow = true;
            }
        }
    }
}

/** @brief Send a packet of data.
 *
 * This function will encode and send a packet of data. After
 * sending, it will send the specified `Packet Marker` defined.
 *
 * @param id The ID of the device sending the data.
 * @param command The command to send.
 * @param send_data A pointer to the data to send.
 * @param length The length of the data to send.
 */
void send_data(uint16_t id, uint8_t command, uint8_t *send_data, uint8_t length)
{
    uint8_t i;
    uint8_t uart_outbound_buffer[DATA_BUFFER_SIZE];

    id <<= 5;
    uart_outbound_buffer[0] = (id >> 8);
    uart_outbound_buffer[1] = id & 0xE0;
    uart_outbound_buffer[1] |= (command & 0x1F);
    uart_outbound_buffer[2] = length;

    for (i = 3; (i < uart_outbound_buffer[2] + 3) && (i < DATA_BUFFER_SIZE); i++)
    {
        uart_outbound_buffer[i] = *send_data;
        send_data++;
    }

    uint8_t encode_buffer[MAX_ENCODED_BUFFER_SIZE];

    size_t num_encoded = cobs_encode(uart_outbound_buffer, length + 3, encode_buffer);
    printf("Encoded %d bytes", num_encoded);

    encode_buffer[num_encoded] = 0x00; // Ensure the last byte is 0

    // Print encode buffer
    for (i = 0; i < num_encoded + 1; i++)
    {
        printf("Encoded buffer[%d]: %#02X", i, encode_buffer[i]);
    }

    for (i = 0; i < num_encoded + 1; i++)
    {
        tud_cdc_n_write_char(0, encode_buffer[i]);
    }
}

/** @brief Task to process inbound messages
 *
 * This function processes inbound messages from the MQTT server on the reception queue
 *
 * @param rx_buffer A pointer to a data buffer to be processed
 */
void process_inbound_data(uint8_t *rx_buffer)
{
    uint16_t rxID = 0;
    uint8_t len = 0;
    uint8_t i = 0;
    uint8_t decoded_data[DATA_BUFFER_SIZE];

    rxID = *rx_buffer++ << 3;
    rxID |= ((*rx_buffer & 0xE0) >> 5);
    printf("dequeued rxID: %d", rxID);

    decoded_data[0] = *rx_buffer++ & 0x1F;
    printf("dequeued cmd: %d", decoded_data[0]);

    len = *rx_buffer++;
    printf("dequeued len: %d", len);

    for (i = 1; (i < len + 1) && (i < DATA_BUFFER_SIZE - 2); i++)
    {
        decoded_data[i] = *rx_buffer++;
        printf("dequeued data [%d]: %#02X", i, decoded_data[i]);
    }

    switch (decoded_data[0])
    {
    /*
     * Implement other inbound commands (LED, DISPLAY, etc)
     */
    case (PC_LEDOUT_CMD):

        break;

    case (PC_ECHO_CMD):
        send_data(rxID, decoded_data[0], &decoded_data[1], len);
        // int64_t delta = esp_timer_get_time() - data_received_time;
        // ESP_LOGI(TAG, "Received delta (rx -> pub): %lld", delta);
        break;

    default:
        break;
    }
}

/** @brief Task to send outbound messages
 *
 * This task will read hardware inputs and send data to the host
 *
 * @param pvParameters Arguments passed to the task
 */
void process_outbound_task()
{
    uint8_t test_data[10];
    test_data[0] = 0x01;
    test_data[1] = 0x02;
    test_data[2] = 0x03;

    // for (;;)
    // {
    //     // Fake data to test communication
    //     // send_data(0x01, PC_LEDOUT_CMD, test_data, 3);
    //     // ESP_LOGD(TAG, "Sent test data and wait 3 seconds");

    //     int rx_freesize = 0;
    //     uart_get_buffered_data_len(EX_UART_NUM, (size_t *)&rx_freesize);

    //     int tx_freesize = 0;
    //     uart_get_tx_buffer_free_size(EX_UART_NUM, (size_t *)&tx_freesize);
    //     //ESP_LOGD(TAG, "UART RX Buffered data len: %d, TX Free Size: %d", rx_freesize, tx_freesize);

    //     vTaskDelay(pdMS_TO_TICKS(3000));
    // }

    // vTaskDelete(NULL);
}

/** @brief Main function
 *
 */
int main(void)
{
    board_init();

    // init device stack on configured roothub port
    tud_init(BOARD_TUD_RHPORT);

    while (1)
    {
        tud_task(); // tinyusb device task

        decode_reception_task();
        process_outbound_task();
    }

    return 0;
}
