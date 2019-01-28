#pragma once
#include <string>

class XmlTV
{
public:
  XmlTV(std::string xmlFile);
  bool GetEPGForChannel(const std::string &cid, unsigned int uniqueChannelId);

private:
  std::string m_xmlFile;
  time_t StringToTime(const std::string &timeString);
};
