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

using namespace std;

/*!
 * @brief PVR macros for string exchange
 */
#define PVR_STRCPY(dest, source) do { strncpy(dest, source, sizeof(dest)-1); dest[sizeof(dest)-1] = '\0'; } while(0)
#define PVR_STRCLR(dest) memset(dest, 0, sizeof(dest))

struct PVRIptvEpgEntry
{
  int iBroadcastId;
  int iChannelId;
  int iGenreType;
  int iGenreSubType;
  time_t startTime;
  time_t endTime;
  string strTitle;
  string strPlotOutline;
  string strPlot;
  string strIconPath;
  string strGenreString;
};

struct PVRIptvEpgChannel
{
  string strId;
  string strName;
  string strIcon;
  vector<PVRIptvEpgEntry> epg;
};

struct ZatChannel
{
  bool bRadio;
  int iUniqueId;
  int iChannelNumber;
  int iEncryptionSystem;
  int iTvgShift;
  int selectiveRecallSeconds;
  bool recordingEnabled;
  string name;
  string strLogoPath;
  string strStreamURL;
  string strTvgId;
  string strTvgName;
  string strTvgLogo;
  string cid;
};

struct ZatRecordingData
{
  string recordingId;
  int playCount;
  int lastPlayedPosition;
  bool stillValid;
};

struct PVRZattooChannelGroup
{
  string name;
  vector<ZatChannel> channels;
};

class ZatData
{
public:
  ZatData(string username, string password, bool favoritesOnly,
      bool alternativeEpgService, string streamType, int provider);
  virtual ~ZatData();
  virtual bool Initialize();
  virtual bool LoadChannels();
  virtual void GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities);
  virtual int GetChannelsAmount(void);
  virtual PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio);
  virtual int GetChannelGroupsAmount(void);
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
  virtual string GetRecordingStreamUrl(string recordingId);
  virtual bool Record(int programId);
  virtual bool DeleteRecording(string recordingId);
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
  string countryCode = "";
  string serviceRegionCountry = "";
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

  bool LoadAppId();
  bool ReadDataJson();
  bool WriteDataJson();
  string GetUUID();
  string GenerateUUID();
  bool SendHello(string uuid);
  bool Login();
  bool InitSession();
  string HttpGetCached(string url, time_t cacheDuration, string userAgent = "");
  string HttpGet(string url, bool isInit = false, string userAgent = "");
  string HttpDelete(string url, bool isInit = false);
  string HttpPost(string url, string postData, bool isInit = false, string userAgent = "");
  string HttpRequest(string action, string url, string postData, bool isInit, string userAgent);
  string HttpRequestToCurl(Curl &curl, string action, string url,
      string postData, int &statusCode);
  virtual map<time_t, PVRIptvEpgEntry>* LoadEPG(time_t iStart, time_t iEnd,
      int uniqueChannelId);
  virtual ZatChannel* FindChannel(int uniqueId);
  virtual PVRZattooChannelGroup* FindGroup(const string &strName);
  virtual int GetChannelId(const char * strChannelName);
  virtual void GetEPGForChannelExternalService(int uniqueChannelId,
      time_t iStart, time_t iEnd);
};
