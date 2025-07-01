#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include "config.h"

// WiFi credentials
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// GitHub repository details
const char* github_user = GITHUB_USER;
const char* github_repo = GITHUB_REPO;
const char* github_token = GITHUB_TOKEN; // Optional, for private repos

// Current firmware version
const String FIRMWARE_VERSION = CURRENT_VERSION;

// Update check interval (in milliseconds)
const unsigned long UPDATE_CHECK_INTERVAL = UPDATE_CHECK_INTERVAL_MS;
unsigned long lastUpdateCheck = 0;

// Function declarations
void connectToWiFi();
void checkForUpdates();
String getLatestReleaseInfo();
bool downloadFirmware(String downloadUrl);
bool downloadAndFlashFirmware(String downloadUrl);
void restartDevice();
void performFlashUpdate(uint8_t* firmwareData, size_t firmwareSize);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Starting Pico W OTA Updater");
  Serial.println("Current firmware version: " + FIRMWARE_VERSION);
  
  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Serial.println("Failed to initialize LittleFS");
    return;
  }
  
  // Connect to WiFi
  connectToWiFi();
  
  // Your main application code here
  Serial.println("Setup complete. Device ready.");
}

void loop() {
  // Check for updates periodically
  if (millis() - lastUpdateCheck > UPDATE_CHECK_INTERVAL) {
    checkForUpdates();
    lastUpdateCheck = millis();
  }
  
  // Your main application code here
  delay(1000);
  Serial.println("Running main application...");
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void checkForUpdates() {
  Serial.println("Checking for updates...");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping update check.");
    return;
  }
  
  String releaseInfo = getLatestReleaseInfo();
  if (releaseInfo.length() == 0) {
    Serial.println("Failed to get release information");
    return;
  }
  
  // Parse JSON response
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, releaseInfo);
  
  String latestVersion = doc["tag_name"];
  String downloadUrl = "";
  
  // Find the firmware file in assets
  JsonArray assets = doc["assets"];
  for (JsonObject asset : assets) {
    String name = asset["name"];
    if (name.endsWith(".bin") || name.endsWith(".ino.bin")) {
      downloadUrl = asset["browser_download_url"];
      break;
    }
  }
  
  if (downloadUrl.length() == 0) {
    Serial.println("No firmware file found in release assets");
    return;
  }
  
  Serial.println("Latest version: " + latestVersion);
  Serial.println("Current version: " + FIRMWARE_VERSION);
  
  if (latestVersion != FIRMWARE_VERSION) {
    Serial.println("New version available! Starting OTA update...");
    if (downloadAndFlashFirmware(downloadUrl)) {
      Serial.println("Update successful! Restarting...");
      delay(2000);
      restartDevice();
    } else {
      Serial.println("Update failed!");
    }
  } else {
    Serial.println("Already running latest version");
  }
}

String getLatestReleaseInfo() {
  HTTPClient http;
  String url = "https://api.github.com/repos/" + String(github_user) + "/" + String(github_repo) + "/releases/latest";
  
  http.begin(url);
  
  // Add authorization header if token is provided
  if (strlen(github_token) > 0) {
    http.addHeader("Authorization", "token " + String(github_token));
  }
  
  http.addHeader("User-Agent", "PicoW-OTA-Updater");
  
  int httpCode = http.GET();
  String payload = "";
  
  if (httpCode == HTTP_CODE_OK) {
    payload = http.getString();
  } else {
    Serial.printf("HTTP GET failed, error: %d\n", httpCode);
  }
  
  http.end();
  return payload;
}

bool downloadFirmware(String downloadUrl) {
  HTTPClient http;
  http.begin(downloadUrl);
  
  // Add authorization header if token is provided
  if (strlen(github_token) > 0) {
    http.addHeader("Authorization", "token " + String(github_token));
  }
  
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Download failed, HTTP code: %d\n", httpCode);
    http.end();
    return false;
  }
  
  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("Invalid content length");
    http.end();
    return false;
  }
  
  Serial.printf("Firmware size: %d bytes\n", contentLength);
  
  // Open file for writing
  File file = LittleFS.open("/firmware.bin", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    http.end();
    return false;
  }
  
  WiFiClient* client = http.getStreamPtr();
  Serial.println("Starting download...");
  size_t written = 0;
  uint8_t buffer[1024];
  
  while (http.connected() && written < contentLength) {
    size_t available = client->available();
    if (available) {
      size_t bytesToRead = min(available, sizeof(buffer));
      size_t bytesRead = client->readBytes(buffer, bytesToRead);
      
      size_t bytesWritten = file.write(buffer, bytesRead);
      written += bytesWritten;
      
      // Show progress
      if (written % 10240 == 0 || written == contentLength) {
        Serial.printf("Progress: %d/%d bytes (%.1f%%)\n", 
                     written, contentLength, (float)written / contentLength * 100);
      }
    }
    delay(1);
  }
  
  file.close();
  http.end();
  
  if (written == contentLength) {
    Serial.println("Firmware downloaded successfully to /firmware.bin");
    return true;
  } else {
    Serial.printf("Download incomplete: %d/%d bytes\n", written, contentLength);
    return false;
  }
}

bool downloadAndFlashFirmware(String downloadUrl) {
  HTTPClient http;
  http.begin(downloadUrl);
  
  // Add authorization header if token is provided
  if (strlen(github_token) > 0) {
    http.addHeader("Authorization", "token " + String(github_token));
  }
  
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Download failed, HTTP code: %d\n", httpCode);
    http.end();
    return false;
  }
  
  int contentLength = http.getSize();
  if (contentLength <= 0 || contentLength > 1024 * 1024) { // Max 1MB firmware
    Serial.println("Invalid content length or firmware too large");
    http.end();
    return false;
  }
  
  Serial.printf("Firmware size: %d bytes\n", contentLength);
  
  // Allocate memory for firmware
  uint8_t* firmwareBuffer = (uint8_t*)malloc(contentLength);
  if (!firmwareBuffer) {
    Serial.println("Failed to allocate memory for firmware");
    http.end();
    return false;
  }
  
  WiFiClient* client = http.getStreamPtr();
  Serial.println("Downloading firmware to memory...");
  size_t downloaded = 0;
  
  while (http.connected() && downloaded < contentLength) {
    size_t available = client->available();
    if (available) {
      size_t bytesToRead = min(available, (size_t)(contentLength - downloaded));
      size_t bytesRead = client->readBytes(firmwareBuffer + downloaded, bytesToRead);
      downloaded += bytesRead;
      
      // Show progress
      if (downloaded % 10240 == 0 || downloaded == contentLength) {
        Serial.printf("Download progress: %d/%d bytes (%.1f%%)\n", 
                     downloaded, contentLength, (float)downloaded / contentLength * 100);
      }
    }
    delay(1);
  }
  
  http.end();
  
  if (downloaded != contentLength) {
    Serial.printf("Download incomplete: %d/%d bytes\n", downloaded, contentLength);
    free(firmwareBuffer);
    return false;
  }
  
  Serial.println("Firmware downloaded successfully. Starting flash update...");
  
  // Perform flash update
  performFlashUpdate(firmwareBuffer, contentLength);
  
  free(firmwareBuffer);
  return true;
}

void performFlashUpdate(uint8_t* firmwareData, size_t firmwareSize) {
  Serial.println("WARNING: Starting flash update. Do not power off!");
  
  // Disable interrupts during flash operation
  uint32_t ints = save_and_disable_interrupts();
  
  // Calculate number of sectors to erase (each sector is 4KB)
  uint32_t sectorsToErase = (firmwareSize + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
  uint32_t flashOffset = 0x100000; // 1MB offset for OTA partition
  
  Serial.printf("Erasing %d sectors at offset 0x%08X\n", sectorsToErase, flashOffset);
  
  // Erase flash sectors
  for (uint32_t i = 0; i < sectorsToErase; i++) {
    flash_range_erase(flashOffset + (i * FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
    if (i % 10 == 0) {
      Serial.printf("Erased sector %d/%d\n", i + 1, sectorsToErase);
    }
  }
  
  Serial.println("Writing firmware to flash...");
  
  // Write firmware in 256-byte pages
  for (size_t offset = 0; offset < firmwareSize; offset += FLASH_PAGE_SIZE) {
    size_t writeSize = min((size_t)FLASH_PAGE_SIZE, firmwareSize - offset);
    
    flash_range_program(flashOffset + offset, firmwareData + offset, writeSize);
    
    if (offset % (FLASH_PAGE_SIZE * 10) == 0) {
      Serial.printf("Written %d/%d bytes\n", offset + writeSize, firmwareSize);
    }
  }
  
  // Restore interrupts
  restore_interrupts(ints);
  
  Serial.println("Flash update completed successfully!");
  Serial.println("Setting boot flag for new firmware...");
  
  // Set a flag to indicate successful update and which partition to boot from
  // This would require a custom bootloader to implement partition switching
  // For now, we'll restart and hope the new firmware is at the right location
}

void restartDevice() {
  Serial.println("Restarting device...");
  delay(1000);
  watchdog_enable(1, 1); // Enable watchdog with 1ms timeout to force restart
  while(1); // Wait for watchdog reset
}
