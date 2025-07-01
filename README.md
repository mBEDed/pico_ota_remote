# Raspberry Pi Pico W OTA Updater

This project enables Over-The-Air (OTA) updates for your Raspberry Pi Pico W using Arduino IDE and GitHub releases.

## Features

- Automatic firmware updates from GitHub releases
- WiFi connectivity management
- Progress monitoring during updates
- Error handling and rollback capabilities
- Configurable update intervals

## Setup Instructions

### 1. Hardware Requirements
- Raspberry Pi Pico W
- USB cable for initial programming
- WiFi network access

### 2. Arduino IDE Setup
1. Install Arduino IDE (version 2.0 or later recommended)
2. Add the Raspberry Pi Pico board support:
   - Go to File → Preferences
   - Add this URL to Additional Boards Manager URLs: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
   - Go to Tools → Board → Board Manager
   - Search for "pico" and install "Raspberry Pi Pico/RP2040"

### 3. Required Libraries
Install these libraries via Arduino IDE Library Manager:
- ArduinoJson (by Benoit Blanchon)
- LittleFS_Mbed_RP2040 (or equivalent filesystem library)

### 4. Configuration
1. Open `config.h` and update:
   - `WIFI_SSID`: Your WiFi network name
   - `WIFI_PASSWORD`: Your WiFi password
   - `GITHUB_USER`: Your GitHub username
   - `GITHUB_REPO`: Your repository name
   - `GITHUB_TOKEN`: GitHub personal access token (optional for public repos)

### 5. GitHub Repository Setup
1. Create a new repository on GitHub
2. Push this code to your repository
3. Enable GitHub Actions in your repository settings

### 6. Creating Releases
To trigger an automatic build and release:
```bash
git tag v1.0.0
git push origin v1.0.0
```

The GitHub Action will automatically build the firmware and create a release with the binary file.

## Usage

1. Upload the initial firmware to your Pico W using Arduino IDE (open pico_ota_remote.ino)
2. The device will connect to WiFi and check for updates every 5 minutes
3. When you push a new version tag to GitHub, the device will automatically download and install the update
4. Monitor the serial output to see update progress

## Troubleshooting

- Ensure your WiFi credentials are correct
- Check that your GitHub repository is accessible
- Verify that releases contain `.bin` files
- Monitor serial output for debugging information

## License

This project is open source. Feel free to modify and distribute as needed.
