#!/bin/bash

# File: setup.sh
# Description: Setup script for installing dependencies and building ResourceMonitor on Linux.

echo "=== ResourceMonitor Setup Script for Linux ==="

# Update package lists
echo "Updating package lists..."
sudo apt-get update

# Install required packages
echo "Installing required packages..."
sudo apt-get install -y \
    build-essential \
    cmake \
    qt5-default \
    libsqlite3-dev \
    libqt5widgets5 \
    libqt5gui5 \
    libqt5core5a \
    doxygen \
    graphviz \
    git \
    libgtest-dev

# Install Google Test
echo "Installing Google Test..."
cd /usr/src/gtest
sudo cmake CMakeLists.txt
sudo make
sudo cp lib/*.a /usr/lib

# Clone the repository (if not already cloned)
# Uncomment the following lines if you haven't cloned the repository yet.
# echo "Cloning ResourceMonitor repository..."
# git clone https://github.com/yourusername/ResourceMonitor.git
# cd ResourceMonitor

# Build the project
echo "Building the project..."
mkdir -p build
cd build
cmake ..
make

echo "=== Build completed successfully! ==="

# Optionally, run the application
# echo "Running ResourceMonitorCLI..."
# ./ResourceMonitorCLI

