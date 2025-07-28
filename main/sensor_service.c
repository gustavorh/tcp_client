/*
 * Sensor Service Implementation
 * 
 * Implements data collection services including CPU temperature simulation
 * and system uptime tracking. Designed for extensibility with GPIO sensors.
 */

#include "sensor_service.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"

// Module logging tag
static const char *TAG = "SENSOR_SVC";

// Module state management
typedef struct {
    bool initialized;
    bool enabled[SENSOR_TYPE_MAX];
    uint32_t read_count;
    uint32_t error_count;
    int64_t last_read_time;
    int64_t start_time;                  // Application start time for uptime calculation
} sensor_context_t;

// Global module context
static sensor_context_t s_context = {
    .initialized = false,
    .enabled = {false},
    .read_count = 0,
    .error_count = 0,
    .last_read_time = 0,
    .start_time = 0
};

/*
 * Internal function to read CPU temperature simulation
 */
static esp_err_t read_cpu_temperature(float *temp)
{
    if (!temp) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get time since boot in seconds for temperature variation
    int64_t time_us = esp_timer_get_time();
    int64_t time_seconds = time_us / 1000000;
    
    // Create a slowly varying temperature using sine wave
    // Uses constants from config.h
    float base_temp = TEMP_SIMULATION_BASE;
    float variation = TEMP_SIMULATION_VARIATION * 
                     sin(time_seconds * 2.0 * 3.14159 / TEMP_SIMULATION_PERIOD);
    
    // Add small random-like variation based on time
    float micro_variation = (time_seconds % 17) * 0.1 - 0.8; // Small noise
    
    float temperature = base_temp + variation + micro_variation;
    
    // Ensure temperature is within realistic bounds
    if (temperature < TEMP_MIN_LIMIT) temperature = TEMP_MIN_LIMIT;
    if (temperature > TEMP_MAX_LIMIT) temperature = TEMP_MAX_LIMIT;
    
    *temp = temperature;
    
    ESP_LOGD(TAG, "CPU temperature: %.1f°C", temperature);
    return ESP_OK;
}

/*
 * Internal function to calculate system uptime
 */
static esp_err_t read_system_uptime(char *uptime_str, size_t max_len)
{
    if (!uptime_str || max_len < UPTIME_STRING_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get current time in microseconds since boot
    int64_t current_time = esp_timer_get_time();
    
    // Calculate uptime in seconds
    int64_t uptime_seconds = (current_time - s_context.start_time) / 1000000;
    
    // Break down into hours, minutes, and seconds
    int hours = uptime_seconds / 3600;
    int minutes = (uptime_seconds % 3600) / 60;
    int seconds = uptime_seconds % 60;
    
    // Format as string
    int written = snprintf(uptime_str, max_len, "%dh %dm %ds", hours, minutes, seconds);
    if (written < 0 || written >= max_len) {
        ESP_LOGE(TAG, "Uptime string formatting failed or truncated");
        return ESP_ERR_INVALID_SIZE;
    }
    
    ESP_LOGD(TAG, "System uptime: %s", uptime_str);
    return ESP_OK;
}

/*
 * Initialize Sensor Service
 */
esp_err_t sensor_service_init(void)
{
    if (s_context.initialized) {
        ESP_LOGW(TAG, "Sensor service already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing sensor service...");
    
    // Record initialization time for uptime calculation
    s_context.start_time = esp_timer_get_time();
    
    // Enable all available sensors by default
    s_context.enabled[SENSOR_TYPE_CPU_TEMP] = true;
    s_context.enabled[SENSOR_TYPE_UPTIME] = true;
    
    // Reset statistics
    s_context.read_count = 0;
    s_context.error_count = 0;
    s_context.last_read_time = 0;
    
    s_context.initialized = true;
    
    ESP_LOGI(TAG, "Sensor service initialized successfully");
    ESP_LOGI(TAG, "Enabled sensors: CPU_TEMP=%s, UPTIME=%s",
             s_context.enabled[SENSOR_TYPE_CPU_TEMP] ? "YES" : "NO",
             s_context.enabled[SENSOR_TYPE_UPTIME] ? "YES" : "NO");
    
    return ESP_OK;
}

/*
 * Read All Sensor Data
 */
esp_err_t sensor_service_read(sensor_data_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_context.initialized) {
        ESP_LOGE(TAG, "Sensor service not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "Reading all sensor data...");
    
    // Initialize data structure
    memset(data, 0, sizeof(sensor_data_t));
    data->timestamp_us = esp_timer_get_time();
    data->data_valid = true;
    
    esp_err_t overall_result = ESP_OK;
    
    // Read CPU temperature if enabled
    if (s_context.enabled[SENSOR_TYPE_CPU_TEMP]) {
        esp_err_t temp_result = read_cpu_temperature(&data->cpu_temp);
        if (temp_result != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read CPU temperature: %s", esp_err_to_name(temp_result));
            data->data_valid = false;
            overall_result = ESP_FAIL;
            s_context.error_count++;
        }
    } else {
        data->cpu_temp = 0.0f;  // Set default value for disabled sensor
    }
    
    // Read system uptime if enabled
    if (s_context.enabled[SENSOR_TYPE_UPTIME]) {
        esp_err_t uptime_result = read_system_uptime(data->uptime, sizeof(data->uptime));
        if (uptime_result != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read system uptime: %s", esp_err_to_name(uptime_result));
            data->data_valid = false;
            overall_result = ESP_FAIL;
            s_context.error_count++;
            strncpy(data->uptime, "ERROR", sizeof(data->uptime) - 1);
        }
    } else {
        strncpy(data->uptime, "DISABLED", sizeof(data->uptime) - 1);
    }
    
    // Update statistics
    if (overall_result == ESP_OK) {
        s_context.read_count++;
        s_context.last_read_time = data->timestamp_us;
        ESP_LOGD(TAG, "Sensor read successful - Temp: %.1f°C, Uptime: %s", 
                 data->cpu_temp, data->uptime);
    } else {
        ESP_LOGW(TAG, "Sensor read completed with errors");
    }
    
    return overall_result;
}

/*
 * Read Specific Sensor
 */
esp_err_t sensor_service_read_single(sensor_type_t sensor_type, void *value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_context.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (sensor_type >= SENSOR_TYPE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_context.enabled[sensor_type]) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "Reading sensor type: %d", sensor_type);
    
    switch (sensor_type) {
        case SENSOR_TYPE_CPU_TEMP:
            return read_cpu_temperature((float*)value);
            
        case SENSOR_TYPE_UPTIME:
            // For uptime, we expect a char[32] buffer
            return read_system_uptime((char*)value, UPTIME_STRING_MAX_LEN);
            
        default:
            ESP_LOGE(TAG, "Sensor type %d not implemented", sensor_type);
            return ESP_ERR_NOT_SUPPORTED;
    }
}

/*
 * Enable/Disable Specific Sensor
 */
esp_err_t sensor_service_enable(sensor_type_t sensor_type, bool enable)
{
    if (sensor_type >= SENSOR_TYPE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // For now, all sensor types are supported
    ESP_LOGI(TAG, "Sensor type %d %s", sensor_type, enable ? "enabled" : "disabled");
    s_context.enabled[sensor_type] = enable;
    
    return ESP_OK;
}

/*
 * Get Sensor Service Status
 */
esp_err_t sensor_service_get_status(sensor_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    status->initialized = s_context.initialized;
    memcpy(status->enabled, s_context.enabled, sizeof(s_context.enabled));
    status->read_count = s_context.read_count;
    status->error_count = s_context.error_count;
    status->last_read_time = s_context.last_read_time;
    
    return ESP_OK;
}

/*
 * Reset Sensor Statistics
 */
esp_err_t sensor_service_reset_stats(void)
{
    if (!s_context.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Resetting sensor statistics...");
    s_context.read_count = 0;
    s_context.error_count = 0;
    s_context.last_read_time = 0;
    
    return ESP_OK;
}

/*
 * Sensor Service Cleanup
 */
esp_err_t sensor_service_cleanup(void)
{
    if (!s_context.initialized) {
        return ESP_OK;  // Already cleaned up
    }
    
    ESP_LOGI(TAG, "Cleaning up sensor service...");
    
    // Disable all sensors
    for (int i = 0; i < SENSOR_TYPE_MAX; i++) {
        s_context.enabled[i] = false;
    }
    
    // Reset context
    memset(&s_context, 0, sizeof(s_context));
    
    ESP_LOGI(TAG, "Sensor service cleanup completed");
    return ESP_OK;
}

/*
 * Standalone Temperature Function
 * 
 * Can be used without full sensor service initialization
 */
float sensor_get_cpu_temperature(void)
{
    float temp = 0.0f;
    esp_err_t result = read_cpu_temperature(&temp);
    
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Temperature read failed, returning default");
        return TEMP_SIMULATION_BASE;  // Return reasonable default
    }
    
    return temp;
}

/*
 * Standalone Uptime Function
 * 
 * Can be used without full sensor service initialization
 */
esp_err_t sensor_get_system_uptime(char *uptime_str, size_t max_len)
{
    if (!uptime_str || max_len < UPTIME_STRING_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // For standalone use, calculate uptime from boot time
    static int64_t boot_time = 0;
    if (boot_time == 0) {
        boot_time = esp_timer_get_time();
    }
    
    int64_t current_time = esp_timer_get_time();
    int64_t uptime_seconds = (current_time - boot_time) / 1000000;
    
    int hours = uptime_seconds / 3600;
    int minutes = (uptime_seconds % 3600) / 60;
    int seconds = uptime_seconds % 60;
    
    int written = snprintf(uptime_str, max_len, "%dh %dm %ds", hours, minutes, seconds);
    if (written < 0 || written >= max_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    return ESP_OK;
} 