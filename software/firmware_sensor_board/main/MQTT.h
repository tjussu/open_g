#ifndef MQTT_H
#define MQTT_H

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define MQTT_CONNECTED_BIT  BIT2

#define MQTT_DEVICE_TOPIC "openg_1"

extern EventGroupHandle_t s_wifi_event_group;

void publish_data(const char* subtopic, const char* data, int qos, int retain);

void mqtt_base_task(void *args);

#endif