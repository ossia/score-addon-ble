#pragma once
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/context_functions.hpp>

#include <boost/container/flat_map.hpp>

#include <simpleble/SimpleBLE.h>

namespace ossia::net
{

}
namespace ossia
{

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
