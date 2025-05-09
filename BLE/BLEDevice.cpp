#include "BLEDevice.hpp"

#include "BLESpecificSettings.hpp"
#include "Protocol.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/network/generic/generic_device.hpp>

#include <ossia-qt/invoke.hpp>

#include <QLabel>
#include <QProgressDialog>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::BLEDevice)
namespace Protocols
{

BLEDevice::BLEDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

BLEDevice::~BLEDevice() { }

bool BLEDevice::reconnect()
{
  disconnect();
  try
  {
    const auto& stgs = settings().deviceSpecificSettings.value<BLESpecificSettings>();
    if(stgs.serial.isEmpty())
    {
      ossia::ble_scan_configuration conf;
      conf.adapter = stgs.adapter.toStdString();
      auto inc = stgs.include_filters.split(",", Qt::SkipEmptyParts);
      auto exc = stgs.exclude_filters.split(",", Qt::SkipEmptyParts);
      for(const auto& e : inc)
        conf.filter_include.push_back(e.toStdString());
      for(const auto& e : exc)
        conf.filter_exclude.push_back(e.toStdString());
      m_dev = std::make_unique<ossia::net::generic_device>(
          std::make_unique<ossia::ble_scan_protocol>(m_ctx, conf),
          settings().name.toStdString());
    }
    else
      m_dev = std::make_unique<ossia::net::generic_device>(
          std::make_unique<ossia::ble_protocol>(
              m_ctx, stgs.adapter.toStdString(), stgs.serial.toStdString()),
          settings().name.toStdString());

    deviceChanged(nullptr, m_dev.get());
  }
  catch(...)
  {
    SCORE_TODO;
  }

  return connected();
}

void BLEDevice::disconnect()
{
  OwningDeviceInterface::disconnect();
}

}
