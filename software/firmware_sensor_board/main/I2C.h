#ifndef I2C_H
#define I2C_H

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define I2C_MASTER_TIMEOUT_MS 1000

#define C_1 0
#define C_2 1
#define C_3 2
#define C_4 3
#define C_5 4
#define C_6 5

#define Q_1 15
#define Q_2 17
#define Q_3 7
#define Q_4 5
#define Q_5 7
#define Q_6 21

extern TaskHandle_t IMS330DHCX_Handle;  // Declare as extern
extern TaskHandle_t MS5525DSO_Handle;

extern i2c_master_dev_handle_t ism330_handle;

extern QueueHandle_t system_velocity_ias_kmh; 
extern QueueHandle_t system_acceleration_norm_g;
extern SemaphoreHandle_t init_semaphor;

typedef struct {

    // raw sensor data
    int16_t acc_x_raw, acc_y_raw, acc_z_raw;

    // for representation in g
    float acc_abs, acc_norm, acc_x, acc_y, acc_z;

}acc_data_t;

typedef struct {

    // raw sensor data
    int16_t gyro_x_raw, gyro_y_raw, gyro_z_raw;

    //for representatoin in dps
    float gyro_x, gyro_y, gyro_z;

}gyro_data_t;

typedef struct {

    uint16_t coefficient[6];

}ms5525_coefficient_list_t;

typedef struct {

    int32_t temp;
    int32_t p_raw;
    float p_psi;
    float p_pa;
}p_data_t;

extern acc_data_t orientation_reference;

// Function for initializing I2C bus
void i2c_master_init_bus(i2c_master_bus_handle_t *bus_handle);

// Function for initializing I2C handle
void i2c_master_init_handle(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t *dev_handle, uint8_t address);

void i2c_write_ism330(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t reg_val);
uint8_t i2c_read_ism330(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr);

void ism330_get_acc_data(i2c_master_dev_handle_t dev_handle, acc_data_t* acc_data);
void ism330_set_orientation_reference(i2c_master_dev_handle_t dev_handle, acc_data_t* acc_ref_data);
void ism330_get_normal_acc_data(i2c_master_dev_handle_t dev_handle, acc_data_t* acc_data);
void ism330_get_gyro_data(i2c_master_dev_handle_t dev_handle, gyro_data_t* gyro_data);

// ISM330DHCX Task function
void ism330dhcx_task(void *arg);


void i2c_master_init_bus_2(i2c_master_bus_handle_t *bus_2_handler);
void i2c_master_init_handle_2(i2c_master_bus_handle_t bus_2_handler, i2c_master_dev_handle_t *dev_handle, uint8_t address);
void ms5525dso_task(void * args);


#endif