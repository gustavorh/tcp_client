/*
 * WiFi Manager Module
 * 
 * Provides a clean abstraction layer for WiFi connectivity management.
 * Handles connection, disconnection, retry logic, and status monitoring.
 * 
 * Features:
 * - Automatic connection with configurable retry logic
 * - Event-driven connection status monitoring
 * - ESP-IDF error handling patterns
 * - Extensible for multiple network configurations
 * 
 * Usage:
 *   esp_err_t ret = wifi_manager_init();
 *   ESP_ERROR_CHECK(ret);
 *   
 *   ret = wifi_manager_connect();
 *   if (ret == ESP_OK) {
 *       // WiFi connected successfully
 *   }
 *   
 *   if (wifi_manager_is_connected()) {
 *       // Ready for network operations
 *   }
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * WiFi Connection Status Enumeration
 */
typedef enum {
    WIFI_STATUS_DISCONNECTED = 0,    // Not connected to any network
    WIFI_STATUS_CONNECTING,          // Currently attempting connection
    WIFI_STATUS_CONNECTED,           // Successfully connected with IP
    WIFI_STATUS_FAILED,              // Connection failed after all retries
    WIFI_STATUS_ERROR                // Critical error occurred
} wifi_status_t;

/*
 * WiFi Manager Initialization
 * 
 * Initializes the WiFi subsystem, TCP/IP stack, and event handling.
 * Must be called before any other WiFi manager functions.
 * This function sets up the infrastructure but doesn't start connection.
 * 
 * Returns:
 *   ESP_OK: WiFi manager initialized successfully
 *   ESP_FAIL: Initialization failed
 *   ESP_ERR_*: Other ESP-IDF specific errors
 */
esp_err_t wifi_manager_init(void);

/*
 * Connect to WiFi Network
 * 
 * Attempts to connect to the WiFi network configured in menuconfig.
 * This function blocks until connection succeeds, fails after max retries,
 * or encounters a critical error.
 * 
 * Connection process:
 * 1. Validates WiFi manager is initialized
 * 2. Starts WiFi station mode
 * 3. Attempts connection with configured credentials
 * 4. Retries on failure up to maximum retry count
 * 5. Returns result
 * 
 * Returns:
 *   ESP_OK: Successfully connected to WiFi with IP address
 *   ESP_FAIL: Connection failed after maximum retries
 *   ESP_ERR_INVALID_STATE: WiFi manager not initialized
 *   ESP_ERR_*: Other connection-specific errors
 */
esp_err_t wifi_manager_connect(void);

/*
 * Check WiFi Connection Status
 * 
 * Returns the current WiFi connection status without blocking.
 * Useful for periodic status checks and connection validation.
 * 
 * Returns:
 *   true: WiFi is connected and has valid IP address
 *   false: WiFi is not connected or connection is unstable
 */
bool wifi_manager_is_connected(void);

/*
 * Get Detailed WiFi Status
 * 
 * Returns detailed connection status information including
 * connection state, retry attempts, and error conditions.
 * 
 * Returns:
 *   wifi_status_t: Current detailed status
 */
wifi_status_t wifi_manager_get_status(void);

/*
 * Get WiFi Signal Strength (RSSI)
 * 
 * Returns the current signal strength in dBm.
 * Only valid when connected to a network.
 * 
 * Parameters:
 *   rssi: Pointer to store RSSI value
 * 
 * Returns:
 *   ESP_OK: RSSI retrieved successfully
 *   ESP_ERR_WIFI_NOT_CONNECT: Not connected to network
 *   ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t wifi_manager_get_rssi(int8_t *rssi);

/*
 * Get Current IP Address
 * 
 * Retrieves the current IP address assigned to the ESP32.
 * Only valid when connected to a network.
 * 
 * Parameters:
 *   ip_info: Pointer to store IP information
 * 
 * Returns:
 *   ESP_OK: IP information retrieved successfully
 *   ESP_ERR_WIFI_NOT_CONNECT: Not connected to network
 *   ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t *ip_info);

/*
 * Disconnect from WiFi Network
 * 
 * Gracefully disconnects from the current WiFi network.
 * Useful for power management or network switching.
 * 
 * Returns:
 *   ESP_OK: Successfully disconnected
 *   ESP_ERR_WIFI_NOT_CONNECT: Already disconnected
 *   ESP_FAIL: Disconnection failed
 */
esp_err_t wifi_manager_disconnect(void);

/*
 * WiFi Manager Cleanup
 * 
 * Cleans up WiFi manager resources and shuts down WiFi subsystem.
 * Should be called during application shutdown.
 * After calling this function, wifi_manager_init() must be called
 * again before using other functions.
 * 
 * Returns:
 *   ESP_OK: Cleanup completed successfully
 *   ESP_FAIL: Cleanup failed
 */
esp_err_t wifi_manager_cleanup(void);

/*
 * Get Connection Retry Count
 * 
 * Returns the number of connection attempts made during the
 * current or last connection process.
 * 
 * Returns:
 *   int: Number of retry attempts (0 = first attempt successful)
 */
int wifi_manager_get_retry_count(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H 