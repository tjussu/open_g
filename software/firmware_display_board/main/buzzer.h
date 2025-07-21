#ifndef BUZZER_H
#define BUZZER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern TaskHandle_t BUZZER_HANDLE;
extern QueueHandle_t alert_queue;

static void IRAM_ATTR gpio_isr_handler(void* arg);
static void button_task(void * args);
void buzzer_task(void* args);

#endif