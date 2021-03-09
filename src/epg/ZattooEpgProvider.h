#ifndef SRC_EPG_ZATTOOEPGPROVIDER_H_
#define SRC_EPG_ZATTOOEPGPROVIDER_H_

#include <string>
#include <list>
#include <map>
#include <atomic>
#include <thread>
#include "EpgProvider.h"
#include "../sql/EpgDB.h"
#include "../http/HttpClient.h"
#include "../categories.h"

struct LoadedTimeslots {
  time_t start;
  time_t end;
  time_t loaded;
};

class ZattooEpgProvider: public EpgProvider
{
public:
  ZattooEpgProvider(
      kodi::addon::CInstancePVRClient *addon,
      std::string providerUrl, 
      EpgDB &epgDB, 
      HttpClient &httpClient, 
      Categories &categories,
      std::map<std::string, ZatChannel> &visibleChannelsByCid, 
      std::string powerHash
  );
  virtual ~ZattooEpgProvider();
  virtual bool LoadEPGForChannel(ZatChannel &zatChannel, time_t iStart, time_t iEnd);
private:
  static std::mutex loadedTimeslotsMutex;
  time_t SkipAlreadyLoaded(time_t startTime, time_t endTime);
  void RegisterAlreadyLoaded(time_t startTime, time_t endTime);
  void CleanupAlreadyLoaded();
  void DetailsThread();
  void SendEpgDBInfo(EpgDBInfo &epgDBInfo);
  time_t lastCleanup;
  EpgDB &m_epgDB;
  HttpClient &m_httpClient;
  Categories &m_categories;
  std::string m_powerHash;
  std::string m_providerUrl;
  std::list<LoadedTimeslots> m_loadedTimeslots;
  std::map<std::string, ZatChannel> &m_visibleChannelsByCid;
  std::atomic<bool> m_detailsThreadRunning = {false};
  std::thread m_detailsThread;
};

#endif /* SRC_EPG_ZATTOOEPGPROVIDER_H_ */
