// ======================== Combined OTA Updater (Streaming Flash Version) ========================

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>

// ======================== Configuration ========================

// WiFi Configuration
 

// Firmware Version
#define CURRENT_VERSION "1.0.0"  // Must be string

// OTA Check Settings
#define UPDATE_CHECK_INTERVAL_MS 30000  // Check every 30 seconds
#define MAX_DOWNLOAD_RETRIES 3
#define CONNECTION_TIMEOUT_MS 10000

// Serial Settings
#define ENABLE_SERIAL_DEBUG true
#define SERIAL_BAUD_RATE 115200

// ======================== Global Variables ========================

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* github_user = GITHUB_USER;
const char* github_repo = GITHUB_REPO;
const char* github_token = GITHUB_TOKEN;

const String FIRMWARE_VERSION = CURRENT_VERSION;
const unsigned long UPDATE_CHECK_INTERVAL = UPDATE_CHECK_INTERVAL_MS;
unsigned long lastUpdateCheck = 0;

// ======================== Function Declarations ========================

void connectToWiFi();
void checkForUpdates();
String getLatestReleaseInfo();
bool downloadAndFlashFirmware(String downloadUrl);
void restartDevice();

// ======================== Setup ========================

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);

  Serial.println("[INFO] Starting setup...");
  if (!LittleFS.begin()) {
    Serial.println("[ERROR] Failed to initialize LittleFS");
    return;
  }

  connectToWiFi();
  Serial.println("[READY] Setup complete. Device ready.");
}

// ======================== Loop ========================

void loop() {
  if (millis() - lastUpdateCheck > UPDATE_CHECK_INTERVAL) {
    Serial.println("\n[OTA] Checking for updates...");
    checkForUpdates();
    lastUpdateCheck = millis();
  }

  Serial.println("[APP] Running main application...");
  delay(5000);
}

// ======================== WiFi Connection ========================

void connectToWiFi() {
  Serial.println("[WIFI] Attempting to connect...");
  WiFi.disconnect();
  delay(1000);

  WiFi.begin(ssid, password);
  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    Serial.print(".");
    delay(1000);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Connected successfully!");
    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WIFI] Failed to connect.");
    Serial.print("[WIFI] Status Code: ");
    Serial.println(WiFi.status());
    Serial.print("[WIFI] SSID: ");
    Serial.println(ssid);
  }
}

// ======================== OTA Update Check ========================

void checkForUpdates() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[OTA] Not connected to WiFi. Reconnecting...");
    connectToWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[OTA] Reconnection failed. Skipping update check.");
      return;
    } else {
      Serial.println("[OTA] Reconnected successfully!");
    }
  }

  String releaseInfo = getLatestReleaseInfo();
  if (releaseInfo.length() == 0) {
    Serial.println("[OTA] Failed to fetch release information");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, releaseInfo);
  if (error) {
    Serial.print("[ERROR] JSON parse failed: ");
    Serial.println(error.f_str());
    return;
  }

  String latestVersion = doc["tag_name"].as<String>();
  String downloadUrl = "";
  JsonArray assets = doc["assets"];

  for (JsonObject asset : assets) {
    String name = asset["name"].as<String>();
    if (name.endsWith(".bin") || name.endsWith(".ino.bin")) {
      downloadUrl = asset["browser_download_url"].as<String>();
      break;
    }
  }

  if (downloadUrl.length() == 0) {
    Serial.println("[OTA] No firmware file found in release assets");
    return;
  }

  Serial.println("[OTA] Latest version: " + latestVersion);
  Serial.println("[OTA] Current version: " + FIRMWARE_VERSION);

  if (latestVersion != FIRMWARE_VERSION) {
    Serial.println("[OTA] New update available. Downloading...");
    if (downloadAndFlashFirmware(downloadUrl)) {
      Serial.println("[SUCCESS] Update complete! Restarting...");
      delay(2000);
      restartDevice();
    } else {
      Serial.println("[ERROR] Update failed");
    }
  } else {
    Serial.println("[OTA] Already up-to-date");
  }
}

// ======================== GitHub Release Info ========================

String getLatestReleaseInfo() {
  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  String url = "https://api.github.com/repos/" + String(github_user) + "/" + String(github_repo) + "/releases/latest";
  Serial.println("[HTTP] Requesting: " + url);

  http.setTimeout(CONNECTION_TIMEOUT_MS);
  http.begin(secureClient, url);
  if (strlen(github_token) > 0) {
    http.addHeader("Authorization", "token " + String(github_token));
  }
  http.addHeader("User-Agent", "PicoW-OTA-Updater");

  int httpCode = http.GET();
  String payload = "";

  if (httpCode == HTTP_CODE_OK) {
    payload = http.getString();
  } else {
    Serial.printf("[HTTP] GET failed with code: %d\n", httpCode);
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
  return payload;
}

// ======================== Firmware Download and Flash ========================

bool downloadAndFlashFirmware(String downloadUrl) {
  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(CONNECTION_TIMEOUT_MS);
  http.begin(secureClient, downloadUrl);

  if (strlen(github_token) > 0) {
    http.addHeader("Authorization", "token " + String(github_token));
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[ERROR] Download failed, code: %d\n", httpCode);
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0 || contentLength > 1024 * 1024) {
    Serial.println("[ERROR] Invalid firmware size");
    http.end();
    return false;
  }

  WiFiClient* client = http.getStreamPtr();
  uint32_t flashOffset = 0x200000;
  uint32_t offset = 0;
  uint8_t buffer[FLASH_PAGE_SIZE];

  Serial.printf("[FLASH] Writing %d bytes directly to flash...\n", contentLength);

  // uint32_t ints = save_and_disable_interrupts();
  Serial.printf("[SYNC] Disabled Intrrupts");
  uint32_t sectors = (contentLength + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
  for (uint32_t i = 0; i < sectors; i++) {
    flash_range_erase(flashOffset + i * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    Serial.printf("[FLASH] Erased sector %lu\n", i);
  }
  // restore_interrupts(ints);

  unsigned long start = millis();
  const unsigned long timeout = 10000; // 10 seconds no data = fail

  while (offset < contentLength) {
    if (!client->connected() && !client->available()) {
      if (millis() - start > timeout) {
        Serial.println("[ERROR] Timeout waiting for data");
        http.end();
        return false;
      }
      delay(10);
      continue;
    }

    size_t available = client->available();
    if (available > 0) {
      size_t readSize = min(sizeof(buffer), available);
      int bytesRead = client->readBytes(buffer, readSize);
      if (bytesRead > 0) {
        flash_range_program(flashOffset + offset, buffer, bytesRead);
        offset += bytesRead;
        Serial.printf("[FLASH] Written %lu/%d bytes\n", offset, contentLength);
        start = millis(); // reset timeout
      }
    } else {
      delay(1);
    }
  }

  http.end();
  if (offset != contentLength) {
    Serial.printf("[ERROR] Incomplete write: %lu/%d\n", offset, contentLength);
    return false;
  }

  Serial.println("[FLASH] Flash update complete.");
  return true;
}

void restartDevice() {
  Serial.println("[SYSTEM] Restarting...");
  delay(500);
  watchdog_enable(1, 1);
  while (1);
}
