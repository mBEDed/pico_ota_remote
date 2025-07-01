// Configuration file for Pico W OTA Updater
// Modify these values according to your setup

#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "wifi"
#define WIFI_PASSWORD "password"

// GitHub Repository Configuration
#define GITHUB_USER "userHarpreet"
#define GITHUB_REPO "pico_ota_remote"
#define GITHUB_TOKEN ""  // Leave empty for public repos, add token for private repos

// Firmware Configuration
#define CURRENT_VERSION "1.0.0"  // Update this with each new version

// Update Settings
#define UPDATE_CHECK_INTERVAL_MS 300000  // 5 minutes (300000 ms)
#define MAX_DOWNLOAD_RETRIES 3
#define CONNECTION_TIMEOUT_MS 10000  // 10 seconds

// Debug Settings
#define ENABLE_SERIAL_DEBUG true
#define SERIAL_BAUD_RATE 115200

// OTA Settings
#define ENABLE_AUTO_UPDATE true  // Set to false to disable automatic updates
#define ENABLE_ROLLBACK true     // Enable rollback on failed updates

#endif
