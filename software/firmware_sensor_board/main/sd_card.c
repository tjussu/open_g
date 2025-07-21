/* Based on IDF Example sd_card/sdspi
*
*/


#include "sd_card.h"
#include <stdio.h>
#include <string.h>
#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <cJSON.h>

#define MOUNT_POINT "/sdcard"

#define MAX_CHAR_SIZE 1024




config_t module_config = {0};
lwd_t load_watchdog_values = {0};


/*
*   Config JSON (config.txt):

    {
        "config": {
            "g_max_p": 9.0,
            "g_max_n": -5.0,
            "v_g_max": 180,
            "v_ne": 250,
            "g_v_ne_p": 5.0,
            "g_v_ne_n": -4.0
        }
    }


    Maximum Event JSON (maximum_event.txt):

    {
        "max_event": {
            "load_percentage": 0.0,
            "g_absolute": 0.0,
            "v_absoulute": 0.0 
        }
    }
*   
*/



char * create_test_config_json_string(){

    cJSON *config_json = cJSON_CreateObject();
    cJSON *config = cJSON_CreateObject();
    cJSON *g_max_p = cJSON_CreateNumber(9.0);
    cJSON *g_max_n = cJSON_CreateNumber(-5.0);
    cJSON *v_g_max = cJSON_CreateNumber(180);
    cJSON *v_ne = cJSON_CreateNumber(250);
    cJSON *g_v_ne_p = cJSON_CreateNumber(5.0);
    cJSON *g_v_ne_n = cJSON_CreateNumber(-4.0);

    cJSON_AddItemToObject(config_json, "config", config);
    cJSON_AddItemToObject(config, "g_max_p", g_max_p);
    cJSON_AddItemToObject(config, "g_max_n", g_max_n);
    cJSON_AddItemToObject(config, "v_g_max", v_g_max);
    cJSON_AddItemToObject(config, "v_ne", v_ne);
    cJSON_AddItemToObject(config, "g_v_ne_p", g_v_ne_p);
    cJSON_AddItemToObject(config, "g_v_ne_n", g_v_ne_n);

    // printf("%s\n", cJSON_Print(config_json));

    return cJSON_PrintUnformatted(config_json);

}

char * create_test_load_watchdog_value_json_string(){
    cJSON *lwd_json = cJSON_CreateObject();
    cJSON * max_event = cJSON_CreateObject();
    cJSON * load_percentage = cJSON_CreateNumber(0.0);
    cJSON * g_absolute = cJSON_CreateNumber(0.0);
    cJSON * v_absolute = cJSON_CreateNumber(0.0);

    cJSON_AddItemToObject(lwd_json, "max_event", max_event);
    cJSON_AddItemToObject(max_event, "load_percentage", load_percentage);
    cJSON_AddItemToObject(max_event, "g_absolute", g_absolute);
    cJSON_AddItemToObject(max_event, "v_absolute", v_absolute);

    return cJSON_PrintUnformatted(lwd_json);
}

char * module_config_to_string(){
    
    cJSON *config_json = cJSON_CreateObject();
    cJSON *config = cJSON_CreateObject();
    cJSON *g_max_p = cJSON_CreateNumber(module_config.g_max_p);
    cJSON *g_max_n = cJSON_CreateNumber(module_config.g_max_n);
    cJSON *v_g_max = cJSON_CreateNumber(module_config.v_g_max);
    cJSON *v_ne = cJSON_CreateNumber(module_config.v_ne);
    cJSON *g_v_ne_p = cJSON_CreateNumber(module_config.g_v_ne_p);
    cJSON *g_v_ne_n = cJSON_CreateNumber(module_config.g_v_ne_n);

    cJSON_AddItemToObject(config_json, "config", config);
    cJSON_AddItemToObject(config, "g_max_p", g_max_p);
    cJSON_AddItemToObject(config, "g_max_n", g_max_n);
    cJSON_AddItemToObject(config, "v_g_max", v_g_max);
    cJSON_AddItemToObject(config, "v_ne", v_ne);
    cJSON_AddItemToObject(config, "g_v_ne_p", g_v_ne_p);
    cJSON_AddItemToObject(config, "g_v_ne_n", g_v_ne_n);

    // printf("%s\n", cJSON_Print(config_json));

    return cJSON_PrintUnformatted(config_json);
}

esp_err_t save_current_module_config(){
    esp_err_t err = ESP_OK;

    const char * config_file = MOUNT_POINT"/config.txt";

    // char config_temp[MAX_CHAR_SIZE];
    // strcpy(config_temp, module_config_to_string());
    // printf("Temp Config: %s\n", config_temp);
    err = s_write_file(config_file, module_config_to_string());
    if(err != ESP_OK){
        printf(">>>ERROR: Config Save Failed\n");
        return err;
    }

    return err;

}

char * load_watchdog_values_to_string(){

    cJSON *lwd_json = cJSON_CreateObject();
    cJSON * max_event = cJSON_CreateObject();
    cJSON * load_percentage = cJSON_CreateNumber(load_watchdog_values.load_percentage);
    cJSON * g_absolute = cJSON_CreateNumber(load_watchdog_values.g_absolute);
    cJSON * v_absolute = cJSON_CreateNumber(load_watchdog_values.v_absolute);

    cJSON_AddItemToObject(lwd_json, "max_event", max_event);
    cJSON_AddItemToObject(max_event, "load_percentage", load_percentage);
    cJSON_AddItemToObject(max_event, "g_absolute", g_absolute);
    cJSON_AddItemToObject(max_event, "v_absolute", v_absolute);

    return cJSON_PrintUnformatted(lwd_json); 
}

void update_lwd_values(lwd_t values){
    esp_err_t err = ESP_OK;

    load_watchdog_values.load_percentage = values.load_percentage;
    load_watchdog_values.g_absolute = values.g_absolute;
    load_watchdog_values.v_absolute = values.v_absolute;

    cJSON *lwd_json = cJSON_CreateObject();
    cJSON * max_event = cJSON_CreateObject();
    cJSON * load_percentage = cJSON_CreateNumber(values.load_percentage);
    cJSON * g_absolute = cJSON_CreateNumber(values.g_absolute);
    cJSON * v_absolute = cJSON_CreateNumber(values.v_absolute);

    cJSON_AddItemToObject(lwd_json, "max_event", max_event);
    cJSON_AddItemToObject(max_event, "load_percentage", load_percentage);
    cJSON_AddItemToObject(max_event, "g_absolute", g_absolute);
    cJSON_AddItemToObject(max_event, "v_absolute", v_absolute);

    const char * lwd_values_file = MOUNT_POINT"/max_values.txt";
    remove(MOUNT_POINT"/MaxVal.txt");
    char lwd_temp[MAX_CHAR_SIZE];
    strcpy(lwd_temp, cJSON_PrintUnformatted(lwd_json));
    printf("Temp LWD: %s\n", lwd_temp);
    err = s_write_file(lwd_values_file, lwd_temp);
     if(err != ESP_OK){
        printf("LWD Write Error\n");
        vTaskDelete(NULL);
    }
}

void set_config(char * config_string){

    //Parsing JSON from String
    cJSON * config_json = cJSON_Parse(config_string);
    if(config_json == NULL){
        const char * err_ptr = cJSON_GetErrorPtr();
        if(err_ptr != NULL){
            printf(">>>ERROR: Parsing Config from JSON: %s\n", err_ptr);
            return;
        }
    }
    cJSON * config = cJSON_GetObjectItemCaseSensitive(config_json, "config");
    if(!cJSON_IsObject(config)){ printf(">>>ERROR: Config invalid [1]\n"); return;}

    cJSON * g_max_p = cJSON_GetObjectItemCaseSensitive(config, "g_max_p");
    if(!cJSON_IsNumber(g_max_p)) {printf(">>>ERROR: Config invalid [2]\n"); return;}
    
    cJSON * g_max_n = cJSON_GetObjectItemCaseSensitive(config, "g_max_n");
    if(!cJSON_IsNumber(g_max_n)) {printf(">>>ERROR: Config invalid [3]\n"); return;}
    
    cJSON *v_g_max = cJSON_GetObjectItemCaseSensitive(config, "v_g_max");
    if(!cJSON_IsNumber(v_g_max)) {printf(">>>ERROR: Config invalid [4]\n"); return;}
    
    cJSON *v_ne = cJSON_GetObjectItemCaseSensitive(config, "v_ne");
    if(!cJSON_IsNumber(v_ne)) {printf(">>>ERROR: Config invalid [5]\n"); return;}

    cJSON *g_v_ne_p = cJSON_GetObjectItemCaseSensitive(config, "g_v_ne_p");
    if(!cJSON_IsNumber(g_v_ne_p)) {printf(">>>ERROR: Config invalid [6]\n"); return;}

    cJSON *g_v_ne_n = cJSON_GetObjectItemCaseSensitive(config, "g_v_ne_n");
    if(!cJSON_IsNumber(g_v_ne_n)) {printf(">>>ERROR: Config invalid [7]\n"); return;}

    // Load Values from JSON into opration structure
    module_config.g_max_p = (float) g_max_p->valuedouble;
    module_config.g_max_n = (float) g_max_n->valuedouble;
    module_config.v_g_max = (float) v_g_max->valuedouble;
    module_config.v_ne = (float) v_ne->valuedouble;
    module_config.g_v_ne_p = (float) g_v_ne_p->valuedouble;
    module_config.g_v_ne_n = (float) g_v_ne_n->valuedouble;

    return;
}

void set_load_watchdog_values(char * lwd_value_string){

    // Parsing JSON from String
    cJSON * lwd_json = cJSON_Parse(lwd_value_string);
    if(lwd_json == NULL){
        const char * err_ptr = cJSON_GetErrorPtr();
        if(err_ptr != NULL){
            printf(">>>ERROR: Parsing LWD Values from JSON: %s\n", err_ptr);
            return;
        }
    }
    cJSON * max_event = cJSON_GetObjectItemCaseSensitive(lwd_json, "max_event");
    if(!cJSON_IsObject(max_event)){ printf(">>>ERROR: LWD invalid [1]\n"); return;}

    cJSON * load_percentage = cJSON_GetObjectItemCaseSensitive(max_event, "load_percentage");
    if(!cJSON_IsNumber(load_percentage)){ printf(">>>ERROR: LWD invalid [2]\n"); return;}

    cJSON * g_absolute = cJSON_GetObjectItemCaseSensitive(max_event, "g_absolute");
    if(!cJSON_IsNumber(g_absolute)){ printf(">>>ERROR: LWD invalid [3]\n"); return;}

    cJSON * v_absolute = cJSON_GetObjectItemCaseSensitive(max_event, "v_absolute");
    if(!cJSON_IsNumber(v_absolute)){ printf(">>>ERROR: LWD invalid [4]\n"); return;}

    // Loading values from JSON into operation structure
    load_watchdog_values.load_percentage = (float) load_percentage->valuedouble;
    load_watchdog_values.g_absolute = (float) g_absolute->valuedouble;
    load_watchdog_values.v_absolute = (float) v_absolute->valuedouble;

    return;
}

esp_err_t set_load_watchdog_values_to_zero(){

    esp_err_t err = ESP_OK;

    cJSON *lwd_json = cJSON_CreateObject();
    cJSON * max_event = cJSON_CreateObject();
    cJSON * load_percentage = cJSON_CreateNumber(0.0);
    cJSON * g_absolute = cJSON_CreateNumber(0.0);
    cJSON * v_absolute = cJSON_CreateNumber(0.0);

    cJSON_AddItemToObject(lwd_json, "max_event", max_event);
    cJSON_AddItemToObject(max_event, "load_percentage", load_percentage);
    cJSON_AddItemToObject(max_event, "g_absolute", g_absolute);
    cJSON_AddItemToObject(max_event, "v_absolute", v_absolute);

    const char * lwd_values_file = MOUNT_POINT"/max_values.txt";

    char lwd_temp[MAX_CHAR_SIZE];
    strcpy(lwd_temp, cJSON_PrintUnformatted(lwd_json));
    // printf("Temp LWD: %s\n", lwd_temp);
    err = s_write_file(lwd_values_file, lwd_temp);
     if(err != ESP_OK){
        printf("LWD Set Zero Error\n");
        return err;
    }
    printf("LWD values set to zero\n");

    return err;
    
}



static esp_err_t s_write_file(const char *path, char *data){
    printf("Opening file %s\n", path);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        printf("Failed to open file for writing\n");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    printf("File written\n");

    return ESP_OK;
}


static esp_err_t s_read_file(const char *path, char* return_string){
    printf("Reading file %s\n", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        printf("Failed to open file for reading\n");
        return ESP_FAIL;
    }
    // char line[MAX_CHAR_SIZE];
    fgets(return_string, MAX_CHAR_SIZE, f);
    int err = fclose(f);
    if(err != 0){
        printf(">>>ERROR: File could not be closed.\n");
    }
    else{
        printf("Reading succsessfull\n");
    }
    

    // strip newline
    // char *pos = strchr(return_string, '\n');
    // if (pos) {
    //     *pos = '\0';
    // }
    // printf("Read from file: '%s'\n", return_string);

    return ESP_OK;
}



void sd_card_task(void* args){

    esp_err_t err;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t * card;
    const char mount_point[] = MOUNT_POINT;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = GPIO_NUM_13,
        .miso_io_num = GPIO_NUM_14,
        .sclk_io_num = GPIO_NUM_15,
        .quadhd_io_num = -1,
        .quadwp_io_num = -1,
        .max_transfer_sz = 4000,

    };

    err = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if(err != ESP_OK){
        printf(">>>ERROR: SPI bus init failed\n");
        vTaskDelete(NULL);
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = GPIO_NUM_12;
    slot_config.host_id = host.slot;

    err = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if(err != ESP_OK){
        if (err == ESP_FAIL) {
            printf("Failed to mount filesystem. If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.\n");
        } else {
            printf("Failed to initialize the card (%s). Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(err));
        }
        vTaskDelete(NULL);
    }

    sdmmc_card_print_info(stdout, card);

    // const char * file_test = MOUNT_POINT"/test.txt";

    // char test_string[MAX_CHAR_SIZE];

    // err = s_read_file(file_test, &test_string[0]);
    // if(err != ESP_OK){
    //     printf("Read error\n");
    //     return;
    // }

    // char data[MAX_CHAR_SIZE];
    // snprintf(data, MAX_CHAR_SIZE, "%s\n", "Hello 123456XY");
    // err = s_write_file(file_test, data);
    // if(err != ESP_OK){
    //     printf("Write error\n");
    //     return;
    // }

    const char * config_file = MOUNT_POINT"/config.txt";
    // char config_temp[MAX_CHAR_SIZE];
    // strcpy(config_temp, write_config());
    // printf("Temp Config: %s\n", config_temp);
    // err = s_write_file(config_file, write_config());
    // if(err != ESP_OK){
    //     printf("Config Write Error\n");
    //     return;
    // }

    char config_string[MAX_CHAR_SIZE];
    err = s_read_file(config_file, config_string);
    if(err != ESP_OK){
        printf("Config Read error\n");
        vTaskDelete(NULL);
    }
    printf("Config: %s\n", config_string);

    set_config(config_string);


    // printf("v_ne: %.0f\n", module_config.v_ne);


    const char * lwd_values_file = MOUNT_POINT"/max_values.txt";
    // remove(MOUNT_POINT"/MaxVal.txt");
    // char lwd_temp[MAX_CHAR_SIZE];
    // strcpy(lwd_temp, create_test_load_watchdog_value_json_string());
    // printf("Temp LWD: %s\n", lwd_temp);
    // err = s_write_file(lwd_values_file, lwd_temp);
    //  if(err != ESP_OK){
    //     printf("LWD Write Error\n");
    //     vTaskDelete(NULL);
    // }

    char lwd_values_string[MAX_CHAR_SIZE];
    err = s_read_file(lwd_values_file, lwd_values_string);
    if(err != ESP_OK){
        printf("LWD Read error\n");
        vTaskDelete(NULL);
    }
    printf("LWD Max Vlaues: %s\n", lwd_values_string);

    set_load_watchdog_values(lwd_values_string);

    // printf("max load: %.0f %%", load_watchdog_values.load_percentage);

    
    vTaskDelete(NULL);

}