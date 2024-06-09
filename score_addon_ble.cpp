#include "score_addon_ble.hpp"

#include <score/plugins/FactorySetup.hpp>
#include <score/widgets/MessageBox.hpp>

#include <QTimer>

#include <BLE/BLEProtocolFactory.hpp>
#include <BLE/BLESpecificSettings.hpp>
#include <BLE/Protocol.hpp>

score_addon_ble::score_addon_ble()
{
  qRegisterMetaType<Protocols::BLESpecificSettings>();
}

score_addon_ble::~score_addon_ble() { }

std::vector<score::InterfaceBase*> score_addon_ble::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Device::ProtocolFactory, Protocols::BLEProtocolFactory>>(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_addon_ble)
