
#include "BLEProtocolFactory.hpp"

#include "BLEDevice.hpp"
#include "BLEProtocolSettingsWidget.hpp"
#include "BLESpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <QObject>

#include <simpleble/SimpleBLE.h>

namespace Protocols
{
struct BLEAdapters : std::enable_shared_from_this<BLEAdapters>
{
  std::vector<SimpleBLE::Adapter> list = SimpleBLE::Adapter::get_adapters();
};

class BLEEnumerator final : public Device::DeviceEnumerator
{
public:
  std::shared_ptr<BLEAdapters> adapters;
  SimpleBLE::Adapter& adapter;
  std::vector<SimpleBLE::Peripheral> peripherals;
  BLEEnumerator(std::shared_ptr<BLEAdapters> ref, SimpleBLE::Adapter& adapter)
      : adapters{std::move(ref)}
      , adapter{adapter}
  {
    adapter.set_callback_on_scan_start([]() {});
    adapter.set_callback_on_scan_stop([]() {});
    adapter.set_callback_on_scan_found([this](SimpleBLE::Peripheral p) {
      if(p.initialized())
      {
        peripherals.push_back(p);
        addNewDevice(p);
      }
    });
    adapter.set_callback_on_scan_updated([](SimpleBLE::Peripheral) {});
    adapter.scan_start();
  }

  ~BLEEnumerator()
  {
    if(adapter.initialized() && adapter.scan_is_active())
      adapter.scan_stop();
  }

private:
  void enumerate(
      std::function<void(const QString&, const Device::DeviceSettings&)>) const override
  {
  }

  void addNewDevice(SimpleBLE::Peripheral& p) noexcept
  {
    using namespace std::literals;

    Device::DeviceSettings set;
    BLESpecificSettings sub;
    sub.adapter = QString::fromStdString(adapter.address());
    sub.serial = QString::fromStdString(p.address());

    QString pretty_name;
    QString device_name;
    if(p.identifier().empty())
    {
      pretty_name = sub.serial;
      device_name = sub.serial;
    }
    else
    {
      pretty_name = QString::fromStdString(p.identifier()) + " (" + sub.serial + ")";
      device_name = QString::fromStdString(p.identifier());
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

Device::DeviceEnumerators
BLEProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  if(!SimpleBLE::Adapter::bluetooth_enabled())
    return {};

  auto adapter_list = std::make_shared<BLEAdapters>();
  if(adapter_list->list.empty())
    return {};

  Device::DeviceEnumerators enums;
  for(auto& adapter : adapter_list->list)
    enums.emplace_back(
        QString::fromStdString(adapter.identifier()),
        new BLEEnumerator{adapter_list, adapter});
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
