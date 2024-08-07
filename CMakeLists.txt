cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

if(NOT TARGET score_lib_base)
    include(ScoreExternalAddon)
endif()

project(score_addon_ble LANGUAGES CXX)
score_common_setup()

if(WIN32 AND NOT MSVC)
  if(OSSIA_SDK)
    # For cppwinrt headers that simpleble requires on windows.
    if(EXISTS "${OSSIA_SDK}/sysroot/include/winrt")
      include_directories("${OSSIA_SDK}/sysroot/include")
    else()
      find_path(WINRT_FOUNDATION_H_PATH winrt/Windows.Foundation.h)
      if(NOT WINRT_FOUNDATION_H_PATH)
        return()
      endif()
      include_directories("${WINRT_FOUNDATION_H_PATH}")
    endif()
  endif()
endif()

include(ble.cmake)
if(NOT TARGET simpleble)
  return()
endif()

# Necessary as SimpleBLE overwrites:
include(GenerateStaticExport)

add_library(score_addon_ble
    "${CMAKE_CURRENT_SOURCE_DIR}/score_addon_ble.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BLE/Protocol.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BLE/BLEDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BLE/BLEProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BLE/BLEProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BLE/BLESpecificSettings.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/score_addon_ble.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BLE/Protocol.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BLE/BLEDevice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BLE/BLEProtocolFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BLE/BLEProtocolSettingsWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/BLE/BLESpecificSettingsSerialization.cpp"

    "${PROJECT_BINARY_DIR}/ble_service_map.cpp"
    "${PROJECT_BINARY_DIR}/ble_characteristic_map.cpp"
    "${PROJECT_BINARY_DIR}/ble_descriptor_map.cpp"
)

target_link_libraries(score_addon_ble
  PUBLIC
    score_plugin_engine
    simpleble::simpleble
)

setup_score_plugin(score_addon_ble)
