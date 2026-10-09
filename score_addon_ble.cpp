#include "score_addon_ble.hpp"

#include <score/plugins/FactorySetup.hpp>
#include <score/widgets/MessageBox.hpp>

#include <QFileInfo>
#include <QTimer>

#include <BLE/BLEProtocolFactory.hpp>
#include <BLE/BLESpecificSettings.hpp>
#include <BLE/Protocol.hpp>

static std::shared_future<bool> g_bluetooth_available{};
score_addon_ble::score_addon_ble()
{
  qRegisterMetaType<Protocols::BLESpecificSettings>();

  g_bluetooth_available = std::async(std::launch::async, [] {
    try
    {
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
      if(!QFileInfo::exists("/sys/class") || !QFileInfo::exists("/sys/class/bluetooth"))
        return false;
#endif
      return SimpleBLE::Adapter::bluetooth_enabled();
    }
    catch(...)
    {
      return false;
    }
  }).share();
}

score_addon_ble::~score_addon_ble() { }

std::vector<score::InterfaceBase*> score_addon_ble::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  try
  {
    if(key == Device::ProtocolFactory::static_interfaceKey())
    {
      auto bt = g_bluetooth_available.wait_for(std::chrono::milliseconds(1));
      if(bt != std::future_status::ready)
        return {};
      if(!g_bluetooth_available.get())
        return {};

      return instantiate_factories<
          score::ApplicationContext,
          FW<Device::ProtocolFactory, Protocols::BLEProtocolFactory>>(ctx, key);
    }
  }
  catch(...)
  {
  }
  return {};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_addon_ble)
