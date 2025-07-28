/*
 * HTTP Client Implementation
 * 
 * Implements HTTP communication services for sending sensor data to REST APIs.
 * Handles JSON payload creation, HTTP requests, and response processing.
 */

#include "http_client.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "cJSON.h"

// Module logging tag
static const char *TAG = "HTTP_CLIENT";

// Module state management
typedef struct {
    bool initialized;
    http_client_stats_t stats;
    http_response_t last_response;
    char *response_buffer;               // Buffer for response data
    size_t response_buffer_size;         // Size of response buffer
} http_client_context_t;

// Global module context
static http_client_context_t s_context = {
    .initialized = false,
    .stats = {0},
    .last_response = {0},
    .response_buffer = NULL,
    .response_buffer_size = 0
};

/*
 * Internal HTTP Event Handler
 * 
 * Handles HTTP client events such as connection, data reception, and completion.
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP Error occurred");
            s_context.stats.network_errors++;
            break;
            
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP Connected to server");
            break;
            
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP Headers sent");
            break;
            
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP Header received: %.*s", evt->data_len, (char*)evt->data);
            break;
            
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP Data received: %.*s", evt->data_len, (char*)evt->data);
            
            // Store response data if we have a buffer
            if (s_context.response_buffer && evt->data_len > 0) {
                size_t available_space = s_context.response_buffer_size - s_context.last_response.response_data_len - 1;
                size_t copy_len = (evt->data_len < available_space) ? evt->data_len : available_space;
                
                if (copy_len > 0) {
                    memcpy(s_context.response_buffer + s_context.last_response.response_data_len, 
                           evt->data, copy_len);
                    s_context.last_response.response_data_len += copy_len;
                    s_context.response_buffer[s_context.last_response.response_data_len] = '\0';
                }
            }
            break;
            
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP Request finished");
            break;
            
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP Disconnected from server");
            break;
            
        default:
            ESP_LOGD(TAG, "Unhandled HTTP event: %d", evt->event_id);
            break;
    }
    return ESP_OK;
}

/*
 * Initialize HTTP Client
 */
esp_err_t http_client_init(void)
{
    if (s_context.initialized) {
        ESP_LOGW(TAG, "HTTP client already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing HTTP client...");
    
    // Allocate response buffer
    s_context.response_buffer_size = 512;  // 512 bytes for response data
    s_context.response_buffer = malloc(s_context.response_buffer_size);
    if (!s_context.response_buffer) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize statistics
    memset(&s_context.stats, 0, sizeof(s_context.stats));
    s_context.stats.initialized = true;
    
    // Initialize last response
    memset(&s_context.last_response, 0, sizeof(s_context.last_response));
    s_context.last_response.response_data = s_context.response_buffer;
    
    s_context.initialized = true;
    
    ESP_LOGI(TAG, "HTTP client initialized successfully");
    ESP_LOGI(TAG, "Default endpoint: %s", API_ENDPOINT);
    
    return ESP_OK;
}

/*
 * Create JSON from Sensor Data
 */
char* http_client_create_json(const sensor_data_t *data)
{
    if (!data) {
        ESP_LOGE(TAG, "Invalid sensor data pointer");
        return NULL;
    }
    
    // Create JSON object
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return NULL;
    }
    
    // Add temperature data (use centralized field names from config.h)
    cJSON *temp_item = cJSON_CreateNumber(data->cpu_temp);
    if (temp_item == NULL) {
        ESP_LOGE(TAG, "Failed to create temperature JSON item");
        cJSON_Delete(json);
        return NULL;
    }
    cJSON_AddItemToObject(json, JSON_FIELD_CPU_TEMP, temp_item);
    
    // Add uptime data
    cJSON *uptime_item = cJSON_CreateString(data->uptime);
    if (uptime_item == NULL) {
        ESP_LOGE(TAG, "Failed to create uptime JSON item");
        cJSON_Delete(json);
        return NULL;
    }
    cJSON_AddItemToObject(json, JSON_FIELD_UPTIME, uptime_item);
    
    // Convert JSON object to string
    char *json_string = cJSON_Print(json);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON");
        cJSON_Delete(json);
        return NULL;
    }
    
    ESP_LOGD(TAG, "Created JSON payload: %s", json_string);
    
    // Cleanup JSON object (string is now independent)
    cJSON_Delete(json);
    
    return json_string;
}

/*
 * Validate JSON Format
 */
bool http_client_validate_json(const char *json_string)
{
    if (!json_string) {
        return false;
    }
    
    cJSON *json = cJSON_Parse(json_string);
    if (json == NULL) {
        return false;
    }
    
    cJSON_Delete(json);
    return true;
}

/*
 * Internal function to perform HTTP POST request
 */
static esp_err_t perform_http_post(const char *url, const char *json_data)
{
    if (!url || !json_data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Sending HTTP POST to: %s", url);
    ESP_LOGD(TAG, "JSON payload: %s", json_data);
    
    // Reset last response data
    memset(&s_context.last_response, 0, sizeof(s_context.last_response));
    s_context.last_response.response_data = s_context.response_buffer;
    if (s_context.response_buffer) {
        s_context.response_buffer[0] = '\0';
    }
    
    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .buffer_size = 1024,                  // HTTP client buffer size
        .buffer_size_tx = 1024,               // Transmit buffer size
    };
    
    // Create HTTP client
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        s_context.stats.failed_requests++;
        return ESP_FAIL;
    }
    
    // Set HTTP headers
    esp_http_client_set_header(client, "Content-Type", HTTP_CONTENT_TYPE);
    esp_http_client_set_header(client, "User-Agent", HTTP_USER_AGENT);
    
    // Set POST data
    esp_http_client_set_post_field(client, json_data, strlen(json_data));
    
    // Update statistics
    s_context.stats.total_requests++;
    s_context.stats.last_request_time = esp_timer_get_time();
    
    // Perform HTTP request
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        // Get response information
        s_context.last_response.status_code = esp_http_client_get_status_code(client);
        s_context.last_response.content_length = esp_http_client_get_content_length(client);
        s_context.stats.last_status_code = s_context.last_response.status_code;
        
        ESP_LOGI(TAG, "HTTP POST completed - Status: %d, Content-Length: %d", 
                 s_context.last_response.status_code, s_context.last_response.content_length);
        
        // Check if status code indicates success
        if (s_context.last_response.status_code >= 200 && s_context.last_response.status_code < 300) {
            s_context.last_response.success = true;
            s_context.stats.successful_requests++;
            ESP_LOGI(TAG, "Data successfully sent to API");
        } else {
            s_context.last_response.success = false;
            s_context.stats.failed_requests++;
            ESP_LOGW(TAG, "API returned non-success status code: %d", s_context.last_response.status_code);
            err = ESP_FAIL;
        }
    } else {
        s_context.stats.failed_requests++;
        if (err == ESP_ERR_TIMEOUT) {
            s_context.stats.timeout_count++;
            ESP_LOGE(TAG, "HTTP POST request timeout");
        } else {
            s_context.stats.network_errors++;
            ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        }
    }
    
    // Cleanup
    esp_http_client_cleanup(client);
    
    return err;
}

/*
 * Send Sensor Data to Default API Endpoint
 */
esp_err_t http_client_post_sensor_data(const sensor_data_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_context.initialized) {
        ESP_LOGE(TAG, "HTTP client not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Create JSON payload
    char *json_string = http_client_create_json(data);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON payload");
        s_context.stats.failed_requests++;
        return ESP_FAIL;
    }
    
    // Send HTTP request
    esp_err_t result = perform_http_post(API_ENDPOINT, json_string);
    
    // Free JSON string
    free(json_string);
    
    return result;
}

/*
 * Send Custom JSON Data
 */
esp_err_t http_client_post_json(const char *json_data)
{
    if (!json_data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_context.initialized) {
        ESP_LOGE(TAG, "HTTP client not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validate JSON format
    if (!http_client_validate_json(json_data)) {
        ESP_LOGE(TAG, "Invalid JSON format");
        return ESP_ERR_INVALID_ARG;
    }
    
    return perform_http_post(API_ENDPOINT, json_data);
}

/*
 * Send Data to Custom Endpoint
 */
esp_err_t http_client_post_to_endpoint(const sensor_data_t *data, const char *endpoint_url)
{
    if (!data || !endpoint_url) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_context.initialized) {
        ESP_LOGE(TAG, "HTTP client not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Create JSON payload
    char *json_string = http_client_create_json(data);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON payload");
        s_context.stats.failed_requests++;
        return ESP_FAIL;
    }
    
    // Send HTTP request to custom endpoint
    esp_err_t result = perform_http_post(endpoint_url, json_string);
    
    // Free JSON string
    free(json_string);
    
    return result;
}

/*
 * Get Last HTTP Response
 */
esp_err_t http_client_get_last_response(http_response_t *response)
{
    if (!response) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_context.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_context.stats.total_requests == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    
    *response = s_context.last_response;
    return ESP_OK;
}

/*
 * Get HTTP Client Statistics
 */
esp_err_t http_client_get_stats(http_client_stats_t *stats)
{
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *stats = s_context.stats;
    return ESP_OK;
}

/*
 * Reset HTTP Client Statistics
 */
esp_err_t http_client_reset_stats(void)
{
    if (!s_context.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Resetting HTTP client statistics...");
    
    // Preserve initialization flag
    bool was_initialized = s_context.stats.initialized;
    memset(&s_context.stats, 0, sizeof(s_context.stats));
    s_context.stats.initialized = was_initialized;
    
    return ESP_OK;
}

/*
 * Test API Endpoint Connectivity
 */
esp_err_t http_client_test_connectivity(void)
{
    if (!s_context.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Testing connectivity to: %s", API_ENDPOINT);
    
    // Create a simple test JSON payload
    const char *test_json = "{\"test\":\"connectivity\"}";
    
    esp_err_t result = perform_http_post(API_ENDPOINT, test_json);
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Connectivity test successful");
    } else {
        ESP_LOGW(TAG, "Connectivity test failed");
    }
    
    return result;
}

/*
 * HTTP Client Cleanup
 */
esp_err_t http_client_cleanup(void)
{
    if (!s_context.initialized) {
        return ESP_OK;  // Already cleaned up
    }
    
    ESP_LOGI(TAG, "Cleaning up HTTP client...");
    
    // Free response buffer
    if (s_context.response_buffer) {
        free(s_context.response_buffer);
        s_context.response_buffer = NULL;
    }
    
    // Reset context
    memset(&s_context, 0, sizeof(s_context));
    
    ESP_LOGI(TAG, "HTTP client cleanup completed");
    return ESP_OK;
} 