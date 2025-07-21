idf_component_register(SRCS "main.c" "I2C.c" "sd_card.c" "UART.c" "MQTT.c"
                        REQUIRES driver fatfs json esp_wifi nvs_flash mqtt)
