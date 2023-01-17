#include <iostream>
#include <string>
#include "ZatData.h"
#include <sstream>
#include <map>
#include <ctime>
#include <utility>
#include "Utils.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <kodi/Filesystem.h>
#include "epg/ZattooEpgProvider.h"

#ifdef TARGET_ANDROID
#include "to_string.h"
#endif

using namespace rapidjson;

constexpr char app_token_file[] = "special://temp/zattoo_app_token";
const char data_file[] = "special://profile/addon_data/pvr.zattoo/data.json";

std::string ZatData::GetManifestType()
{
  switch (m_settings->GetStreamType())
  {
    case HLS:
      return "hls";
    default:
      return "mpd";
  }
}

std::string ZatData::GetMimeType()
{
  switch (m_settings->GetStreamType())
  {
    case HLS:
      return "application/x-mpegURL";
    default:
      return "application/xml+dash";
  }
}

void ZatData::SetStreamProperties(
    std::vector<kodi::addon::PVRStreamProperty>& properties,
    const std::string& url)
{
  properties.emplace_back(PVR_STREAM_PROPERTY_STREAMURL, url);
  properties.emplace_back(PVR_STREAM_PROPERTY_INPUTSTREAM, "inputstream.adaptive");
  properties.emplace_back("inputstream.adaptive.manifest_type", GetManifestType());
  properties.emplace_back(PVR_STREAM_PROPERTY_MIMETYPE, GetMimeType());

  if (m_settings->GetStreamType() == DASH || m_settings->GetStreamType() == DASH_WIDEVINE)
  {
    properties.emplace_back("inputstream.adaptive.manifest_update_parameter", "full");
  }
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

  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError())
  {
    kodi::Log(ADDON_LOG_ERROR, "Parsing data.json failed.");
    return false;
  }
  
  if (doc.HasMember("recordings")) {
    const Value& recordings = doc["recordings"];
    for (Value::ConstValueIterator itr = recordings.Begin();
        itr != recordings.End(); ++itr)
    {
      const Value& recording = (*itr);
      
      RecordingDBInfo recordingDBInfo;
      recordingDBInfo.recordingId = Utils::JsonStringOrEmpty(recording, "recordingId");
      recordingDBInfo.playCount = recording["playCount"].GetInt();
      recordingDBInfo.lastPlayedPosition = recording["lastPlayedPosition"].GetInt();
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
  Document favDoc;
  favDoc.Parse(jsonString.c_str());

  if (favDoc.GetParseError() || !favDoc["success"].GetBool())
  {
    return false;
  }
  const Value& favs = favDoc["favorites"];

  std::ostringstream urlStream;
  urlStream << m_session->GetProviderUrl() + "/zapi/v3/cached/"  << m_session->GetPowerHash() << "/channels";
  jsonString = m_httpClient->HttpGet(urlStream.str(), statusCode);

  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc.HasMember("channels"))
  {
    kodi::Log(ADDON_LOG_ERROR, "Failed to load channels");
    return false;
  }

  int channelNumber = favs.Size();
  const Value& groups = doc["groups"];

  //Load the channel groups and channels
  for (Value::ConstValueIterator itr = groups.Begin(); itr != groups.End();
      ++itr)
  {
    PVRZattooChannelGroup group;
    const Value& groupItem = (*itr);
    group.name = Utils::JsonStringOrEmpty(groupItem, "name");
    m_channelGroups.insert(m_channelGroups.end(), group);
  }
  const Value& channels = doc["channels"];
  for (Value::ConstValueIterator itr1 = channels.Begin();
      itr1 != channels.End(); ++itr1)
  {
    const Value& channelItem = (*itr1);
    const Value& qualities = channelItem["qualities"];
    for (Value::ConstValueIterator itr2 = qualities.Begin();
        itr2 != qualities.End(); ++itr2)
    {
      const Value& qualityItem = (*itr2);
      std::string avail = Utils::JsonStringOrEmpty(qualityItem, "availability");
      if (avail == "available")
      {
        ZatChannel channel;
        channel.name = Utils::JsonStringOrEmpty(qualityItem, "title");
        std::string cid = Utils::JsonStringOrEmpty(channelItem, "cid");
        channel.iUniqueId = Utils::GetChannelId(cid.c_str());
        channel.cid = cid;
        channel.iChannelNumber = ++channelNumber;
        channel.strLogoPath = "http://logos.zattic.com";
        channel.strLogoPath.append(
            Utils::JsonStringOrEmpty(qualityItem, "logo_white_84"));
        channel.recordingEnabled =
            channelItem.HasMember("recording") ?
                channelItem["recording"].GetBool() : false;
        PVRZattooChannelGroup &group = m_channelGroups[channelItem["group_index"].GetInt()];
        group.channels.insert(group.channels.end(), channel);
        allChannels[cid] = channel;
        m_channelsByCid[channel.cid] = channel;
        m_channelsByUid[channel.iUniqueId] = channel;
        break;
      }
    }
  }

  PVRZattooChannelGroup favGroup;
  favGroup.name = "Favoriten";

  for (Value::ConstValueIterator itr = favs.Begin(); itr != favs.End(); ++itr)
  {
    const Value& favItem = (*itr);
    std::string favCid = favItem.GetString();
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

std::string ZatData::GetStreamUrl(std::string& jsonString, std::vector<kodi::addon::PVRStreamProperty>& properties) {
  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc.HasMember("stream"))
  {
    return "";
  }
  const Value& watchUrls = doc["stream"]["watch_urls"];
  std::string url = Utils::JsonStringOrEmpty(doc["stream"], "url");
  for (Value::ConstValueIterator itr = watchUrls.Begin(); itr != watchUrls.End(); ++itr)
  {
    const Value& watchUrl = (*itr);
    kodi::Log(ADDON_LOG_DEBUG, "Selected url for maxrate: %d", watchUrl["maxrate"].GetInt());
    url = Utils::JsonStringOrEmpty(watchUrl, "url");
    if (m_settings->GetStreamType() == DASH_WIDEVINE) {
      std::string licenseUrl = Utils::JsonStringOrEmpty(watchUrl, "license_url");
      properties.emplace_back("inputstream.adaptive.license_key", licenseUrl + "||A{SSM}|");
      properties.emplace_back("inputstream.adaptive.license_type", "com.widevine.alpha");
    }
    break;
  }
  kodi::Log(ADDON_LOG_DEBUG, "Got url: %s", url.c_str());
  return url;
}

PVR_ERROR ZatData::GetChannelStreamProperties(const kodi::addon::PVRChannel& channel,
                                              std::vector<kodi::addon::PVRStreamProperty>& properties)
{
  PVR_ERROR ret = PVR_ERROR_FAILED;

  ZatChannel* ownChannel = FindChannel(channel.GetUniqueId());
  kodi::Log(ADDON_LOG_DEBUG, "Get live url for channel %s", ownChannel->cid.c_str());

  std::ostringstream dataStream;
  dataStream << GetStreamParameters() << "&format=json&timeshift=10800";

  int statusCode;
  std::string jsonString = m_httpClient->HttpPost(m_session->GetProviderUrl() + "/zapi/watch/live/" + ownChannel->cid, dataStream.str(), statusCode);

  std::string strUrl = GetStreamUrl(jsonString, properties);
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

bool ZatData::ParseRecordingsTimers(const Value& recordings, std::map<int, ZatRecordingDetails>& detailsById)
{
  Value::ConstValueIterator recordingsItr = recordings.Begin();
  while (recordingsItr != recordings.End())
  {
    int bucketSize = 100;
    std::ostringstream urlStream;
    urlStream << m_session->GetProviderUrl() << "/zapi/v2/cached/program/power_details/"
        << m_session->GetPowerHash() << "?complete=True&program_ids=";
    while (bucketSize > 0 && recordingsItr != recordings.End())
    {
      const Value& recording = (*recordingsItr);
      if (bucketSize < 100)
      {
        urlStream << ",";
      }
      urlStream << recording["program_id"].GetInt();
      ++recordingsItr;
      bucketSize--;
    }
    int statusCode;
    std::string jsonString = m_httpClient->HttpGetCached(urlStream.str(), 60 * 60 * 24 * 30, statusCode);
    Document detailDoc;
    detailDoc.Parse(jsonString.c_str());
    if (detailDoc.GetParseError() || !detailDoc["success"].GetBool())
    {
      kodi::Log(ADDON_LOG_ERROR, "Failed to load details for recordings.");
    }
    else
    {
      const Value& programs = detailDoc["programs"];
      for (Value::ConstValueIterator progItr = programs.Begin();
          progItr != programs.End(); ++progItr)
      {
        const Value &program = *progItr;
        ZatRecordingDetails details;
        if (program.HasMember("g") && program["g"].IsArray()
            && program["g"].Begin() != program["g"].End())
        {
          details.genre = program["g"].Begin()->GetString();
        }
        else
        {
          details.genre = "";
        }
        details.description = Utils::JsonStringOrEmpty(program, "d");
        detailsById.insert(
            std::pair<int, ZatRecordingDetails>(program["id"].GetInt(), details));
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

  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc["success"].GetBool())
  {
    return PVR_ERROR_FAILED;
  }

  const Value& recordings = doc["recordings"];
  std::map<int, ZatRecordingDetails> detailsById;
  ParseRecordingsTimers(recordings, detailsById);

  time_t current_time;
  time(&current_time);

  for (Value::ConstValueIterator itr = recordings.Begin();
      itr != recordings.End(); ++itr)
  {
    const Value& recording = (*itr);
    int programId = recording["program_id"].GetInt();

    std::string cid = Utils::JsonStringOrEmpty(recording, "cid");
    auto iterator = m_channelsByCid.find(cid);
    if (iterator == m_channelsByCid.end())
    {
      kodi::Log(ADDON_LOG_ERROR, "Channel %s not found for recording: %i",
          cid.c_str(), programId);
      continue;
    }
    ZatChannel channel = iterator->second;

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

      tag.SetClientIndex(static_cast<unsigned int>(recording["id"].GetInt()));
      tag.SetTitle(Utils::JsonStringOrEmpty(recording, "title"));
      tag.SetSummary(Utils::JsonStringOrEmpty(recording, "episode_title"));
      time_t endTime = Utils::StringToTime(
          Utils::JsonStringOrEmpty(recording, "end").c_str());
      tag.SetStartTime(startTime);
      tag.SetEndTime(endTime);
      tag.SetState(PVR_TIMER_STATE_SCHEDULED);
      tag.SetTimerType(1);
      tag.SetEPGUid(static_cast<unsigned int>(recording["program_id"].GetInt()));
      tag.SetClientChannelUid(channel.iUniqueId);
      if (genre)
      {
        tag.SetGenreSubType(genre & 0x0F);
        tag.SetGenreType(genre & 0xF0);
      }
      results.Add(tag);
      UpdateThread::SetNextRecordingUpdate(startTime);
    }
  }
  
  if (doc.HasMember("recorded_tv_series")) {
    const Value& recordingsTvSeries = doc["recorded_tv_series"];
  
    for (Value::ConstValueIterator itr = recordingsTvSeries.Begin();
        itr != recordingsTvSeries.End(); ++itr)
    {
      const Value& recording = (*itr);
      int tvSeriesId = recording["tv_series_id"].GetInt();
  
      std::string cid = Utils::JsonStringOrEmpty(recording, "cid");
      auto iterator = m_channelsByCid.find(cid);
      if (iterator == m_channelsByCid.end())
      {
        kodi::Log(ADDON_LOG_ERROR, "Channel %s not found for series recording: %i",
            cid.c_str(), tvSeriesId);
        continue;
      }
      ZatChannel channel = iterator->second;
    
      //genre
      kodi::addon::PVRTimer tag;

      tag.SetClientIndex(static_cast<unsigned int>(tvSeriesId));
      tag.SetTitle(Utils::JsonStringOrEmpty(recording, "title"));
      tag.SetState(PVR_TIMER_STATE_SCHEDULED);
      tag.SetTimerType(2);
      tag.SetClientChannelUid(channel.iUniqueId);
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

  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc["success"].GetBool())
  {
    return PVR_ERROR_FAILED;
  }

  const Value& recordings = doc["recordings"];

  amount = 0;
  for (Value::ConstValueIterator itr = recordings.Begin();
      itr != recordings.End(); ++itr)
  {
    const Value& recording = (*itr);
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

    Document doc;
    doc.Parse(jsonString.c_str());
    if (doc.GetParseError() || !doc["success"].GetBool())
    {
      return PVR_ERROR_FAILED;
    }

    const Value& recordings = doc["recordings"];

    for (Value::ConstValueIterator itr = recordings.Begin();
        itr != recordings.End(); ++itr)
    {
      const Value& recording = (*itr);
      
      int seriesId = recording["tv_series_id"].GetInt();
      
      if (seriesId == timer.GetClientIndex()) {
        recordingId = recording["id"].GetInt();
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

  Document doc;
  doc.Parse(jsonString.c_str());
  kodi::addon::CInstancePVRClient::TriggerTimerUpdate();
  return !doc.GetParseError() && doc["success"].GetBool() ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
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

  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc["success"].GetBool())
  {
    return PVR_ERROR_FAILED;
  }

  const Value& recordings = doc["recordings"];
  std::map<int, ZatRecordingDetails> detailsById;
  ParseRecordingsTimers(recordings, detailsById);

  time_t current_time;
  time(&current_time);

  for (Value::ConstValueIterator itr = recordings.Begin();
      itr != recordings.End(); ++itr)
  {
    const Value& recording = (*itr);
    int programId = recording["program_id"].GetInt();

    std::string cid = Utils::JsonStringOrEmpty(recording, "cid");
    auto iterator = m_channelsByCid.find(cid);
    if (iterator == m_channelsByCid.end())
    {
      kodi::Log(ADDON_LOG_ERROR, "Channel %s not found for recording: %i",
          cid.c_str(), programId);
      continue;
    }
    ZatChannel channel = iterator->second;

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

      tag.SetRecordingId(std::to_string(recording["id"].GetInt()));
      tag.SetTitle(Utils::JsonStringOrEmpty(recording, "title"));
      tag.SetEpisodeName(Utils::JsonStringOrEmpty(recording, "episode_title"));
      tag.SetPlot(hasDetails ? detailIterator->second.description : "");

      std::string imageToken = Utils::JsonStringOrEmpty(recording, "image_token");
      std::string imageUrl = Utils::GetImageUrl(imageToken);;
      tag.SetIconPath(imageUrl);
      tag.SetChannelUid(channel.iUniqueId);
      tag.SetChannelName(channel.name);
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

  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc["success"].GetBool())
  {
    return PVR_ERROR_FAILED;
  }

  const Value& recordings = doc["recordings"];

  amount = 0;
  for (Value::ConstValueIterator itr = recordings.Begin();
      itr != recordings.End(); ++itr)
  {
    const Value& recording = (*itr);
    time_t startTime = Utils::StringToTime(
        Utils::JsonStringOrEmpty(recording, "start"));
    if (startTime <= current_time)
    {
      amount++;
    }
  }
  return PVR_ERROR_NO_ERROR;
}

std::string ZatData::GetStreamParameters() {
  std::string params = m_settings->GetZatEnableDolby() ? "&enable_eac3=true" : "";
  params += "&stream_type=" + GetStreamTypeString();

  if (!m_settings->GetParentalPin().empty()) {
    params += "&youth_protection_pin=" + m_settings->GetParentalPin();
  }

  return params;
}

std::string ZatData::GetStreamTypeString() {
  switch (m_settings->GetStreamType()) {
    case HLS:
      return "hls7";
    case DASH_WIDEVINE:
      return "dash_widevine";
    default:
      return "dash";
  }
}

PVR_ERROR ZatData::GetRecordingStreamProperties(const kodi::addon::PVRRecording& recording,
                                                std::vector<kodi::addon::PVRStreamProperty>& properties)
{
  kodi::Log(ADDON_LOG_DEBUG, "Get url for recording %s", recording.GetRecordingId().c_str());

  std::ostringstream dataStream;
  dataStream << GetStreamParameters();

  int statusCode;
  std::string jsonString = m_httpClient->HttpPost(m_session->GetProviderUrl() + "/zapi/watch/recording/" + recording.GetRecordingId(), dataStream.str(), statusCode);

  std::string strUrl = GetStreamUrl(jsonString, properties);
  PVR_ERROR ret = PVR_ERROR_FAILED;
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
  Document doc;
  doc.Parse(jsonString.c_str());
  return !doc.GetParseError() && doc["success"].GetBool();
}

PVR_ERROR ZatData::DeleteRecording(const kodi::addon::PVRRecording& recording)
{
  kodi::Log(ADDON_LOG_DEBUG, "Delete recording %s", recording.GetRecordingId().c_str());
  std::ostringstream dataStream;
  dataStream << "recording_id=" << recording.GetRecordingId() << "";

  int statusCode;
  std::string jsonString = m_httpClient->HttpPost(m_session->GetProviderUrl() + "/zapi/playlist/remove", dataStream.str(), statusCode);

  Document doc;
  doc.Parse(jsonString.c_str());
  kodi::addon::CInstancePVRClient::TriggerRecordingUpdate();
  return !doc.GetParseError() && doc["success"].GetBool() ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
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

    Document doc;
    doc.Parse(jsonString.c_str());
    if (doc.GetParseError())
    {
      kodi::Log(ADDON_LOG_ERROR, "Loading epg failed at %i", referenceTime);
      return PVR_ERROR_FAILED;
    }
    const Value& channels = doc["channels"];
    if (!channels.HasMember(channel.cid.c_str())) {
      kodi::Log(ADDON_LOG_ERROR, "Channel not found in epg.");
      return PVR_ERROR_FAILED;
    }
    const Value& channelEpg = channels[channel.cid.c_str()];
    if (!channelEpg.IsArray() || channelEpg.GetArray().Empty()) {
      kodi::Log(ADDON_LOG_ERROR, "Channel has no program at time %i.", referenceTime);
      return PVR_ERROR_FAILED;
    }
    const Value& program = channelEpg.GetArray()[0];
    int newProgramId = program["id"].GetInt();
    
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
  std::ostringstream dataStream;

  std::string jsonString;

  kodi::Log(ADDON_LOG_DEBUG, "Get timeshift url for channel %s and program %i", cid.c_str(), programId);

  dataStream << GetStreamParameters();
  dataStream << "&pre_padding=0&post_padding=0";
  int statusCode;
  jsonString = m_httpClient->HttpPost(m_session->GetProviderUrl() + "/zapi/v3/watch/replay/" + cid + "/" + std::to_string(programId), dataStream.str(), statusCode);

  std::string strUrl = GetStreamUrl(jsonString, properties);
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
  return PVR_ERROR_NO_ERROR;
}

void ZatData::UpdateConnectionState(const std::string& connectionString, PVR_CONNECTION_STATE newState, const std::string& message) {
  kodi::addon::CInstancePVRClient::ConnectionStateChange(connectionString, newState, message);
}

bool ZatData::SessionInitialized()
{
  if (m_epgProvider) {
    delete m_epgProvider;
  }
  kodi::Log(ADDON_LOG_INFO, "Stream type: %s", GetStreamTypeString().c_str());
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
