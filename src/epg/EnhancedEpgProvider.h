#ifndef SRC_EPG_ENHANCEDEPGPROVIDER_H_
#define SRC_EPG_ENHANCEDEPGPROVIDER_H_

#include <string>
#include <list>
#include <map>
#include "EpgProvider.h"
#include "../sql/EpgDB.h"
#include "../http/HttpClient.h"
#include "../categories.h"

class EnhancedEpgProvider: public EpgProvider
{
public:
  EnhancedEpgProvider(
      kodi::addon::CInstancePVRClient *addon,
      EpgDB &epgDB, 
      HttpClient &httpClient, 
      Categories &categories,
      std::string powerHash,
      std::string countryCode
  );
  virtual ~EnhancedEpgProvider();
  virtual bool LoadEPGForChannel(ZatChannel &zatChannel, time_t iStart, time_t iEnd);
private:
  EpgDB &m_epgDB;
  HttpClient &m_httpClient;
  Categories &m_categories;
  std::string m_powerHash;
  std::string m_countryCode;
};

#endif /* SRC_EPG_ENHANCEDEPGPROVIDER_H_ */
