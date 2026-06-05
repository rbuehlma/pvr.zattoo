#include <iostream>
#include <string>
#include "ZatData.h"
#include <sstream>
#include <map>
#include <ctime>
#include <utility>
#include "Utils.h"
#include <kodi/Filesystem.h>
#include "epg/ZattooEpgProvider.h"

#ifdef TARGET_ANDROID
#include "to_string.h"
#endif

using json = nlohmann::json;

constexpr char app_token_file[] = "special://temp/zattoo_app_token";
const char data_file[] = "special://profile/addon_data/pvr.zattoo/data.json";

void ZatData::SetStreamProperties(
    std::vector<kodi::addon::PVRStreamProperty>& properties,
    const std::string& url)
{
  properties.emplace_back(PVR_STREAM_PROPERTY_STREAMURL, url);
  properties.emplace_back(PVR_STREAM_PROPERTY_INPUTSTREAM, "inputstream.adaptive");
  properties.emplace_back(PVR_STREAM_PROPERTY_MIMETYPE, "application/xml+dash");
}

bool ZatData::ReadDataJson()
{
  if (!kodi::vfs::FileExists(data_file, true))
  {
    return true;
  }
  std::string jsonString = Utils::ReadFile(data_file);
  if (jsonString.empty())
  {
    kodi::Log(ADDON_LOG_ERROR, "Loading data.json failed.");
    return false;
  }

  json doc;
  doc = json::parse(jsonString, nullptr, false);
  if (doc.is_discarded())
  {
    kodi::Log(ADDON_LOG_ERROR, "Parsing data.json failed.");
    return false;
  }

  if (doc.contains("recordings")) {
    const json& recordings = doc["recordings"];
    for (const auto& recording : recordings)
    {
      RecordingDBInfo recordingDBInfo;
      recordingDBInfo.recordingId = Utils::JsonStringOrEmpty(recording, "recordingId");
      recordingDBInfo.playCount = recording["playCount"].get<int>();
      recordingDBInfo.lastPlayedPosition = recording["lastPlayedPosition"].get<int>();
      m_recordingsDB->Set(recordingDBInfo);
    }
  }
  kodi::Log(ADDON_LOG_DEBUG, "Loaded data.json.");
  return true;
}

bool ZatData::LoadChannels()
{
  std::map<std::string, ZatChannel> allChannels;
  int statusCode;
  std::string jsonString = m_httpClient->HttpGet(m_session->GetProviderUrl() + "/zapi/channels/favorites", statusCode);
  json favDoc;
  favDoc = json::parse(jsonString, nullptr, false);

  if (favDoc.is_discarded() || !favDoc["success"].get<bool>())
  {
    return false;
  }
  const json& favs = favDoc["favorites"];

  std::ostringstream urlStream;
  urlStream << m_session->GetProviderUrl() + "/zapi/v3/cached/"  << m_session->GetPowerHash() << "/channels";
  jsonString = m_httpClient->HttpGet(urlStream.str(), statusCode);

  json doc;
  doc = json::parse(jsonString, nullptr, false);
  if (doc.is_discarded() || !doc.contains("channels"))
  {
    kodi::Log(ADDON_LOG_ERROR, "Failed to load channels");
    return false;
  }

  int channelNumber = static_cast<int>(favs.size());
  const json& groups = doc["groups"];

  //Load the channel groups and channels
  for (const auto& groupItem : groups)
  {
    PVRZattooChannelGroup group;
    group.name = Utils::JsonStringOrEmpty(groupItem, "name");
    m_channelGroups.insert(m_channelGroups.end(), group);
  }
  const json& channels = doc["channels"];
  for (const auto& channelItem : channels)
  {
    ZatChannel channel;
    std::string cid = Utils::JsonStringOrEmpty(channelItem, "cid");
    channel.iUniqueId = Utils::GetChannelId(cid.c_str());
    channel.cid = cid;
    channel.iChannelNumber = ++channelNumber;
    channel.recordingEnabled =
        channelItem.contains("recording") ?
            channelItem["recording"].get<bool>() : false;

    const json& qualities = channelItem["qualities"];
    for (const auto& qualityItem : qualities)
    {
      std::string avail = Utils::JsonStringOrEmpty(qualityItem, "availability");
      if (avail != "available") {
        continue;
      }
      bool drmRequired = qualityItem.contains("drm_required") ? qualityItem["drm_required"].get<bool>() : false;
      channel.qualityWithDrm.push_back({Utils::JsonStringOrEmpty(qualityItem, "level"), drmRequired});
      if (channel.name.empty()) {
        channel.name = Utils::JsonStringOrEmpty(qualityItem, "title");
        channel.strLogoPath = "http://logos.zattic.com" + Utils::JsonStringOrEmpty(qualityItem, "logo_white_84");
      }
    }
    PVRZattooChannelGroup &group = m_channelGroups[channelItem["group_index"].get<int>()];
    group.channels.insert(group.channels.end(), channel);
    allChannels[cid] = channel;
    m_channelsByCid[channel.cid] = channel;
    m_channelsByUid[channel.iUniqueId] = channel;
  }

  PVRZattooChannelGroup favGroup;
  favGroup.name = "Favoriten";

  for (const auto& favItem : favs)
  {
    std::string favCid = favItem.get<std::string>();
    if (allChannels.find(favCid) != allChannels.end())
    {
      ZatChannel channel = allChannels[favCid];
      channel.iChannelNumber = static_cast<int>(favGroup.channels.size() + 1);
      favGroup.channels.insert(favGroup.channels.end(), channel);
      m_channelsByCid[channel.cid] = channel;
      m_channelsByUid[channel.iUniqueId] = channel;
    }
  }

  if (m_settings->GetZatFavoritesOnly()) {
    m_channelGroups.clear();
  }

  if (!favGroup.channels.empty())
    m_channelGroups.insert(m_channelGroups.end(), favGroup);

  for (const PVRZattooChannelGroup &group : m_channelGroups) {
    for (const ZatChannel &channel : group.channels) {
      m_visibleChannelsByCid[channel.cid] = channel;
    }
  }
  return true;
}

PVR_ERROR ZatData::GetChannelGroupsAmount(int& amount)
{

  if (!m_session->IsConnected())
  {
    return PVR_ERROR_SERVER_ERROR;
  }

  amount = static_cast<int>(m_channelGroups.size());
  return PVR_ERROR_NO_ERROR;
}

ZatData::ZatData() :
  m_settings(new CSettings())
{
  m_settings->Load();
  m_epgDB = new EpgDB(UserPath());
  m_recordingsDB = new RecordingsDB(UserPath());
  m_parameterDB = new ParameterDB(UserPath());
  m_httpClient = new HttpClient(m_parameterDB);
  m_session = new Session(m_httpClient, this, m_settings, m_parameterDB);
  m_httpClient->SetStatusCodeHandler(m_session);

  UpdateConnectionState("Initializing", PVR_CONNECTION_STATE_CONNECTING, "");

  ReadDataJson();

  for (int i = 0; i < 3; ++i)
  {
    m_updateThreads.emplace_back(new UpdateThread(*this, i, this));
  }
}

ZatData::~ZatData()
{
  for (auto updateThread : m_updateThreads)
  {
    delete updateThread;
  }
  m_channelGroups.clear();

  if (m_epgProvider) {
    delete m_epgProvider;
  }
  delete m_session;
  delete m_httpClient;
  delete m_parameterDB;
  delete m_recordingsDB;
  delete m_epgDB;
}

PVR_ERROR ZatData::GetCapabilities(kodi::addon::PVRCapabilities& capabilities)
{
  capabilities.SetSupportsEPG(true);
  capabilities.SetSupportsEPGEdl(true);
  capabilities.SetSupportsTV(true);
  capabilities.SetSupportsRadio(true);
  capabilities.SetSupportsChannelGroups(true);
  capabilities.SetSupportsRecordingPlayCount(true);
  capabilities.SetSupportsLastPlayedPosition(true);
  capabilities.SetSupportsRecordingsRename(true);
  capabilities.SetSupportsRecordingsLifetimeChange(false);
  capabilities.SetSupportsDescrambleInfo(false);
  capabilities.SetSupportsRecordingEdl(true);
  capabilities.SetSupportsRecordings(m_session->IsRecordingEnabled());
  capabilities.SetSupportsRecordingsDelete(m_session->IsRecordingEnabled());
  capabilities.SetSupportsTimers(m_session->IsRecordingEnabled());

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetBackendName(std::string& name)
{
  name = "Zattoo PVR Add-on";
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetBackendVersion(std::string& version)
{
  version = STR(IPTV_VERSION);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetBackendHostname(std::string& hostname)
{
  hostname = "";
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetConnectionString(std::string& connection)
{
  connection = m_session->IsConnected() ? "connected" : "not connected";
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetChannelGroups(bool radio, kodi::addon::PVRChannelGroupsResultSet& results)
{
  if (radio)
    return PVR_ERROR_NOT_IMPLEMENTED;

  if (!m_session->IsConnected())
  {
    return PVR_ERROR_SERVER_ERROR;
  }

  for (const auto& group : m_channelGroups)
  {
    kodi::addon::PVRChannelGroup xbmcGroup;

    xbmcGroup.SetPosition(0); /* not supported  */
    xbmcGroup.SetIsRadio(false); /* is radio group */
    xbmcGroup.SetGroupName(group.name);

    results.Add(xbmcGroup);
  }
  return PVR_ERROR_NO_ERROR;
}

PVRZattooChannelGroup *ZatData::FindGroup(const std::string &strName)
{
  std::vector<PVRZattooChannelGroup>::iterator it;
  for (it = m_channelGroups.begin(); it < m_channelGroups.end(); ++it)
  {
    if (it->name == strName)
      return &*it;
  }

  return nullptr;
}

PVR_ERROR ZatData::GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group,
                                          kodi::addon::PVRChannelGroupMembersResultSet& results)
{

  if (!m_session->IsConnected())
  {
    return PVR_ERROR_SERVER_ERROR;
  }

  PVRZattooChannelGroup *myGroup;
  if ((myGroup = FindGroup(group.GetGroupName())) != nullptr)
  {
    std::vector<ZatChannel>::iterator it;
    for (const auto& channel : myGroup->channels)
    {
      kodi::addon::PVRChannelGroupMember member;

      member.SetGroupName(group.GetGroupName());
      member.SetChannelUniqueId(static_cast<unsigned int>(channel.iUniqueId));
      member.SetChannelNumber(static_cast<unsigned int>(channel.iChannelNumber));

      results.Add(member);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetChannelsAmount(int& amount)
{
  if (!m_session->IsConnected())
  {
    return PVR_ERROR_SERVER_ERROR;
  }

  amount = static_cast<int>(m_visibleChannelsByCid.size());
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results)
{
  if (radio)
    return PVR_ERROR_NO_ERROR;

  if (!m_session->IsConnected())
  {
    return PVR_ERROR_SERVER_ERROR;
  }

  std::vector<PVRZattooChannelGroup>::iterator it;
  for (it = m_channelGroups.begin(); it != m_channelGroups.end(); ++it)
  {
    std::vector<ZatChannel>::iterator it2;
    for (it2 = it->channels.begin(); it2 != it->channels.end(); ++it2)
    {
      ZatChannel &channel = (*it2);

      kodi::addon::PVRChannel kodiChannel;

      kodiChannel.SetUniqueId(static_cast<unsigned int>(channel.iUniqueId));
      kodiChannel.SetIsRadio(false);
      kodiChannel.SetChannelNumber(static_cast<unsigned int>(channel.iChannelNumber));
      kodiChannel.SetChannelName(channel.name);
      kodiChannel.SetEncryptionSystem(0);

      std::ostringstream iconStream;
      iconStream
          << "special://home/addons/pvr.zattoo/resources/media/channel_logo/"
          << channel.cid << ".png";
      std::string iconPath = iconStream.str();
      if (!kodi::vfs::FileExists(iconPath, true))
      {
        std::ostringstream iconStreamSystem;
        iconStreamSystem
            << "special://xbmc/addons/pvr.zattoo/resources/media/channel_logo/"
            << channel.cid << ".png";
        iconPath = iconStreamSystem.str();
        if (!kodi::vfs::FileExists(iconPath, true))
        {
          kodi::Log(ADDON_LOG_INFO,
              "No logo found for channel '%s'. Fallback to Zattoo-Logo.",
              channel.cid.c_str());
          iconPath = channel.strLogoPath;
        }
      }

      kodiChannel.SetIconPath(iconPath);
      kodiChannel.SetIsHidden(false);

      results.Add(kodiChannel);

    }
  }
  return PVR_ERROR_NO_ERROR;
}

bool ZatData::IsDrmLimitApplied(json& doc) {
  return doc.contains("drm_limit_applied") && doc["drm_limit_applied"].get<bool>();
}

std::string ZatData::GetStreamUrl(json& doc, std::vector<kodi::addon::PVRStreamProperty>& properties) {
  if (!doc.contains("stream"))
  {
    return "";
  }
  const json& watchUrls = doc["stream"]["watch_urls"];
  std::string url = Utils::JsonStringOrEmpty(doc["stream"], "url");
  for (const auto& watchUrl : watchUrls)
  {
    kodi::Log(ADDON_LOG_DEBUG, "Selected url for maxrate: %d", watchUrl["maxrate"].get<int>());
    url = Utils::JsonStringOrEmpty(watchUrl, "url");
    std::string licenseUrl = Utils::JsonStringOrEmpty(watchUrl, "license_url");
    properties.emplace_back("inputstream.adaptive.drm", "{\"com.widevine.alpha\":{\"license\":{\"server_url\":\"" + licenseUrl + "\"}}}");
    break;
  }
  kodi::Log(ADDON_LOG_DEBUG, "Got url: %s", url.c_str());
  return url;
}

PVR_ERROR ZatData::GetChannelStreamProperties(const kodi::addon::PVRChannel& channel,
                                              PVR_SOURCE source,
                                              std::vector<kodi::addon::PVRStreamProperty>& properties)
{
  PVR_ERROR ret = PVR_ERROR_FAILED;

  ZatChannel* ownChannel = FindChannel(channel.GetUniqueId());
  kodi::Log(ADDON_LOG_DEBUG, "Get live url for channel %s", ownChannel->cid.c_str());

  bool forceWithoutDrm = GetDrmLevel() <= 0;
  json doc;

  while (true) {
    std::ostringstream dataStream;
    bool requiresDrm;
    dataStream << GetQualityStreamParameter(ownChannel->cid, forceWithoutDrm, requiresDrm);
    dataStream << GetBasicStreamParameters(requiresDrm) << "&format=json&timeshift=10800";
    kodi::Log(ADDON_LOG_INFO, "Stream properties: %s.", dataStream.str().c_str());
    int statusCode;
    std::string jsonString = m_httpClient->HttpPost(m_session->GetProviderUrl() + "/zapi/watch/live/" + ownChannel->cid, dataStream.str(), statusCode);

    doc = json::parse(jsonString, nullptr, false);
    if (doc.is_discarded())
    {
      return ret;
    }

    if (forceWithoutDrm || !IsDrmLimitApplied(doc)) {
      break;
    }
    forceWithoutDrm = true;
    kodi::Log(ADDON_LOG_INFO, "Fallback to no-drm version.");
    doc = json();
  }

  std::string strUrl = GetStreamUrl(doc, properties);
  if (!strUrl.empty())
  {
    SetStreamProperties(properties, strUrl);
    properties.emplace_back(PVR_STREAM_PROPERTY_ISREALTIMESTREAM, "true");
    ret = PVR_ERROR_NO_ERROR;
  }
  return ret;
}

ZatChannel *ZatData::FindChannel(int uniqueId)
{
  std::vector<PVRZattooChannelGroup>::iterator it;
  for (it = m_channelGroups.begin(); it != m_channelGroups.end(); ++it)
  {
    std::vector<ZatChannel>::iterator it2;
    for (it2 = it->channels.begin(); it2 != it->channels.end(); ++it2)
    {
      ZatChannel &channel = (*it2);
      if (channel.iUniqueId == uniqueId)
      {
        return &channel;
      }
    }
  }
  return nullptr;
}

PVR_ERROR ZatData::GetEPGForChannel(int channelUid, time_t start, time_t end, kodi::addon::PVREPGTagsResultSet& results)
{
  // Aligning the start- and end times improves caching
  time_t aligendStart = start - (start % 86400);
  time_t alignedEnd = end - (end % 86400) + 86400;

  UpdateThread::LoadEpg(channelUid, aligendStart, alignedEnd);
  return PVR_ERROR_NO_ERROR;
}

void ZatData::GetEPGForChannelAsync(int uniqueChannelId, time_t iStart, time_t iEnd)
{
  if (!m_epgProvider) {
    kodi::Log(ADDON_LOG_WARNING, "EPG Provider not ready.");
    return;
  }
  ZatChannel* channel = FindChannel(uniqueChannelId);
  m_epgProvider->LoadEPGForChannel(*channel, iStart, iEnd);
}

PVR_ERROR ZatData::SetRecordingPlayCount(const kodi::addon::PVRRecording& recording, int count)
{
  std::string recordingId = recording.GetRecordingId();
  RecordingDBInfo recordingDBInfo = m_recordingsDB->Get(recordingId);
  recordingDBInfo.playCount = count;
  m_recordingsDB->Set(recordingDBInfo);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::SetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recording,
    int lastplayedposition)
{
  std::string recordingId = recording.GetRecordingId();
  RecordingDBInfo recordingDBInfo = m_recordingsDB->Get(recordingId);
  recordingDBInfo.lastPlayedPosition = lastplayedposition;
  m_recordingsDB->Set(recordingDBInfo);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recording, int& position)
{
  std::string recordingId = recording.GetRecordingId();
  RecordingDBInfo recordingDBInfo = m_recordingsDB->Get(recordingId);
  position = recordingDBInfo.lastPlayedPosition;
  return PVR_ERROR_NO_ERROR;
}

bool ZatData::ParseRecordingsTimers(const json& recordings, std::map<int, ZatRecordingDetails>& detailsById)
{
  size_t recordingsIdx = 0;
  while (recordingsIdx < recordings.size())
  {
    int bucketSize = 100;
    std::ostringstream urlStream;
    urlStream << m_session->GetProviderUrl() << "/zapi/v2/cached/program/power_details/"
        << m_session->GetPowerHash() << "?complete=True&program_ids=";
    while (bucketSize > 0 && recordingsIdx < recordings.size())
    {
      const json& recording = recordings[recordingsIdx];
      if (bucketSize < 100)
      {
        urlStream << ",";
      }
      urlStream << recording["program_id"].get<int>();
      ++recordingsIdx;
      bucketSize--;
    }
    int statusCode;
    std::string jsonString = m_httpClient->HttpGetCached(urlStream.str(), 60 * 60 * 24 * 30, statusCode);
    json detailDoc;
    detailDoc = json::parse(jsonString, nullptr, false);
    if (detailDoc.is_discarded() || !detailDoc["success"].get<bool>())
    {
      kodi::Log(ADDON_LOG_ERROR, "Failed to load details for recordings.");
    }
    else
    {
      const json& programs = detailDoc["programs"];
      for (const auto& program : programs)
      {
        ZatRecordingDetails details;
        if (program.contains("g") && program["g"].is_array()
            && !program["g"].empty())
        {
          details.genre = program["g"][0].get<std::string>();
        }
        else
        {
          details.genre = "";
        }
        details.description = Utils::JsonStringOrEmpty(program, "d");
        details.seriesNumber = program.contains("s_no") && !program["s_no"].is_null() ? program["s_no"].get<int>() : EPG_TAG_INVALID_SERIES_EPISODE;
        details.episodeNumber = program.contains("e_no") && !program["e_no"].is_null() ? program["e_no"].get<int>() : EPG_TAG_INVALID_SERIES_EPISODE;

        detailsById.insert(
            std::pair<int, ZatRecordingDetails>(program["id"].get<int>(), details));
      }
    }

  }

  return true;
}

PVR_ERROR ZatData::GetTimerTypes(std::vector<kodi::addon::PVRTimerType>& types)
{
  AddTimerType(types, 0, PVR_TIMER_TYPE_REQUIRES_EPG_TAG_ON_CREATE);
  AddTimerType(types, 1, PVR_TIMER_TYPE_REQUIRES_EPG_SERIES_ON_CREATE | PVR_TIMER_TYPE_IS_REPEATING);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetTimers(kodi::addon::PVRTimersResultSet& results)
{

  if (!m_session->IsConnected())
  {
    return PVR_ERROR_SERVER_ERROR;
  }

  int statusCode;
  std::string jsonString = m_httpClient->HttpGet(m_session->GetProviderUrl() + "/zapi/v2/playlist", statusCode);

  json doc;
  doc = json::parse(jsonString, nullptr, false);
  if (doc.is_discarded() || !doc["success"].get<bool>())
  {
    return PVR_ERROR_FAILED;
  }

  const json& recordings = doc["recordings"];
  std::map<int, ZatRecordingDetails> detailsById;
  ParseRecordingsTimers(recordings, detailsById);

  time_t current_time;
  time(&current_time);

  for (const auto& recording : recordings)
  {
    int programId = recording["program_id"].get<int>();

    auto detailIterator = detailsById.find(programId);
    bool hasDetails = detailIterator != detailsById.end();

    //genre
    int genre = 0;
    if (hasDetails)
    {
      genre = m_categories.Category(detailIterator->second.genre);
    }

    time_t startTime = Utils::StringToTime(
        Utils::JsonStringOrEmpty(recording, "start"));
    if (startTime > current_time)
    {
      kodi::addon::PVRTimer tag;

      tag.SetClientIndex(static_cast<unsigned int>(recording["id"].get<int>()));
      tag.SetTitle(Utils::JsonStringOrEmpty(recording, "title"));
      tag.SetSummary(Utils::JsonStringOrEmpty(recording, "episode_title"));
      time_t endTime = Utils::StringToTime(
          Utils::JsonStringOrEmpty(recording, "end").c_str());
      tag.SetStartTime(startTime);
      tag.SetEndTime(endTime);
      tag.SetState(PVR_TIMER_STATE_SCHEDULED);
      tag.SetTimerType(1);
      tag.SetEPGUid(static_cast<unsigned int>(recording["program_id"].get<int>()));
      std::string cid = Utils::JsonStringOrEmpty(recording, "cid");
      auto iterator = m_channelsByCid.find(cid);
      if (iterator != m_channelsByCid.end())
      {
        ZatChannel channel = iterator->second;
        tag.SetClientChannelUid(channel.iUniqueId);
      }

      if (genre)
      {
        tag.SetGenreSubType(genre & 0x0F);
        tag.SetGenreType(genre & 0xF0);
      }
      results.Add(tag);
      UpdateThread::SetNextRecordingUpdate(startTime);
    }
  }

  if (doc.contains("recorded_tv_series")) {
    const json& recordingsTvSeries = doc["recorded_tv_series"];

    for (const auto& recording : recordingsTvSeries)
    {
      int tvSeriesId = recording["tv_series_id"].get<int>();

      kodi::addon::PVRTimer tag;

      std::string cid = Utils::JsonStringOrEmpty(recording, "cid");
      auto iterator = m_channelsByCid.find(cid);
      if (iterator != m_channelsByCid.end())
      {
        ZatChannel channel = iterator->second;
        tag.SetClientChannelUid(channel.iUniqueId);
      }

      tag.SetClientIndex(static_cast<unsigned int>(tvSeriesId));
      tag.SetTitle(Utils::JsonStringOrEmpty(recording, "title"));
      tag.SetState(PVR_TIMER_STATE_SCHEDULED);
      tag.SetTimerType(2);
      results.Add(tag);
    }
  }
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetTimersAmount(int& amount)
{

  if (!m_session->IsConnected())
  {
    return PVR_ERROR_SERVER_ERROR;
  }
  int statusCode;
  std::string jsonString = m_httpClient->HttpGetCached(m_session->GetProviderUrl() + "/zapi/v2/playlist", 60, statusCode);

  time_t current_time;
  time(&current_time);

  json doc;
  doc = json::parse(jsonString, nullptr, false);
  if (doc.is_discarded() || !doc["success"].get<bool>())
  {
    return PVR_ERROR_FAILED;
  }

  const json& recordings = doc["recordings"];

  amount = 0;
  for (const auto& recording : recordings)
  {
    time_t startTime = Utils::StringToTime(
        Utils::JsonStringOrEmpty(recording, "start"));
    if (startTime > current_time)
    {
      amount++;
    }
  }
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::AddTimer(const kodi::addon::PVRTimer& timer)
{
  PVR_ERROR ret = PVR_ERROR_NO_ERROR;
  if (timer.GetEPGUid() <= EPG_TAG_INVALID_UID)
  {
    ret = PVR_ERROR_REJECTED;
  }
  else if (!Record(timer.GetEPGUid(), timer.GetTimerType() == 2))
  {
    ret = PVR_ERROR_REJECTED;
  }
  else
  {
    kodi::addon::CInstancePVRClient::TriggerTimerUpdate();
    kodi::addon::CInstancePVRClient::TriggerRecordingUpdate();
  }

  return ret;
}

PVR_ERROR ZatData::DeleteTimer(const kodi::addon::PVRTimer& timer, bool forceDelete)
{
  bool series = timer.GetTimerType() == 2;
  int recordingId = -1;
  int statusCode;

  if (series) {
    std::string jsonString = m_httpClient->HttpGet(m_session->GetProviderUrl() + "/zapi/v2/playlist", statusCode);

    json doc;
    doc = json::parse(jsonString, nullptr, false);
    if (doc.is_discarded() || !doc["success"].get<bool>())
    {
      return PVR_ERROR_FAILED;
    }

    const json& recordings = doc["recordings"];

    for (const auto& recording : recordings)
    {
      unsigned int seriesId = recording["tv_series_id"].get<int>();

      if (seriesId == timer.GetClientIndex()) {
        recordingId = recording["id"].get<int>();
        break;
      }
    }
    if (recordingId == -1) {
      kodi::Log(ADDON_LOG_ERROR, "Did not find recording for serie %d.", timer.GetClientIndex());
      return PVR_ERROR_FAILED;
    }
  } else {
    recordingId = timer.GetClientIndex();
  }

  kodi::Log(ADDON_LOG_DEBUG, "Delete timer %d", recordingId);

  std::ostringstream dataStream;
  dataStream << "remove_recording=false&recording_id=" << recordingId << "";

  std::string path = series ? "/zapi/series_recording/remove" : "/zapi/playlist/remove";

  std::string jsonString = m_httpClient->HttpPost(m_session->GetProviderUrl() + path, dataStream.str(), statusCode);

  json doc;
  doc = json::parse(jsonString, nullptr, false);
  kodi::addon::CInstancePVRClient::TriggerTimerUpdate();
  return !doc.is_discarded() && doc["success"].get<bool>() ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

void ZatData::AddTimerType(std::vector<kodi::addon::PVRTimerType>& types, int idx, int attributes)
{
  kodi::addon::PVRTimerType type;
  type.SetId(static_cast<unsigned int>(idx + 1));
  type.SetAttributes(static_cast<unsigned int>(attributes));
  types.emplace_back(type);
}

PVR_ERROR ZatData::GetRecordings(bool deleted, kodi::addon::PVRRecordingsResultSet& results)
{

  if (!m_session->IsConnected())
  {
    return PVR_ERROR_SERVER_ERROR;
  }
  int statusCode;
  std::string jsonString = m_httpClient->HttpGet(m_session->GetProviderUrl() + "/zapi/v2/playlist", statusCode);

  json doc;
  doc = json::parse(jsonString, nullptr, false);
  if (doc.is_discarded() || !doc["success"].get<bool>())
  {
    return PVR_ERROR_FAILED;
  }

  const json& recordings = doc["recordings"];
  std::map<int, ZatRecordingDetails> detailsById;
  ParseRecordingsTimers(recordings, detailsById);

  time_t current_time;
  time(&current_time);

  for (const auto& recording : recordings)
  {
    int programId = recording["program_id"].get<int>();

    auto detailIterator = detailsById.find(programId);
    bool hasDetails = detailIterator != detailsById.end();

    //genre
    int genre = 0;
    if (hasDetails)
    {
      genre = m_categories.Category(detailIterator->second.genre);
    }

    time_t startTime = Utils::StringToTime(
        Utils::JsonStringOrEmpty(recording, "start"));
    if (startTime <= current_time)
    {
      kodi::addon::PVRRecording tag;

      tag.SetIsDeleted(false);

      tag.SetRecordingId(std::to_string(recording["id"].get<int>()));
      tag.SetTitle(Utils::JsonStringOrEmpty(recording, "title"));
      tag.SetEpisodeName(Utils::JsonStringOrEmpty(recording, "episode_title"));
      if (hasDetails) {
        tag.SetPlot(detailIterator->second.description);
        tag.SetSeriesNumber(detailIterator->second.seriesNumber);
        tag.SetEpisodeNumber(detailIterator->second.episodeNumber);
      }

      std::string imageToken = Utils::JsonStringOrEmpty(recording, "image_token");
      std::string imageUrl = Utils::GetImageUrl(imageToken);;
      tag.SetIconPath(imageUrl);

      std::string cid = Utils::JsonStringOrEmpty(recording, "cid");
      auto iterator = m_channelsByCid.find(cid);
      if (iterator != m_channelsByCid.end())
      {
        ZatChannel channel = iterator->second;
        tag.SetChannelUid(channel.iUniqueId);
        tag.SetChannelName(channel.name);
      } else {
        tag.SetChannelName(cid);
      }

      time_t endTime = Utils::StringToTime(
          Utils::JsonStringOrEmpty(recording, "end").c_str());
      tag.SetRecordingTime(startTime);
      tag.SetDuration(static_cast<int>(endTime - startTime));

      if (genre)
      {
        tag.SetGenreSubType(genre & 0x0F);
        tag.SetGenreType(genre & 0xF0);
      }

      if (Utils::JsonIntOrZero(recording, "tv_series_id")) {
          tag.SetDirectory(tag.GetTitle());
      }

      RecordingDBInfo recordingDBInfo = m_recordingsDB->Get(tag.GetRecordingId());
      tag.SetPlayCount(recordingDBInfo.playCount);
      tag.SetLastPlayedPosition(recordingDBInfo.lastPlayedPosition);
      m_recordingsDB->Set(recordingDBInfo);

      results.Add(tag);
    }

    m_recordingsDB->Cleanup();
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetRecordingsAmount(bool deleted, int& amount)
{

  if (!m_session->IsConnected())
  {
    return PVR_ERROR_SERVER_ERROR;
  }

  int statusCode;
  std::string jsonString = m_httpClient->HttpGetCached(m_session->GetProviderUrl() + "/zapi/v2/playlist", 60, statusCode);

  time_t current_time;
  time(&current_time);

  json doc;
  doc = json::parse(jsonString, nullptr, false);
  if (doc.is_discarded() || !doc["success"].get<bool>())
  {
    return PVR_ERROR_FAILED;
  }

  const json& recordings = doc["recordings"];

  amount = 0;
  for (const auto& recording : recordings)
  {
    time_t startTime = Utils::StringToTime(
        Utils::JsonStringOrEmpty(recording, "start"));
    if (startTime <= current_time)
    {
      amount++;
    }
  }
  return PVR_ERROR_NO_ERROR;
}

std::string ZatData::GetBasicStreamParameters(bool requiresDrm) {
  std::string params = m_settings->GetZatEnableDolby() ? "&enable_eac3=true" : "";

  params += "&stream_type=" + GetStreamTypeString(requiresDrm);
  int drmLevel = GetDrmLevel();
  if (drmLevel > 0) {
    params += "&max_drm_lvl=" + std::to_string(drmLevel);
  }

  if (!m_settings->GetParentalPin().empty()) {
    params += "&youth_protection_pin=" + m_settings->GetParentalPin();
  }

  return params;
}

std::string ZatData::GetQualityStreamParameter(const std::string& cid, bool forceWithoutDrm, bool& requiresDrm) {
  requiresDrm = !forceWithoutDrm;
  auto iterator = m_channelsByCid.find(cid);
  if (iterator != m_channelsByCid.end())
  {
    ZatChannel channel = iterator->second;
    std::string selectedQuality;
    for (auto const& pair : channel.qualityWithDrm)
    {
      if (!forceWithoutDrm || !pair.second) {
        selectedQuality = pair.first;
        requiresDrm = pair.second;
        break;
      }
    }

    if (!selectedQuality.empty()) {
      kodi::Log(ADDON_LOG_INFO, "Selected quality: %s, requiring drm: %s", selectedQuality.c_str(), requiresDrm ? "true" : "false");
      return "&quality=" + selectedQuality;
    }
  }

  return "";
}

int ZatData::GetDrmLevel() {
  int drmLevel = m_settings->DrmLevel();
  if (drmLevel == 0) {
    return Utils::RunsOnLinux() ? 3 : 1;
  } else {
    return drmLevel;
  }
}

std::string ZatData::GetStreamTypeString(bool withDrm) {
    if (!withDrm) {
      return "dash";
    }
    return "dash_widevine";
}

PVR_ERROR ZatData::GetRecordingStreamProperties(const kodi::addon::PVRRecording& recording,
                                                std::vector<kodi::addon::PVRStreamProperty>& properties)
{
  kodi::Log(ADDON_LOG_DEBUG, "Get url for recording %s", recording.GetRecordingId().c_str());
  PVR_ERROR ret = PVR_ERROR_FAILED;

  std::string cid = "";
  if (m_channelsByUid.count(recording.GetChannelUid())) {
    ZatChannel& channel = m_channelsByUid[recording.GetChannelUid()];
    cid = channel.cid;
  }

  json doc;

  bool useWidevine = GetDrmLevel() > -1 && GetDrmLevel() < 3;

  std::ostringstream dataStream;
  dataStream << GetBasicStreamParameters(useWidevine);
  kodi::Log(ADDON_LOG_INFO, "Stream properties: %s.", dataStream.str().c_str());
  int statusCode;

  std::string jsonString = m_httpClient->HttpPost(m_session->GetProviderUrl() + "/zapi/watch/recording/" + recording.GetRecordingId(), dataStream.str(), statusCode);

  doc = json::parse(jsonString, nullptr, false);
  if (doc.is_discarded())
  {
    return ret;
  }

  std::string strUrl = GetStreamUrl(doc, properties);
  if (!strUrl.empty())
  {
    SetStreamProperties(properties, strUrl);
    ret = PVR_ERROR_NO_ERROR;
  }

  return ret;
}

bool ZatData::Record(int programId, bool series)
{
  std::ostringstream dataStream;

  dataStream << "program_id=" << programId << "&series_force=False&series=" << (series ? "True" : "False");
  int statusCode;
  std::string jsonString = m_httpClient->HttpPost(m_session->GetProviderUrl() + "/zapi/playlist/program", dataStream.str(), statusCode);
  json doc;
  doc = json::parse(jsonString, nullptr, false);
  return !doc.is_discarded() && doc["success"].get<bool>();
}

PVR_ERROR ZatData::DeleteRecording(const kodi::addon::PVRRecording& recording)
{
  kodi::Log(ADDON_LOG_DEBUG, "Delete recording %s", recording.GetRecordingId().c_str());
  std::ostringstream dataStream;
  dataStream << "recording_id=" << recording.GetRecordingId() << "";

  int statusCode;
  std::string jsonString = m_httpClient->HttpPost(m_session->GetProviderUrl() + "/zapi/playlist/remove", dataStream.str(), statusCode);

  json doc;
  doc = json::parse(jsonString, nullptr, false);
  kodi::addon::CInstancePVRClient::TriggerRecordingUpdate();
  return !doc.is_discarded() && doc["success"].get<bool>() ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

PVR_ERROR ZatData::IsEPGTagPlayable(const kodi::addon::PVREPGTag& tag, bool& isPlayable)
{
  time_t current_time;
  time(&current_time);
  if (tag.GetStartTime() > current_time)
  {
    isPlayable = false;
  }
  else
  {
    EpgDBInfo dbInfo = m_epgDB->Get(tag.GetUniqueBroadcastId());
    isPlayable = (dbInfo.replayUntil > current_time) || (dbInfo.restartUntil > current_time);
  }
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::IsEPGTagRecordable(const kodi::addon::PVREPGTag& tag, bool& isRecordable)
{
  if (!m_session->IsRecordingEnabled())
  {
    isRecordable = false;
  }
  else
  {
    time_t current_time;
    time(&current_time);
    EpgDBInfo dbInfo = m_epgDB->Get(tag.GetUniqueBroadcastId());
    isRecordable = dbInfo.recordUntil > current_time;
  }
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetEPGTagStreamProperties(const kodi::addon::PVREPGTag& tag, std::vector<kodi::addon::PVRStreamProperty>& properties)
{
  std::ostringstream dataStream;
  ZatChannel channel = m_channelsByUid[tag.GetUniqueChannelId()];

  std::string url = GetStreamUrlForProgram(channel.cid, tag.GetUniqueBroadcastId(), properties);

  if (url.empty()) {
    kodi::Log(ADDON_LOG_WARNING, "Could not get url for channel %s and program %i. Try to get new EPG tag.", channel.cid.c_str(), tag.GetUniqueBroadcastId());
    time_t referenceTime = (tag.GetStartTime() / 2) + (tag.GetEndTime() / 2);
    std::ostringstream urlStream;
    urlStream << m_session->GetProviderUrl() << "/zapi/v3/cached/" + m_session->GetPowerHash() + "/guide"
        << "?end=" << referenceTime << "&start=" << referenceTime
        << "&format=json";

    int statusCode;
    std::string jsonString = m_httpClient->HttpGet(urlStream.str(), statusCode);

    json doc;
    doc = json::parse(jsonString, nullptr, false);
    if (doc.is_discarded())
    {
      kodi::Log(ADDON_LOG_ERROR, "Loading epg failed at %i", referenceTime);
      return PVR_ERROR_FAILED;
    }
    const json& channels = doc["channels"];
    if (!channels.contains(channel.cid)) {
      kodi::Log(ADDON_LOG_ERROR, "Channel not found in epg.");
      return PVR_ERROR_FAILED;
    }
    const json& channelEpg = channels[channel.cid];
    if (!channelEpg.is_array() || channelEpg.empty()) {
      kodi::Log(ADDON_LOG_ERROR, "Channel has no program at time %i.", referenceTime);
      return PVR_ERROR_FAILED;
    }
    const json& program = channelEpg[0];
    int newProgramId = program["id"].get<int>();

    url = GetStreamUrlForProgram(channel.cid, newProgramId, properties);

    if (url.empty()) {
      kodi::Log(ADDON_LOG_ERROR, "Could not get url for channel %s and program %i.", channel.cid.c_str(), newProgramId);
      return PVR_ERROR_FAILED;
    }
  }

  SetStreamProperties(properties, url);
  return PVR_ERROR_NO_ERROR;
}

std::string ZatData::GetStreamUrlForProgram(const std::string& cid, int programId, std::vector<kodi::addon::PVRStreamProperty>& properties)
{
  kodi::Log(ADDON_LOG_DEBUG, "Get timeshift url for channel %s and program %i", cid.c_str(), programId);

  bool forceWithoutDrm = GetDrmLevel() <= 0;
  json doc;

  while (true) {
    std::ostringstream dataStream;
    bool requiresDrm;
    dataStream << GetQualityStreamParameter(cid, forceWithoutDrm, requiresDrm);
    dataStream << GetBasicStreamParameters(requiresDrm);
    dataStream << "&pre_padding=0&post_padding=0";
    kodi::Log(ADDON_LOG_INFO, "Stream properties: %s.", dataStream.str().c_str());
    int statusCode;
    std::string jsonString = m_httpClient->HttpPost(m_session->GetProviderUrl() + "/zapi/v3/watch/replay/" + cid + "/" + std::to_string(programId), dataStream.str(), statusCode);
    doc = json::parse(jsonString, nullptr, false);
    if (doc.is_discarded())
    {
      return "";
    }

    if (forceWithoutDrm || !IsDrmLimitApplied(doc)) {
      break;
    }
    forceWithoutDrm = true;
    kodi::Log(ADDON_LOG_INFO, "Fallback to no-drm version.");
    doc = json();
  }

  std::string strUrl = GetStreamUrl(doc, properties);
  return strUrl;
}

PVR_ERROR ZatData::GetEPGTagEdl(const kodi::addon::PVREPGTag& tag, std::vector<kodi::addon::PVREDLEntry>& edl)
{
  if (m_settings->GetSkipStartOfProgramme()) {
    kodi::addon::PVREDLEntry entry;
    entry.SetStart(0);
    entry.SetEnd(300000);
    entry.SetType(PVR_EDL_TYPE_COMBREAK);
    edl.emplace_back(entry);
  }
  if (m_settings->GetSkipEndOfProgramme()) {
      kodi::addon::PVREDLEntry entry;
      unsigned long duration = (tag.GetEndTime() - tag.GetStartTime() + 25 * 60) * 1000;
      entry.SetStart(duration - 20 * 60 * 1000);
      entry.SetEnd(duration + 2000);
      entry.SetType(PVR_EDL_TYPE_COMBREAK);
      edl.emplace_back(entry);
    }
  if (m_settings->GetSkipCommercials() && m_channelsByUid.count(tag.GetUniqueChannelId())) {
    json doc;
    ZatChannel& channel = m_channelsByUid[tag.GetUniqueChannelId()];
    if (FetchStreamJsonForEDL("replay", channel.cid, tag.GetUniqueBroadcastId(), doc)) {
      AddCommercialBreaks(doc, edl);
    }
  }
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetRecordingEdl(const kodi::addon::PVRRecording& recording, std::vector<kodi::addon::PVREDLEntry>& edl)
{
  if (m_settings->GetSkipStartOfProgramme()) {
    kodi::addon::PVREDLEntry entry;
    entry.SetStart(0);
    entry.SetEnd(300000);
    entry.SetType(PVR_EDL_TYPE_COMBREAK);
    edl.emplace_back(entry);
  }
  if (m_settings->GetSkipCommercials()) {
    json doc;
    if (FetchStreamJsonForEDL("recording", recording.GetRecordingId(), 0, doc)) {
      AddCommercialBreaks(doc, edl);
    }
  }
  return PVR_ERROR_NO_ERROR;
}

bool ZatData::FetchStreamJsonForEDL(const std::string& type, const std::string& cid, int programId, json& doc)
{
  if (!m_session->IsConnected()) {
    return false;
  }
  std::ostringstream dataStream;
  dataStream << GetBasicStreamParameters(false);
  dataStream << "&with_schedule=True";
  int statusCode;
  std::string jsonString = m_httpClient->HttpPost(m_session->GetProviderUrl() + "/zapi/v3/watch/" + type + "/" + cid + "/" + std::to_string(programId), dataStream.str(), statusCode);
  doc = json::parse(jsonString, nullptr, false);
  if (doc.is_discarded()) {
    kodi::Log(ADDON_LOG_ERROR, "Could not get JSON data for replay: %s/%i.", cid.c_str(), programId);
    return false;
  }
  return true;
}

void ZatData::AddCommercialBreaks(const json& doc, std::vector<kodi::addon::PVREDLEntry>& edl)
{
  if (!doc.contains("stream") || !doc["stream"].contains("schedule") || !doc["stream"]["schedule"].is_array()) {
    return;
  }
  const json& schedule = doc["stream"]["schedule"];
  for (const auto& schedule_item : schedule) {
    if (schedule_item.contains("ad_breaks") && schedule_item["ad_breaks"].is_array()) {
      const json& ad_breaks = schedule_item["ad_breaks"];
      for (const auto& ad_break : ad_breaks) {
        if (ad_break.contains("start") && ad_break.contains("end")) {
          kodi::addon::PVREDLEntry entry;
          entry.SetStart(ad_break["start"].get<int>() + 5000);
          entry.SetEnd(ad_break["end"].get<int>() - 5000);
          entry.SetType(PVR_EDL_TYPE_COMBREAK);
          edl.emplace_back(entry);
        }
      }
    }
  }
}

void ZatData::UpdateConnectionState(const std::string& connectionString, PVR_CONNECTION_STATE newState, const std::string& message) {
  kodi::addon::CInstancePVRClient::ConnectionStateChange(connectionString, newState, message);
}

bool ZatData::SessionInitialized()
{
  if (m_epgProvider) {
    delete m_epgProvider;
  }
  kodi::Log(ADDON_LOG_INFO, "DRM Level: %i", m_settings->DrmLevel());
  if (!LoadChannels()) {
    return false;
  }
  m_epgProvider = new ZattooEpgProvider(this, m_session->GetProviderUrl(), *m_epgDB, *m_httpClient, m_categories, m_visibleChannelsByCid, m_session->GetPowerHash());
  return true;
}

ADDON_STATUS ZatData::SetSetting(const std::string& settingName, const kodi::addon::CSettingValue& settingValue)
{
  ADDON_STATUS result = m_settings->SetSetting(settingName, settingValue);
  if (!m_settings->VerifySettings()) {
    return ADDON_STATUS_NEED_SETTINGS;
  }
  return result;
}

ADDON_STATUS ZatData::Create()
{
  kodi::Log(ADDON_LOG_DEBUG, "%s - Creating the PVR Zattoo add-on", __FUNCTION__);
  return m_session->Start();
}

ADDONCREATOR(ZatData);
