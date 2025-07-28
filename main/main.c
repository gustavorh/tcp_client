#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "TCP_CLIENT";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting TCP Client Application");
    
    // Initialize your application here
    
    while (1) {
        ESP_LOGI(TAG, "TCP Client running...");
        vTaskDelay(pdMS_TO_TICKS(5000)); // Delay for 5 seconds
    }
} 