#pragma once

#include <verdigris>

namespace Protocols
{

struct BLESpecificSettings
{
  QString adapter;
  QString serial;
};

}

Q_DECLARE_METATYPE(Protocols::BLESpecificSettings)
W_REGISTER_ARGTYPE(Protocols::BLESpecificSettings)
