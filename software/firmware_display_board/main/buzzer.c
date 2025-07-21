/* Based on IDF generic_gpio example
*
*/

#include "buzzer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#include "UART.h"

#define GPIO_BUZZER_EN 12
#define GPIO_PIN_SEL ((1ULL << GPIO_BUZZER_EN))
#define GPIO_QUERY_BUTTON 13
#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_QUERY_BUTTON))

QueueHandle_t alert_queue = NULL;
QueueHandle_t gpio_evt_queue = NULL;

volatile bool interrupt_flag = false;


TaskHandle_t BUZZER_HANDLE = NULL;

void init_gpio(){
    gpio_config_t io_config = {};

    io_config.intr_type = GPIO_INTR_DISABLE;
    io_config.mode = GPIO_MODE_OUTPUT;
    io_config.pin_bit_mask = GPIO_PIN_SEL;
    io_config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_config.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_config);

    io_config.intr_type = GPIO_INTR_ANYEDGE;
    io_config.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_config.mode = GPIO_MODE_INPUT;
    io_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_config.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_config);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    xTaskCreate(button_task, "Run Query Button", 2048, NULL, 10, NULL);

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(GPIO_QUERY_BUTTON, gpio_isr_handler, (void*)GPIO_QUERY_BUTTON);

}

void buzzer_alert_task(void* args){

    bool on_off = false;
    TickType_t xLWT = xTaskGetTickCount();

    while(1){
        xQueueReceive(alert_queue, &on_off, 1/portTICK_PERIOD_MS);

        if(on_off){
            gpio_set_level(GPIO_BUZZER_EN, 1);
            xTaskDelayUntil(&xLWT, pdMS_TO_TICKS(500));
            gpio_set_level(GPIO_BUZZER_EN, 0);
            xTaskDelayUntil(&xLWT, pdMS_TO_TICKS(500));
        }

        xTaskDelayUntil(&xLWT, pdMS_TO_TICKS(100));

    }
}

static void IRAM_ATTR gpio_isr_handler(void* arg){
    if(interrupt_flag == false){
        interrupt_flag = true;
        uint32_t gpio_num = (uint32_t) arg;
        // xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
    }   
}
 
void setup_button(){
    bool btn_setup = false;
    xQueueOverwrite(button_queue, &btn_setup);
}

static void button_task(void * args){

    uint32_t gpio_num;

    setup_button();

    while(1){
        if(xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY)){
            gpio_isr_handler_remove(GPIO_QUERY_BUTTON);
            vTaskDelay(150/portTICK_PERIOD_MS);
            printf("GPIO: %ld intr, value: %d\n", gpio_num, gpio_get_level(gpio_num));
            if(gpio_num == GPIO_QUERY_BUTTON){
                int level = gpio_get_level(gpio_num);
                vTaskDelay(10/portTICK_PERIOD_MS);
                if(level == 1){
                    bool on = true;
                    xQueueOverwrite(button_queue, &on);
                }
                else{
                    bool off = false;
                    xQueueOverwrite(button_queue, &off);
                }
            }
            gpio_isr_handler_add(GPIO_QUERY_BUTTON, gpio_isr_handler, (void*) GPIO_QUERY_BUTTON);
            interrupt_flag = false;
        }
    }

}


void buzzer_task(void* args){

    init_gpio();

    alert_queue = xQueueCreate(10, sizeof(bool));

    // bool on_off = false;

    xTaskCreate(buzzer_alert_task, "Run Alert", 1024, NULL, 9, NULL);


    TickType_t xLWT = xTaskGetTickCount();
    while(1){

        // xQueueSend(alert_queue, &on_off, portMAX_DELAY);
        // xTaskDelayUntil(&xLWT, pdMS_TO_TICKS(5000));
        // on_off = !on_off;
        xTaskDelayUntil(&xLWT, pdMS_TO_TICKS(100));


    }
    
    vTaskDelete(NULL);

}
