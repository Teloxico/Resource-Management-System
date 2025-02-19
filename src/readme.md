# ResourceMonitor

ResourceMonitor is a cross-platform resource monitoring tool that provides real-time information on CPU, memory, and network usage. It features both a Command-Line Interface (CLI) and a Graphical User Interface (GUI) built with Qt. The application logs data and can store metrics in an SQLite database, making it a comprehensive tool for system monitoring and analysis.

- [Setup Instructions](#setup-instructions)
  - [Prerequisites](#prerequisites)
  - [Linux Setup](#linux-setup)
  - [Windows Setup](#windows-setup)
- [Modules Overview](#modules-overview)
  - [CLI Module](#cli-module)
  - [GUI Module](#gui-module)
  - [Core Module](#core-module)
    - [CPU Monitoring](#cpu-monitoring)
    - [Memory Monitoring](#memory-monitoring)
    - [Network Monitoring](#network-monitoring)
  - [Database Module](#database-module)
  - [Utilities Module](#utilities-module)
  - [Tests Module](#tests-module)

## Setup Instructions

### Prerequisites

Before running the setup scripts, ensure that you have:

- An active internet connection: Required for downloading packages and dependencies.
- Administrative privileges: Necessary for installing software and modifying system settings.

### Linux Setup

#### Step-by-Step Instructions

1. **Make the Script Executable**

   ```bash
   chmod +x setup.sh
   ```

2. **Run the Setup Script**

   ```bash
   ./setup.sh
   ```

   The script will:
   - Update package lists.
   - Install required packages and dependencies.
   - Compile Google Test libraries.
   - Build the project using CMake.

3. **Run the Application**

   Navigate to the build directory and run the CLI application:

   ```bash
   cd build
   ./ResourceMonitorCLI
   ```

   To run the GUI application (if built):

   ```bash
   ./ResourceMonitorGUI
   ```

### Windows Setup

#### Step-by-Step Instructions


1. **Review the Setup Script**

   It's advisable to review the setup.ps1 script before running it:

   ```powershell
   Get-Content .\setup.ps1
   ```

2. **Set Execution Policy**

   Open PowerShell as Administrator and set the execution policy to allow script execution:

   ```powershell
   Set-ExecutionPolicy RemoteSigned
   ```

3. **Run the Setup Script**

   ```powershell
   .\setup.ps1
   ```

   The script will:
   - Install Chocolatey (if not already installed).
   - Install required packages and dependencies.
   - Clone and build Google Test libraries.
   - Build the project using CMake.

4. **Run the Application**

   Navigate to the build directory and run the CLI application:

   ```powershell
   cd build
   .\Release\ResourceMonitorCLI.exe
   ```

   To run the GUI application (if built):

   ```powershell
   .\Release\ResourceMonitorGUI.exe
   ```
   
##Module Review

- **Location**: `src/cli/`
- **Description**: Provides a Command-Line Interface for interacting with the ResourceMonitor. Users can execute commands to retrieve real-time system metrics directly from the terminal.

### GUI Module

- **Location**: `src/gui/`
- **Description**: Offers a Graphical User Interface built with Qt. It visually presents system metrics, allowing users to monitor resources through interactive charts and dashboards.

### Core Module

- **Location**: `src/core/`
- **Description**: Contains the core functionalities of the application, divided into sub-modules for CPU, memory, network monitoring, and database interactions.

#### CPU Monitoring

- **Location**: `src/core/cpu/`
- **Description**: Interfaces with the operating system to gather CPU metrics such as usage percentage, clock frequency, and thread count.

#### Memory Monitoring

- **Location**: `src/core/memory/`
- **Description**: Monitors memory metrics including total and available memory, usage percentage, and identifies top memory-consuming processes.

#### Network Monitoring

- **Location**: `src/core/network/`
- **Description**: Tracks network metrics such as upload/download rates, total bandwidth usage, and identifies processes consuming the most bandwidth.

### Database Module

- **Location**: `src/core/database/`
- **Description**: Manages data persistence using SQLite. Stores collected metrics for historical analysis and logging purposes.

### Utilities Module

- **Location**: `src/utils/`
- **Description**: Provides utility functions and classes used across different modules, including logging mechanisms and helper functions.

### Tests Module

- **Location**: `src/tests/`
- **Description**: Contains unit and integration tests for various modules using the Google Test framework. Ensures the reliability and correctness of the application.

