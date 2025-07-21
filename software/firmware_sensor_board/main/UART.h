#ifndef UART_H
#define UART_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

extern QueueHandle_t load_queue;
extern QueueHandle_t alarm_queue;
extern QueueHandle_t button_queue;

extern EventGroupHandle_t fLED_event_group;

void uart_event_task(void * args);



#endif