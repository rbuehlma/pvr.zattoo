#pragma once
#include <string>
#include <unordered_set>
#include <p8-platform/threads/mutex.h>
#include "ZatChannel.h"
#include <map>

class XmlTV
{
public:
  XmlTV(std::string xmlFile);
  ~XmlTV();
  bool GetEPGForChannel(const std::string &cid, std::map<std::string, ZatChannel> &channelsByCid);

private:
  std::string m_xmlFile;
  time_t m_lastUpdate;
  P8PLATFORM::CMutex m_mutex;
  std::unordered_set<std::string> m_loadedChannels;
  time_t StringToTime(const std::string &timeString);
  bool isUpdateDue();
};
