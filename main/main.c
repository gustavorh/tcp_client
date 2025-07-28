/*
 * ESP32 TCP Client - Modular Architecture
 * 
 * This application demonstrates a clean, modular approach to ESP32 development.
 * It connects to WiFi and periodically sends system data to a REST API endpoint.
 * 
 * Architecture:
 * - config.h: Centralized configuration management
 * - wifi_manager: WiFi connectivity service
 * - sensor_service: Data collection service (temperature, uptime)
 * - http_client: HTTP communication service
 * - main.c: Application orchestration (this file)
 * 
 * Features:
 * - Clean separation of concerns
 * - Proper ESP-IDF error handling patterns
 * - Extensible for additional sensors and endpoints
 * - Statistics and status monitoring
 * - Ready for unit testing
 * 
 * Configuration:
 * Use `idf.py menuconfig` to set WiFi credentials and API endpoint:
 * - Component config -> TCP Client Config
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

// Application modules
#include "config.h"
#include "wifi_manager.h"
#include "sensor_service.h"
#include "http_client.h"

// Logging tag for this module
static const char *TAG = APP_NAME;

/*
 * Initialize NVS Flash
 * 
 * Initializes Non-Volatile Storage which is required for WiFi and other
 * ESP-IDF components that need persistent storage.
 */
static esp_err_t init_nvs_flash(void)
{
    ESP_LOGI(TAG, "Initializing NVS flash...");
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_LOGW(TAG, "NVS partition needs to be erased, performing erase...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "NVS flash initialized successfully");
    } else {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/*
 * Initialize All Application Services
 * 
 * Initializes all required services in the correct order.
 * Uses ESP-IDF error handling patterns for robust startup.
 */
static esp_err_t init_application_services(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing application services...");
    
    // Initialize WiFi manager
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize sensor service
    ret = sensor_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize sensor service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize HTTP client
    ret = http_client_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "All application services initialized successfully");
    return ESP_OK;
}

/*
 * Connect to WiFi Network
 * 
 * Establishes WiFi connectivity using the WiFi manager service.
 * Returns only after successful connection or failure.
 */
static esp_err_t connect_to_wifi(void)
{
    ESP_LOGI(TAG, "Connecting to WiFi network...");
    
    esp_err_t ret = wifi_manager_connect();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connection established successfully");
        
        // Get and display connection information
        esp_netif_ip_info_t ip_info;
        if (wifi_manager_get_ip_info(&ip_info) == ESP_OK) {
            ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
            ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
            ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&ip_info.gw));
        }
        
        // Get signal strength if available
        int8_t rssi;
        if (wifi_manager_get_rssi(&rssi) == ESP_OK) {
            ESP_LOGI(TAG, "Signal strength: %d dBm", rssi);
        }
    } else {
        ESP_LOGE(TAG, "WiFi connection failed: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "Check your WiFi credentials in menuconfig");
    }
    
    return ret;
}

/*
 * Perform Data Transmission Cycle
 * 
 * Collects sensor data and transmits it to the API endpoint.
 * This function encapsulates one complete data transmission cycle.
 */
static esp_err_t perform_data_transmission(void)
{
    ESP_LOGI(TAG, "--- Starting data transmission cycle ---");
    
    // Check WiFi connection status
    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "WiFi not connected, skipping transmission");
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    
    // Read sensor data
    sensor_data_t sensor_data;
    esp_err_t ret = sensor_service_read(&sensor_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read sensor data: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Sensor data - Temperature: %.1f°C, Uptime: %s", 
             sensor_data.cpu_temp, sensor_data.uptime);
    
    // Send data to API
    ret = http_client_post_sensor_data(&sensor_data);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Data transmission completed successfully");
        
        // Get HTTP response details
        http_response_t response;
        if (http_client_get_last_response(&response) == ESP_OK) {
            ESP_LOGI(TAG, "HTTP Response - Status: %d, Content-Length: %d", 
                     response.status_code, response.content_length);
        }
    } else {
        ESP_LOGW(TAG, "Data transmission failed: %s", esp_err_to_name(ret));
        
        // Log HTTP statistics for debugging
        http_client_stats_t stats;
        if (http_client_get_stats(&stats) == ESP_OK) {
            ESP_LOGI(TAG, "HTTP Stats - Total: %lu, Success: %lu, Failed: %lu", 
                     stats.total_requests, stats.successful_requests, stats.failed_requests);
        }
    }
    
    return ret;
}

/*
 * Display Application Status
 * 
 * Periodically displays status information from all services.
 * Useful for monitoring and debugging.
 */
static void display_application_status(void)
{
    static uint32_t status_counter = 0;
    
    // Display status every 10 cycles (approximately every 100 seconds with 10s interval)
    if (++status_counter % 10 == 0) {
        ESP_LOGI(TAG, "=== Application Status Report ===");
        
        // WiFi status
        wifi_status_t wifi_status = wifi_manager_get_status();
        ESP_LOGI(TAG, "WiFi Status: %s", 
                 (wifi_status == WIFI_STATUS_CONNECTED) ? "CONNECTED" :
                 (wifi_status == WIFI_STATUS_CONNECTING) ? "CONNECTING" :
                 (wifi_status == WIFI_STATUS_DISCONNECTED) ? "DISCONNECTED" : "ERROR");
        
        // Sensor service status
        sensor_status_t sensor_status;
        if (sensor_service_get_status(&sensor_status) == ESP_OK) {
            ESP_LOGI(TAG, "Sensor Status - Reads: %lu, Errors: %lu", 
                     sensor_status.read_count, sensor_status.error_count);
        }
        
        // HTTP client status
        http_client_stats_t http_stats;
        if (http_client_get_stats(&http_stats) == ESP_OK) {
            ESP_LOGI(TAG, "HTTP Status - Total: %lu, Success: %lu, Failed: %lu, Timeouts: %lu", 
                     http_stats.total_requests, http_stats.successful_requests, 
                     http_stats.failed_requests, http_stats.timeout_count);
        }
        
        // Memory status
        ESP_LOGI(TAG, "Free heap memory: %lu bytes", esp_get_free_heap_size());
        
        ESP_LOGI(TAG, "=== End Status Report ===");
    }
}

/*
 * Main Application Entry Point
 * 
 * This is the simplified main function that focuses only on orchestration.
 * All complex logic has been moved to dedicated service modules.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32 TCP Client - Modular Architecture ===");
    ESP_LOGI(TAG, "Application: %s v%s", APP_NAME, APP_VERSION);
    ESP_LOGI(TAG, "Compiled: %s %s", __DATE__, __TIME__);
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    
    // Step 1: Initialize NVS Flash
    ESP_ERROR_CHECK(init_nvs_flash());
    
    // Step 2: Initialize all application services
    ESP_ERROR_CHECK(init_application_services());
    
    // Step 3: Connect to WiFi
    ESP_ERROR_CHECK(connect_to_wifi());
    
    // Step 4: Display configuration information
    ESP_LOGI(TAG, "=== Configuration ===");
    ESP_LOGI(TAG, "API Endpoint: %s", API_ENDPOINT);
    ESP_LOGI(TAG, "Transmission Interval: %d seconds", POST_INTERVAL_SEC);
    ESP_LOGI(TAG, "WiFi SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "=== Starting Data Transmission Loop ===");
    
    // Step 5: Main application loop
    uint32_t cycle_count = 0;
    while (1) {
        cycle_count++;
        ESP_LOGI(TAG, "=== Cycle %lu ===", cycle_count);
        
        // Perform data transmission
        esp_err_t transmission_result = perform_data_transmission();
        
        // Display status information periodically
        display_application_status();
        
        // Log next transmission time
        ESP_LOGI(TAG, "Next transmission in %d seconds...", POST_INTERVAL_SEC);
        
        // Wait for the configured interval
        vTaskDelay(MS_TO_TICKS(POST_INTERVAL_MS));
    }
    
    // This code should never be reached, but included for completeness
    ESP_LOGI(TAG, "Application shutting down...");
    
    // Cleanup services in reverse order
    http_client_cleanup();
    sensor_service_cleanup();
    wifi_manager_cleanup();
    
    ESP_LOGI(TAG, "Application shutdown complete");
} 