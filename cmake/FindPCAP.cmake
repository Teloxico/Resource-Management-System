# FindPCAP.cmake

# This module helps CMake find the PCAP library and its headers.
#
# It defines the following variables:
#  - PCAP_FOUND: Set to TRUE if PCAP is found.
#  - PCAP_LIBRARIES: The PCAP library to link against.
#  - PCAP_INCLUDE_DIRS: The directory containing pcap.h.
#  - PCAP_VERSION_STRING: The version number of PCAP.

# Search for the directory containing pcap.h
find_path(PCAP_INCLUDE_DIR
  NAMES pcap.h
  PATHS /usr/include /usr/local/include /opt/local/include
)

# Search for the PCAP library
find_library(PCAP_LIBRARY
  NAMES pcap
  PATHS /usr/lib /usr/local/lib /opt/local/lib
)

# Include standard module to handle finding packages
include(FindPackageHandleStandardArgs)

# Use the standard method to handle the results of the search
find_package_handle_standard_args(PCAP
  REQUIRED_VARS PCAP_LIBRARY PCAP_INCLUDE_DIR
)

# If PCAP is found, set the necessary variables
if(PCAP_FOUND)
  # Set the libraries and include directories
  set(PCAP_LIBRARIES ${PCAP_LIBRARY})
  set(PCAP_INCLUDE_DIRS ${PCAP_INCLUDE_DIR})

  # Attempt to extract the PCAP version from pcap.h
  if(EXISTS "${PCAP_INCLUDE_DIR}/pcap/pcap.h")
    # Read the version string from pcap.h
    file(STRINGS "${PCAP_INCLUDE_DIR}/pcap/pcap.h" pcap_version_str REGEX "^#define[\t ]+PCAP_VERSION_STRING[\t ]+\".*\"")
    # Extract just the version number using a regex replace
    string(REGEX REPLACE "^#define[\t ]+PCAP_VERSION_STRING[\t ]+\"([^\"]*)\".*" "\\1" PCAP_VERSION_STRING "${pcap_version_str}")
    # Remove the temporary variable
    unset(pcap_version_str)
  endif()
endif()

# Mark variables as advanced to hide them from standard view in GUIs
mark_as_advanced(PCAP_INCLUDE_DIR PCAP_LIBRARY)

