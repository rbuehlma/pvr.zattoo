//
// Created by johannes on 04.02.16.
//

#include "client.h"
#include "UpdateThread.h"
#include "categories.h"
#include "Curl.h"
#include <map>
#include <thread>
#include <p8-platform/threads/mutex.h>
#include "rapidjson/document.h"
#include "XmlTV.h"
#include "ZatChannel.h"

enum STREAM_TYPE: int
{
    DASH,
    HLS,
    DASH_WIDEVINE
};

/*!
 * @brief PVR macros for std::string exchange
 */
#define PVR_STRCPY(dest, source) do { strncpy(dest, source, sizeof(dest)-1); (dest)[sizeof(dest)-1] = '\0'; } while(0)

struct PVRIptvEpgEntry
{
  int iBroadcastId;
  int iChannelId;
  time_t startTime;
  time_t endTime;
  std::string strTitle;
  std::string strPlot;
  std::string strIconPath;
  std::string strGenreString;
};

struct ZatRecordingData
{
  std::string recordingId;
  int playCount;
  int lastPlayedPosition;
  bool stillValid;
};

struct ZatRecordingDetails
{
  std::string genre;
  std::string description;
};

struct PVRZattooChannelGroup
{
  std::string name;
  std::vector<ZatChannel> channels;
};

class ZatData
{
public:
  ZatData(const std::string& username, const std::string& password, bool favoritesOnly,
      bool m_alternativeEpgService, const STREAM_TYPE& streamType, int provider,
      const std::string& xmlTVFile);
  ~ZatData();
  bool Initialize();
  bool LoadChannels();
  void GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities);
  int GetChannelsAmount();
  PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio);
  int GetChannelGroupsAmount();
  PVR_ERROR GetChannelGroups(ADDON_HANDLE handle);
  PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle,
      const PVR_CHANNEL_GROUP &group);
  void GetEPGForChannel(const PVR_CHANNEL &channel, time_t iStart,
      time_t iEnd);
  void GetEPGForChannelAsync(int uniqueChannelId, time_t iStart,
      time_t iEnd);
  std::string GetChannelStreamUrl(int uniqueId, std::map<std::string, std::string> &additionalPropertiesOut);
  void GetRecordings(ADDON_HANDLE handle, bool future);
  int GetRecordingsAmount(bool future);
  std::string GetRecordingStreamUrl(const std::string& recordingId, std::map<std::string, std::string> &additionalPropertiesOut);
  bool Record(int programId);
  bool DeleteRecording(const std::string& recordingId);
  void SetRecordingPlayCount(const PVR_RECORDING &recording, int count);
  void SetRecordingLastPlayedPosition(const PVR_RECORDING &recording,
      int lastplayedposition);
  int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording);
  bool IsPlayable(const EPG_TAG *tag);
  int GetRecallSeconds(const EPG_TAG *tag);
  bool IsRecordable(const EPG_TAG *tag);
  std::string GetEpgTagUrl(const EPG_TAG *tag, std::map<std::string, std::string> &additionalPropertiesOut);
  bool RecordingEnabled()
  {
    return m_recordingEnabled;
  }

private:
  bool m_alternativeEpgService;
  bool m_favoritesOnly;
  STREAM_TYPE m_streamType;
  std::string m_username;
  std::string m_password;
  std::string m_appToken;
  std::string m_powerHash;
  std::string m_countryCode;
  std::string m_serviceRegionCountry;
  bool m_recallEnabled = false;
  bool m_selectiveRecallEnabled = false;
  bool m_recordingEnabled = false;
  std::vector<PVRZattooChannelGroup> m_channelGroups;
  std::map<int, ZatChannel> m_channelsByUid;
  std::map<std::string, ZatChannel> m_channelsByCid;
  std::map<std::string, ZatRecordingData*> m_recordingsData;
  int64_t m_maxRecallSeconds = 0;
  std::string m_beakerSessionId;
  std::string m_pzuid;
  std::vector<UpdateThread*> m_updateThreads;
  std::string m_uuid = "";
  Categories m_categories;
  std::string m_providerUrl;
  bool m_recordingsLoaded = false;
  XmlTV *m_xmlTV = nullptr;

  bool LoadAppId();
  bool ReadDataJson();
  bool WriteDataJson();
  std::string GetUUID();
  std::string GenerateUUID();
  bool SendHello(std::string uuid);
  rapidjson::Document Login();
  bool InitSession();
  std::string HttpGetCached(const std::string& url, time_t cacheDuration, const std::string& userAgent = "");
  std::string HttpGet(const std::string& url, bool isInit = false, const std::string& userAgent = "");
  std::string HttpDelete(const std::string& url, bool isInit = false);
  std::string HttpPost(const std::string& url, const std::string& postData, bool isInit = false, const std::string& userAgent = "");
  std::string HttpRequest(const std::string& action, const std::string& url, const std::string& postData, bool isInit, const std::string& userAgent);
  std::string HttpRequestToCurl(Curl &curl, const std::string& action, const std::string& url,
                           const std::string& postData, int &statusCode);
  std::map<time_t, PVRIptvEpgEntry>* LoadEPG(time_t iStart, time_t iEnd,
      int uniqueChannelId);
  ZatChannel* FindChannel(int uniqueId);
  PVRZattooChannelGroup* FindGroup(const std::string& strName);
  int GetChannelId(const char * strChannelName);
  void GetEPGForChannelExternalService(int uniqueChannelId, time_t iStart, time_t iEnd);
  std::string GetStringOrEmpty(const rapidjson::Value& jsonValue, const char* fieldName);
  std::string GetImageUrl(const std::string& imageToken);
  std::string GetStreamTypeString();
  std::string GetStreamUrl(std::string& jsonString, std::map<std::string, std::string>& additionalPropertiesOut);
};
