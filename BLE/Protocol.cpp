#include "Protocol.hpp"

#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_functions.hpp>

#include <ossia/detail/config.hpp>

#include <ossia/detail/case_insensitive.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/context.hpp>

#include <simpleble/SimpleBLE.h>
#include <string>

namespace ossia
{
using ble_map_type = ossia::case_insensitive_string_map<std::string>;
const ble_map_type& ble_service_map();
const ble_map_type& ble_characteristic_map();
const ble_map_type& ble_descriptor_map();

void expose_manufacturer_data_as_ossia_nodes(
    ossia::net::node_base& device_node,
    const std::map<uint16_t, SimpleBLE::ByteArray>& manufacturer_data)
{
  for(const auto& [id, data] : manufacturer_data)
  {
    bool got_good_cbor = false;
    // If the id is the special BLE CBOR id, we have a special advertisement containing CBOR data
    if(id == ossia::special_ble_cbor_id)
    {
      // Try to parse the CBOR and expose the data found as a side effect
      got_good_cbor = ossia::expose_cbor_as_ossia_nodes(device_node, data);
    }
    // If we didn't get good CBOR, expose the raw data as bytes in addition to whatever good data we had found during
    // CBOR parsing
    if(!got_good_cbor)
    {
      ossia::net::node_base& data_node
          = ossia::net::find_or_create_node(device_node, std::to_string(id));
      auto param = data_node.create_parameter(ossia::val_type::STRING);
      param->set_value(data);
    }
  }
}

bool expose_cbor_as_ossia_nodes(ossia::net::node_base& device_node, const SimpleBLE::ByteArray& cbor_data)
{
  QCborStreamReader reader;
  reader.addData(cbor_data.data(), cbor_data.size());
  if(!reader.isMap())
  {
    // Right now, we only support non nested CBOR maps. Anything else would be kind of a waste of the precious 27 bytes of data
    // BLE adverts can give us anyways...
    return false;
  }
  reader.enterContainer();
  while (reader.lastError() == QCborError::NoError && reader.hasNext()) {
    // This only supports string keys. It will simply giveup if you give it a map with silly keys like
    // a CBOR flag or a float.
    std::string key;
    if(reader.isString())
    {
      key = read_next_cbor_string(reader).toStdString();
    }
    else
    {
      // Skip the entire pair if the key is not a string
      reader.next();
      reader.next();
      continue;
    }
    ossia::net::node_base& data_node
          = ossia::net::find_or_create_node(device_node, key);
    // gets the value. Only supports string, int, float and double.
    if(reader.isBool())
    {
      auto param = data_node.create_parameter(ossia::val_type::BOOL);
      param->set_value(reader.toBool());
      reader.next();
    }
    else if(reader.isString())
    {
      std::string val = read_next_cbor_string(reader).toStdString();
      auto param = data_node.create_parameter(ossia::val_type::STRING);
      param->set_value(val);
    }
    else if(reader.isInteger())
    {
      auto param = data_node.create_parameter(ossia::val_type::INT);
      // This will overflow if the cbor encoded a very large int.
      param->set_value(static_cast<int>(reader.toInteger()));
      reader.next();
    }
    else if(reader.isFloat())
    {
      auto param = data_node.create_parameter(ossia::val_type::FLOAT);
      param->set_value(reader.toFloat());
      reader.next();
    }
    else if(reader.isDouble())
    {
      auto param = data_node.create_parameter(ossia::val_type::FLOAT);
      param->set_value(static_cast<float>(reader.toDouble()));
      reader.next();
    }
    else
    {
      // anything else we just ignore and skip over.
      reader.next();
    }
  }
  if(reader.lastError() == QCborError::NoError)
  {
    reader.leaveContainer();
    return true;
  }
  return false;
}

QString read_next_cbor_string(QCborStreamReader &reader)
{
  QString result;
  if(!reader.isString())
  {
    return result;
  }
  auto r = reader.readString();
  while (r.status == QCborStreamReader::Ok) {
    result += r.data;
    r = reader.readString();
  }

  if (r.status == QCborStreamReader::Error) {
    // handle error condition
    result.clear();
  }
  return result;
}

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
        try {
          auto val = m_peripheral.read(service.uuid(), characteristic.uuid());
          param->set_value(val);
        } catch(...) {

        }
      }

      if(characteristic.can_write_request() || characteristic.can_write_command()
         || !characteristic.descriptors().empty())
      {
        m_params.emplace(param, ble_param_id{service.uuid(), characteristic});
      }

      if(characteristic.can_notify())
      {
        try {
        m_peripheral.notify(
            service.uuid(), characteristic.uuid(),
            [=](const SimpleBLE::ByteArray& arr) mutable { param->set_value(arr); });
        } catch(...) {

        }
      }
    }
  }
  ossia::expose_manufacturer_data_as_ossia_nodes(m_device->get_root_node(), m_peripheral.manufacturer_data());
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
    ossia::net::network_context_ptr ptr, ble_scan_configuration conf)
    : protocol_base{flags{SupportsMultiplex}}
    , m_context{ptr}
    , m_strand{boost::asio::make_strand(m_context->context)}
    , m_conf{std::move(conf)}
{
  // First look for the correct adapter, or take the first one if
  // the exact one cannot be found
  auto adapters = SimpleBLE::Adapter::get_adapters();
  if(adapters.empty())
    return;
  for(auto& adapter : adapters)
  {
    if(conf.adapter == adapter.address())
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

static auto name_or_uuid(const ble_map_type& map, const std::string& uuid)
    -> const std::string&
{
  if(auto it = map.find(uuid); it != map.end())
    return it->second;
  else
    return uuid;
}

void ble_scan_protocol::scan_services()
{
  if(!m_device)
    return;
  auto& service_names = ble_service_map();
  auto& char_names = ble_characteristic_map();
  auto& desc_names = ble_descriptor_map();

  for(auto peripheral : m_adapter.scan_get_results())
  {
    auto services = peripheral.services();
    apply_filters(services);
    if(services.empty())
      continue;

    std::string periph_name = peripheral.identifier().empty() ? peripheral.address()
                                                              : peripheral.identifier();
    ossia::net::sanitize_name(periph_name);
    auto& prp_node
        = ossia::net::find_or_create_node(m_device->get_root_node(), periph_name);
    for(auto& service : peripheral.services())
    {
      auto& svc_node = ossia::net::find_or_create_node(
          prp_node, name_or_uuid(service_names, service.uuid()));
      auto param = svc_node.create_parameter(ossia::val_type::STRING);

      param->set_value(service.data());
    }
    ossia::expose_manufacturer_data_as_ossia_nodes(
        prp_node, peripheral.manufacturer_data());
  }
}

void ble_scan_protocol::apply_filters(
    std::vector<SimpleBLE::Service>& services) const noexcept
{
  auto& service_names = ble_service_map();

  // 1. Filter out the excluded ones
  if(!this->m_conf.filter_exclude.empty())
  {
    for(auto it = services.begin(); it != services.end();)
    {
      auto& service = *it;
      if(ossia::contains(this->m_conf.filter_exclude, service.uuid()))
      {
        it = services.erase(it);
        continue;
      }
      else if(auto pretty_name_it = service_names.find(service.uuid());
              pretty_name_it != service_names.end())
      {
        if(ossia::contains(this->m_conf.filter_exclude, pretty_name_it->second))
        {
          it = services.erase(it);
          continue;
        }
      }
      ++it;
    }
  }

  // 2. Filter out the ones not in the include list
  if(!this->m_conf.filter_include.empty())
  {
    for(auto it = services.begin(); it != services.end();)
    {
      auto& service = *it;
      if(!ossia::contains(this->m_conf.filter_include, service.uuid()))
      {
        it = services.erase(it);
        continue;
      }
      else if(auto pretty_name_it = service_names.find(service.uuid());
              pretty_name_it != service_names.end())
      {
        if(!ossia::contains(this->m_conf.filter_exclude, pretty_name_it->second))
        {
          it = services.erase(it);
          continue;
        }
      }
      ++it;
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
  // FIXME we need to finish the strands before deleting this
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
