idf_component_register(SRCS "UART.c" "main.c" "max7318.c" "buzzer.c" "UART.c"
                    REQUIRES driver
                    PRIV_REQUIRES spi_flash
                    INCLUDE_DIRS "")
