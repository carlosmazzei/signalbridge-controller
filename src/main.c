#include "main.h"

/**
 * Queue to store cobs encoded received data
 */
static QueueHandle_t encoded_reception_queue = NULL;

/**
 * Queue to store data events to be sent to the host
 */
static QueueHandle_t data_event_queue = NULL;

/**
 * Store error counters
 */
static error_counters_t error_counters;

/**
 * Store task handles
 */
static task_handles_t task_handles;

/** @brief Receive data from uart
 *
 *  Receives data from UART and post it to a reception queue. If the data is encoded, it should be decoded accordingly
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
            for (uint32_t i = 0; (i < count) && (i < MAX_ENCODED_BUFFER_SIZE); i++)
            {
                BaseType_t success = xQueueSend(encoded_reception_queue, &receive_buffer[i], portMAX_DELAY);
                if (success != pdTRUE)
                    error_counters.queue_send_error++;
            }
        }
        watchdog_update();
    }
    vTaskDelete(NULL);
}

/** @brief CDC tasks
 *
 * CDC task needed to update the USB stack
 *
 * @param pvParameters Pointer to parameters passed to the task
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

        watchdog_update();
    }
    vTaskDelete(NULL);
}

/** @brief Decode reception task
 *
 * Process bytes received from UART and decoded the data. The received data is using COBS encoding.
 *
 * @param pvParameters Pointer to parameters passed to the task
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
        watchdog_update();
    }
    vTaskDelete(NULL);
}

/** @brief Send error counters to host
 *
 *  Send error counters to the host and format the message in chunks of bytes
 *
 */
static inline void send_status()
{
    uint8_t data[10];

    data[0] = error_counters.display_out_error >> 8;
    data[1] = error_counters.display_out_error;
    data[2] = error_counters.queue_send_error >> 8;
    data[3] = error_counters.queue_send_error;
    data[4] = error_counters.queue_receive_error >> 8;
    data[5] = error_counters.queue_receive_error;
    data[6] = error_counters.led_out_error >> 8;
    data[7] = error_counters.led_out_error;
    data[8] = error_counters.watchdog_error >> 8;
    data[9] = error_counters.watchdog_error;

    send_data(PANEL_ID, PC_STATUS_CMD, data, sizeof(data));
}

/** @brief Send status of the heap of each task
 *
 * Send heap usage of each task and overall heap usage from the idle task
 *
 */
void send_heap_status()
{
    uint8_t data[28];
    uint32_t heap_usage;

    heap_usage = uxTaskGetStackHighWaterMark(task_handles.adc_read_task_handle);
    data[0] = heap_usage >> 24;
    data[1] = heap_usage >> 16;
    data[2] = heap_usage >> 8;
    data[3] = heap_usage;

    heap_usage = uxTaskGetStackHighWaterMark(task_handles.cdc_task_handle);
    data[4] = heap_usage >> 24;
    data[5] = heap_usage >> 16;
    data[6] = heap_usage >> 8;
    data[7] = heap_usage;

    heap_usage = uxTaskGetStackHighWaterMark(task_handles.decode_reception_task_handle);
    data[8] = heap_usage >> 24;
    data[9] = heap_usage >> 16;
    data[10] = heap_usage >> 8;
    data[11] = heap_usage;

    heap_usage = uxTaskGetStackHighWaterMark(task_handles.encoder_read_task_handle);
    data[12] = heap_usage >> 24;
    data[13] = heap_usage >> 16;
    data[14] = heap_usage >> 8;
    data[15] = heap_usage;

    heap_usage = uxTaskGetStackHighWaterMark(task_handles.keypad_task_handle);
    data[16] = heap_usage >> 24;
    data[17] = heap_usage >> 16;
    data[18] = heap_usage >> 8;
    data[19] = heap_usage;

    heap_usage = uxTaskGetStackHighWaterMark(task_handles.process_outbound_task_handle);
    data[20] = heap_usage >> 24;
    data[21] = heap_usage >> 16;
    data[22] = heap_usage >> 8;
    data[23] = heap_usage;

    heap_usage = uxTaskGetStackHighWaterMark(task_handles.uart_event_task_handle);
    data[24] = heap_usage >> 24;
    data[25] = heap_usage >> 16;
    data[26] = heap_usage >> 8;
    data[27] = heap_usage;
    
    send_data(PANEL_ID, PC_HEAP_STATUS_CMD, data, sizeof(data));

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
    /**
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

    case (PC_HEAP_STATUS_CMD):
        send_heap_status();
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
            send_data(PANEL_ID, data_event.command, data_event.data, data_event.data_length);
        }
        else
        {
            error_counters.queue_receive_error++;
        }
        watchdog_update();
    }
    vTaskDelete(NULL);
}

/** @brief Main function
 *
 */
int main(void)
{
    // Set error state to false
    BaseType_t success;
    error_counters.error_state = false;

    /** @todo Implement restart from watchdog timer. Check if handles and queues were created properly */
    if (watchdog_caused_reboot())
        error_counters.watchdog_error++;

    board_init(); // TinyUSB init

    // init device stack on configured roothub port
    if (!tud_init(BOARD_TUD_RHPORT))
        error_counters.error_state = true;

    // Initialize hardware specific config
    if (!prvSetupHardware())
        error_counters.error_state = true;

    // Create a task to handle UART event from ISR
    success = xTaskCreate(cdc_task, "cdc_task", 512, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, &task_handles.cdc_task_handle);
    if (success != pdPASS)
        error_counters.error_state = true;

    // Create queue to receive encoded data
    encoded_reception_queue = xQueueCreate(ENCODED_QUEUE_SIZE, sizeof(uint8_t)); // The size of a single byte
    if (encoded_reception_queue != NULL)
    {
        // Created the queue  successfully
        success = xTaskCreate(uart_event_task, "uart_event_task", 2 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, &task_handles.uart_event_task_handle);
        if (success != pdPASS)
            error_counters.error_state = true;

        success = xTaskCreate(decode_reception_task, "decode_reception_task", 2 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY + 1, &task_handles.decode_reception_task_handle);
        if (success != pdPASS)
            error_counters.error_state = true;
    }
    else
    {
        error_counters.error_state = true;
    }

    // Create queue to receive data events
    success = xTaskCreate(process_outbound_task, "process_outbound_task", 2 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, &task_handles.process_outbound_task_handle);
    if (success != pdPASS)
        error_counters.error_state = true;

    // Initiate task to read the inputs
    success = xTaskCreate(adc_read_task, "adc_read_task", 2 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, &task_handles.adc_read_task_handle);
    if (success != pdPASS)
        error_counters.error_state = true;

    success = xTaskCreate(keypad_task, "keypad_task", 2 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, &task_handles.keypad_task_handle);
    if (success != pdPASS)
        error_counters.error_state = true;

    success = xTaskCreate(encoder_read_task, "encoder_task", 2 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, &task_handles.encoder_read_task_handle);
    if (success != pdPASS)
        error_counters.error_state = true;

    /* Start the tasks and timer running. */
    if (error_counters.error_state == false)
        vTaskStartScheduler();
    else
        enter_error_state();

    /* Should not enter if everything initiated correctly */
    for (;;)
        ;

    return 0;
}

/** @brief Setup hardware
 *
 * Setup hardware such as UART, LED, Keyboard and internal structures.
 *
 * @return Return true if initialization was successfull and false if failed to create any resource
 */
static inline bool prvSetupHardware(void)
{
    stdio_init_all();

    output_init();

    // Create queue to receive data events
    data_event_queue = xQueueCreate(DATA_EVENT_QUEUE_SIZE, sizeof(data_events_t)); // The size of a single byte, created before hardware setup?
    if (data_event_queue == NULL)
        return false;

    // Enable inputs (Keypad, ADC and Rotaries)
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

    // Init error counters and handles
    error_counters.display_out_error = 0;
    error_counters.led_out_error = 0;
    error_counters.queue_receive_error = 0;
    error_counters.queue_send_error = 0;

    task_handles.adc_read_task_handle = NULL;
    task_handles.decode_reception_task_handle = NULL;
    task_handles.cdc_task_handle = NULL;
    task_handles.keypad_task_handle = NULL;
    task_handles.encoder_read_task_handle = NULL;
    task_handles.process_outbound_task_handle = NULL;
    task_handles.uart_event_task_handle = NULL;

    watchdog_enable(1000, true);

    return true;
}

/** @brief Enter error state
 *
 *  Enter error loop blinking led if any error ocurred during startup
 *
 */
static inline void enter_error_state()
{
    bool state = true;
    while (true)
    {
        gpio_put(PICO_DEFAULT_LED_PIN, state ^= 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        watchdog_update();
    }
}