#pragma once
#include <string>
#include "kodi/libXBMC_pvr.h"
#include "kodi/libXBMC_addon.h"

using namespace ADDON;

class XmlTV
{
public:
  XmlTV(std::string xmlFile);
  bool GetEPGForChannel(std::string cid, unsigned int uniqueChannelId);

private:
  std::string xmlFile;
  time_t StringToTime(std::string timeString);
};
