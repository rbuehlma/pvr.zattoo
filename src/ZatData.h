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

using namespace rapidjson;

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

struct ZatChannel
{
  int iUniqueId;
  int iChannelNumber;
  int selectiveRecallSeconds;
  bool recordingEnabled;
  std::string name;
  std::string strLogoPath;
  std::string cid;
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
      bool m_alternativeEpgService, const std::string& streamType, int provider,
      const std::string& xmlTVFile);
  virtual ~ZatData();
  virtual bool Initialize();
  virtual bool LoadChannels();
  virtual void GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities);
  virtual int GetChannelsAmount();
  virtual PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio);
  virtual int GetChannelGroupsAmount();
  virtual PVR_ERROR GetChannelGroups(ADDON_HANDLE handle);
  virtual PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle,
      const PVR_CHANNEL_GROUP &group);
  virtual void GetEPGForChannel(const PVR_CHANNEL &channel, time_t iStart,
      time_t iEnd);
  virtual void GetEPGForChannelAsync(int uniqueChannelId, time_t iStart,
      time_t iEnd);
  virtual std::string GetChannelStreamUrl(int uniqueId);
  virtual void GetRecordings(ADDON_HANDLE handle, bool future);
  virtual int GetRecordingsAmount(bool future);
  virtual std::string GetRecordingStreamUrl(const std::string& recordingId);
  virtual bool Record(int programId);
  virtual bool DeleteRecording(const std::string& recordingId);
  virtual void SetRecordingPlayCount(const PVR_RECORDING &recording, int count);
  virtual void SetRecordingLastPlayedPosition(const PVR_RECORDING &recording,
      int lastplayedposition);
  virtual int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording);
  virtual bool IsPlayable(const EPG_TAG *tag);
  virtual int GetRecallSeconds(const EPG_TAG *tag);
  virtual bool IsRecordable(const EPG_TAG *tag);
  virtual std::string GetEpgTagUrl(const EPG_TAG *tag);
  virtual bool RecordingEnabled()
  {
    return m_recordingEnabled;
  }

private:
  bool m_alternativeEpgService;
  bool m_favoritesOnly;
  std::string m_streamType;
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
  Document Login();
  bool InitSession();
  std::string HttpGetCached(const std::string& url, time_t cacheDuration, const std::string& userAgent = "");
  std::string HttpGet(const std::string& url, bool isInit = false, const std::string& userAgent = "");
  std::string HttpDelete(const std::string& url, bool isInit = false);
  std::string HttpPost(const std::string& url, const std::string& postData, bool isInit = false, const std::string& userAgent = "");
  std::string HttpRequest(const std::string& action, const std::string& url, const std::string& postData, bool isInit, const std::string& userAgent);
  std::string HttpRequestToCurl(Curl &curl, const std::string& action, const std::string& url,
                           const std::string& postData, int &statusCode);
  virtual std::map<time_t, PVRIptvEpgEntry>* LoadEPG(time_t iStart, time_t iEnd,
      int uniqueChannelId);
  virtual ZatChannel* FindChannel(int uniqueId);
  virtual PVRZattooChannelGroup* FindGroup(const std::string& strName);
  virtual int GetChannelId(const char * strChannelName);
  virtual void GetEPGForChannelExternalService(int uniqueChannelId,
      time_t iStart, time_t iEnd);
  virtual std::string GetStringOrEmpty(const Value& jsonValue, const char* fieldName);
};
