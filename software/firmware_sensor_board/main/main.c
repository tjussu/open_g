/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 * 
 * ESP-IDF Version 5.4.2
 * 
 * OpenG Project Sensor-Board Test-Code
 * Date: 07.07.2025
 * 
 * TODO: Implement Button (short and long press. Calibration)
 * 
 */

 
#include "I2C.h"
#include "sd_card.h"
#include "UART.h"
#include "MQTT.h"

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"




void lwd_task(void * args){

    vTaskDelay(pdMS_TO_TICKS(500));
    xSemaphoreTake(init_semaphor, portMAX_DELAY);

    vSemaphoreDelete(init_semaphor);

    uint8_t load_percent = 0;

    float m_p = (module_config.g_v_ne_p - module_config.g_max_p)/(module_config.v_ne-module_config.v_g_max);
    float m_n = (module_config.g_v_ne_n - module_config.g_max_n)/(module_config.v_ne-module_config.v_g_max);
    float b_p = (module_config.g_max_p)-(m_p*module_config.v_g_max);
    float b_n = (module_config.g_max_n)-(m_n*module_config.v_g_max);

    TickType_t xLWT = xTaskGetTickCount();
    while(1){

        float velocity = 0;
        xQueueReceive(system_velocity_ias_kmh, &velocity, portMAX_DELAY);
        float acc = 0;
        xQueueReceive(system_acceleration_norm_g, &acc, portMAX_DELAY);

        if(s_wifi_event_group != NULL){
            if(((uint8_t) xEventGroupGetBits(s_wifi_event_group) & MQTT_CONNECTED_BIT) == MQTT_CONNECTED_BIT){

                char velocity_str[10];
                snprintf(velocity_str, 10, "%.0f", velocity);
                publish_data("/speed", velocity_str, 2, 0);
                char acc_str[10];
                snprintf(acc_str, 10, "%.2f", acc);
                publish_data("/acc", acc_str, 2, 0);

            }
        }
        

        // printf("acc= %+0.2f \t v= %.0f\n", acc, velocity);
        load_percent = 0;

        if(acc >= 0){
            load_percent = (uint8_t) (acc / (m_p*velocity+b_p) * 100);
        }
        else{
            load_percent = (uint8_t) abs(acc / (m_n*velocity+b_n) * 100);
        }

        if(load_percent > load_watchdog_values.load_percentage){
            lwd_t values = {0};
            values.load_percentage = load_percent;
            values.g_absolute = acc;
            values.v_absolute = velocity;

            update_lwd_values(values);
        }

        if(load_percent >= 100){
            bool alarm = true;
            xQueueOverwrite(alarm_queue, &alarm);
        }

        // printf("Load: %d\n", load_percent);

        xQueueSend(load_queue, &load_percent, portMAX_DELAY);

        vTaskDelayUntil(&xLWT, pdMS_TO_TICKS(10));

    }

    vTaskDelete(NULL);
}


void app_main(void)
{
    init_semaphor = xSemaphoreCreateBinary();
    xSemaphoreGive(init_semaphor);

    load_queue = xQueueCreate(1, sizeof(uint8_t));
    alarm_queue = xQueueCreate(1, sizeof(bool));
    bool alarm_q = false;
    bool btn_q = false;
    xQueueOverwrite(alarm_queue, &alarm_q);
    button_queue = xQueueCreate(1, sizeof(bool));
    xQueueOverwrite(button_queue, &btn_q);

    fLED_event_group = xEventGroupCreate();

    xTaskCreate(sd_card_task, "Run SD", 1024*32, NULL, 10, NULL);

    i2c_master_bus_handle_t bus_handle;
    i2c_master_init_bus(&bus_handle);

    xTaskCreate(ism330dhcx_task, "Run ISM330DHCX", 4096, (void*) bus_handle, 10, &IMS330DHCX_Handle);

    i2c_master_bus_handle_t bus_2_handler;
    i2c_master_init_bus_2(&bus_2_handler);

    xTaskCreate(ms5525dso_task, "Run MS5525DSO", 4096, (void*) bus_2_handler, 10, &MS5525DSO_Handle);


    xTaskCreate(lwd_task, "Rund LWD", 4096, NULL, 8, NULL);

    xTaskCreate(uart_event_task, "RUN UART", 2048, NULL, 11, NULL);
  
    

}
