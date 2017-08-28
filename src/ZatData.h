//
// Created by johannes on 04.02.16.
//

#include "client.h"
#include "UpdateThread.h"
#include "categories.h"
#include "Curl.h"
#include <map>

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
      bool alternativeEpgService, string streamType);
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
  virtual PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle,
      const PVR_CHANNEL &channel, time_t iStart, time_t iEnd);
  virtual PVR_ERROR GetEPGForChannelExternalService(ADDON_HANDLE handle,
      const PVR_CHANNEL &channel, time_t iStart, time_t iEnd);
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
  virtual bool IsRecordable(const EPG_TAG *tag);
  virtual string GetEpgTagUrl(const EPG_TAG *tag);

private:
  int m_iLastStart;
  int m_iLastEnd;
  string appToken;
  string powerHash;
  string countryCode;
  bool recallEnabled;
  bool recordingEnabled;
  string streamType;
  string username;
  string password;
  bool favoritesOnly;
  bool alternativeEpgService;
  vector<PVRZattooChannelGroup> channelGroups;
  map<int, ZatChannel> channelsByUid;
  map<string, ZatChannel> channelsByCid;
  map<string, ZatRecordingData*> recordingsData;
  map<string, map<time_t, PVRIptvEpgEntry>*> epgCache;
  int64_t maxRecallSeconds;
  Curl *curl;
  UpdateThread *updateThread;
  string uuid;
  Categories categories;

  bool LoadAppId();
  bool ReadDataJson();
  bool WriteDataJson();
  string GetUUID();
  string GenerateUUID();
  bool SendHello(string uuid);
  bool Login();
  bool InitSession();
  virtual string HttpGet(string url, bool isInit = false);
  virtual string HttpPost(string url, string postData, bool isInit = false);
  virtual bool LoadEPG(time_t iStart, time_t iEnd);
  virtual ZatChannel* FindChannel(int uniqueId);
  virtual PVRZattooChannelGroup* FindGroup(const string &strName);
  virtual int GetChannelId(const char * strChannelName);
  time_t StringToTime(string timeString);
  bool LoadAppIdFromFile();
};
