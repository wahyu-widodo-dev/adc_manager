idf_component_register(
    SRCS "adc_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_adc esp_common esp_driver_gpio
    PRIV_REQUIRES freertos log
)
