#include "max7318.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "UART.h"

#define MAX7318_ADDR 0x20

#define INPUT_PORT 0x00
#define OUTPUT_PORT 0x02
#define POLARITY_INVERSION 0x04
#define CONFIGURATION 0x06



TaskHandle_t MAX7318AWG_Handle = NULL;

i2c_master_dev_handle_t max7318_handler;


void i2c_master_init_bus(i2c_master_bus_handle_t *bus_1_handler) {
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_17,
        .scl_io_num = GPIO_NUM_18,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = false} 
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_1_handler));
}

void i2c_master_init_handle(i2c_master_bus_handle_t bus_1_handler, i2c_master_dev_handle_t *dev_handle, uint8_t address) {
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 100000,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_1_handler, &dev_config, dev_handle));
}

max7318_reg_data_t i2c_read_max7318(i2c_master_dev_handle_t dev_handler, uint8_t command_byte){

    esp_err_t err;
    uint8_t write_buf[1] = {command_byte};
    max7318_reg_data_t reg_data = {0};
    uint8_t read_buf [2];


    err = i2c_master_transmit_receive(dev_handler, write_buf, sizeof(write_buf), &read_buf[0], sizeof(read_buf), 200);
    if(err == ESP_OK){

        reg_data.port1 = read_buf[0];
        reg_data.port2 = read_buf[1];
        return reg_data;
        
    }
    else{
        ESP_ERROR_CHECK(err);
        printf(">>>ERROR: ISM330 could not read register; code: %d\n", err);
        return reg_data;
    }
}

void i2c_write_max7318(i2c_master_dev_handle_t dev_handler,uint8_t cmd_byte ,max7318_reg_data_t reg_data){

    uint8_t write_buffer[3] = {cmd_byte, reg_data.port1, reg_data.port2};
    
    esp_err_t err = i2c_master_transmit(dev_handler, &write_buffer[0], 3, 200);

    if(err != ESP_OK){
        printf(">>>ERROR: MAX7318 write failed; code: err\n");
        
    }
    else{
        // printf("Written successfully\n");
    }

    return;

}

void i2c_set_led_max7318(i2c_master_dev_handle_t dev_handler, uint16_t led_ref, uint8_t on_off){
    max7318_reg_data_t reg_data = i2c_read_max7318(dev_handler, OUTPUT_PORT);

    uint16_t old_reg = (reg_data.port2 << 8) | reg_data.port1;
    uint16_t new_reg;

    if(on_off >= LED_ON){
        new_reg = old_reg | (led_ref);
    }
    else if(on_off == LED_OFF){
        new_reg = old_reg & ~(led_ref);
    }

    reg_data.port2 = (new_reg>>8)&0x00FF;
    reg_data.port1 = new_reg&0x00FF;

    i2c_write_max7318(dev_handler, OUTPUT_PORT, reg_data);

    return;


}

void i2c_set_led_strip_max7318(i2c_master_dev_handle_t dev_handler, uint8_t percentage){

    max7318_reg_data_t reg_data = i2c_read_max7318(dev_handler, OUTPUT_PORT);
    reg_data.port1 = 0;
    reg_data.port2 &= (0x1C); // Bit mask filter for white LEDs

    i2c_write_max7318(dev_handler, OUTPUT_PORT, reg_data);

    if(percentage > 100) percentage = 100;

    uint16_t led_config = 0;

    switch (percentage /= 10){
    case 0:
        i2c_set_led_max7318(dev_handler, LED_G_1, LED_OFF);
        break;

    case 1:
        i2c_set_led_max7318(dev_handler, LED_G_1, LED_ON);
        break;

    case 2:
        led_config = LED_G_1 | LED_G_2;
        i2c_set_led_max7318(dev_handler, led_config, LED_ON);
        break;

    case 3:
        led_config = LED_G_1 | LED_G_2 | LED_G_3;
        i2c_set_led_max7318(dev_handler, led_config, LED_ON);
        break;

    case 4:
        led_config = LED_G_1 | LED_G_2 | LED_G_3 | LED_O_1;
        i2c_set_led_max7318(dev_handler, led_config, LED_ON);
        break;

    case 5:
        led_config = LED_G_1 | LED_G_2 | LED_G_3 | LED_O_1 | LED_O_2;
        i2c_set_led_max7318(dev_handler, led_config, LED_ON);
        break;

    case 6:
        led_config = LED_G_1 | LED_G_2 | LED_G_3 | LED_O_1 | LED_O_2 | LED_O_3;
        i2c_set_led_max7318(dev_handler, led_config, LED_ON);
        break;

    case 7:
        led_config = LED_G_1 | LED_G_2 | LED_G_3 | LED_O_1 | LED_O_2 | LED_O_3 | LED_O_4;
        i2c_set_led_max7318(dev_handler, led_config, LED_ON);
        break;

    case 8:
        led_config = LED_G_1 | LED_G_2 | LED_G_3 | LED_O_1 | LED_O_2 | LED_O_3 | LED_O_4 | LED_O_5;
        i2c_set_led_max7318(dev_handler, led_config, LED_ON);
        break;

    case 9:
        led_config = LED_G_1 | LED_G_2 | LED_G_3 | LED_O_1 | LED_O_2 | LED_O_3 | LED_O_4 | LED_O_5 | LED_O_6;
        i2c_set_led_max7318(dev_handler, led_config, LED_ON);
        break;

    case 10:
        led_config = LED_G_1 | LED_G_2 | LED_G_3 | LED_O_1 | LED_O_2 | LED_O_3 | LED_O_4 | LED_O_5 | LED_O_6 | LED_R_1;
        i2c_set_led_max7318(dev_handler, led_config, LED_ON);
        break;
    
    default:
        led_config = LED_G_1 | LED_O_1 | LED_R_1;
        i2c_set_led_max7318(dev_handler, led_config, LED_ON);
        break;
    }

    return;

}

void max7318awg_task(void * args){
    i2c_master_bus_handle_t bus_1_handler = (i2c_master_bus_handle_t) args;
    // i2c_master_dev_handle_t max7318_handler;

    i2c_master_init_handle(bus_1_handler, &max7318_handler, MAX7318_ADDR);
    

    esp_err_t err = i2c_master_probe(bus_1_handler, MAX7318_ADDR, 1000);
    
    if (err == ESP_OK) {
        // if(i2c_read_ism330(ism330_handle, 0x0F) == 0x6B){
            printf("MAX7318AWG+ available \n");
        // }
        // else{
        //     //TODO
        // }
        
    }
    else{
        printf(">>>ERROR: MAX7318AWG+ not found on I2C\n");
    }

    max7318_reg_data_t config_data = {0x00, 0x00};
    i2c_write_max7318(max7318_handler, CONFIGURATION, config_data);
    i2c_write_max7318(max7318_handler, OUTPUT_PORT, config_data);
    config_data.port1 = 0;
    config_data.port2 = 0;
    i2c_write_max7318(max7318_handler, OUTPUT_PORT, config_data);

    uint8_t load_val = 0;

    TickType_t xLWT = xTaskGetTickCount();
    while (1){

        // i2c_set_led_max7318(max7318_handler, LED_W_1, LED_ON);
        // i2c_set_led_strip_max7318(max7318_handler, 0);

        // xTaskDelayUntil(&xLWT, pdMS_TO_TICKS(333));

        // i2c_set_led_max7318(max7318_handler, LED_W_1, LED_OFF);
        // i2c_set_led_max7318(max7318_handler, LED_W_2, LED_ON);
        // i2c_set_led_strip_max7318(max7318_handler, 50);

        // xTaskDelayUntil(&xLWT, pdMS_TO_TICKS(333));

        // i2c_set_led_max7318(max7318_handler, LED_W_2, LED_OFF);
        // i2c_set_led_max7318(max7318_handler, LED_W_3, LED_ON);
        // i2c_set_led_strip_max7318(max7318_handler, 100);

        // xTaskDelayUntil(&xLWT, pdMS_TO_TICKS(334));

        // i2c_set_led_max7318(max7318_handler, LED_W_3, LED_OFF);



        // for (int i = 0; i < 11; i++){

        //     i2c_set_led_strip_max7318(max7318_handler, i*10);
        //     xTaskDelayUntil(&xLWT, pdMS_TO_TICKS(100));
        // }
        
        // xQueuePeek(load_queue, &load_val, portMAX_DELAY);
        // i2c_set_led_strip_max7318(max7318_handler, load_val);
        xTaskDelayUntil(&xLWT, pdTICKS_TO_MS(10));

        
       
    }
    
    vTaskDelete(NULL);
}