#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
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
bool downloadAndInstallUpdate(String downloadUrl);
void performOTAUpdate(WiFiClient& client, size_t contentLength);

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
    Serial.println("New version available! Starting update...");
    if (downloadAndInstallUpdate(downloadUrl)) {
      Serial.println("Update successful! Rebooting...");
      delay(1000);
      ESP.restart();
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

bool downloadAndInstallUpdate(String downloadUrl) {
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
  
  WiFiClient* client = http.getStreamPtr();
  
  if (!Update.begin(contentLength)) {
    Serial.println("Not enough space for update");
    http.end();
    return false;
  }
  
  Serial.println("Starting update...");
  size_t written = 0;
  uint8_t buffer[1024];
  
  while (http.connected() && written < contentLength) {
    size_t available = client->available();
    if (available) {
      size_t bytesToRead = min(available, sizeof(buffer));
      size_t bytesRead = client->readBytes(buffer, bytesToRead);
      
      size_t bytesWritten = Update.write(buffer, bytesRead);
      written += bytesWritten;
      
      // Show progress
      if (written % 10240 == 0 || written == contentLength) {
        Serial.printf("Progress: %d/%d bytes (%.1f%%)\n", 
                     written, contentLength, (float)written / contentLength * 100);
      }
    }
    delay(1);
  }
  
  http.end();
  
  if (written == contentLength) {
    Serial.println("Update written successfully");
  } else {
    Serial.printf("Update incomplete: %d/%d bytes\n", written, contentLength);
    Update.abort();
    return false;
  }
  
  if (Update.end()) {
    Serial.println("Update completed successfully");
    return true;
  } else {
    Serial.printf("Update failed: %s\n", Update.errorString());
    return false;
  }
}
