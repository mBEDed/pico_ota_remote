# Pico W OTA Updater Setup Script
# This script helps you configure and set up the OTA updater

Write-Host "=== Raspberry Pi Pico W OTA Updater Setup ===" -ForegroundColor Green
Write-Host ""

# Check if Arduino IDE is installed
$arduinoPath = Get-Command "arduino-cli" -ErrorAction SilentlyContinue
if (-not $arduinoPath) {
    Write-Host "Arduino CLI not found. Please install Arduino CLI first:" -ForegroundColor Yellow
    Write-Host "1. Download from: https://github.com/arduino/arduino-cli/releases"
    Write-Host "2. Or install via chocolatey: choco install arduino-cli"
    Write-Host ""
}

# Check if Git is available
$gitPath = Get-Command "git" -ErrorAction SilentlyContinue
if (-not $gitPath) {
    Write-Host "Git not found. Please install Git first:" -ForegroundColor Yellow
    Write-Host "Download from: https://git-scm.com/download/win"
    Write-Host ""
} else {
    Write-Host "✓ Git is installed" -ForegroundColor Green
}

# Function to update config.h
function Update-Config {
    $configFile = "config.h"
    
    Write-Host "Current configuration:" -ForegroundColor Cyan
    Get-Content $configFile | Select-String "YOUR_" | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
    Write-Host ""
    
    $wifiSSID = Read-Host "Enter your WiFi SSID"
    $wifiPassword = Read-Host "Enter your WiFi Password" -AsSecureString
    $wifiPasswordText = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($wifiPassword))
    $githubUser = Read-Host "Enter your GitHub username"
    $githubRepo = Read-Host "Enter your GitHub repository name"
    
    # Update config.h
    $content = Get-Content $configFile
    $content = $content -replace 'YOUR_WIFI_SSID', $wifiSSID
    $content = $content -replace 'YOUR_WIFI_PASSWORD', $wifiPasswordText
    $content = $content -replace 'YOUR_GITHUB_USERNAME', $githubUser
    $content = $content -replace 'YOUR_REPOSITORY_NAME', $githubRepo
    
    $content | Set-Content $configFile
    Write-Host "✓ Configuration updated!" -ForegroundColor Green
}

# Function to create GitHub repository
function New-GitHubRepo {
    $repoName = Read-Host "Enter repository name for GitHub"
    
    Write-Host "Creating GitHub repository..." -ForegroundColor Cyan
    Write-Host "Please create the repository manually at: https://github.com/new" -ForegroundColor Yellow
    Write-Host "Repository name: $repoName" -ForegroundColor Yellow
    Write-Host ""
    
    $repoUrl = Read-Host "Enter the GitHub repository URL (e.g., https://github.com/username/repo.git)"
    
    # Add remote and push
    git remote add origin $repoUrl
    Write-Host "✓ Remote repository added" -ForegroundColor Green
    
    return $repoName
}

# Function to install Arduino libraries
function Install-ArduinoLibraries {
    Write-Host "Installing required Arduino libraries..." -ForegroundColor Cyan
    
    if ($arduinoPath) {
        arduino-cli core update-index
        arduino-cli core install rp2040:rp2040
        arduino-cli lib install "ArduinoJson"
        arduino-cli lib install "LittleFS_Mbed_RP2040"
        Write-Host "✓ Arduino libraries installed" -ForegroundColor Green
    } else {
        Write-Host "Please install these libraries manually in Arduino IDE:" -ForegroundColor Yellow
        Write-Host "  - ArduinoJson (by Benoit Blanchon)" -ForegroundColor Yellow
        Write-Host "  - LittleFS_Mbed_RP2040" -ForegroundColor Yellow
    }
}

# Main setup flow
Write-Host "Choose setup option:" -ForegroundColor Cyan
Write-Host "1. Full setup (configure everything)"
Write-Host "2. Update configuration only"
Write-Host "3. Install Arduino libraries only"
Write-Host "4. Create GitHub repository"
Write-Host "5. Exit"

$choice = Read-Host "Enter your choice (1-5)"

switch ($choice) {
    "1" {
        Update-Config
        Install-ArduinoLibraries
        $repoName = New-GitHubRepo
        
        Write-Host ""
        Write-Host "=== Setup Complete! ===" -ForegroundColor Green
        Write-Host "Next steps:" -ForegroundColor Cyan
        Write-Host "1. Push code to GitHub: git push -u origin main"
        Write-Host "2. Open Arduino IDE and load pico_ota_updater.ino"
        Write-Host "3. Select board: Raspberry Pi Pico W"
        Write-Host "4. Upload the code to your Pico W"
        Write-Host "5. Create a release: git tag v1.0.0 && git push origin v1.0.0"
    }
    "2" {
        Update-Config
    }
    "3" {
        Install-ArduinoLibraries
    }
    "4" {
        New-GitHubRepo
    }
    "5" {
        Write-Host "Setup cancelled." -ForegroundColor Yellow
        exit
    }
    default {
        Write-Host "Invalid choice. Please run the script again." -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "Setup script completed!" -ForegroundColor Green
