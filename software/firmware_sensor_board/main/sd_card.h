#ifndef SD_CARD_H
#define SD_CARD_H

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
    float g_max_p, g_max_n, v_g_max, v_ne, g_v_ne_p, g_v_ne_n;
}config_t;

typedef struct {
    float load_percentage, g_absolute, v_absolute;
}lwd_t;

extern config_t module_config;
extern lwd_t load_watchdog_values;

char * module_config_to_string();
esp_err_t save_current_module_config();
char * load_watchdog_values_to_string();
void update_lwd_values(lwd_t values);
void set_config(char * config_string);
esp_err_t set_load_watchdog_values_to_zero();
static esp_err_t s_write_file(const char *path, char *data);

void sd_card_task(void * args);


#endif