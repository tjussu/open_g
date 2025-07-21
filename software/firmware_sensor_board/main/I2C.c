#include "I2C.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include <math.h>
#include "rom/ets_sys.h"

#define ISM330_ADDR 0x6A
#define MS5525_ADDR 0x77

TaskHandle_t IMS330DHCX_Handle = NULL;  // Define the TaskHandle variable
TaskHandle_t MS5525DSO_Handle = NULL;

i2c_master_dev_handle_t ism330_handle;

acc_data_t orientation_reference = {0};
float orientation_reference_length = 0;

ms5525_coefficient_list_t ms5525_coefficients = {0};
int32_t ms5525_offset = 0.0;
float air_density = 1.225;

QueueHandle_t system_velocity_ias_kmh; 
QueueHandle_t system_acceleration_norm_g;
SemaphoreHandle_t init_semaphor;

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
        .scl_speed_hz = 400000,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_1_handler, &dev_config, dev_handle));
}

void i2c_write_ism330(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t reg_val){

    esp_err_t err;

    uint8_t data[] = {reg_addr, reg_val};

    err = i2c_master_transmit(dev_handle, &data[0], 2, 200);

    if(err != ESP_OK){
        printf(">>>ERROR: ISM Write failed: %d\n", err);
    }
    else if(err == ESP_OK){
        printf("ISM330 written value: 0x%X to register: 0x%X \n", reg_val, reg_addr);
    }

    return;
}

uint8_t i2c_read_ism330(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr){

    esp_err_t err;
    uint8_t write_buf[1] = {reg_addr};
    uint8_t read_buf [1];


    err = i2c_master_transmit_receive(dev_handle, write_buf, sizeof(write_buf), &read_buf[0], sizeof(read_buf), 200);
    if(err == ESP_OK){
        return read_buf[0];
        
    }
    else{
        ESP_ERROR_CHECK(err);
        printf(">>>ERROR: ISM330 could not read: %d\n", err);
        return 0;
    }
}

void ism330_get_acc_data(i2c_master_dev_handle_t dev_handle, acc_data_t* acc_data){
    uint8_t acc_x_l = i2c_read_ism330(dev_handle, 0x28);
    uint8_t acc_x_h = i2c_read_ism330(dev_handle, 0x29);
    acc_data->acc_x_raw = (acc_x_h << 8) ^ (acc_x_l);
    acc_data->acc_x = acc_data->acc_x_raw / (2047.0);

    uint8_t acc_y_l = i2c_read_ism330(dev_handle, 0x2A);
    uint8_t acc_y_h = i2c_read_ism330(dev_handle, 0x2B);
    acc_data->acc_y_raw = (acc_y_h << 8) ^ (acc_y_l);
    acc_data->acc_y = acc_data->acc_y_raw / (2047.0);

    uint8_t acc_z_l = i2c_read_ism330(dev_handle, 0x2C);
    uint8_t acc_z_h = i2c_read_ism330(dev_handle, 0x2D);
    acc_data->acc_z_raw = (acc_z_h << 8) ^ (acc_z_l);
    acc_data->acc_z = acc_data->acc_z_raw / (2047.0);

    acc_data->acc_abs = sqrt(pow(acc_data->acc_x,2) + pow(acc_data->acc_y,2) + pow(acc_data->acc_z,2));

    return;
}

void ism330_get_normal_acc_data(i2c_master_dev_handle_t dev_handle, acc_data_t* acc_data){

    if(orientation_reference_length == 0){
        printf(">>>ERROR: orientation reference not set \n");
        return;
    }

    ism330_get_acc_data(dev_handle, acc_data);
    
    // Calculation of the acceleration component in g-direction. 
    // Using the scalar product the orientation is calculated.

    float scalar = (orientation_reference.acc_x * acc_data->acc_x 
                    + orientation_reference.acc_y * acc_data->acc_y
                    + orientation_reference.acc_z * acc_data->acc_z);

    // using the orientation_reference as noraml vector of the viratual horizon-plane, the normal accalaration can be calculated as the distance of the acceleration vector to the plane:
    acc_data->acc_norm = ((scalar) / (orientation_reference_length));

    return;
}

void ism330_set_orientation_reference(i2c_master_dev_handle_t dev_handle, acc_data_t* acc_ref_data){

    //TODO: Mean value over 100 samples

    ism330_get_acc_data(dev_handle, acc_ref_data);

    orientation_reference_length = sqrt(pow(acc_ref_data->acc_x,2) + pow(acc_ref_data->acc_y,2) + pow(acc_ref_data->acc_z,2));

    printf("Orientation set to: [x = %+0.2f, y = %+0.2f, z = %+0.2f], length: %+0.2f\n", acc_ref_data->acc_x, acc_ref_data->acc_y, acc_ref_data->acc_z, orientation_reference_length);

    return;
}

void ism330_get_gyro_data(i2c_master_dev_handle_t dev_handle, gyro_data_t* gyro_data){
    uint8_t gyro_x_l = i2c_read_ism330(dev_handle, 0x22);
    uint8_t gyro_x_h = i2c_read_ism330(dev_handle, 0x23);
    gyro_data->gyro_x_raw = (gyro_x_h << 8) ^ (gyro_x_l);
    gyro_data->gyro_x = gyro_data->gyro_x_raw / (131.068);

    uint8_t gyro_y_l = i2c_read_ism330(dev_handle, 0x24);
    uint8_t gyro_y_h = i2c_read_ism330(dev_handle, 0x25);
    gyro_data->gyro_y_raw = (gyro_y_h << 8) ^ (gyro_y_l);
    gyro_data->gyro_y = gyro_data->gyro_y_raw / (131.068);

    uint8_t gyro_z_l = i2c_read_ism330(dev_handle, 0x26);
    uint8_t gyro_z_h = i2c_read_ism330(dev_handle, 0x27);
    gyro_data->gyro_z_raw = (gyro_z_h << 8) ^ (gyro_z_l);
    gyro_data->gyro_z = gyro_data->gyro_z_raw / (131.068);

}


void ism330dhcx_task(void *arg) {
    xSemaphoreTake(init_semaphor, portMAX_DELAY);
    i2c_master_bus_handle_t bus_1_handler = (i2c_master_bus_handle_t) arg;

    i2c_master_init_handle(bus_1_handler, &ism330_handle, ISM330_ADDR);

    esp_err_t err = i2c_master_probe(bus_1_handler, ISM330_ADDR, 1000);
    
    if (err == ESP_OK) {
        if(i2c_read_ism330(ism330_handle, 0x0F) == 0x6B){
            printf("ISM330DHCX available \n");
        }
        else{
            //TODO
        }
        
    }
    else{
        printf(">>>ERROR: ISM330DHCX not found on I2C\n");
    }

    // Activate the acceleromenter with ODR=416 Hz and full-scale of 16g
    i2c_write_ism330(ism330_handle, 0x10, 0b01100100);
    // Activate the gyroscope with ODR = 833 Hz and full-scale of 250 dps
    i2c_write_ism330(ism330_handle, 0x11, 0b01110000);

    
    ism330_set_orientation_reference(ism330_handle, &orientation_reference);

    acc_data_t acc_data = {0};

    system_acceleration_norm_g = xQueueCreate(1, sizeof(float));

    xSemaphoreGive(init_semaphor);

    TickType_t xLWT = xTaskGetTickCount();
    while (1) {
        
        ism330_get_normal_acc_data(ism330_handle, &acc_data);

        xQueueSend(system_acceleration_norm_g, &(acc_data.acc_norm), portMAX_DELAY);

        // printf("ACC: acc = %+0.2f [x = %+0.2f, y = %+0.2f, z = %+0.2f]\n", acc_data.acc_norm, acc_data.acc_x, acc_data.acc_y, acc_data.acc_z);
        xTaskDelayUntil(&xLWT, pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}

/*---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
* Code for the MS5525DSO
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

void i2c_master_init_bus_2(i2c_master_bus_handle_t *bus_2_handler){
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = GPIO_NUM_22,
        .scl_io_num = GPIO_NUM_23,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = false} // Corrected initialization
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_2_handler));
}

void i2c_master_init_handle_2(i2c_master_bus_handle_t bus_2_handler, i2c_master_dev_handle_t *dev_handle, uint8_t address) {
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 100000,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_2_handler, &dev_config, dev_handle));
}

ms5525_coefficient_list_t i2c_read_prom_ms5525(i2c_master_dev_handle_t dev_handle){

    ms5525_coefficient_list_t coefficient_list = {0};

    esp_err_t err;
    uint8_t command_byte_mask = 0b10100000;

    uint8_t command_byte = 0;

    uint8_t read_buf [2];

    for (int i = 0; i < 6; i++){

        command_byte = command_byte_mask ^ ((i+1)<<1);
        read_buf[0] = 0;
        read_buf[1] = 0;

        err = i2c_master_transmit(dev_handle, &command_byte, 1, 200);

        if(err != ESP_OK){
            printf(">>>ERROR: read_prom_ms5525 failed: 1, code: %d\n", err);
            return coefficient_list;
        }

        err = i2c_master_receive(dev_handle, &read_buf[0], 2, 200);

        if(err != ESP_OK){
            printf(">>>ERROR: read_prom_ms5525 failed: 2, code: %d\n", err);
            return coefficient_list;
        }

        coefficient_list.coefficient[i] = (read_buf[0]<<8) ^ (read_buf[1]);

        printf("C%d data: %04X\n", (i+1), coefficient_list.coefficient[i]);
    }
    
    return coefficient_list;


    // err = i2c_master_transmit_receive(dev_handle, write_buf, sizeof(write_buf), &read_buf[0], sizeof(read_buf), 200);
    // if(err == ESP_OK){
    //     return read_buf[0];
        
    // }
    // else{
    //     ESP_ERROR_CHECK(err);
    //     printf(">>>ERROR: ISM330 could not read: %d\n", err);
    //     return 0;
    // }

}

p_data_t i2c_read_preassure_ms5525(i2c_master_dev_handle_t dev_handle){

    esp_err_t err;
    p_data_t p_data = {0};
    int measure_delay_us = 3000;
    uint8_t d1_osr = 0x44;
    uint8_t d2_osr = 0x54;
    // TickType_t xLWT = xTaskGetTickCount();

    // Conversion D1 @ 256 Hz OSR
    uint8_t d1_command_byte = d1_osr;
    err = i2c_master_transmit(dev_handle, &d1_command_byte, 1, -1);

    if(err != ESP_OK){
        printf(">>>ERROR: write d1, code: %d\n", err);
        return p_data;
    }

    // Measureing delay
    // xTaskDelayUntil(&xLWT, pdMS_TO_TICKS(5));
    ets_delay_us(measure_delay_us);
    // vTaskDelay(measure_delay/portTICK_PERIOD_MS);

    // Read ADC Command
    uint8_t adc_read_command_byte = 0x00;
    err = i2c_master_transmit(dev_handle, &adc_read_command_byte, 1, 200);

    if(err != ESP_OK){
        printf(">>>ERROR: write adc 1, code: %d\n", err);
        return p_data;
    }

    uint8_t d1_raw[3] = {0};
    err = i2c_master_receive(dev_handle, &d1_raw[0], 3, 200);

    if(err != ESP_OK){
        printf(">>>ERROR: read adc d1, code: %d\n", err);
        return p_data;
    }

    uint32_t d1 = (d1_raw[0]<<16) | (d1_raw[1]<<8) | (d1_raw[2]);

    // printf("P: %08lx, 1: %X, 2: %x, 3: %x\n", d1, d1_raw[0], d1_raw[1], d1_raw[2]);

    // Conversion D2 @ 256 Hz OSR
    uint8_t d2_command_byte = d2_osr;
    i2c_master_transmit(dev_handle, &d2_command_byte, 1, 200);

    // Measureing delay
    // vTaskDelay(measure_delay/portTICK_PERIOD_MS);
    ets_delay_us(measure_delay_us);

    // Read ADC Command
    i2c_master_transmit(dev_handle, &adc_read_command_byte, 1, 200);

    uint8_t d2_raw[3] = {0};
    i2c_master_receive(dev_handle, &d2_raw[0], 3, 200);

    uint32_t d2 = (d2_raw[0]<<16) | (d2_raw[1]<<8) | (d2_raw[2]);

    int32_t dT = d2 - (ms5525_coefficients.coefficient[C_5] * pow(2, Q_5));
    p_data.temp = 2000 + dT*(ms5525_coefficients.coefficient[C_6]/pow(2, Q_6));

    // printf("T: %ld, %ld\n", p_data.temp, dT);

    int64_t off =  ms5525_coefficients.coefficient[C_2] * pow(2, Q_2) + (ms5525_coefficients.coefficient[C_4]*dT)/pow(2, Q_4);
    int64_t sens = ms5525_coefficients.coefficient[C_1] * pow(2, Q_1) + (ms5525_coefficients.coefficient[C_3]*dT)/pow(2, Q_3);
    p_data.p_raw = ((d1*sens/pow(2,21)) - off)/pow(2,15);

    p_data.p_psi = ((p_data.p_raw-ms5525_offset)/ 10000.0);

    p_data.p_pa = p_data.p_psi*6895.0;

    

    return p_data;

}

void reset_ms5525dso(i2c_master_dev_handle_t dev_handle){

    esp_err_t err;

    uint8_t command_byte = 0b00011110;
    err = i2c_master_transmit(dev_handle, &command_byte, 1, 200);

    if(err != ESP_OK){
        printf(">>>ERROR: MS5525 Reset not executed, code: %d\n", err);
        return;
    }

    printf("MS5525 Reset\n");
    return;
}

void init_ms5525dso(i2c_master_dev_handle_t dev_handle){

    // Reset Sensor
    reset_ms5525dso(dev_handle);

    vTaskDelay(100/portTICK_PERIOD_MS);

    // Read Coefficients for Calculations
    ms5525_coefficients = i2c_read_prom_ms5525(dev_handle);

    // Setting Offset and Air Density
    int32_t temp = 0;
    int32_t offset = 0;

    //Meassure offset and temp over 100 samples
    for (int i = 0; i < 100; i++){
        p_data_t p_data =  i2c_read_preassure_ms5525(dev_handle);
        offset += p_data.p_raw;
        temp += p_data.temp;
    }

    while((offset/100) <  -500){
        offset = 0;
        temp = 0;

        for (int i = 0; i < 100; i++){
            p_data_t p_data =  i2c_read_preassure_ms5525(dev_handle);
            offset += p_data.p_raw;
            temp += p_data.temp;
        }
        vTaskDelay(pdMS_TO_TICKS(100));

    }

    // setting offset
    ms5525_offset = offset / 100;
    printf("MS5525 Offset: %ld\n", ms5525_offset);

    //calc air_density
    float mean_temp = temp / 100.0;

    air_density = 101325.0f/(287.1f*(273.15f + (mean_temp/100)));
    printf("Air Density: %0.3f\n", air_density);

    return;

}


void ms5525dso_task(void * args){
    xSemaphoreTake(init_semaphor, portMAX_DELAY);
    i2c_master_bus_handle_t bus_2_handler = (i2c_master_bus_handle_t) args;
    i2c_master_dev_handle_t ms5525_handler;

    i2c_master_init_handle_2(bus_2_handler, &ms5525_handler, MS5525_ADDR);

    esp_err_t err = i2c_master_probe(bus_2_handler, MS5525_ADDR, 1000);
    
    if (err == ESP_OK) {
        // if(i2c_read_ism330(ism330_handle, 0x0F) == 0x6B){
            printf("MS5525DSO available \n");
        // }
        // else{
        //     //TODO
        // }
        
    }
    else{
        printf(">>>ERROR: MS5525DSO not found on I2C\n");
    }

    init_ms5525dso(ms5525_handler);

    system_velocity_ias_kmh = xQueueCreate(1, sizeof(float));

    xSemaphoreGive(init_semaphor);

    TickType_t xLWT = xTaskGetTickCount();
    while(1){

        p_data_t p_data;
        float p_pa_mean = 0;

        for (int i = 0; i < 10; i++){
            p_data = i2c_read_preassure_ms5525(ms5525_handler);
            p_pa_mean += p_data.p_pa/10;
        }

        if(p_pa_mean<0){
            p_pa_mean = 0.0;
        }
        float v_ias_kmh = sqrt(2.0*(p_pa_mean)/air_density)*3.6;

        xQueueSend(system_velocity_ias_kmh, &v_ias_kmh, portMAX_DELAY);
        

        // printf("P: %+0.6f PSI\t T: %+0.2f °C \t v: %+0.2f km/h\n", p_data.p_psi, (float)p_data.temp/100.0f, v_ias_kmh);


        xTaskDelayUntil(&xLWT, pdMS_TO_TICKS(100));
    }
    
    vTaskDelete(NULL);

}