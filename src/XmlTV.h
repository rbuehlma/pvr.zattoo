#pragma once
#include <string>

class XmlTV
{
public:
  XmlTV(std::string xmlFile);
  ~XmlTV();
  bool GetEPGForChannel(const std::string &cid, unsigned int uniqueChannelId);

private:
  std::string m_xmlFile;
  time_t m_lastUpdate;
  time_t StringToTime(const std::string &timeString);
  bool isUpdateDue();
};
