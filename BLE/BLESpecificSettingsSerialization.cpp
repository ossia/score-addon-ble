#include "BLESpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Protocols::BLESpecificSettings& n)
{
  m_stream << n.adapter << n.serial << n.include_filters << n.exclude_filters;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::BLESpecificSettings& n)
{
  m_stream >> n.adapter >> n.serial >> n.include_filters >> n.exclude_filters;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::BLESpecificSettings& n)
{
  obj["Adapter"] = n.adapter;
  obj["Serial"] = n.serial;
  if(!n.include_filters.isEmpty())
    obj["Include"] = n.include_filters;
  if(!n.exclude_filters.isEmpty())
    obj["Exclude"] = n.exclude_filters;
}

template <>
void JSONWriter::write(Protocols::BLESpecificSettings& n)
{
  n.adapter = obj["Adapter"].toString();
  n.serial = obj["Serial"].toString();
  if(auto filter = obj.tryGet("Include"))
  {
    n.include_filters = filter->toString();
  }
  if(auto filter = obj.tryGet("Exclude"))
  {
    n.exclude_filters = filter->toString();
  }
}
