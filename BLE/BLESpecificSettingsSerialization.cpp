#include "BLESpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Protocols::BLESpecificSettings& n)
{
  m_stream << n.adapter << n.serial;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::BLESpecificSettings& n)
{
  m_stream >> n.adapter >> n.serial;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::BLESpecificSettings& n)
{
  obj["Adapter"] = n.adapter;
  obj["Serial"] = n.serial;
}

template <>
void JSONWriter::write(Protocols::BLESpecificSettings& n)
{
  n.adapter = obj["Adapter"].toString();
  n.serial = obj["Serial"].toString();
}
