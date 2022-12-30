#ifndef SRC_SESSION_H_
#define SRC_SESSION_H_

#include "http/HttpClient.h"
#include "http/HttpStatusCodeHandler.h"
#include "Utils.h"
#include "Settings.h"

#include <thread>
#include <atomic>

class ZatData;

class Session: public HttpStatusCodeHandler
{
public:
  Session(HttpClient* httpClient, ZatData* zatData, CSettings* settings, ParameterDB *parameterDB);
  ~Session();
  ADDON_STATUS Start();
  void Stop();
  void LoginThread();
  void Reset();
  void ErrorStatusCode (int statusCode);
  bool IsConnected() {
    return m_isConnected;
  }
  std::string GetAppToken() {
    return m_appToken;
  }
  std::string GetPowerHash() {
    return m_powerHash;
  }
  std::string GetCountryCode() {
    return m_countryCode;
  }
  std::string GetServiceRegionCountry() {
    return m_serviceRegionCountry;
  }
  bool IsRecallEnabled() {
    return m_recallEnabled;
  }
  bool IsRecordingEnabled() {
    return m_recordingEnabled;
  }
  std::string GetProviderUrl() {
    return m_providerUrl;
  }
private:
  bool Login(std::string u, std::string p);
  bool LoadAppId();
  bool LoadAppTokenFromTokenJson(std::string tokenJsonPath);
  bool LoadAppTokenFromFile();
  bool LoadAppTokenFromJson(std::string html);
  bool LoadAppTokenFromHtml(std::string html);
  bool SendHello();
  void SetProviderUrl();
  HttpClient* m_httpClient;
  ZatData* m_zatData;
  CSettings* m_settings;
  ParameterDB *m_parameterDB;
  time_t m_nextLoginAttempt = 0;
  bool m_isConnected = false;
  std::atomic<bool> m_running = {false};
  std::thread m_thread;
  std::string m_appToken;
  std::string m_powerHash;
  std::string m_countryCode;
  std::string m_serviceRegionCountry;
  bool m_recallEnabled = false;
  bool m_recordingEnabled = false;
  std::string m_providerUrl;
};



#endif /* SRC_SESSION_H_ */
