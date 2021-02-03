#ifndef SRC_EPG_EPGPROVIDER_H_
#define SRC_EPG_EPGPROVIDER_H_

#include "../ZatChannel.h"
#include <kodi/addon-instance/PVR.h>
#include <ctime>
#include <mutex>

class EpgProvider
{
public:
  EpgProvider(kodi::addon::CInstancePVRClient *addon): m_addon(addon) {} 
  virtual ~EpgProvider() {};
  virtual bool LoadEPGForChannel(ZatChannel &zatChannel, time_t iStart, time_t iEnd) = 0;
protected:
  static std::mutex sendEpgToKodiMutex;
  void SendEpg(kodi::addon::PVREPGTag &tag) {
    m_addon->EpgEventStateChange(tag, EPG_EVENT_CREATED);
  }
  kodi::addon::CInstancePVRClient *m_addon;
};

#endif /* SRC_EPG_EPGPROVIDER_H_ */
