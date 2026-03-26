# File: src/core/CMakeLists.txt

# Platform-specific source files and libraries
if(WIN32)
    # Windows-specific source files
    set(PLATFORM_SOURCES
        # CPU
        cpu/cpu_windows.cpp
        cpu/cpu_windows.h

        # Memory
        memory/memory_windows.cpp
        memory/memory_windows.h

        # Network
        network/network_windows.cpp
        network/network_windows.h

        # Disk
        disk/disk_windows.cpp
        disk/disk_windows.h

        # GPU
        gpu/gpu_windows.cpp
        gpu/gpu_windows.h

        # Process
        process/process_windows.cpp
        process/process_windows.h
    )

    # Windows-specific libraries
    set(PLATFORM_LIBRARIES
        ws2_32
        iphlpapi
        psapi
        pdh
        wbemuuid
        advapi32
        dxgi
    )

    # SQLite3 is provided by the root CMakeLists via FetchContent
elseif(UNIX)
    # Linux-specific source files
    set(PLATFORM_SOURCES
        # CPU
        cpu/cpu_linux.cpp
        cpu/cpu_linux.h

        # Memory
        memory/memory_linux.cpp
        memory/memory_linux.h

        # Network
        network/network_linux.cpp
        network/network_linux.h

        # Disk
        disk/disk_linux.cpp
        disk/disk_linux.h

        # GPU
        gpu/gpu_linux.cpp
        gpu/gpu_linux.h

        # Process
        process/process_linux.cpp
        process/process_linux.h
    )

    # Linux-specific libraries
    set(PLATFORM_LIBRARIES
        pthread
        dl
    )

    # SQLite3 is provided by the root CMakeLists via FetchContent

else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Add the ResourceCore library
add_library(ResourceCore
    # Common sources
    # CPU
    cpu/cpu_common.cpp
    cpu/cpu_common.h
    cpu/cpu_factory.cpp  # Added CPU factory

    # Memory
    memory/memory_common.cpp
    memory/memory_common.h
    memory/memory_factory.cpp  # Added Memory factory

    # Network
    network/network_common.cpp
    network/network_common.h
    network/network_factory.cpp  # Include the factory unconditionally

    # Disk
    disk/disk_common.cpp
    disk/disk_common.h
    disk/disk_factory.cpp

    # GPU
    gpu/gpu_common.cpp
    gpu/gpu_common.h
    gpu/gpu_factory.cpp

    # Process
    process/process_common.cpp
    process/process_common.h
    process/process_factory.cpp

    # System Info
    system_info/system_info.cpp
    system_info/system_info.h

    # Alerts
    alerts/alert_manager.cpp
    alerts/alert_manager.h

    # Database
    database/database.cpp
    database/database.h

    # Platform-specific sources
    ${PLATFORM_SOURCES}
)

# Include directories
target_include_directories(ResourceCore PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu
    ${CMAKE_CURRENT_SOURCE_DIR}/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/disk
    ${CMAKE_CURRENT_SOURCE_DIR}/gpu
    ${CMAKE_CURRENT_SOURCE_DIR}/process
    ${CMAKE_CURRENT_SOURCE_DIR}/system_info
    ${CMAKE_CURRENT_SOURCE_DIR}/alerts
    ${CMAKE_CURRENT_SOURCE_DIR}/database
)

# Link libraries
target_link_libraries(ResourceCore PUBLIC
    sqlite3
    ${PLATFORM_LIBRARIES}
)


