#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <verdigris>

class QLineEdit;

namespace Protocols
{

class BLEProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(BLEProtocolSettingsWidget)

public:
  explicit BLEProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~BLEProtocolSettingsWidget();
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  QLineEdit* m_deviceNameEdit{};
  Device::DeviceSettings m_settings;
};

}
