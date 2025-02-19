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
    )

    # Windows-specific libraries
    set(PLATFORM_LIBRARIES
        ws2_32
        iphlpapi
        psapi
        pdh
	wbemuuid
    )

    # Specify the path to SQLite3 on Windows
    set(SQLITE3_INCLUDE_DIRS "C:/sqlite3")
    set(SQLITE3_LIBRARIES "C:/sqlite3/sqlite3.lib")
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
    )

    # Linux-specific libraries
    set(PLATFORM_LIBRARIES
        pthread
        ${PCAP_LIBRARIES}
    )

    # Find SQLite3 on Linux
    find_package(SQLite3 REQUIRED)
    set(SQLITE3_INCLUDE_DIRS ${SQLite3_INCLUDE_DIRS})
    set(SQLITE3_LIBRARIES ${SQLite3_LIBRARIES})

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
    ${CMAKE_CURRENT_SOURCE_DIR}/database
    ${SQLITE3_INCLUDE_DIRS}
)

if(UNIX)
    target_include_directories(ResourceCore PUBLIC ${PCAP_INCLUDE_DIRS})
endif()

# Link libraries
target_link_libraries(ResourceCore PUBLIC
    ${SQLITE3_LIBRARIES}
    ${PLATFORM_LIBRARIES}
)

message(STATUS "SQLITE3_INCLUDE_DIRS: ${SQLITE3_INCLUDE_DIRS}")
message(STATUS "SQLITE3_LIBRARIES: ${SQLITE3_LIBRARIES}")

if(UNIX)
    message(STATUS "PCAP_INCLUDE_DIR: ${PCAP_INCLUDE_DIR}")
    message(STATUS "PCAP_LIBRARY: ${PCAP_LIBRARY}")
endif()

