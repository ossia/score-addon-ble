#pragma once

#include <Explorer/DefaultProtocolFactory.hpp>

namespace Protocols
{

class BLEProtocolFactory final : public DefaultProtocolFactory
{
  SCORE_CONCRETE("cdefb5de-f465-4df5-8c43-b6ea88ed09bf")

  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  Device::DeviceEnumerators getEnumerators(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;

  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data, const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};

}
