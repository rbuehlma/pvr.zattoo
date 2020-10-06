#pragma once
#include <string>
#include <unordered_set>
#include "ZatChannel.h"
#include <map>
#include <mutex>
#include <kodi/addon-instance/PVR.h>

class XmlTV
{
public:
  XmlTV(kodi::addon::CInstancePVRClient& instance, std::string xmlFile);
  ~XmlTV();
  bool GetEPGForChannel(const std::string &cid, std::map<std::string, ZatChannel> &channelsByCid);

private:
  std::string m_xmlFile;
  time_t m_lastUpdate;
  std::mutex m_mutex;
  std::unordered_set<std::string> m_loadedChannels;
  kodi::addon::CInstancePVRClient& m_instance;
  time_t StringToTime(const std::string &timeString);
  bool isUpdateDue();
};
