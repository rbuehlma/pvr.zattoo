//
// Created by johannes on 04.02.16.
//

#include "UpdateThread.h"
#include "categories.h"
#include "Curl.h"
#include <map>
#include <thread>
#include <mutex>
#include "rapidjson/document.h"
#include "XmlTV.h"
#include "ZatChannel.h"

class CZattooTVAddon;

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
  bool selectiveReplay;
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

class ATTRIBUTE_HIDDEN ZatData : public kodi::addon::CInstancePVRClient
{
public:
  ZatData(KODI_HANDLE instance, const std::string& version,
      const std::string& username, const std::string& password, bool favoritesOnly,
      bool alternativeEpgService, const STREAM_TYPE& streamType, bool enableDolby, int provider,
      const std::string& xmlTVFile, const std::string& parentalPin);
  ~ZatData();
  bool Initialize();
  bool LoadChannels();

  PVR_ERROR GetCapabilities(kodi::addon::PVRCapabilities& capabilities) override;
  PVR_ERROR GetBackendName(std::string& name) override;
  PVR_ERROR GetBackendVersion(std::string& version) override;
  PVR_ERROR GetBackendHostname(std::string& hostname) override;
  PVR_ERROR GetConnectionString(std::string& connection) override;

  PVR_ERROR GetChannelsAmount(int& amount) override;
  PVR_ERROR GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results) override;
  PVR_ERROR GetChannelGroupsAmount(int& amount) override;
  PVR_ERROR GetChannelGroups(bool radio, kodi::addon::PVRChannelGroupsResultSet& results) override;
  PVR_ERROR GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group,
                                   kodi::addon::PVRChannelGroupMembersResultSet& results) override;
  PVR_ERROR GetEPGForChannel(int channelUid, time_t start, time_t end, kodi::addon::PVREPGTagsResultSet& results) override;
  PVR_ERROR GetChannelStreamProperties(const kodi::addon::PVRChannel& channel,
                                       std::vector<kodi::addon::PVRStreamProperty>& properties) override;
  PVR_ERROR GetTimerTypes(std::vector<kodi::addon::PVRTimerType>& types) override;
  PVR_ERROR GetTimers(kodi::addon::PVRTimersResultSet& results) override;
  PVR_ERROR GetTimersAmount(int& amount) override;
  PVR_ERROR AddTimer(const kodi::addon::PVRTimer& timer) override;
  PVR_ERROR DeleteTimer(const kodi::addon::PVRTimer& timer, bool forceDelete) override;
  PVR_ERROR GetRecordings(bool deleted, kodi::addon::PVRRecordingsResultSet& results) override;
  PVR_ERROR GetRecordingsAmount(bool deleted,  int& amount) override;
  PVR_ERROR GetRecordingStreamProperties(const kodi::addon::PVRRecording& recording,
                                         std::vector<kodi::addon::PVRStreamProperty>& properties) override;
  PVR_ERROR DeleteRecording(const kodi::addon::PVRRecording& recording) override;
  PVR_ERROR SetRecordingPlayCount(const kodi::addon::PVRRecording& recording, int count) override;
  PVR_ERROR SetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recording,
                                           int lastplayedposition) override;
  PVR_ERROR GetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recording, int& position) override;
  PVR_ERROR IsEPGTagPlayable(const kodi::addon::PVREPGTag& tag, bool& isPlayable) override;
  PVR_ERROR IsEPGTagRecordable(const kodi::addon::PVREPGTag& tag, bool& isRecordable) override;
  PVR_ERROR GetEPGTagStreamProperties(const kodi::addon::PVREPGTag& tag, std::vector<kodi::addon::PVRStreamProperty>& properties) override;
  PVR_ERROR GetEPGTagEdl(const kodi::addon::PVREPGTag& tag, std::vector<kodi::addon::PVREDLEntry>& edl) override;

  int GetRecallSeconds(const kodi::addon::PVREPGTag& tag);
  void GetEPGForChannelAsync(int uniqueChannelId, time_t iStart, time_t iEnd);
  bool RecordingEnabled()
  {
    return m_recordingEnabled;
  }

private:
  bool m_alternativeEpgService;
  bool m_favoritesOnly;
  bool m_enableDolby;
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
  std::string m_parentalPin;
  XmlTV *m_xmlTV = nullptr;

  bool LoadAppId();
  bool LoadAppTokenFromFile();
  bool LoadAppTokenFromJson(std::string html);
  bool LoadAppTokenFromHtml(std::string html);
  bool ReadDataJson();
  bool WriteDataJson();
  std::string GetUUID();
  std::string GenerateUUID();
  bool SendHello(std::string uuid);
  rapidjson::Document Login();
  bool InitSession(bool isReinit);
  bool ReinitSession();
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
  std::string GetStreamUrl(std::string& jsonString, std::vector<kodi::addon::PVRStreamProperty>& properties);
  static std::mutex sendEpgToKodiMutex;
  std::string GetStreamParameters();
  bool ParseRecordingsTimers(const rapidjson::Value& recordings, std::map<int, ZatRecordingDetails>& detailsById);
  void AddTimerType(std::vector<kodi::addon::PVRTimerType>& types, int idx, int attributes);
  bool Record(int programId);
  std::string GetManifestType();
  std::string GetMimeType();
  void SetStreamProperties(std::vector<kodi::addon::PVRStreamProperty>& properties, const std::string& url);
};
