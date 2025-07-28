# ESP32 TCP Client - Modular Architecture

This ESP32 application demonstrates a clean, modular approach to IoT development. It connects to WiFi and periodically sends system data (CPU temperature and uptime) to a REST API endpoint via HTTP POST requests.

## 🏗️ Architecture Overview

This project implements a **layered service-oriented architecture** with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────────┐
│                        main.c                              │
│                   (Application Orchestration)              │
├─────────────────────────────────────────────────────────────┤
│  wifi_manager  │  sensor_service  │  http_client  │ config.h │
│  (Connectivity)│  (Data Collection)│ (Communication)│ (Config) │
├─────────────────────────────────────────────────────────────┤
│                    ESP-IDF Framework                        │
└─────────────────────────────────────────────────────────────┘
```

### Key Design Principles

- **Separation of Concerns**: Each module has a single responsibility
- **Dependency Injection**: Services don't know about each other directly
- **Event-Driven**: Leverages FreeRTOS events for loose coupling
- **ESP-IDF Error Handling**: Proper error propagation and handling
- **Extensibility**: Easy to add new sensors, endpoints, or features

## 📁 Project Structure

```
tcp_client/
├── main/
│   ├── main.c              # Application orchestration
│   ├── config.h            # Centralized configuration
│   ├── wifi_manager.h/.c   # WiFi connectivity service
│   ├── sensor_service.h/.c # Data collection service
│   ├── http_client.h/.c    # HTTP communication service
│   ├── CMakeLists.txt      # Build configuration
│   └── Kconfig.projbuild   # Configuration options
├── CMakeLists.txt          # Project configuration
├── sdkconfig.defaults      # Default SDK settings
└── README.md              # This file
```

### Module Responsibilities

| Module             | Purpose                   | Key Features                                                         |
| ------------------ | ------------------------- | -------------------------------------------------------------------- |
| **main.c**         | Application orchestration | Service initialization, main loop, status monitoring                 |
| **config.h**       | Configuration management  | Centralized constants, menuconfig integration                        |
| **wifi_manager**   | Network connectivity      | WiFi connection, retry logic, status monitoring                      |
| **sensor_service** | Data collection           | Temperature simulation, uptime tracking, extensible for GPIO sensors |
| **http_client**    | API communication         | JSON creation, HTTP POST, response handling, statistics              |

## 🚀 Features

- 📡 **WiFi Connection Management**: Automatic connection with retry logic and status monitoring
- 🌡️ **Temperature Monitoring**: CPU temperature simulation with realistic variations
- ⏱️ **Uptime Tracking**: System uptime since boot in human-readable format
- 🔄 **Periodic Transmission**: Configurable data transmission intervals
- 📊 **JSON Payload**: Structured data format for API consumption
- 📈 **Statistics Tracking**: HTTP request statistics and sensor monitoring
- 🔧 **Modular Design**: Easy to extend with new sensors and endpoints
- ⚙️ **Configurable Settings**: All settings via menuconfig
- 🧪 **Unit Test Ready**: Clean interfaces enable easy testing

## 📊 Data Format

The application sends JSON data in the following format:

```json
{
  "cpu_temp": 25.4,
  "sys_uptime": "0h 2m 35s"
}
```

## 🛠️ Prerequisites

1. **ESP-IDF**: Version 4.4 or later
   - Follow the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
2. **Hardware**: ESP32 development board (ESP32-DevKitC, NodeMCU-32S, etc.)
3. **API Server**: REST API running at your specified endpoint that accepts POST requests

## 🚀 Quick Start

### 1. Clone and Navigate

```bash
cd tcp_client
```

### 2. Configure WiFi and API Settings

```bash
idf.py menuconfig
```

Navigate to: **TCP Client Configuration** and set:

- **WiFi SSID**: Your WiFi network name
- **WiFi Password**: Your WiFi password
- **API Endpoint URL**: Your REST API endpoint (default: `http://192.168.1.122:9000/api/esp32`)
- **Data transmission interval**: How often to send data (default: 10 seconds)
- **Maximum retry attempts**: WiFi connection retry limit (default: 5)

### 3. Build the Project

```bash
idf.py build
```

### 4. Flash to ESP32

```bash
idf.py -p /dev/ttyUSB0 flash
```

_Replace `/dev/ttyUSB0` with your ESP32's port (may be `/dev/ttyUSB1`, `COM3`, etc.)_

### 5. Monitor Output

```bash
idf.py -p /dev/ttyUSB0 monitor
```

Press `Ctrl+]` to exit the monitor.

## ⚙️ Configuration Options

All settings can be configured via `idf.py menuconfig` under **TCP Client Configuration**:

| Setting               | Description                 | Default                               |
| --------------------- | --------------------------- | ------------------------------------- |
| WiFi SSID             | Network name to connect to  | `YOUR_WIFI_SSID`                      |
| WiFi Password         | Network password            | `YOUR_WIFI_PASSWORD`                  |
| API Endpoint URL      | Complete REST API URL       | `http://192.168.1.122:9000/api/esp32` |
| Transmission Interval | Seconds between data sends  | `10`                                  |
| Maximum Retry         | WiFi connection retry limit | `5`                                   |

## 📝 Expected Console Output

```
I (1234) TCP_CLIENT: === ESP32 TCP Client - Modular Architecture ===
I (1245) TCP_CLIENT: Application: TCP_CLIENT v1.0.0
I (1256) WIFI_MGR: Initializing WiFi manager...
I (1267) SENSOR_SVC: Initializing sensor service...
I (1278) HTTP_CLIENT: Initializing HTTP client...
I (1289) WIFI_MGR: Connected to WiFi! IP: 192.168.1.100
I (1300) TCP_CLIENT: Signal strength: -45 dBm
I (1311) TCP_CLIENT: === Cycle 1 ===
I (1322) SENSOR_SVC: CPU temperature: 27.3°C
I (1333) TCP_CLIENT: Sensor data - Temperature: 27.3°C, Uptime: 0h 0m 12s
I (1344) HTTP_CLIENT: HTTP POST completed - Status: 200, Content-Length: 25
I (1355) TCP_CLIENT: Data transmission completed successfully
```

## 🔧 Extending the Application

### Adding New Sensors

1. **Define sensor in sensor_service.h**:

```c
typedef enum {
    SENSOR_TYPE_CPU_TEMP = 0,
    SENSOR_TYPE_UPTIME,
    SENSOR_TYPE_GPIO_ANALOG_1,    // Add new sensor type
    SENSOR_TYPE_MAX
} sensor_type_t;
```

2. **Update sensor data structure**:

```c
typedef struct {
    float cpu_temp;
    char uptime[32];
    float gpio_sensor_1;          // Add new sensor data
    bool data_valid;
} sensor_data_t;
```

3. **Implement sensor reading in sensor_service.c**
4. **Update JSON creation in http_client.c**

### Adding Multiple API Endpoints

1. **Define endpoint configurations in config.h**
2. **Use `http_client_post_to_endpoint()` function**
3. **Implement endpoint selection logic in main.c**

### Adding OTA Updates

1. **Create new `ota_manager.h/.c` module**
2. **Add OTA endpoint to configuration**
3. **Integrate OTA checks in main application loop**

## 🧪 Development Workflow

### Building and Testing

```bash
# Clean build
idf.py fullclean && idf.py build

# Flash and monitor in one command
idf.py flash monitor

# Build size analysis
idf.py size

# Component analysis
idf.py size-components
```

### Adding Unit Tests

The modular architecture enables easy unit testing:

1. Mock individual services
2. Test service interfaces independently
3. Use ESP-IDF's unit test framework

### Code Style

- Use ESP-IDF error handling patterns (`esp_err_t`, `ESP_ERROR_CHECK`)
- Document all public APIs with comprehensive comments
- Follow consistent naming conventions
- Keep modules focused on single responsibilities

## 🌐 API Server Requirements

Your REST API server should:

1. **Accept POST requests** at the configured endpoint
2. **Handle JSON content** with `Content-Type: application/json`
3. **Expect this JSON structure**:
   ```json
   {
     "cpu_temp": float,     // Temperature in Celsius
     "sys_uptime": string   // Format: "Xh Ym Zs"
   }
   ```

### Example Node.js Server (backend-iot)

A simple Express.js server is included in the `backend-iot` directory:

```bash
cd ../backend-iot
npm install
npm start
```

This creates an API endpoint at `http://localhost:9000/api/esp32` that accepts the ESP32 data.

## 🐛 Troubleshooting

### Build Issues

- ✅ Ensure ESP-IDF is properly installed and sourced
- ✅ Try cleaning the build: `idf.py fullclean`
- ✅ Check that all required components are available
- ✅ Verify all header files are properly included

### WiFi Connection Issues

- ✅ Verify SSID and password in menuconfig
- ✅ Check WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
- ✅ Ensure WiFi network is in range
- ✅ Check WiFi manager status: logs show detailed connection state

### HTTP Request Failures

- ✅ Verify API endpoint URL is correct
- ✅ Check if API server is running and accessible
- ✅ Ensure firewall allows connections on the specified port
- ✅ Monitor HTTP client statistics for detailed error information
- ✅ Test API endpoint manually with curl:
  ```bash
  curl -X POST -H "Content-Type: application/json" \
       -d '{"cpu_temp":25.5,"sys_uptime":"0h 1m 30s"}' \
       http://192.168.1.122:9000/api/esp32
  ```

### Module Integration Issues

- ✅ Verify all modules are included in `CMakeLists.txt`
- ✅ Check service initialization order in `main.c`
- ✅ Use service status functions to debug individual modules
- ✅ Monitor application status reports for service health

## 📚 Additional Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [ESP32 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [FreeRTOS Documentation](https://freertos.org/Documentation/RTOS_book.html)
- [cJSON Library Documentation](https://github.com/DaveGamble/cJSON)

## 📈 Performance Metrics

The modular architecture provides several performance benefits:

- **Memory Efficiency**: Each module manages its own memory
- **CPU Utilization**: Non-blocking operations and proper task management
- **Network Efficiency**: Connection pooling and retry logic
- **Debugging**: Isolated modules simplify troubleshooting

Monitor these metrics using the built-in status reporting system.

## 🤝 Contributing

This modular architecture makes contributions easier:

1. Fork the repository
2. Create a feature branch for your module
3. Implement your module following the established patterns
4. Add tests for your module
5. Update documentation
6. Submit a pull request

## 📄 License

This project is provided as-is for educational and development purposes.

---

**Note**: The CPU temperature is simulated since ESP32 doesn't have a built-in temperature sensor. For production applications requiring precise temperature measurements, consider using dedicated temperature sensors like DS18B20.
