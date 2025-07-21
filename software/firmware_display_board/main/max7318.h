#ifndef MAX7318_H
#define MAX7318_h

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define LED_ON 1
#define LED_OFF 0


// Port1 and Port2 as 16 bit: ---W WWRO OOOO OGGG
//                            \_______/ \_______/
//                              PORT2     PORT1

#define LED_W_1 (1 << 10) //port 2 bit 2
#define LED_W_2 (1 << 11) //port 2 bit 3
#define LED_W_3 (1 << 12) //port 2 bit 4

#define LED_G_1 (1 << 0) //port 1 bit 0
#define LED_G_2 (1 << 1) //port 1 bit 1
#define LED_G_3 (1 << 2) //port 1 bit 2
#define LED_O_1 (1 << 3) //port 1 bit 3
#define LED_O_2 (1 << 4) //port 1 bit 4
#define LED_O_3 (1 << 5) //port 1 bit 5
#define LED_O_4 (1 << 6) //port 1 bit 6
#define LED_O_5 (1 << 7) //port 1 bit 7
#define LED_O_6 (1 << 8) //port 2 bit 0
#define LED_R_1 (1 << 9) //port 2 bit 1

extern TaskHandle_t MAX7318AWG_Handle;

extern i2c_master_dev_handle_t max7318_handler;

typedef struct{
    uint8_t port1;
    uint8_t port2;
}max7318_reg_data_t;

void i2c_master_init_bus(i2c_master_bus_handle_t *bus_1_handler);
void i2c_master_init_handle(i2c_master_bus_handle_t bus_1_handler, i2c_master_dev_handle_t *dev_handle, uint8_t address);

void i2c_set_led_max7318(i2c_master_dev_handle_t dev_handler, uint16_t led_ref, uint8_t on_off);
void i2c_set_led_strip_max7318(i2c_master_dev_handle_t dev_handler, uint8_t percentage);

void max7318awg_task(void * args);

#endif