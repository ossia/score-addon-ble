#pragma once
#include "simpleble/Types.h"
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/context_functions.hpp>

#include <boost/container/flat_map.hpp>

#include <simpleble/SimpleBLE.h>
#include <cstdint>
#include <QCborStreamReader>

namespace ossia::net
{

}
namespace ossia
{

/**
 * This id denotes a special manufacturer data that we knwo should contain CBOR encoded data.
 */
constexpr int special_ble_cbor_id = 0xffff;

/**
 * Expose the manufacturer data found in BLE advertisements as child nodes of device_node.
 * This will call expose_cbor_as_ossia_nodes if it detects the special_ble_cbor_id.
 */
void expose_manufacturer_data_as_ossia_nodes(ossia::net::node_base& device_node, const std::map<uint16_t, SimpleBLE::ByteArray>& manufacturer_data);

/**
 * basically qt's CBOR string reading example. could be replaced by a readAllString() call in qt 6.7 but I'm developing with qt 6.2.
 * returns a QString with the content or an empty QString if there was an error.
 */
QString read_next_cbor_string(QCborStreamReader& reader);

/**
 * Tries to parse the manufacturer data bytes as a subset of CBOR and expose the values as nodes.
 * The supported subset of CBOR is a single map with string keys and float|double|int|bool|string values.
 * Returns false if there was an error.
 *
 * In case of error, this may still expose some of the data if there was valid data to expose.
 */
bool expose_cbor_as_ossia_nodes(ossia::net::node_base& device_node, const SimpleBLE::ByteArray& cbor_data);

class OSSIA_EXPORT ble_protocol final : public ossia::net::protocol_base
{
public:
  explicit ble_protocol(
      ossia::net::network_context_ptr, std::string_view adapter,
      std::string_view serial);
  ~ble_protocol();

private:
  void set_device(ossia::net::device_base& dev) override;
  bool pull(net::parameter_base&) override;
  bool push(const net::parameter_base&, const ossia::value& v) override;
  bool push_raw(const net::full_parameter_data&) override;
  bool observe(net::parameter_base&, bool) override;
  bool update(net::node_base& node_base) override;

  void scan_services();

  SimpleBLE::Adapter m_adapter;
  SimpleBLE::Peripheral m_peripheral;

  ossia::net::device_base* m_device{};
  ossia::net::network_context_ptr m_context;
  ossia::net::strand_type m_strand;

  using ble_param_id = std::pair<std::string, SimpleBLE::Characteristic>;
  boost::container::flat_map<const ossia::net::parameter_base*, ble_param_id> m_params;
};

class OSSIA_EXPORT ble_scan_protocol final : public ossia::net::protocol_base
{
public:
  explicit ble_scan_protocol(ossia::net::network_context_ptr, std::string_view adapter);
  ~ble_scan_protocol();

private:
  void set_device(ossia::net::device_base& dev) override;
  bool pull(net::parameter_base&) override;
  bool push(const net::parameter_base&, const ossia::value& v) override;
  bool push_raw(const net::full_parameter_data&) override;
  bool observe(net::parameter_base&, bool) override;
  bool update(net::node_base& node_base) override;

  void scan_services();

  SimpleBLE::Adapter m_adapter;

  ossia::net::device_base* m_device{};
  ossia::net::network_context_ptr m_context;
  ossia::net::strand_type m_strand;
};
}
