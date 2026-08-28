
#include "BLEProtocolFactory.hpp"

#include "BLEDevice.hpp"
#include "BLEProtocolSettingsWidget.hpp"
#include "BLESpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothLocalDevice>
#include <QBluetoothPermission>
#include <QObject>
#include <QUrl>

namespace Protocols
{
class BLEEnumerator final : public Device::DeviceEnumerator
{
public:
  QBluetoothAddress adapter_address;
  QBluetoothDeviceDiscoveryAgent* discovery_agent{};
  QList<QBluetoothDeviceInfo> peripherals;

  BLEEnumerator(const QBluetoothAddress& addr)
      : adapter_address{addr}
  {
    discovery_agent = new QBluetoothDeviceDiscoveryAgent(adapter_address);
    discovery_agent->setLowEnergyDiscoveryTimeout(5000);

    QObject::connect(discovery_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
                     [this](const QBluetoothDeviceInfo& info) {
      peripherals.push_back(info);
      addNewDevice(info);
    });

    try
    {
      discovery_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    }
    catch(const std::exception& e)
    {
      qDebug() << "Error while initiating BLE scan:" << e.what();
    }
  }

  ~BLEEnumerator()
  {
    if(discovery_agent)
    {
      if(discovery_agent->isActive())
        discovery_agent->stop();
      discovery_agent->deleteLater();
    }
  }

private:
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    using namespace std::literals;

    Device::DeviceSettings set;
    BLESpecificSettings sub;
    sub.adapter = adapter_address.toString();

    set.name = "Advertisements";

    set.protocol = BLEProtocolFactory::static_concreteKey();
    set.deviceSpecificSettings = QVariant::fromValue(std::move(sub));

    f("Advertisements", set);
  }

  void addNewDevice(const QBluetoothDeviceInfo& p) noexcept
  {
    using namespace std::literals;

    Device::DeviceSettings set;
    BLESpecificSettings sub;
    sub.adapter = adapter_address.toString();
    if(!p.address().isNull())
      sub.serial = p.address().toString();
    else
      sub.serial = p.deviceUuid().toString(QBluetoothUuid::WithoutBraces);

    QString pretty_name;
    QString device_name;
    if(p.name().isEmpty())
    {
      pretty_name = sub.serial;
      device_name = sub.serial;
    }
    else
    {
      pretty_name = p.name() + " (" + sub.serial + ")";
      device_name = p.name();
    }
    set.name = device_name;

    set.protocol = BLEProtocolFactory::static_concreteKey();
    set.deviceSpecificSettings = QVariant::fromValue(std::move(sub));
    deviceAdded(pretty_name, set);
  }
};

QString BLEProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("BLE");
}

QString BLEProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

QUrl BLEProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/ble-device.html");
}

Device::DeviceEnumerators
BLEProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  auto adapter_list = QBluetoothLocalDevice::allDevices();
  if(adapter_list.empty())
    return {};

  Device::DeviceEnumerators enums;
  for(const auto& adapter : adapter_list)
    enums.emplace_back(
        adapter.name(),
        new BLEEnumerator{adapter.address()});
  return enums;
}

Device::DeviceInterface* BLEProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new BLEDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& BLEProtocolFactory::defaultSettings() const noexcept
{
  // FIXME we need to make sure we cannot instantiate an unknown BT device
  static const Device::DeviceSettings& settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "BLE";
    BLESpecificSettings settings;
    s.deviceSpecificSettings = QVariant::fromValue(settings);
    return s;
  }();

  return settings;
}

Device::ProtocolSettingsWidget* BLEProtocolFactory::makeSettingsWidget()
{
  return new BLEProtocolSettingsWidget;
}

QVariant BLEProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<BLESpecificSettings>(visitor);
}

void BLEProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<BLESpecificSettings>(data, visitor);
}

bool BLEProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}

}
