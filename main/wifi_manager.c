/*
 * WiFi Manager Implementation
 * 
 * Implements WiFi connectivity management with clean abstraction,
 * proper error handling, and event-driven architecture.
 */

#include "wifi_manager.h"
#include "config.h"

#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

// Module logging tag
static const char *TAG = "WIFI_MGR";

// Module state management
typedef struct {
    bool initialized;
    wifi_status_t status;
    int retry_count;
    EventGroupHandle_t event_group;
    esp_netif_t *netif_instance;
    esp_event_handler_instance_t wifi_handler_instance;
    esp_event_handler_instance_t ip_handler_instance;
} wifi_manager_context_t;

// Global module context
static wifi_manager_context_t s_context = {
    .initialized = false,
    .status = WIFI_STATUS_DISCONNECTED,
    .retry_count = 0,
    .event_group = NULL,
    .netif_instance = NULL,
    .wifi_handler_instance = NULL,
    .ip_handler_instance = NULL
};

/*
 * Internal WiFi Event Handler
 * 
 * Handles all WiFi and IP events, updating module status and
 * managing connection retry logic.
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi station started, initiating connection...");
                s_context.status = WIFI_STATUS_CONNECTING;
                esp_wifi_connect();
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED:
                if (s_context.retry_count < WIFI_MAXIMUM_RETRY) {
                    esp_wifi_connect();
                    s_context.retry_count++;
                    ESP_LOGI(TAG, "Retrying WiFi connection (%d/%d)", 
                            s_context.retry_count, WIFI_MAXIMUM_RETRY);
                    s_context.status = WIFI_STATUS_CONNECTING;
                } else {
                    ESP_LOGE(TAG, "WiFi connection failed after %d attempts", 
                            WIFI_MAXIMUM_RETRY);
                    s_context.status = WIFI_STATUS_FAILED;
                    xEventGroupSetBits(s_context.event_group, WIFI_FAIL_BIT);
                }
                break;
                
            default:
                ESP_LOGD(TAG, "Unhandled WiFi event: %ld", event_id);
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                {
                    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                    ESP_LOGI(TAG, "Connected to WiFi! IP: " IPSTR, 
                            IP2STR(&event->ip_info.ip));
                    s_context.retry_count = 0;  // Reset retry counter on success
                    s_context.status = WIFI_STATUS_CONNECTED;
                    xEventGroupSetBits(s_context.event_group, WIFI_CONNECTED_BIT);
                }
                break;
                
            default:
                ESP_LOGD(TAG, "Unhandled IP event: %ld", event_id);
                break;
        }
    }
}

/*
 * Initialize WiFi Manager
 */
esp_err_t wifi_manager_init(void)
{
    if (s_context.initialized) {
        ESP_LOGW(TAG, "WiFi manager already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing WiFi manager...");
    
    // Create event group for WiFi synchronization
    s_context.event_group = xEventGroupCreate();
    if (s_context.event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi event group");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize TCP/IP stack
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        goto error_cleanup;
    }
    
    // Create default event loop
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        goto error_cleanup;
    }
    
    // Create default WiFi station interface
    s_context.netif_instance = esp_netif_create_default_wifi_sta();
    if (s_context.netif_instance == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi station interface");
        ret = ESP_FAIL;
        goto error_cleanup;
    }
    
    // Initialize WiFi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        goto error_cleanup;
    }
    
    // Register event handlers
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            &wifi_event_handler,
                                            NULL,
                                            &s_context.wifi_handler_instance);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        goto error_cleanup;
    }
    
    ret = esp_event_handler_instance_register(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            &wifi_event_handler,
                                            NULL,
                                            &s_context.ip_handler_instance);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        goto error_cleanup;
    }
    
    s_context.initialized = true;
    s_context.status = WIFI_STATUS_DISCONNECTED;
    s_context.retry_count = 0;
    
    ESP_LOGI(TAG, "WiFi manager initialized successfully");
    return ESP_OK;
    
error_cleanup:
    if (s_context.event_group) {
        vEventGroupDelete(s_context.event_group);
        s_context.event_group = NULL;
    }
    return ret;
}

/*
 * Connect to WiFi Network
 */
esp_err_t wifi_manager_connect(void)
{
    if (!s_context.initialized) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_context.status == WIFI_STATUS_CONNECTED) {
        ESP_LOGI(TAG, "Already connected to WiFi");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", WIFI_SSID);
    
    // Configure WiFi connection parameters
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_MODE,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    // Reset connection state
    s_context.retry_count = 0;
    s_context.status = WIFI_STATUS_CONNECTING;
    
    // Configure and start WiFi
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        s_context.status = WIFI_STATUS_ERROR;
        return ret;
    }
    
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        s_context.status = WIFI_STATUS_ERROR;
        return ret;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        s_context.status = WIFI_STATUS_ERROR;
        return ret;
    }
    
    // Wait for connection result
    EventBits_t bits = xEventGroupWaitBits(s_context.event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Successfully connected to WiFi");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi after %d attempts", WIFI_MAXIMUM_RETRY);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "WiFi connection timeout");
        s_context.status = WIFI_STATUS_FAILED;
        return ESP_ERR_TIMEOUT;
    }
}

/*
 * Check WiFi Connection Status
 */
bool wifi_manager_is_connected(void)
{
    return (s_context.initialized && s_context.status == WIFI_STATUS_CONNECTED);
}

/*
 * Get Detailed WiFi Status
 */
wifi_status_t wifi_manager_get_status(void)
{
    return s_context.status;
}

/*
 * Get WiFi Signal Strength (RSSI)
 */
esp_err_t wifi_manager_get_rssi(int8_t *rssi)
{
    if (!rssi) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!wifi_manager_is_connected()) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        *rssi = ap_info.rssi;
    }
    
    return ret;
}

/*
 * Get Current IP Address
 */
esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t *ip_info)
{
    if (!ip_info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!wifi_manager_is_connected()) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    
    return esp_netif_get_ip_info(s_context.netif_instance, ip_info);
}

/*
 * Disconnect from WiFi Network
 */
esp_err_t wifi_manager_disconnect(void)
{
    if (!s_context.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_context.status == WIFI_STATUS_DISCONNECTED) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    
    ESP_LOGI(TAG, "Disconnecting from WiFi...");
    
    esp_err_t ret = esp_wifi_disconnect();
    if (ret == ESP_OK) {
        s_context.status = WIFI_STATUS_DISCONNECTED;
    }
    
    return ret;
}

/*
 * WiFi Manager Cleanup
 */
esp_err_t wifi_manager_cleanup(void)
{
    if (!s_context.initialized) {
        return ESP_OK;  // Already cleaned up
    }
    
    ESP_LOGI(TAG, "Cleaning up WiFi manager...");
    
    // Disconnect if connected
    if (s_context.status == WIFI_STATUS_CONNECTED) {
        wifi_manager_disconnect();
    }
    
    // Stop WiFi
    esp_wifi_stop();
    
    // Unregister event handlers
    if (s_context.wifi_handler_instance) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                             s_context.wifi_handler_instance);
    }
    if (s_context.ip_handler_instance) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                             s_context.ip_handler_instance);
    }
    
    // Cleanup WiFi
    esp_wifi_deinit();
    
    // Cleanup event group
    if (s_context.event_group) {
        vEventGroupDelete(s_context.event_group);
        s_context.event_group = NULL;
    }
    
    // Reset context
    memset(&s_context, 0, sizeof(s_context));
    
    ESP_LOGI(TAG, "WiFi manager cleanup completed");
    return ESP_OK;
}

/*
 * Get Connection Retry Count
 */
int wifi_manager_get_retry_count(void)
{
    return s_context.retry_count;
} 