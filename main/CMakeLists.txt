idf_component_register(SRCS "main.c"
                          "wifi_manager.c"
                          "sensor_service.c" 
                          "http_client.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_http_client nvs_flash json esp_adc esp_system esp_timer esp_netif esp_event freertos) 