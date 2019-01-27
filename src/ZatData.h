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
using namespace std;

/*!
 * @brief PVR macros for string exchange
 */
#define PVR_STRCPY(dest, source) do { strncpy(dest, source, sizeof(dest)-1); (dest)[sizeof(dest)-1] = '\0'; } while(0)

struct PVRIptvEpgEntry
{
  int iBroadcastId;
  int iChannelId;
  time_t startTime;
  time_t endTime;
  string strTitle;
  string strPlot;
  string strIconPath;
  string strGenreString;
};

struct ZatChannel
{
  int iUniqueId;
  int iChannelNumber;
  int selectiveRecallSeconds;
  bool recordingEnabled;
  string name;
  string strLogoPath;
  string cid;
};

struct ZatRecordingData
{
  string recordingId;
  int playCount;
  int lastPlayedPosition;
  bool stillValid;
};

struct ZatRecordingDetails
{
  string genre;
  string description;
};

struct PVRZattooChannelGroup
{
  string name;
  vector<ZatChannel> channels;
};

class ZatData
{
public:
  ZatData(const string& username, const string& password, bool favoritesOnly,
      bool alternativeEpgService, const string& streamType, int provider,
      const string& xmlTVFile);
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
  virtual string GetChannelStreamUrl(int uniqueId);
  virtual void GetRecordings(ADDON_HANDLE handle, bool future);
  virtual int GetRecordingsAmount(bool future);
  virtual string GetRecordingStreamUrl(const string& recordingId);
  virtual bool Record(int programId);
  virtual bool DeleteRecording(const string& recordingId);
  virtual void SetRecordingPlayCount(const PVR_RECORDING &recording, int count);
  virtual void SetRecordingLastPlayedPosition(const PVR_RECORDING &recording,
      int lastplayedposition);
  virtual int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording);
  virtual bool IsPlayable(const EPG_TAG *tag);
  virtual int GetRecallSeconds(const EPG_TAG *tag);
  virtual bool IsRecordable(const EPG_TAG *tag);
  virtual string GetEpgTagUrl(const EPG_TAG *tag);
  virtual bool RecordingEnabled()
  {
    return recordingEnabled;
  }

private:
  string appToken;
  string powerHash;
  string countryCode;
  string serviceRegionCountry;
  bool recallEnabled = false;
  bool selectiveRecallEnabled = false;
  bool recordingEnabled = false;
  string streamType;
  string username;
  string password;
  bool favoritesOnly;
  bool alternativeEpgService;
  vector<PVRZattooChannelGroup> channelGroups;
  map<int, ZatChannel> channelsByUid;
  map<string, ZatChannel> channelsByCid;
  map<string, ZatRecordingData*> recordingsData;
  int64_t maxRecallSeconds = 0;
  string beakerSessionId;
  string pzuid;
  vector<UpdateThread*> updateThreads;
  string uuid;
  Categories categories;
  string providerUrl;
  bool recordingsLoaded = false;
  string xmlTVFile;
  XmlTV *xmlTV;

  bool LoadAppId();
  bool ReadDataJson();
  bool WriteDataJson();
  string GetUUID();
  string GenerateUUID();
  bool SendHello(string uuid);
  Document Login();
  bool InitSession();
  string HttpGetCached(const string& url, time_t cacheDuration, const string& userAgent = "");
  string HttpGet(const string& url, bool isInit = false, const string& userAgent = "");
  string HttpDelete(const string& url, bool isInit = false);
  string HttpPost(const string& url, const string& postData, bool isInit = false, const string& userAgent = "");
  string HttpRequest(const string& action, const string& url, const string& postData, bool isInit, const string& userAgent);
  string HttpRequestToCurl(Curl &curl, const string& action, const string& url,
                           const string& postData, int &statusCode);
  virtual map<time_t, PVRIptvEpgEntry>* LoadEPG(time_t iStart, time_t iEnd,
      int uniqueChannelId);
  virtual ZatChannel* FindChannel(int uniqueId);
  virtual PVRZattooChannelGroup* FindGroup(const string& strName);
  virtual int GetChannelId(const char * strChannelName);
  virtual void GetEPGForChannelExternalService(int uniqueChannelId,
      time_t iStart, time_t iEnd);
  virtual string GetStringOrEmpty(const Value& jsonValue, const char* fieldName);
};
