#pragma once
#include <string>
#include <unordered_set>
#include "../ZatChannel.h"
#include <map>
#include <mutex>
#include <kodi/addon-instance/PVR.h>
#include "EpgProvider.h"

class XmlTVEpgProvider: public EpgProvider
{
public:
  XmlTVEpgProvider(kodi::addon::CInstancePVRClient *addon, std::string xmlFile, std::map<std::string, ZatChannel>& channelsByCid, EpgProvider &fallbackProvider);
  ~XmlTVEpgProvider();
  virtual bool LoadEPGForChannel(ZatChannel &zatChannel, time_t iStart, time_t iEnd);

private:
  bool LoadEPGFromFile(ZatChannel &zatChannel);
  time_t StringToTime(const std::string &timeString);
  bool isUpdateDue();
  bool copyFileToTempFileIfNewer(std::string &tempFile);
  std::string m_xmlFile;
  time_t m_lastUpdate;
  std::mutex m_mutex;
  std::unordered_set<std::string> m_loadedChannels;
  kodi::addon::CInstancePVRClient* m_addon;
  std::map<std::string, ZatChannel>& m_channelsByCid;
  EpgProvider &m_fallbackProvider;
  time_t m_lastFileModification = 0;
};
