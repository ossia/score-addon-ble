
#include "BLEProtocolSettingsWidget.hpp"

#include "BLEProtocolFactory.hpp"
#include "BLESpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/widgets/HelpInteraction.hpp>

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
  m_include = new QLineEdit;
  score::setHelp(
      m_include, tr("Set a filter to match specified advertisement identifiers."));
  m_include->setPlaceholderText("Ex.: 14506,301");
  m_exclude = new QLineEdit;
  m_exclude->setText(
      "76,0000febe-0000-1000-8000-00805f9b34fb,0000fef3-0000-1000-8000-00805f9b34fb");
  m_exclude->setPlaceholderText("Ex.: Fast Pair Service,76,6,121");
  score::setHelp(
      m_exclude, tr("Set a filter to exclude specified advertisement identifiers."));

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Include filters"), m_include);
  layout->addRow(tr("Exclude filters"), m_exclude);
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
  if(settings.deviceSpecificSettings.canConvert<BLESpecificSettings>())
  {
    auto stgs = settings.deviceSpecificSettings.value<BLESpecificSettings>();
    if(!stgs.include_filters.isEmpty())
      m_include->setText(stgs.include_filters);
    if(!stgs.exclude_filters.isEmpty())
      m_exclude->setText(stgs.exclude_filters);
  }
  m_settings = settings;
}

}
