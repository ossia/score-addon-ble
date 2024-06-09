#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/command/Command.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <vector>

class score_addon_ble final
    : public score::Plugin_QtInterface
    , public score::FactoryInterface_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "d69503f2-6b38-47bd-b18c-be7b2ce05fe2")

public:
  score_addon_ble();
  ~score_addon_ble() override;

private:
  std::vector<score::InterfaceBase*> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& key) const override;
};
