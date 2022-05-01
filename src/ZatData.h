#include "UpdateThread.h"
#include "categories.h"
#include <map>
#include <thread>
#include <mutex>
#include "rapidjson/document.h"
#include "ZatChannel.h"
#include "Settings.h"
#include "sql/EpgDB.h"
#include "sql/RecordingsDB.h"
#include "sql/ParameterDB.h"
#include "http/HttpClient.h"
#include "epg/EpgProvider.h"
#include "Session.h"

class CZattooTVAddon;

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

class ATTR_DLL_LOCAL ZatData :  public kodi::addon::CAddonBase,
                                public kodi::addon::CInstancePVRClient
{
public:
  ADDON_STATUS Create() override;
  ZatData();
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
  PVR_ERROR GetRecordingEdl(const kodi::addon::PVRRecording& recording, std::vector<kodi::addon::PVREDLEntry>& edl) override;  

  int GetRecallSeconds(const kodi::addon::PVREPGTag& tag);
  void GetEPGForChannelAsync(int uniqueChannelId, time_t iStart, time_t iEnd);
  bool RecordingEnabled()
  {
    return m_session->IsRecordingEnabled();
  }
  void UpdateConnectionState(const std::string& connectionString, PVR_CONNECTION_STATE newState, const std::string& message);
  bool SessionInitialized();
  ADDON_STATUS SetSetting(const std::string& settingName,
                          const kodi::addon::CSettingValue& settingValue) override;

private:
  std::vector<PVRZattooChannelGroup> m_channelGroups;
  std::map<int, ZatChannel> m_channelsByUid;
  std::map<std::string, ZatChannel> m_channelsByCid;
  std::map<std::string, ZatChannel> m_visibleChannelsByCid;
  std::vector<UpdateThread*> m_updateThreads;
  Categories m_categories;
  EpgDB *m_epgDB;
  RecordingsDB *m_recordingsDB;
  ParameterDB *m_parameterDB;
  HttpClient *m_httpClient;
  EpgProvider *m_epgProvider = nullptr;
  CSettings* m_settings;
  Session *m_session;

  bool ReadDataJson();
  rapidjson::Document Login();
  bool InitSession(bool isReinit);
  bool ReinitSession();
  ZatChannel* FindChannel(int uniqueId);
  PVRZattooChannelGroup* FindGroup(const std::string& strName);
  std::string GetStreamTypeString();
  std::string GetStreamUrl(std::string& jsonString, std::vector<kodi::addon::PVRStreamProperty>& properties);
  std::string GetStreamParameters();
  bool ParseRecordingsTimers(const rapidjson::Value& recordings, std::map<int, ZatRecordingDetails>& detailsById);
  void AddTimerType(std::vector<kodi::addon::PVRTimerType>& types, int idx, int attributes);
  bool Record(int programId, bool series);
  std::string GetManifestType();
  std::string GetMimeType();
  void SetStreamProperties(std::vector<kodi::addon::PVRStreamProperty>& properties, const std::string& url);
  std::string GetStreamUrlForProgram(const std::string& cid, int programId, std::vector<kodi::addon::PVRStreamProperty>& properties);
  bool TryToReinitIf403(int statusCode);
};
