#include "main.h"

/*
 * Queue to store cobs encoded received data
 */
static QueueHandle_t encoded_reception_queue = NULL;

/*
 * Queue to store data events to be sent to the host
 */
static QueueHandle_t data_event_queue = NULL;

/*
 * Store error counters
 */
error_counters_t error_counters;

/** @brief Receive data from uart
 *
 * @param pvParameters Pointer to parameters passed to the task
 */
static void uart_event_task(void *pvParameters)
{
    uint8_t receive_buffer[MAX_ENCODED_BUFFER_SIZE];

    while (true)
    {
        if (tud_cdc_n_available(0))
        {
            uint32_t count = tud_cdc_n_read(0, receive_buffer, sizeof(receive_buffer));
            for (uint32_t i = 0; i < count; i++)
            {
                BaseType_t success = xQueueSend(encoded_reception_queue, &receive_buffer[i], portMAX_DELAY);
                if (success != pdTRUE)
                    error_counters.queue_send_error++;
            }
        }
    }

    vTaskDelete(NULL);
}

/** @brief CDC tasks
 *
 * @param pvParameters Pointer to parameters passed to the task
 *
 */
static void cdc_task(void *pvParameters)
{
    while (true)
    {
        tud_task(); // tinyusb device task

        if (tud_cdc_n_connected)
            gpio_put(PICO_DEFAULT_LED_PIN, 1);
        else
            gpio_put(PICO_DEFAULT_LED_PIN, 0);
    }

    vTaskDelete(NULL);
}

/** @brief Decode reception task
 *
 * Process bytes received from UART.
 *
 */
static void decode_reception_task(void *pvParameters)
{
    uint8_t receive_buffer[MAX_ENCODED_BUFFER_SIZE];
    size_t receive_buffer_index = 0;
    bool receive_buffer_overflow = false;

    while (true)
    {
        uint8_t data;
        if (xQueueReceive(encoded_reception_queue, (void *)&data, portMAX_DELAY))
        {
            if (data == PACKET_MARKER)
            {
                uint8_t decode_buffer[receive_buffer_index];
                size_t num_decoded = cobs_decode(receive_buffer, receive_buffer_index, decode_buffer);

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
        else
        {
            error_counters.queue_receive_error++;
        }
    }

    vTaskDelete(NULL);
}

/** @brief Send error counters to host
 *
 */
static inline void send_status()
{
    uint8_t data[8];
    data[0] = error_counters.display_out_error >> 8;
    data[1] = error_counters.display_out_error;
    data[2] = error_counters.queue_send_error >> 8;
    data[3] = error_counters.queue_send_error;
    data[4] = error_counters.queue_receive_error >> 8;
    data[5] = error_counters.queue_receive_error;
    data[6] = error_counters.led_out_error >> 8;
    data[7] = error_counters.led_out_error;
    send_data(0, PC_STATUS_CMD, data, sizeof(data));
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
static void send_data(uint16_t id, uint8_t command, uint8_t *send_data, uint8_t length)
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
    encode_buffer[num_encoded] = 0x00; // Ensure the last byte is 0

    for (i = 0; i < num_encoded + 1; i++)
    {
        tud_cdc_n_write_char(0, encode_buffer[i]);
    }
    tud_cdc_write_flush();
}

/** @brief Task to process inbound messages
 *
 * This function processes inbound messages from the MQTT server on the reception queue
 *
 * @param rx_buffer A pointer to a data buffer to be processed
 */
static void process_inbound_data(uint8_t *rx_buffer)
{
    uint16_t rxID = 0;
    uint8_t len = 0;
    uint8_t i = 0;
    uint8_t decoded_data[DATA_BUFFER_SIZE];
    uint8_t leds[2];

    rxID = *rx_buffer++ << 3;
    rxID |= ((*rx_buffer & 0xE0) >> 5);
    decoded_data[0] = *rx_buffer++ & 0x1F;
    len = *rx_buffer++;

    for (i = 1; (i < len + 1) && (i < DATA_BUFFER_SIZE - 2); i++)
    {
        decoded_data[i] = *rx_buffer++;
    }

    switch (decoded_data[0])
    {
    /*
     * Implement other inbound commands (LED, DISPLAY, etc)
     */
    case (PC_LEDOUT_CMD):
        leds[0] = decoded_data[2]; // Offset of the byte state to change
        leds[1] = decoded_data[3]; // States of the LEDs to change
        if (led_out(decoded_data[1], leds, sizeof(leds)) != true)
            error_counters.led_out_error++;
        break;

    case (PC_PWM_CMD):
        set_pwm_duty(decoded_data[1]);
        break;

    case (PC_DPYCTL_CMD):
        if (display_out(&decoded_data[1], len) != len)
            error_counters.display_out_error++;
        break;

    case (PC_ECHO_CMD):
        send_data(rxID, decoded_data[0], &decoded_data[1], len);
        break;

    case (PC_STATUS_CMD):
        send_status();
        break;

    default:
        break;
    }
}

/** @brief Task to send outbound messages
 *
 * This task reads hardware inputs and send data to the host
 *
 * @param pvParameters Arguments passed to the task
 */
static void process_outbound_task(void *pvParameters)
{
    while (true)
    {
        data_events_t data_event;
        if (xQueueReceive(data_event_queue, (void *)&data_event, portMAX_DELAY))
        {
            send_data(0x01, data_event.command, data_event.data, data_event.data_length);
        }
        else
        {
            error_counters.queue_receive_error++;
        }
    }

    vTaskDelete(NULL);
}

/** @brief Main function
 *
 */
int main(void)
{
    board_init(); // TinyUSB init
    prvSetupHardware();

    // init device stack on configured roothub port
    tud_init(BOARD_TUD_RHPORT);

    // Create queue to received encoded data
    encoded_reception_queue = xQueueCreate(ENCODED_QUEUE_SIZE, sizeof(uint8_t)); // The size of a single byte

    // Create a task to handle UART event from ISR
    xTaskCreate(cdc_task, "cdc_task", 512, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, NULL);
    xTaskCreate(uart_event_task, "uart_event_task", 2 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, NULL);
    xTaskCreate(decode_reception_task, "decode_reception_task", 2 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY + 1, NULL);
    xTaskCreate(process_outbound_task, "process_outbound_task", 2 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, NULL);

    // Initiate task to read the inputs
    xTaskCreate(adc_read_task, "adc_read_task", 2 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, NULL);
    xTaskCreate(keypad_task, "keypad_task", 2 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, NULL);
    xTaskCreate(encoder_read_task, "encoder_task", 2 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, NULL);

    /* Start the tasks and timer running. */
    vTaskStartScheduler();

    for (;;)
        ;

    return 0;
}

/** @brief Setup hardware
 *
 * Setup hardware such as UART, LED, etc.
 *
 */
static void prvSetupHardware(void)
{
    stdio_init_all();

    output_init();

    // Enable inputs (Keypad, ADC and Rotaries)
    data_event_queue = xQueueCreate(DATA_EVENT_QUEUE_SIZE, sizeof(data_events_t)); // The size of a single byte, created before hardware setup?
    const input_config_t config = {
        .columns = 8,
        .rows = 8,
        .key_settling_time_ms = 20,
        .input_event_queue = data_event_queue,
        .adc_banks = 2,
        .adc_channels = 8,
        .adc_settling_time_ms = 100,
        .encoder_settling_time_ms = 10,
        .encoder_mask[7] = true}; // Enable encoder on row 8 (last row)

    input_init(&config);
}