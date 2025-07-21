/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 * 
 * ESP-IDF Version 5.4.2
 * 
 * OpenG Project Display-Board Test-Code
 * Date: 07.07.2025
 * 
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "max7318.h"
#include "buzzer.h"
#include "UART.h"

void app_main(void){

    button_queue = xQueueCreate(1, sizeof(bool));
    load_queue = xQueueCreate(1, sizeof(uint8_t));
    alarm_queue = xQueueCreate(1, sizeof(bool));

    i2c_master_bus_handle_t bus_1_handler;
    i2c_master_init_bus(&bus_1_handler);

    xTaskCreate(max7318awg_task, "Run MAX7318AWG+", 4096, (void*) bus_1_handler, 10, &MAX7318AWG_Handle);

    xTaskCreate(buzzer_task, "Run Buzzer", 2048, NULL, 10, &BUZZER_HANDLE);

    xTaskCreate(uart_event_task, "Run UART", 2048, NULL, 9, NULL);

}
