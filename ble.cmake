set(LIBFMT_VENDORIZE OFF)
set(SIMPLEBLE_INSTALL OFF)
add_subdirectory(3rdparty/SimpleBLE/simpleble "${CMAKE_BINARY_DIR}/simpleble-build")

set(ble-database_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/bluetooth-numbers-database/")

set(BLE_SERVICE_UUIDS_JSON "${ble-database_SOURCE_DIR}/v1/service_uuids.json")
file(READ "${BLE_SERVICE_UUIDS_JSON}" BLE_SERVICE_UUIDS)
string(JSON BLE_SERVICE_COUNT LENGTH "${BLE_SERVICE_UUIDS}")
math(EXPR BLE_SERVICE_COUNT "${BLE_SERVICE_COUNT} - 1")

function(ble_reformat_uuid var)
    set(varname "${var}")
    set(val "${${var}}")
    string(LENGTH "${val}" len)
    if("${len}" EQUAL 4)
        set("${var}" "0000${val}-0000-1000-8000-00805F9B34FB" PARENT_SCOPE)
    endif()
endfunction()

set(BLE_SERVICE_ARRAY "")
foreach(IDX RANGE ${BLE_SERVICE_COUNT})
    string(JSON BLE_SERVICE_NAME GET "${BLE_SERVICE_UUIDS}" "${IDX}" name)
    string(JSON BLE_SERVICE_UUID GET "${BLE_SERVICE_UUIDS}" "${IDX}" uuid)

    ble_reformat_uuid(BLE_SERVICE_UUID)
    string(APPEND BLE_SERVICE_ARRAY "ble_services_table[\"${BLE_SERVICE_UUID}\"] = \"${BLE_SERVICE_NAME}\";\n")
endforeach()

set(BLE_CHAR_UUIDS_JSON "${ble-database_SOURCE_DIR}/v1/characteristic_uuids.json")
file(READ "${BLE_CHAR_UUIDS_JSON}" BLE_CHAR_UUIDS)
string(JSON BLE_CHAR_COUNT LENGTH "${BLE_CHAR_UUIDS}")
math(EXPR BLE_CHAR_COUNT "${BLE_CHAR_COUNT} - 1")
set(BLE_CHAR_ARRAY "")
foreach(IDX RANGE ${BLE_CHAR_COUNT})
    string(JSON BLE_CHAR_NAME GET "${BLE_CHAR_UUIDS}" "${IDX}" name)
    string(JSON BLE_CHAR_UUID GET "${BLE_CHAR_UUIDS}" "${IDX}" uuid)

    ble_reformat_uuid(BLE_CHAR_UUID)
    string(APPEND BLE_CHAR_ARRAY "ble_characteristic_table[\"${BLE_CHAR_UUID}\"] = \"${BLE_CHAR_NAME}\";\n")
endforeach()

set(BLE_DESC_UUIDS_JSON "${ble-database_SOURCE_DIR}/v1/descriptor_uuids.json")
file(READ "${BLE_DESC_UUIDS_JSON}" BLE_DESC_UUIDS)
string(JSON BLE_DESC_COUNT LENGTH "${BLE_DESC_UUIDS}")
math(EXPR BLE_DESC_COUNT "${BLE_DESC_COUNT} - 1")
set(BLE_DESC_ARRAY "")
foreach(IDX RANGE ${BLE_DESC_COUNT})
    string(JSON BLE_DESC_NAME GET "${BLE_DESC_UUIDS}" "${IDX}" name)
    string(JSON BLE_DESC_UUID GET "${BLE_DESC_UUIDS}" "${IDX}" uuid)

    ble_reformat_uuid(BLE_DESC_UUID)
    string(APPEND BLE_DESC_ARRAY "ble_descriptor_table[\"${BLE_DESC_UUID}\"] = \"${BLE_DESC_NAME}\";\n")
endforeach()

file(CONFIGURE
  OUTPUT
    "${PROJECT_BINARY_DIR}/ble_service_map.cpp"
  CONTENT
"#include <ossia/detail/case_insensitive.hpp>\n\
namespace ossia {\n\
using ble_map_type = ossia::case_insensitive_string_map<std::string>;\n\
const ble_map_type& ble_service_map() {\n\
  static const ble_map_type res = [] {\n\
    ble_map_type ble_services_table;\n\
    ${BLE_SERVICE_ARRAY}\n\
    return ble_services_table;\n\
  }();\n\
  return res;\n\
}\n\
}\n\
")

file(CONFIGURE
  OUTPUT
    "${PROJECT_BINARY_DIR}/ble_characteristic_map.cpp"
  CONTENT
"#include <ossia/detail/case_insensitive.hpp>\n\
namespace ossia {\n\
using ble_map_type = ossia::case_insensitive_string_map<std::string>;\n\
const ble_map_type& ble_characteristic_map() {\n\
  static const ble_map_type res = [] {\n\
    ble_map_type ble_characteristic_table;\n\
    ${BLE_CHAR_ARRAY}\n\
    return ble_characteristic_table;\n\
  }();\n\
  return res;\n\
}\n\
}\n\
")

file(CONFIGURE
  OUTPUT
    "${PROJECT_BINARY_DIR}/ble_descriptor_map.cpp"
  CONTENT
  "#include <ossia/detail/case_insensitive.hpp>\n\
  namespace ossia {\n\
  using ble_map_type = ossia::case_insensitive_string_map<std::string>;\n\
  const ble_map_type& ble_descriptor_map() {\n\
    static const ble_map_type res = [] {\n\
      ble_map_type ble_descriptor_table;\n\
      ${BLE_DESC_ARRAY}\n\
      return ble_descriptor_table;\n\
    }();\n\
  return res;\n\
  }\n\
  }\n\
  "
)
