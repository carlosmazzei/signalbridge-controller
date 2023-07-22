#include "main.h"

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
                // TO-DO: Implement error counters
                // if (success != pdTRUE)
                // {
                //
                // }
            }
        }
    }
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
    }

    vTaskDelete(NULL);
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

        break;

    case (PC_ECHO_CMD):
        send_data(rxID, decoded_data[0], &decoded_data[1], len);
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
static void process_outbound_task(void *pvParameters)
{
    uint8_t test_data[10];
    test_data[0] = 0x01;
    test_data[1] = 0x02;
    test_data[2] = 0x03;

    for (;;)
    {
        // Fake data to test communication
        // send_data(0x01, PC_LEDOUT_CMD, test_data, 3);
        // ESP_LOGD(TAG, "Sent test data and wait 3 seconds");

        // int rx_freesize = 0;
        // uart_get_buffered_data_len(EX_UART_NUM, (size_t *)&rx_freesize);

        // int tx_freesize = 0;
        // uart_get_tx_buffer_free_size(EX_UART_NUM, (size_t *)&tx_freesize);
        // ESP_LOGD(TAG, "UART RX Buffered data len: %d, TX Free Size: %d", rx_freesize, tx_freesize);

        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    vTaskDelete(NULL);
}

/** @brief Main function
 *
 */
int main(void)
{
    board_init();
    prvSetupHardware();

    // init device stack on configured roothub port
    tud_init(BOARD_TUD_RHPORT);

    // Create queue to received encoded data
    encoded_reception_queue = xQueueCreate(ENCODED_QUEUE_SIZE, sizeof(uint8_t)); // The size of a single byte

    // Create a task to handler UART event from ISR
    xTaskCreate(cdc_task, "cdc_task", 512, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, NULL);
    xTaskCreate(uart_event_task, "uart_event_task", 3 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, NULL);
    xTaskCreate(decode_reception_task, "decode_reception_task", 3 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, NULL);
    xTaskCreate(process_outbound_task, "process_outbound_task", 3 * configMINIMAL_STACK_SIZE, NULL, mainPROCESS_QUEUE_TASK_PRIORITY, NULL);

    /* Start the tasks and timer running. */
    vTaskStartScheduler();

    /* If all is well, the scheduler will now be running, and the following
    line will never be reached.  If the following line does execute, then
    there was insufficient FreeRTOS heap memory available for the Idle and/or
    timer tasks to be created.  See the memory management section on the
    FreeRTOS web site for more details on the FreeRTOS heap
    http://www.freertos.org/a00111.html. */
    for (;;);

    return 0;
}

/*-----------------------------------------------------------*/

static void prvSetupHardware(void)
{
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, 1);
    gpio_put(PICO_DEFAULT_LED_PIN, !PICO_DEFAULT_LED_PIN_INVERTED);
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook(void)
{
    /* Called if a call to pvPortMalloc() fails because there is insufficient
    free memory available in the FreeRTOS heap.  pvPortMalloc() is called
    internally by FreeRTOS API functions that create tasks, queues, software
    timers, and semaphores.  The size of the FreeRTOS heap is set by the
    configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

    /* Force an assert. */
    configASSERT((volatile void *)NULL);
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
    (void)pcTaskName;
    (void)pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */

    /* Force an assert. */
    configASSERT((volatile void *)NULL);
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook(void)
{
    volatile size_t xFreeHeapSpace;

    /* This is just a trivial example of an idle hook.  It is called on each
    cycle of the idle task.  It must *NOT* attempt to block.  In this case the
    idle task just queries the amount of FreeRTOS heap that remains.  See the
    memory management section on the http://www.FreeRTOS.org web site for memory
    management options.  If there is a lot of heap memory free then the
    configTOTAL_HEAP_SIZE value in FreeRTOSConfig.h can be reduced to free up
    RAM. */
    xFreeHeapSpace = xPortGetFreeHeapSize();

    /* Remove compiler warning about xFreeHeapSpace being set but never used. */
    (void)xFreeHeapSpace;
}
/*-----------------------------------------------------------*/

void vApplicationTickHook(void)
{
#if mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 0
    {
/* The full demo includes a software timer demo/test that requires
prodding periodically from the tick interrupt. */
#if (mainENABLE_TIMER_DEMO == 1)
        vTimerPeriodicISRTests();
#endif

/* Call the periodic queue overwrite from ISR demo. */
#if (mainENABLE_QUEUE_OVERWRITE == 1)
        vQueueOverwritePeriodicISRDemo();
#endif

/* Call the periodic event group from ISR demo. */
#if (mainENABLE_EVENT_GROUP == 1)
        vPeriodicEventGroupsProcessing();
#endif

/* Call the code that uses a mutex from an ISR. */
#if (mainENABLE_INTERRUPT_SEMAPHORE == 1)
        vInterruptSemaphorePeriodicTest();
#endif

/* Call the code that 'gives' a task notification from an ISR. */
#if (mainENABLE_TASK_NOTIFY == 1)
        xNotifyTaskFromISR();
#endif
    }
#endif
}