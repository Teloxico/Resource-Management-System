# File: setup.ps1
# Description: Setup script for installing dependencies and building ResourceMonitor on Windows.

Write-Host "=== ResourceMonitor Setup Script for Windows ==="

# Function to check if Chocolatey is installed
Function Is-ChocolateyInstalled {
    Get-Command choco.exe -ErrorAction SilentlyContinue -ne $null
}

# Install Chocolatey if not installed
If (-Not (Is-ChocolateyInstalled)) {
    Write-Host "Chocolatey not found. Installing Chocolatey..."
    Set-ExecutionPolicy Bypass -Scope Process -Force
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
} else {
    Write-Host "Chocolatey is already installed."
}

# Refresh environment variables
RefreshEnv

# Install required packages
Write-Host "Installing required packages..."
choco install -y cmake git doxygen graphviz sqlite visualstudio2019buildtools

# Install Qt (this may take some time)
Write-Host "Installing Qt..."
choco install -y qt5-default

# Clone the repository (if not already cloned)
# Uncomment the following lines if you haven't cloned the repository yet.
# Write-Host "Cloning ResourceMonitor repository..."
# git clone https://github.com/yourusername/ResourceMonitor.git
# Set-Location ResourceMonitor

# Install Google Test (gtest)
Write-Host "Installing Google Test (gtest)..."

# Clone Google Test repository
git clone https://github.com/google/googletest.git
Set-Location googletest
mkdir build
Set-Location build
cmake ..
cmake --build . --config Release
# Copy the built libraries to a known location or adjust your CMakeLists.txt accordingly.

# Return to project root
Set-Location ../..

# Build the project
Write-Host "Building the project..."
mkdir build
Set-Location build
cmake ..
cmake --build . --config Release

Write-Host "=== Build completed successfully! ==="

# Optionally, run the application
# Write-Host "Running ResourceMonitorCLI..."
# .\Release\ResourceMonitorCLI.exe

