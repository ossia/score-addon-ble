#include "score_addon_ble.hpp"

#include <score/plugins/FactorySetup.hpp>
#include <score/widgets/MessageBox.hpp>

#include <QBluetoothLocalDevice>
#include <QBluetoothPermission>
#include <QCoreApplication>
#include <QFileInfo>
#include <QTimer>

#include <BLE/BLEProtocolFactory.hpp>
#include <BLE/BLESpecificSettings.hpp>
#include <BLE/Protocol.hpp>

static bool g_bluetooth_allowed = true;
score_addon_ble::score_addon_ble()
{
  qRegisterMetaType<Protocols::BLESpecificSettings>();

#if defined(__APPLE__)
  static std::atomic_bool ok{};
  QCoreApplication::instance()->requestPermission(
      QBluetoothPermission{}, [](const QPermission& permission) {
    g_bluetooth_allowed = permission.status() == Qt::PermissionStatus::Granted;
    ok = true;
  });
  while(!ok)
  {
    QCoreApplication::processEvents();
  }
#endif
}

score_addon_ble::~score_addon_ble() { }

std::vector<score::InterfaceBase*> score_addon_ble::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  if(key != Device::ProtocolFactory::static_interfaceKey())
    return {};

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
  if(!QFileInfo::exists("/sys/class") || !QFileInfo::exists("/sys/class/bluetooth"))
    return {};
#endif

  try
  {
    if(!QBluetoothLocalDevice::allDevices().isEmpty())
    {
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
