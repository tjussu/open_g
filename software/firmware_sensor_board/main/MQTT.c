/*
* Based on MQTT TCP and WIFI station examples from ESP-IDF documentation (IDF v5.4.2)
*
*/
#include "sd_card.h"
#include "UART.h"
#include "I2C.h"


#include "mqtt.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"



#define WIFI_SSID "OpenGnet"
#define WIFI_PWD  "openg1234"
#define NUM_RECONNECT_TRYS 10

EventGroupHandle_t s_wifi_event_group;
esp_mqtt_client_handle_t client = NULL;

static int s_retry_num = 0;

void publish_data(const char* subtopic, const char* data, int qos, int retain){

    size_t topic_len = strlen(MQTT_DEVICE_TOPIC) + strlen(subtopic);
    char * topic = (char*) calloc(topic_len+2, sizeof(char));
    strcat(topic, MQTT_DEVICE_TOPIC);
    strcat(topic, subtopic);

    esp_mqtt_client_publish(client, topic, data, 0, qos, retain);
    
    free(topic);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE("MQTT", "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    // printf("Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t e_client = event->client;
    // int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        printf("MQTT_EVENT_CONNECTED\n");
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        // printf("sent publish successful, msg_id=%d\n", msg_id);

        esp_mqtt_client_publish(e_client, MQTT_DEVICE_TOPIC "/con_stat", "1", 0, 2, 1);
        esp_mqtt_client_publish(e_client, MQTT_DEVICE_TOPIC "/config", module_config_to_string(), 0, 2, 1);
        esp_mqtt_client_publish(e_client, MQTT_DEVICE_TOPIC "/max_val", load_watchdog_values_to_string(), 0, 1, 0);
        esp_mqtt_client_subscribe_single(e_client, MQTT_DEVICE_TOPIC "/config", 2);
        esp_mqtt_client_subscribe_single(e_client, MQTT_DEVICE_TOPIC "/calibration", 2);
        esp_mqtt_client_subscribe_single(e_client, MQTT_DEVICE_TOPIC "/reset", 1);

        xEventGroupSetBits(s_wifi_event_group, MQTT_CONNECTED_BIT);

        break;
    case MQTT_EVENT_DISCONNECTED:
        printf("MQTT_EVENT_DISCONNECTED\n");
        xEventGroupClearBits(s_wifi_event_group, MQTT_CONNECTED_BIT);
        esp_mqtt_client_reconnect(client);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        printf("MQTT_EVENT_SUBSCRIBED, msg_id=%d, return code=0x%02x \n", event->msg_id, (uint8_t)*event->data);
        
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        printf("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d\n", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        // printf("MQTT_EVENT_PUBLISHED, msg_id=%d\n", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        printf("MQTT_EVENT_DATA\n");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        char * topic = (char*) calloc(event->topic_len+1, sizeof(char));
        snprintf(topic, event->topic_len+1, "%.*s", event->topic_len, event->topic);
        char * data = (char*) calloc(event->data_len+1, sizeof(char));
        snprintf(data, event->data_len+1, "%.*s", event->data_len, event->data);

        printf(topic);
        printf("\n");

        if(strcmp(topic, MQTT_DEVICE_TOPIC "/config") == 0){

            // Set Config
            printf("Setting Config via MQTT\n");
            set_config(data);
            esp_err_t err = save_current_module_config();
            if(err != ESP_OK){
                printf(">>>ERROR: Failed to save config via MQTT!\n");
            }
            else{
                printf("Set and saved Config successfully\n");
            }

        }
        else if(strcmp(topic, MQTT_DEVICE_TOPIC "/calibration") == 0){
            if(strcmp(data, "true") == 0){
                // Set Calibration
                printf("Setting Calibration via MQTT\n");
                ism330_set_orientation_reference(ism330_handle, &orientation_reference);
                
                esp_mqtt_client_publish(e_client, MQTT_DEVICE_TOPIC "/calibration", "false", 0, 1, 0);
            }
        }
        else if(strcmp(topic, MQTT_DEVICE_TOPIC "/reset") == 0){
            if(strcmp(data, "true") == 0){
                // Reset Max Values
                printf("Resetting Max Values via MQTT\n");
                set_load_watchdog_values_to_zero();
                esp_mqtt_client_publish(e_client, MQTT_DEVICE_TOPIC "/reset", "false", 0, 1, 0);
            }
        }
        

        free(topic);
        free(data);
        
        break;
    case MQTT_EVENT_ERROR:
        printf("MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI("MQTT", "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        printf("Other event id:%d\n", event->event_id);
        break;
    }
}

esp_err_t init_mqtt(){

    esp_err_t err = ESP_OK;

    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = "mqtt://192.0.0.2:1883",
        .session.last_will = {
            .topic = MQTT_DEVICE_TOPIC "/con_stat",
            .msg = "0",
            .msg_len = 0,
            .qos = 1,
            .retain = 1
        }
    };

    client = esp_mqtt_client_init(&mqtt_config);
    err = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if(err != ESP_OK) return err;
    esp_mqtt_client_start(client);
    if(err != ESP_OK) return err;

    return ESP_OK;
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void* event_data){

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < NUM_RECONNECT_TRYS) {
            esp_wifi_connect();
            s_retry_num++;
            printf("retry to connect to the AP\n");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        printf("connect to the AP fail\n");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WIFI","got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(fLED_event_group, (1<<1));
        
        esp_err_t err = init_mqtt();
        if(err != ESP_OK){
             ESP_LOGE("MQTT", "Error: %s", esp_err_to_name(err));
        }
    }
}


esp_err_t init_wifi(){

    s_wifi_event_group = xEventGroupCreate();

    esp_err_t err = ESP_OK;
    err = esp_netif_init();
    if(err != ESP_OK) return err;

    err = esp_event_loop_create_default();
    if(err != ESP_OK) return err;

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_init_cfg);
    if(err != ESP_OK) return err;

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id);
    if(err != ESP_OK) return err;
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip);
    if(err != ESP_OK) return err;

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PWD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if(err != ESP_OK) return err;

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if(err != ESP_OK) return err;

    err = esp_wifi_start();
    if(err != ESP_OK) return err;

    return ESP_OK;


}


void mqtt_base_task(void *args){

    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if(err != ESP_OK){
        printf(">>ERROR: NVS Flash init failed; code: %d\n", err);
        vTaskDelete(NULL);
    }
    err = init_wifi();
    if(err != ESP_OK){
        printf(">>ERROR WiFi: %s\n",esp_err_to_name(err));
        vTaskDelete(NULL);
    }

    vTaskDelete(NULL);
}