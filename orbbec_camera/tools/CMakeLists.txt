cmake_minimum_required(VERSION 3.10)
project(orbbec_cli_tool)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect architecture and map to SDK directory name
if(NOT CMAKE_SYSTEM_PROCESSOR)
  execute_process(COMMAND uname -m OUTPUT_VARIABLE CMAKE_SYSTEM_PROCESSOR OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# Map architecture name to SDK lib directory
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
  set(SDK_ARCH "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "amd64")
  set(SDK_ARCH "x64")
else()
  message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Find OrbbecSDK
set(ORBBEC_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../SDK)
set(ORBBEC_SDK_INCLUDE ${ORBBEC_SDK_DIR}/include)
set(ORBBEC_SDK_LIB_DIR ${ORBBEC_SDK_DIR}/lib/${SDK_ARCH})

# Check if SDK exists
if(NOT EXISTS ${ORBBEC_SDK_INCLUDE}/libobsensor/ObSensor.hpp)
  message(FATAL_ERROR "OrbbecSDK not found at ${ORBBEC_SDK_DIR}")
endif()

# Include directories
include_directories(${ORBBEC_SDK_INCLUDE})

# Link directories
link_directories(${ORBBEC_SDK_LIB_DIR})

# Build orbbec_cli
add_executable(orbbec_cli orbbec_cli.cpp)
target_link_libraries(orbbec_cli OrbbecSDK)

# Install targets to current directory for easy access
install(TARGETS orbbec_cli
  RUNTIME DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}
)

# Print build info
message(STATUS "========================================")
message(STATUS "Building Orbbec CLI Tool")
message(STATUS "  SDK Directory: ${ORBBEC_SDK_DIR}")
message(STATUS "  SDK Lib Directory: ${ORBBEC_SDK_LIB_DIR}")
message(STATUS "  System Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  SDK Architecture: ${SDK_ARCH}")
message(STATUS "========================================")
