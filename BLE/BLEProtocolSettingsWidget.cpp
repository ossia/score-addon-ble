
#include "BLEProtocolSettingsWidget.hpp"

#include "BLEProtocolFactory.hpp"
#include "BLESpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <QFormLayout>
#include <QLabel>
#include <QVariant>

#include <simpleble/SimpleBLE.h>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::BLEProtocolSettingsWidget)

namespace Protocols
{

BLEProtocolSettingsWidget::BLEProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);
  m_deviceNameEdit->setText("BLE");

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(new QLabel(
      tr("Bluetooth status: %1")
          .arg(SimpleBLE::Adapter::bluetooth_enabled() ? "enabled" : "disabled")));

  setLayout(layout);
}

BLEProtocolSettingsWidget::~BLEProtocolSettingsWidget() { }

Device::DeviceSettings BLEProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  return s;
}

void BLEProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  m_settings = settings;
}

}
