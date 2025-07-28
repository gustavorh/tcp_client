/*
 * HTTP Client Module
 * 
 * Provides HTTP communication services for sending sensor data to REST APIs.
 * Handles JSON payload creation, HTTP POST requests, and response processing.
 * Designed to be extensible for multiple endpoints and data formats.
 * 
 * Features:
 * - JSON payload creation from sensor data
 * - HTTP POST request handling with proper headers
 * - Response status checking and error handling
 * - Extensible for multiple API endpoints
 * - Proper ESP-IDF error handling patterns
 * - Request/response statistics tracking
 * 
 * Usage:
 *   esp_err_t ret = http_client_init();
 *   ESP_ERROR_CHECK(ret);
 *   
 *   sensor_data_t data = { .cpu_temp = 25.5, .uptime = "1h 30m 45s" };
 *   ret = http_client_post_sensor_data(&data);
 *   if (ret == ESP_OK) {
 *       // Data sent successfully
 *   }
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "esp_err.h"
#include "sensor_service.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * HTTP Response Information
 */
typedef struct {
    int status_code;                     // HTTP status code (200, 404, 500, etc.)
    int content_length;                  // Response content length
    char *response_data;                 // Response body (if any)
    size_t response_data_len;            // Length of response data
    bool success;                        // true if status_code indicates success (2xx)
} http_response_t;

/*
 * HTTP Client Statistics
 */
typedef struct {
    bool initialized;                    // Module initialization status
    uint32_t total_requests;             // Total number of requests sent
    uint32_t successful_requests;        // Number of successful requests (2xx status)
    uint32_t failed_requests;            // Number of failed requests
    uint32_t timeout_count;              // Number of timeout errors
    uint32_t network_errors;             // Number of network-related errors
    int64_t last_request_time;           // Timestamp of last request
    int last_status_code;                // Status code of last request
} http_client_stats_t;

/*
 * Endpoint Configuration
 * 
 * For future support of multiple endpoints
 */
typedef struct {
    char *url;                           // Complete endpoint URL
    char *content_type;                  // Content-Type header value
    char *user_agent;                    // User-Agent header value
    int timeout_ms;                      // Request timeout in milliseconds
    bool enabled;                        // Whether this endpoint is active
} endpoint_config_t;

/*
 * Initialize HTTP Client
 * 
 * Initializes the HTTP client module and prepares for network communication.
 * Sets up default configurations and internal data structures.
 * 
 * Returns:
 *   ESP_OK: HTTP client initialized successfully
 *   ESP_FAIL: Initialization failed
 *   ESP_ERR_NO_MEM: Insufficient memory for initialization
 *   ESP_ERR_*: Other ESP-IDF specific errors
 */
esp_err_t http_client_init(void);

/*
 * Send Sensor Data to Default API Endpoint
 * 
 * Creates a JSON payload from sensor data and sends it to the configured
 * API endpoint via HTTP POST. This is the primary function for sending
 * sensor data to the backend.
 * 
 * JSON Format:
 * {
 *   "cpu_temp": 25.4,
 *   "sys_uptime": "1h 30m 45s"
 * }
 * 
 * Parameters:
 *   data: Pointer to sensor_data_t structure containing sensor readings
 * 
 * Returns:
 *   ESP_OK: Data sent successfully (HTTP 2xx response)
 *   ESP_ERR_INVALID_ARG: Invalid data pointer
 *   ESP_ERR_INVALID_STATE: HTTP client not initialized
 *   ESP_ERR_TIMEOUT: Request timeout
 *   ESP_FAIL: HTTP request failed or non-2xx response
 *   ESP_ERR_*: Other network or HTTP errors
 */
esp_err_t http_client_post_sensor_data(const sensor_data_t *data);

/*
 * Send Custom JSON Data
 * 
 * Sends a pre-formatted JSON string to the configured API endpoint.
 * Useful for custom payloads or when JSON is already prepared.
 * 
 * Parameters:
 *   json_data: Null-terminated JSON string to send
 * 
 * Returns:
 *   ESP_OK: Data sent successfully
 *   ESP_ERR_INVALID_ARG: Invalid JSON data pointer
 *   ESP_ERR_INVALID_STATE: HTTP client not initialized
 *   ESP_ERR_TIMEOUT: Request timeout
 *   ESP_FAIL: HTTP request failed
 */
esp_err_t http_client_post_json(const char *json_data);

/*
 * Send Data to Custom Endpoint
 * 
 * Sends sensor data to a custom endpoint URL instead of the default.
 * Useful for multi-endpoint configurations or failover scenarios.
 * 
 * Parameters:
 *   data: Pointer to sensor_data_t structure
 *   endpoint_url: Custom endpoint URL to send data to
 * 
 * Returns:
 *   ESP_OK: Data sent successfully
 *   ESP_ERR_INVALID_ARG: Invalid parameters
 *   ESP_ERR_INVALID_STATE: HTTP client not initialized
 *   ESP_ERR_TIMEOUT: Request timeout
 *   ESP_FAIL: HTTP request failed
 */
esp_err_t http_client_post_to_endpoint(const sensor_data_t *data, const char *endpoint_url);

/*
 * Get Last HTTP Response
 * 
 * Returns detailed information about the last HTTP request/response.
 * Useful for debugging and monitoring API communication.
 * 
 * Parameters:
 *   response: Pointer to http_response_t structure to populate
 * 
 * Returns:
 *   ESP_OK: Response information retrieved successfully
 *   ESP_ERR_INVALID_ARG: Invalid response pointer
 *   ESP_ERR_INVALID_STATE: No previous request or HTTP client not initialized
 */
esp_err_t http_client_get_last_response(http_response_t *response);

/*
 * Get HTTP Client Statistics
 * 
 * Returns detailed statistics about HTTP client usage including
 * request counts, success rates, and error information.
 * 
 * Parameters:
 *   stats: Pointer to http_client_stats_t structure to populate
 * 
 * Returns:
 *   ESP_OK: Statistics retrieved successfully
 *   ESP_ERR_INVALID_ARG: Invalid stats pointer
 */
esp_err_t http_client_get_stats(http_client_stats_t *stats);

/*
 * Reset HTTP Client Statistics
 * 
 * Resets all statistics counters to zero.
 * Useful for monitoring performance over specific time periods.
 * 
 * Returns:
 *   ESP_OK: Statistics reset successfully
 *   ESP_ERR_INVALID_STATE: HTTP client not initialized
 */
esp_err_t http_client_reset_stats(void);

/*
 * Test API Endpoint Connectivity
 * 
 * Performs a simple connectivity test to the configured API endpoint.
 * Useful for network diagnostics and endpoint validation.
 * 
 * Returns:
 *   ESP_OK: Endpoint is reachable
 *   ESP_ERR_TIMEOUT: Connection timeout
 *   ESP_FAIL: Endpoint unreachable or other error
 *   ESP_ERR_INVALID_STATE: HTTP client not initialized
 */
esp_err_t http_client_test_connectivity(void);

/*
 * HTTP Client Cleanup
 * 
 * Cleans up HTTP client resources and shuts down network connections.
 * Should be called during application shutdown.
 * After calling this function, http_client_init() must be called
 * again before using other functions.
 * 
 * Returns:
 *   ESP_OK: Cleanup completed successfully
 *   ESP_FAIL: Cleanup failed
 */
esp_err_t http_client_cleanup(void);

/*
 * Utility Functions for JSON Handling
 */

/*
 * Create JSON from Sensor Data
 * 
 * Creates a JSON string from sensor data structure.
 * Caller is responsible for freeing the returned string.
 * 
 * Parameters:
 *   data: Pointer to sensor_data_t structure
 * 
 * Returns:
 *   char*: Allocated JSON string (must be freed by caller)
 *   NULL: JSON creation failed
 */
char* http_client_create_json(const sensor_data_t *data);

/*
 * Validate JSON Format
 * 
 * Validates that a JSON string is properly formatted.
 * Useful for testing and debugging JSON payloads.
 * 
 * Parameters:
 *   json_string: JSON string to validate
 * 
 * Returns:
 *   true: JSON is valid
 *   false: JSON is invalid or malformed
 */
bool http_client_validate_json(const char *json_string);

#ifdef __cplusplus
}
#endif

#endif // HTTP_CLIENT_H 