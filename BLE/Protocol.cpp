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

#include <QBluetoothLocalDevice>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyDescriptor>
#include <string>

namespace ossia
{
using ble_map_type = ossia::case_insensitive_string_map<std::string>;
const ble_map_type& ble_service_map();
const ble_map_type& ble_characteristic_map();
const ble_map_type& ble_descriptor_map();

void expose_manufacturer_data_as_ossia_nodes(
    ossia::net::node_base& device_node,
    const QMultiHash<quint16, QByteArray>& manufacturer_data)
{
  for(auto it = manufacturer_data.begin(); it != manufacturer_data.end(); ++it)
  {
    const quint16 id = it.key();
    const QByteArray& data = it.value();

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
      param->set_value(std::string(data.data(), data.size()));
    }
  }
}

bool expose_cbor_as_ossia_nodes(ossia::net::node_base& device_node, const QByteArray& cbor_data)
{
  QCborStreamReader reader;
  reader.addData(cbor_data);
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
    , m_targetSerial{QString::fromUtf8(serial.data(), serial.size())}
    , m_context{ptr}
    , m_strand{boost::asio::make_strand(m_context->context)}
{
  // First look for the correct adapter, or take the first one if
  // the exact one cannot be found
  auto adapters = QBluetoothLocalDevice::allDevices();
  if(!adapters.empty())
  {
    bool found = false;
    QString adapter_str = QString::fromUtf8(adapter_uuid.data(), adapter_uuid.size());
    for(const auto& adapter : adapters)
    {
      if(adapter.address().toString() == adapter_str)
      {
        m_adapterAddress = adapter.address();
        found = true;
        break;
      }
    }
    if(!found)
      m_adapterAddress = adapters[0].address();
  }

  // Create discovery agent
  if(!m_adapterAddress.isNull())
  {
    m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(m_adapterAddress, this);
    m_discoveryAgent->setLowEnergyDiscoveryTimeout(5000);

    QObject::connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
                     this, &ble_protocol::onDeviceDiscovered);
    QObject::connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceUpdated,
                     this, &ble_protocol::onDeviceUpdated);
  }
  else
  {
    m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    m_discoveryAgent->setLowEnergyDiscoveryTimeout(5000);

    QObject::connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
                     this, &ble_protocol::onDeviceDiscovered);
    QObject::connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceUpdated,
                     this, &ble_protocol::onDeviceUpdated);
  }
}

void ble_protocol::onDeviceDiscovered(const QBluetoothDeviceInfo& info)
{
  // Check if this is the device we're looking for
  if(info.address().toString() == m_targetSerial || info.name() == m_targetSerial
     || info.deviceUuid().toString(QBluetoothUuid::WithoutBraces) == m_targetSerial)
  {
    // Stop discovery
    if(m_discoveryAgent)
      m_discoveryAgent->stop();

    // Create controller
    if(!m_adapterAddress.isNull())
      m_controller = QLowEnergyController::createCentral(info, m_adapterAddress, this);
    else
      m_controller = QLowEnergyController::createCentral(info, this);

    QObject::connect(m_controller, &QLowEnergyController::connected,
                     this, &ble_protocol::onConnected);
    QObject::connect(m_controller, &QLowEnergyController::disconnected,
                     this, &ble_protocol::onDisconnected);
    QObject::connect(m_controller, &QLowEnergyController::serviceDiscovered,
                     this, &ble_protocol::onServiceDiscovered);
    QObject::connect(m_controller, &QLowEnergyController::discoveryFinished,
                     this, &ble_protocol::onDiscoveryFinished);

    m_controller->connectToDevice();
  }
}

void ble_protocol::onDeviceUpdated(const QBluetoothDeviceInfo& info, QBluetoothDeviceInfo::Fields updatedFields)
{
  // Could handle manufacturer data updates here if needed
  if(m_controller && m_controller->state() != QLowEnergyController::ConnectedState)
  {
    if(info.address().toString() == m_targetSerial || info.name() == m_targetSerial)
    {
      // Expose manufacturer data
      if(m_device && updatedFields.testFlag(QBluetoothDeviceInfo::Field::ManufacturerData))
      {
        expose_manufacturer_data_as_ossia_nodes(m_device->get_root_node(), info.manufacturerData());
      }
    }
  }
}

void ble_protocol::onConnected()
{
  if(m_controller)
  {
    m_controller->discoverServices();
  }
}

void ble_protocol::onDisconnected()
{
  // Could attempt reconnection here
}

void ble_protocol::onServiceDiscovered(const QBluetoothUuid& service)
{
  // Services will be processed in onDiscoveryFinished
}

void ble_protocol::onDiscoveryFinished()
{
  boost::asio::post(m_strand, [this] { scan_services(); });
}

void ble_protocol::scan_services()
{
  if(!m_device || !m_controller)
    return;

  auto& service_names = ble_service_map();
  auto& char_names = ble_characteristic_map();

  auto name_or_uuid
      = [&](const ble_map_type& map, const QString& uuid) -> std::string {
    std::string uuid_str = uuid.toStdString();
    if(auto it = map.find(uuid_str); it != map.end())
      return it->second;
    else
      return uuid_str;
  };

  for(const auto& serviceUuid : m_controller->services())
  {
    QLowEnergyService* service = m_controller->createServiceObject(serviceUuid, this);
    if(!service)
      continue;

    m_services[serviceUuid] = service;

    auto& svc_node = ossia::net::find_or_create_node(
        m_device->get_root_node(),
        name_or_uuid(
            service_names, serviceUuid.toString(QBluetoothUuid::WithoutBraces)));
    auto svc_param = svc_node.create_parameter(ossia::val_type::STRING);

    // Discover service details
    QObject::connect(service, &QLowEnergyService::stateChanged,
                     this, [this, service, svc_param, &char_names](QLowEnergyService::ServiceState newState) {
      if(newState == QLowEnergyService::RemoteServiceDiscovered)
      {
        auto& svc_node = svc_param->get_node();

        for(const auto& characteristic : service->characteristics())
        {
          QString char_uuid = characteristic.uuid().toString();
          auto& chara_node = ossia::net::find_or_create_node(
              svc_node, [&] {
                std::string uuid_str = char_uuid.toStdString();
                if(auto it = char_names.find(uuid_str); it != char_names.end())
                  return it->second;
                return uuid_str;
              }());

          auto param = chara_node.create_parameter(ossia::val_type::STRING);

          // Read characteristic if readable
          if(characteristic.properties() & QLowEnergyCharacteristic::Read)
          {
            QByteArray val = characteristic.value();
            if(!val.isEmpty())
              param->set_value(std::string(val.data(), val.size()));
          }

          // Store for writing
          if((characteristic.properties() & QLowEnergyCharacteristic::Write)
             || (characteristic.properties() & QLowEnergyCharacteristic::WriteNoResponse)
             || (characteristic.properties() & QLowEnergyCharacteristic::WriteSigned)
             || !characteristic.descriptors().empty())
          {
            m_params.emplace(param, ble_param_id{service->serviceUuid(), characteristic});
          }

          // Setup notifications
          if(characteristic.properties() & QLowEnergyCharacteristic::Notify)
          {
            QObject::connect(service, &QLowEnergyService::characteristicChanged,
                           this, [param](const QLowEnergyCharacteristic& info, const QByteArray& value) {
              param->set_value(std::string(value.data(), value.size()));
            });

            // Enable notifications
            QLowEnergyDescriptor cccd = characteristic.clientCharacteristicConfiguration();
            if(cccd.isValid())
            {
              service->writeDescriptor(cccd, QLowEnergyCharacteristic::CCCDEnableNotification);
            }
          }
        }
      }
    });

    service->discoverDetails();
  }
}

void ble_protocol::set_device(net::device_base& dev)
{
  m_device = &dev;
  if(m_discoveryAgent)
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

ble_protocol::~ble_protocol()
{
  if(m_discoveryAgent && m_discoveryAgent->isActive())
    m_discoveryAgent->stop();

  if(m_controller)
  {
    if(m_controller->state() != QLowEnergyController::UnconnectedState)
      m_controller->disconnectFromDevice();
  }

  // Clean up services
  for(auto& [uuid, service] : m_services)
  {
    if(service)
      service->deleteLater();
  }
  m_services.clear();
}

bool ble_protocol::pull(ossia::net::parameter_base&)
{
  return false;
}

bool ble_protocol::push(const ossia::net::parameter_base& p, const ossia::value& v)
{
  if(m_controller && m_controller->state() == QLowEnergyController::ConnectedState)
  {
    if(auto it = m_params.find(&p); it != m_params.end())
    {
      auto& [svc_uuid, chara] = it->second;

      auto service_it = m_services.find(svc_uuid);
      if(service_it == m_services.end())
        return false;

      QLowEnergyService* service = service_it->second;
      if(!service)
        return false;

      std::string val_str = ossia::convert<std::string>(v);
      QByteArray data(val_str.data(), val_str.size());

      if(chara.properties() & QLowEnergyCharacteristic::Write)
        service->writeCharacteristic(chara, data, QLowEnergyService::WriteWithResponse);
      else if(chara.properties() & QLowEnergyCharacteristic::WriteNoResponse)
        service->writeCharacteristic(chara, data, QLowEnergyService::WriteWithoutResponse);
      else if(chara.properties() & QLowEnergyCharacteristic::WriteSigned)
        service->writeCharacteristic(chara, data, QLowEnergyService::WriteSigned);
      else if(!chara.descriptors().empty())
        service->writeDescriptor(chara.descriptors().first(), data);

      return true;
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
  auto adapters = QBluetoothLocalDevice::allDevices();
  if(!adapters.empty())
  {
    bool found = false;
    QString adapter_str = QString::fromStdString(m_conf.adapter);
    for(const auto& adapter : adapters)
    {
      if(adapter.address().toString() == adapter_str)
      {
        m_adapterAddress = adapter.address();
        found = true;
        break;
      }
    }
    if(!found)
      m_adapterAddress = adapters[0].address();
  }

  // Create discovery agent
  if(!m_adapterAddress.isNull())
    m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(m_adapterAddress, this);
  else
    m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);

  if(m_discoveryAgent)
  {
    m_discoveryAgent->setLowEnergyDiscoveryTimeout(15000);

    QObject::connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
                     this, &ble_scan_protocol::onDeviceDiscovered);
  }
}

void ble_scan_protocol::onDeviceDiscovered(const QBluetoothDeviceInfo& info)
{
  if(!should_include_device(info))
    return;

  boost::asio::post(m_strand, [this, info] { scan_services(); });
}

bool ble_scan_protocol::should_include_device(const QBluetoothDeviceInfo& info) const noexcept
{
  auto& service_names = ble_service_map();

  // Get service UUIDs from the device
  QList<QBluetoothUuid> serviceUuids = info.serviceUuids();

  // If no filters, include all BLE devices
  if(m_conf.filter_exclude.empty() && m_conf.filter_include.empty())
    return true;

  // Check exclude filters
  if(!m_conf.filter_exclude.empty())
  {
    for(const auto& service_uuid : serviceUuids)
    {
      std::string uuid_str
          = service_uuid.toString(QBluetoothUuid::WithoutBraces).toStdString();
      if(ossia::contains(m_conf.filter_exclude, uuid_str))
        return false;

      // Check pretty name too
      if(auto it = service_names.find(uuid_str); it != service_names.end())
      {
        if(ossia::contains(m_conf.filter_exclude, it->second))
          return false;
      }
    }
  }

  // Check include filters
  if(!m_conf.filter_include.empty())
  {
    bool found = false;
    for(const auto& service_uuid : serviceUuids)
    {
      std::string uuid_str
          = service_uuid.toString(QBluetoothUuid::WithoutBraces).toStdString();
      if(ossia::contains(m_conf.filter_include, uuid_str))
      {
        found = true;
        break;
      }

      // Check pretty name too
      if(auto it = service_names.find(uuid_str); it != service_names.end())
      {
        if(ossia::contains(m_conf.filter_include, it->second))
        {
          found = true;
          break;
        }
      }
    }
    return found;
  }

  return true;
}

void ble_scan_protocol::scan_services()
{
  if(!m_device || !m_discoveryAgent)
    return;

  auto& service_names = ble_service_map();

  for(const auto& deviceInfo : m_discoveryAgent->discoveredDevices())
  {
    if(!should_include_device(deviceInfo))
      continue;

    QString periph_name
        = deviceInfo.name().isEmpty()
              ? deviceInfo.address().isNull()
                    ? deviceInfo.deviceUuid().toString(QBluetoothUuid::WithoutBraces)
                    : deviceInfo.address().toString()
              : deviceInfo.name();
    std::string periph_name_std = periph_name.toStdString();
    ossia::net::sanitize_name(periph_name_std);

    auto& prp_node
        = ossia::net::find_or_create_node(m_device->get_root_node(), periph_name_std);

    for(const auto& service_uuid : deviceInfo.serviceUuids())
    {
      std::string uuid_str
          = service_uuid.toString(QBluetoothUuid::WithoutBraces).toStdString();
      std::string node_name;
      if(auto it = service_names.find(uuid_str); it != service_names.end())
        node_name = it->second;
      else
        node_name = uuid_str;

      auto& svc_node = ossia::net::find_or_create_node(prp_node, node_name);
      auto param = svc_node.create_parameter(ossia::val_type::STRING);
      param->set_value(uuid_str);
    }

    ossia::expose_manufacturer_data_as_ossia_nodes(prp_node, deviceInfo.manufacturerData());
  }
}

void ble_scan_protocol::set_device(net::device_base& dev)
{
  m_device = &dev;
  if(m_discoveryAgent)
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

ble_scan_protocol::~ble_scan_protocol()
{
  if(m_discoveryAgent && m_discoveryAgent->isActive())
    m_discoveryAgent->stop();
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
