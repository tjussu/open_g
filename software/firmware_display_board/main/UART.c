/*
    Code is Based on the "UART Events Example" from the ESP-IDF documentation (IDF v5.4.2)


*/
#include "UART.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "max7318.h"
#include "buzzer.h"


#define UART_BUF_SIZE 1024

QueueHandle_t uart0_queue;

QueueHandle_t load_queue;
QueueHandle_t alarm_queue;
QueueHandle_t fLED_queue;
QueueHandle_t button_queue;


void setup_queues(){
    uint8_t ld = 0;
    xQueueOverwrite(load_queue, &ld);
}

void uart_send_update_task(void * args){

    TickType_t xLWT = xTaskGetTickCount();

    setup_queues();

    while(load_queue == NULL){
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    

    while(1){
        uint8_t msg[8] = {0};
        msg[0] = '$';   // Start
        msg[1] = '$';   // Start
        msg[2] = '$';   // Start
        msg[3] = 0x02;
        xQueuePeek(load_queue, &msg[4], portMAX_DELAY);  // Load value

        // Button and fLEDs
        bool btn = false;
        xQueuePeek(button_queue, &btn, portMAX_DELAY);
        msg[5] |= (btn<<7); 
        msg[6] = 0x0d;  // End
        msg[7] = 0x0a;  // End

        uart_write_bytes(UART_NUM_0, &msg[0], 8);

        xTaskDelayUntil(&xLWT, pdMS_TO_TICKS(100));
    }
}

void uart_event_task(void * args){
    uart_event_t event;
    size_t buffered_size;
    uint8_t *datap = (uint8_t*) malloc(UART_BUF_SIZE);

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    uart_driver_install(UART_NUM_0, UART_BUF_SIZE, UART_BUF_SIZE, 20, &uart0_queue, 0);
    uart_param_config(UART_NUM_0, &uart_config);

    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    
    uart_enable_pattern_det_baud_intr(UART_NUM_0, '$', 3, 7, 0, 0);
    uart_pattern_queue_reset(UART_NUM_0, 20);

    xTaskCreate(uart_send_update_task, "UART Update", 2048, NULL, 11, NULL);


    while(1){

        if(xQueueReceive(uart0_queue, (void*) &event, (TickType_t)portMAX_DELAY)){
            bzero(datap, UART_BUF_SIZE);
            switch (event.type){
                case UART_DATA:
                    // uart_read_bytes(UART_NUM_0, datap, event.size, portMAX_DELAY);
                    // printf("Message:");
                    // uart_write_bytes(UART_NUM_0, (const char*) datap, event.size);
                    break;
                case UART_FIFO_OVF:
                    printf(">>>ERROR: UART FIFO overflow\n");
                    uart_flush_input(UART_NUM_0);
                    xQueueReset(uart0_queue);
                    break;
                case UART_BUFFER_FULL:
                    printf(">>>ERROR: UART buffer overflow\n");
                    uart_flush_input(UART_NUM_0);
                    xQueueReset(uart0_queue);
                    break;
                case UART_BREAK:
                    printf(">>>ERROR: UART RX break\n");
                    break;
                case UART_PARITY_ERR:
                    printf(">>>ERROR: UART Parity error\n");
                    break;
                case UART_FRAME_ERR:
                    printf(">>>ERROR: UART Frame error\n");
                    break;
                case UART_PATTERN_DET:
                    uart_get_buffered_data_len(UART_NUM_0, &buffered_size);
                    int pos = uart_pattern_pop_pos(UART_NUM_0);
                    if(pos == -1){
                        printf(">>>ERROR: Pattern error\n");
                        uart_flush_input(UART_NUM_0);
                    }
                    else{
                        //dumping unneeded data
                        uart_read_bytes(UART_NUM_0, datap, pos, 100 / portTICK_PERIOD_MS);
                        bzero(datap, UART_BUF_SIZE);

                        // Reading START and Length
                        uart_read_bytes(UART_NUM_0, datap, 4, 100 / portTICK_PERIOD_MS);
                        int package_len = datap[3];

                        // If length is valid, overwrite buffer with data
                        if(package_len >= 2){
                            uart_read_bytes(UART_NUM_0, datap, package_len+2, 100/portTICK_PERIOD_MS);

                            // If END is valid, parse data
                            if((datap[package_len] == 0x0d) && (datap[package_len+1] == 0x0a)){
                                // printf("Format: len: %d %c %c\n", package_len, datap[0], datap[1]);

                                // Read Load
                                // printf("Load: %d %%\n", datap[0]);
                                xQueueOverwrite(load_queue, &datap[0]);
                                i2c_set_led_strip_max7318(max7318_handler, datap[0]);

                                // Read Alarm
                                bool alm = false;
                                if(datap[1] & (1<<0)){
                                    alm = true;
                                    xQueueSend(alert_queue, &alm, portMAX_DELAY);
                                }
                                else {
                                    xQueueSend(alert_queue, &alm, portMAX_DELAY);
                                }
                                

                                // Read fLED state
                                if(datap[1] & (1<<1)) i2c_set_led_max7318(max7318_handler, LED_W_1, LED_ON);
                                else i2c_set_led_max7318(max7318_handler, LED_W_1, LED_OFF);

                                if(datap[1] & (1<<2)) i2c_set_led_max7318(max7318_handler, LED_W_2, LED_ON);
                                else i2c_set_led_max7318(max7318_handler, LED_W_2, LED_OFF);

                                if(datap[1] & (1<<3)) i2c_set_led_max7318(max7318_handler, LED_W_3, LED_ON);
                                else i2c_set_led_max7318(max7318_handler, LED_W_3, LED_OFF);

                                
                            }
                            else if(datap[package_len] != 0x0d){
                                printf(">>>ERROR: invalid Frame - stop 1 not found\n");
                            }
                            else if(datap[package_len] != 0x0a){
                                printf(">>>ERROR: invalid Frame - stop 2 not found\n");
                            }

                        }
                        else{
                            printf(">>>ERROR: invalid Frame - too short\n");
                        }
                        
                        
                        
                    }
                    break;
                default:
                    printf("Event Type %d\n", event.type);
                    break;
            }
        }

    }

    free(datap);
    datap = NULL;
    vTaskDelete(NULL);

}


