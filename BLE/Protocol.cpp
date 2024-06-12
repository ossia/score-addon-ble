#include "Protocol.hpp"

#include <ossia/detail/config.hpp>

#include <ossia/detail/case_insensitive.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/context.hpp>

#include <simpleble/SimpleBLE.h>
namespace ossia
{
using ble_map_type = ossia::case_insensitive_string_map<std::string>;
const ble_map_type& ble_service_map();
const ble_map_type& ble_characteristic_map();
const ble_map_type& ble_descriptor_map();

// FIXME the addresses that are created on the fly maybe won't work
// if one does --auto-play
ble_protocol::ble_protocol(
    ossia::net::network_context_ptr ptr, std::string_view adapter_uuid,
    std::string_view serial)
    : protocol_base{flags{SupportsMultiplex}}
    , m_context{ptr}
    , m_strand{boost::asio::make_strand(m_context->context)}
{
  // First look for the correct adapter, or take the first one if
  // the exact one cannot be found
  auto adapters = SimpleBLE::Adapter::get_adapters();
  if(adapters.empty())
    return;
  for(auto& adapter : adapters)
  {
    if(adapter_uuid == adapter.address())
    {
      m_adapter = adapter;
      break;
    }
  }
  if(!m_adapter.initialized())
    m_adapter = adapters[0];

  if(m_adapter.initialized())
  {
    // Setup our callbacks
    m_adapter.set_callback_on_scan_start([] {});
    m_adapter.set_callback_on_scan_stop([] {});
    m_adapter.set_callback_on_scan_found(
        [this, serial = std::string(serial)](SimpleBLE::Peripheral p) {
      if(p.address() == serial)
      {
        m_peripheral = p;
        m_peripheral.connect();

        boost::asio::post(m_strand, [this] { scan_services(); });
      }
    });
    m_adapter.set_callback_on_scan_updated(
        [this, serial = std::string(serial)](SimpleBLE::Peripheral p) {
      if(p.address() == serial)
      {
        if(!p.is_connected())
          p.connect();

        boost::asio::post(m_strand, [this] { scan_services(); });
      }
    });
  }
}

void ble_protocol::scan_services()
{
  if(!m_device)
    return;
  auto& service_names = ble_service_map();
  auto& char_names = ble_characteristic_map();
  auto& desc_names = ble_descriptor_map();

  auto name_or_uuid
      = [&](const ble_map_type& map, const std::string& uuid) -> const std::string& {
    if(auto it = map.find(uuid); it != map.end())
      return it->second;
    else
      return uuid;
  };
  for(auto& service : m_peripheral.services())
  {
    auto& svc_node = ossia::net::find_or_create_node(
        m_device->get_root_node(), name_or_uuid(service_names, service.uuid()));
    auto param = svc_node.create_parameter(ossia::val_type::STRING);
    param->set_value(service.data());

    for(auto& characteristic : service.characteristics())
    {
      auto& chara_node = ossia::net::find_or_create_node(
          svc_node, name_or_uuid(char_names, characteristic.uuid()));
      auto param = chara_node.create_parameter(ossia::val_type::STRING);
      if(characteristic.can_read())
      {
        param->set_value(m_peripheral.read(service.uuid(), characteristic.uuid()));
      }

      if(characteristic.can_write_request() || characteristic.can_write_command()
         || !characteristic.descriptors().empty())
      {
        m_params.emplace(param, ble_param_id{service.uuid(), characteristic});
      }

      if(characteristic.can_notify())
      {
        m_peripheral.notify(
            service.uuid(), characteristic.uuid(),
            [=](const SimpleBLE::ByteArray& arr) mutable { param->set_value(arr); });
      }
    }
  }
  for(auto [id, data] : m_peripheral.manufacturer_data())
  {
    auto& data_node
        = ossia::net::find_or_create_node(m_device->get_root_node(), std::to_string(id));
    auto param = data_node.create_parameter(ossia::val_type::STRING);
    param->set_value(data);
  }
}

void ble_protocol::set_device(net::device_base& dev)
{
  m_device = &dev;
  m_adapter.scan_start();
}

ble_protocol::~ble_protocol()
{
  if(m_adapter.initialized())
    if(m_adapter.scan_is_active())
      m_adapter.scan_stop();

  if(m_peripheral.initialized())
    if(m_peripheral.is_connected())
      m_peripheral.disconnect();
}

bool ble_protocol::pull(ossia::net::parameter_base&)
{
  return false;
}

bool ble_protocol::push(const ossia::net::parameter_base& p, const ossia::value& v)
{
  if(m_peripheral.is_connected())
  {
    if(auto it = m_params.find(&p); it != m_params.end())
    {
      auto& [svc, chara] = it->second;

      if(chara.can_write_request())
        m_peripheral.write_request(svc, chara.uuid(), ossia::convert<std::string>(v));
      else if(chara.can_write_command())
        m_peripheral.write_request(svc, chara.uuid(), ossia::convert<std::string>(v));
      else if(!chara.descriptors().empty())
        m_peripheral.write(
            svc, chara.uuid(), chara.descriptors().front().uuid(),
            ossia::convert<std::string>(v));
    }
  }
  return false;
}

bool ble_protocol::push_raw(const ossia::net::full_parameter_data&)
{
  return false;
}

bool ble_protocol::observe(ossia::net::parameter_base&, bool)
{
  return false;
}

bool ble_protocol::update(ossia::net::node_base& node_base)
{
  return false;
}

// FIXME the addresses that are created on the fly maybe won't work
// if one does --auto-play
ble_scan_protocol::ble_scan_protocol(
    ossia::net::network_context_ptr ptr, std::string_view adapter_uuid)
    : protocol_base{flags{SupportsMultiplex}}
    , m_context{ptr}
    , m_strand{boost::asio::make_strand(m_context->context)}
{
  // First look for the correct adapter, or take the first one if
  // the exact one cannot be found
  auto adapters = SimpleBLE::Adapter::get_adapters();
  if(adapters.empty())
    return;
  for(auto& adapter : adapters)
  {
    if(adapter_uuid == adapter.address())
    {
      m_adapter = adapter;
      break;
    }
  }
  if(!m_adapter.initialized())
    m_adapter = adapters[0];

  if(m_adapter.initialized())
  {
    // Setup our callbacks
    m_adapter.set_callback_on_scan_start([] {});
    m_adapter.set_callback_on_scan_stop([] {});
    m_adapter.set_callback_on_scan_found([this](SimpleBLE::Peripheral p) {
      boost::asio::post(m_strand, [this] { scan_services(); });
    });
    m_adapter.set_callback_on_scan_updated([this](SimpleBLE::Peripheral p) {
      boost::asio::post(m_strand, [this] { scan_services(); });
    });
  }
}

void ble_scan_protocol::scan_services()
{
  if(!m_device)
    return;
  auto& service_names = ble_service_map();
  auto& char_names = ble_characteristic_map();
  auto& desc_names = ble_descriptor_map();

  auto name_or_uuid
      = [&](const ble_map_type& map, const std::string& uuid) -> const std::string& {
    if(auto it = map.find(uuid); it != map.end())
      return it->second;
    else
      return uuid;
  };

  for(auto& m_peripheral : m_adapter.scan_get_results())
  {
    std::string periph_name = m_peripheral.identifier().empty()
                                  ? m_peripheral.address()
                                  : m_peripheral.identifier();
    auto& prp_node
        = ossia::net::find_or_create_node(m_device->get_root_node(), periph_name);
    for(auto& service : m_peripheral.services())
    {
      auto& svc_node = ossia::net::find_or_create_node(
          prp_node, name_or_uuid(service_names, service.uuid()));
      auto param = svc_node.create_parameter(ossia::val_type::STRING);
      param->set_value(service.data());
    }
    for(auto [id, data] : m_peripheral.manufacturer_data())
    {
      auto& data_node = ossia::net::find_or_create_node(prp_node, std::to_string(id));
      auto param = data_node.create_parameter(ossia::val_type::STRING);
      param->set_value(data);
    }
  }
}

void ble_scan_protocol::set_device(net::device_base& dev)
{
  m_device = &dev;
  m_adapter.scan_start();
}

ble_scan_protocol::~ble_scan_protocol()
{
  if(m_adapter.initialized())
    m_adapter.scan_stop();
}

bool ble_scan_protocol::pull(ossia::net::parameter_base&)
{
  return false;
}

bool ble_scan_protocol::push(const ossia::net::parameter_base& p, const ossia::value& v)
{
  return false;
}

bool ble_scan_protocol::push_raw(const ossia::net::full_parameter_data&)
{
  return false;
}

bool ble_scan_protocol::observe(ossia::net::parameter_base&, bool)
{
  return false;
}

bool ble_scan_protocol::update(ossia::net::node_base& node_base)
{
  return false;
}
}
