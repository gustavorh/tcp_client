/*
 * Application Configuration Header
 * 
 * Centralized configuration for ESP32 TCP Client application.
 * All constants, timeouts, and settings are defined here for easy maintenance.
 * 
 * This file consolidates:
 * - WiFi connection settings
 * - API endpoint configuration  
 * - System operation parameters
 * - Hardware-specific constants
 * - ESP-IDF menuconfig values
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "esp_bit_defs.h"

/*
 * Application Information
 */
#define APP_NAME                    "TCP_CLIENT"
#define APP_VERSION                 "1.0.0"

/*
 * WiFi Configuration
 * Values come from menuconfig (idf.py menuconfig -> TCP Client Configuration)
 */
#define WIFI_SSID                   CONFIG_TCP_CLIENT_WIFI_SSID
#define WIFI_PASSWORD               CONFIG_TCP_CLIENT_WIFI_PASSWORD
#define WIFI_MAXIMUM_RETRY          CONFIG_TCP_CLIENT_MAXIMUM_RETRY
#define WIFI_CONNECT_TIMEOUT_MS     10000                              // 10 seconds
#define WIFI_AUTH_MODE              WIFI_AUTH_WPA2_PSK                 // Security mode

// WiFi Event Bits for FreeRTOS event groups
#define WIFI_CONNECTED_BIT          BIT0
#define WIFI_FAIL_BIT              BIT1

/*
 * HTTP Client Configuration
 */
#define API_ENDPOINT               CONFIG_TCP_CLIENT_API_ENDPOINT
#define HTTP_TIMEOUT_MS            5000                               // 5 seconds
#define HTTP_CONTENT_TYPE          "application/json"
#define HTTP_USER_AGENT            "ESP32-TCP-Client/1.0"

/*
 * Data Transmission Configuration
 */
#define POST_INTERVAL_SEC          CONFIG_TCP_CLIENT_POST_INTERVAL
#define POST_INTERVAL_MS           (POST_INTERVAL_SEC * 1000)        // Convert to milliseconds

/*
 * Sensor Configuration
 */
#define TEMP_SIMULATION_BASE       28.0f                             // Base temperature in °C
#define TEMP_SIMULATION_VARIATION  5.0f                              // Temperature variation range
#define TEMP_SIMULATION_PERIOD     300.0f                            // 5 minute variation period
#define TEMP_MIN_LIMIT             20.0f                             // Minimum realistic temperature
#define TEMP_MAX_LIMIT             45.0f                             // Maximum realistic temperature

/*
 * System Configuration
 */
#define UPTIME_STRING_MAX_LEN      32                                // Buffer size for uptime string
#define JSON_BUFFER_SIZE           256                               // JSON payload buffer size
#define LOG_TAG_MAX_LEN            16                                // Maximum log tag length

/*
 * Error Handling Configuration
 */
#define MAX_ERROR_RETRY_COUNT      3                                 // Maximum retries for recoverable errors
#define ERROR_RECOVERY_DELAY_MS    1000                              // Delay between error recovery attempts

/*
 * Memory Configuration
 */
#define TASK_STACK_SIZE           4096                               // Default task stack size
#define EVENT_QUEUE_SIZE          10                                 // Event queue depth

/*
 * Development & Debugging
 */
#ifdef CONFIG_LOG_DEFAULT_LEVEL_DEBUG
    #define DEBUG_ENABLED          1
    #define VERBOSE_LOGGING        1
#else
    #define DEBUG_ENABLED          0
    #define VERBOSE_LOGGING        0
#endif

/*
 * Hardware Configuration
 * (Ready for future GPIO sensor expansion)
 */
#define ADC_WIDTH                  ADC_WIDTH_BIT_12
#define ADC_ATTEN                  ADC_ATTEN_DB_0

/*
 * JSON Field Names
 * Centralized for consistency across modules
 */
#define JSON_FIELD_CPU_TEMP        "cpu_temp"
#define JSON_FIELD_UPTIME          "sys_uptime"
#define JSON_FIELD_TIMESTAMP       "timestamp"          // For future use
#define JSON_FIELD_DEVICE_ID       "device_id"          // For future use

/*
 * Utility Macros
 */
#define ARRAY_SIZE(arr)            (sizeof(arr) / sizeof((arr)[0]))
#define MS_TO_TICKS(ms)           (pdMS_TO_TICKS(ms))
#define SEC_TO_MS(sec)            ((sec) * 1000)
#define MIN_TO_SEC(min)           ((min) * 60)

#endif // CONFIG_H 