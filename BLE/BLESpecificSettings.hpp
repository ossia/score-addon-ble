#pragma once

#include <QStringList>

#include <verdigris>

namespace Protocols
{

struct BLESpecificSettings
{
  QString adapter;
  QString serial;
  QString include_filters;
  QString exclude_filters;
};

}

Q_DECLARE_METATYPE(Protocols::BLESpecificSettings)
W_REGISTER_ARGTYPE(Protocols::BLESpecificSettings)
