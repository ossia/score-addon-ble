#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{

class BLEDevice final : public Device::OwningDeviceInterface
{

  W_OBJECT(BLEDevice)
public:
  BLEDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  ~BLEDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
};

}
