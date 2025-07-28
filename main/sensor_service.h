/*
 * Sensor Service Module
 * 
 * Provides data collection services for system monitoring and sensor readings.
 * Currently implements CPU temperature simulation and system uptime tracking.
 * Designed to be extensible for GPIO-based sensors and additional data sources.
 * 
 * Features:
 * - CPU temperature simulation with realistic variations
 * - System uptime tracking and formatting
 * - Extensible sensor data structure
 * - Proper ESP-IDF error handling
 * - Ready for GPIO sensor integration
 * 
 * Usage:
 *   esp_err_t ret = sensor_service_init();
 *   ESP_ERROR_CHECK(ret);
 *   
 *   sensor_data_t data;
 *   ret = sensor_service_read(&data);
 *   if (ret == ESP_OK) {
 *       printf("Temperature: %.1f°C, Uptime: %s\n", data.cpu_temp, data.uptime);
 *   }
 */

#ifndef SENSOR_SERVICE_H
#define SENSOR_SERVICE_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sensor Data Structure
 * 
 * Contains all sensor readings and system data.
 * Designed to be extensible for additional sensors.
 */
typedef struct {
    // System sensors
    float cpu_temp;                      // Simulated CPU temperature in Celsius
    char uptime[32];                     // Formatted system uptime string
    
    // Future expansion ready
    // float gpio_sensor_1;              // GPIO-based sensor 1
    // float gpio_sensor_2;              // GPIO-based sensor 2
    // bool digital_input_1;             // Digital input state
    // uint32_t counter_value;           // Event counter
    
    // Metadata
    uint64_t timestamp_us;               // Timestamp when data was collected (microseconds)
    bool data_valid;                     // Indicates if all sensor data is valid
} sensor_data_t;

/*
 * Sensor Types Enumeration
 * 
 * Defines available sensor types for individual sensor operations.
 * Useful for enabling/disabling specific sensors or error handling.
 */
typedef enum {
    SENSOR_TYPE_CPU_TEMP = 0,            // CPU temperature sensor
    SENSOR_TYPE_UPTIME,                  // System uptime tracker
    // SENSOR_TYPE_GPIO_ANALOG_1,        // Future: GPIO analog sensor 1
    // SENSOR_TYPE_GPIO_DIGITAL_1,       // Future: GPIO digital sensor 1
    SENSOR_TYPE_MAX                      // Total number of sensor types
} sensor_type_t;

/*
 * Sensor Status Information
 */
typedef struct {
    bool initialized;                    // Module initialization status
    bool enabled[SENSOR_TYPE_MAX];       // Per-sensor enable status
    uint32_t read_count;                 // Total number of successful reads
    uint32_t error_count;                // Total number of read errors
    int64_t last_read_time;              // Timestamp of last successful read
} sensor_status_t;

/*
 * Initialize Sensor Service
 * 
 * Initializes the sensor service module and prepares all sensors for operation.
 * Sets up timers, calibration data, and any required hardware peripherals.
 * 
 * Returns:
 *   ESP_OK: Sensor service initialized successfully
 *   ESP_FAIL: Initialization failed
 *   ESP_ERR_NO_MEM: Insufficient memory for initialization
 *   ESP_ERR_*: Other ESP-IDF specific errors
 */
esp_err_t sensor_service_init(void);

/*
 * Read All Sensor Data
 * 
 * Performs a complete sensor reading cycle, collecting data from all
 * enabled sensors and populating the sensor_data_t structure.
 * 
 * This function:
 * 1. Validates sensor service is initialized
 * 2. Reads all enabled sensors
 * 3. Updates timestamps and validity flags
 * 4. Returns consolidated data structure
 * 
 * Parameters:
 *   data: Pointer to sensor_data_t structure to populate
 * 
 * Returns:
 *   ESP_OK: All sensor data read successfully
 *   ESP_ERR_INVALID_ARG: Invalid data pointer
 *   ESP_ERR_INVALID_STATE: Sensor service not initialized
 *   ESP_FAIL: One or more sensors failed to read
 */
esp_err_t sensor_service_read(sensor_data_t *data);

/*
 * Read Specific Sensor
 * 
 * Reads data from a specific sensor type without affecting others.
 * Useful for selective monitoring or troubleshooting individual sensors.
 * 
 * Parameters:
 *   sensor_type: Type of sensor to read
 *   value: Pointer to store sensor value (format depends on sensor type)
 * 
 * Returns:
 *   ESP_OK: Sensor read successfully
 *   ESP_ERR_INVALID_ARG: Invalid arguments
 *   ESP_ERR_NOT_SUPPORTED: Sensor type not implemented
 *   ESP_ERR_INVALID_STATE: Sensor not initialized or disabled
 *   ESP_FAIL: Sensor read failed
 */
esp_err_t sensor_service_read_single(sensor_type_t sensor_type, void *value);

/*
 * Enable/Disable Specific Sensor
 * 
 * Allows runtime control of individual sensors for power management
 * or selective monitoring scenarios.
 * 
 * Parameters:
 *   sensor_type: Type of sensor to control
 *   enable: true to enable, false to disable
 * 
 * Returns:
 *   ESP_OK: Sensor state changed successfully
 *   ESP_ERR_INVALID_ARG: Invalid sensor type
 *   ESP_ERR_NOT_SUPPORTED: Sensor type not implemented
 */
esp_err_t sensor_service_enable(sensor_type_t sensor_type, bool enable);

/*
 * Get Sensor Service Status
 * 
 * Returns detailed status information about the sensor service including
 * initialization state, sensor enable status, and statistics.
 * 
 * Parameters:
 *   status: Pointer to sensor_status_t structure to populate
 * 
 * Returns:
 *   ESP_OK: Status retrieved successfully
 *   ESP_ERR_INVALID_ARG: Invalid status pointer
 */
esp_err_t sensor_service_get_status(sensor_status_t *status);

/*
 * Reset Sensor Statistics
 * 
 * Resets read count, error count, and other statistics counters.
 * Useful for monitoring sensor performance over specific time periods.
 * 
 * Returns:
 *   ESP_OK: Statistics reset successfully
 *   ESP_ERR_INVALID_STATE: Sensor service not initialized
 */
esp_err_t sensor_service_reset_stats(void);

/*
 * Sensor Service Cleanup
 * 
 * Cleans up sensor service resources and shuts down all sensors.
 * Should be called during application shutdown.
 * After calling this function, sensor_service_init() must be called
 * again before using other functions.
 * 
 * Returns:
 *   ESP_OK: Cleanup completed successfully
 *   ESP_FAIL: Cleanup failed
 */
esp_err_t sensor_service_cleanup(void);

/*
 * Temperature Sensor Functions
 * These can be used directly for applications that only need temperature data.
 */

/*
 * Get CPU Temperature
 * 
 * Returns simulated CPU temperature with realistic variations.
 * Can be called independently without full sensor service initialization.
 * 
 * Returns:
 *   float: Temperature in Celsius (20.0 - 45.0°C range)
 */
float sensor_get_cpu_temperature(void);

/*
 * Get System Uptime
 * 
 * Calculates and formats system uptime since boot.
 * Can be called independently without full sensor service initialization.
 * 
 * Parameters:
 *   uptime_str: Buffer to store formatted uptime string
 *   max_len: Maximum length of the buffer (recommended: 32 bytes)
 * 
 * Returns:
 *   ESP_OK: Uptime calculated and formatted successfully
 *   ESP_ERR_INVALID_ARG: Invalid parameters
 *   ESP_ERR_INVALID_SIZE: Buffer too small
 */
esp_err_t sensor_get_system_uptime(char *uptime_str, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_SERVICE_H 