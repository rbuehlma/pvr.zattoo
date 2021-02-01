#include "client.h"

#include <iostream>
#include <string>
#include "ZatData.h"
#include <sstream>
#include <map>
#include <ctime>
#include <random>
#include <utility>
#include "Utils.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "Cache.h"
#include "md5.h"
#include <kodi/Filesystem.h>
#include <unistd.h>

#ifdef TARGET_ANDROID
#include "to_string.h"
#endif

using namespace rapidjson;

constexpr char app_token_file[] = "special://temp/zattoo_app_token";
const char data_file[] = "special://profile/addon_data/pvr.zattoo/data.json";
static const std::string user_agent = std::string("Kodi/")
    + std::string(STR(KODI_VERSION)) + std::string(" pvr.zattoo/")
    + std::string(STR(ZATTOO_VERSION)) + std::string(" (Kodi PVR addon)");
std::mutex ZatData::sendEpgToKodiMutex;

std::string ZatData::GetManifestType()
{
  switch (m_streamType)
  {
    case HLS:
      return "hls";
    default:
      return "mpd";
  }
}

std::string ZatData::GetMimeType()
{
  switch (m_streamType)
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

  if (m_streamType == DASH || m_streamType == DASH_WIDEVINE)
  {
    properties.emplace_back("inputstream.adaptive.manifest_update_parameter", "full");
  }
}

std::string ZatData::HttpGetCached(const std::string& url, time_t cacheDuration,
    const std::string& userAgent)
{

  std::string content;
  std::string cacheKey = md5(url);
  if (!Cache::Read(cacheKey, content))
  {
    content = HttpGet(url, false, userAgent);
    if (!content.empty())
    {
      time_t validUntil;
      time(&validUntil);
      validUntil += cacheDuration;
      Cache::Write(cacheKey, content, validUntil);
    }
  }
  return content;
}

std::string ZatData::HttpGet(const std::string& url, bool isInit, const std::string& userAgent)
{
  return HttpRequest("GET", url, "", isInit, userAgent);
}

std::string ZatData::HttpDelete(const std::string& url, bool isInit)
{
  return HttpRequest("DELETE", url, "", isInit, "");
}

std::string ZatData::HttpPost(const std::string& url, const std::string& postData, bool isInit,
    const std::string& userAgent)
{
  return HttpRequest("POST", url, postData, isInit, userAgent);
}

std::string ZatData::HttpRequest(const std::string& action, const std::string& url,
    const std::string& postData, bool isInit, const std::string& userAgent)
{
  Curl curl;
  int statusCode;

  curl.AddOption("acceptencoding", "gzip,deflate");

  if (!m_beakerSessionId.empty())
  {
    curl.AddOption("Cookie", "beaker.session.id=" + m_beakerSessionId);
  }

  if (!m_uuid.empty())
  {
    curl.AddOption("Cookie", "uuid=" + m_uuid);
  }

  if (!m_zattooSession.empty())
  {
    curl.AddOption("Cookie", "zattoo.session=" + m_zattooSession);
  }

  if (!userAgent.empty())
  {
    curl.AddHeader("User-Agent", userAgent);
  }

  std::string content = HttpRequestToCurl(curl, action, url, postData, statusCode);

  if (statusCode == 403 && !isInit)
  {
    kodi::Log(ADDON_LOG_ERROR, "Open URL failed with 403. Try to re-init session.");
    if (!InitSession(false))
    {
      kodi::Log(ADDON_LOG_ERROR, "Re-init of session. Failed.");
      return "";
    }
    return HttpRequestToCurl(curl, action, url, postData, statusCode);
  }
  if (statusCode >= 400) {
    kodi::Log(ADDON_LOG_ERROR, "Open URL failed with %i.", statusCode);
    return "";
  }
  std::string sessionId = curl.GetCookie("beaker.session.id");
  if (!sessionId.empty() && m_beakerSessionId != sessionId)
  {
    kodi::Log(ADDON_LOG_DEBUG, "Got new beaker.session.id: %s..",
        sessionId.substr(0, 5).c_str());
    m_beakerSessionId = sessionId;
  }

  std::string zattooSession = curl.GetCookie("zattoo.session");
  if (!zattooSession.empty() && m_zattooSession != zattooSession)
  {
    kodi::Log(ADDON_LOG_DEBUG, "Got new zattooSession: %s..", zattooSession.substr(0, 5).c_str());
    m_zattooSession = zattooSession;
    m_parameterDB->Set("zattooSession", zattooSession);
  }

  return content;
}

std::string ZatData::HttpRequestToCurl(Curl &curl, const std::string& action,
    const std::string& url, const std::string& postData, int &statusCode)
{
  kodi::Log(ADDON_LOG_DEBUG, "Http-Request: %s %s.", action.c_str(), url.c_str());
  std::string content;
  if (action == "POST")
  {
    content = curl.Post(url, postData, statusCode);
  }
  else if (action == "DELETE")
  {
    content = curl.Delete(url, statusCode);
  }
  else
  {
    content = curl.Get(url, statusCode);
  }
  return content;

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
      recordingDBInfo.recordingId = GetStringOrEmpty(recording, "recordingId");
      recordingDBInfo.playCount = recording["playCount"].GetInt();
      recordingDBInfo.lastPlayedPosition = recording["lastPlayedPosition"].GetInt();
      m_recordingsDB->Set(recordingDBInfo);
    }
  }
  kodi::Log(ADDON_LOG_DEBUG, "Loaded data.json.");
  return true;
}

std::string ZatData::GetUUID()
{
  if (!m_uuid.empty())
  {
    return m_uuid;
  }

  m_uuid = GenerateUUID();
  m_parameterDB->Set("uuid", m_uuid);
  return m_uuid;
}

std::string ZatData::GenerateUUID()
{
    std::string tmp_s;
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "-";
    
    srand( (unsigned) time(NULL) * getpid());
    
    for (int i = 0; i < 21; ++i)
    {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    
    return tmp_s;
}

bool ZatData::LoadAppId()
{
  if (!m_appToken.empty()) {
    return true;
  }
  
  std::string html = HttpGet(m_providerUrl + "/login", true);

  if (!LoadAppTokenFromHtml(html)) {
    if (!LoadAppTokenFromJson(html)) {
      return LoadAppTokenFromFile();
    }
  }
  kodi::vfs::CFile file;
  if (!file.OpenFileForWrite(app_token_file, true))
  {
    kodi::Log(ADDON_LOG_ERROR, "Could not save app taken to %s", app_token_file);
  }
  else
  {
    file.Write(m_appToken.c_str(), m_appToken.length());
  }
  return true;
}

bool ZatData::LoadAppTokenFromFile() {
  if (!kodi::vfs::FileExists(app_token_file, true))
  {
    return false;
  }

  kodi::Log(ADDON_LOG_INFO,
      "Could not get app token from page. Try to load from file.");
  m_appToken = Utils::ReadFile(app_token_file);
  return !m_appToken.empty();
}

bool ZatData::LoadAppTokenFromHtml(std::string html) {
  size_t basePos = html.find("window.appToken = '") + 19;
  if (basePos > 19)
  {
    size_t endPos = html.find("'", basePos);
    m_appToken = html.substr(basePos, endPos - basePos);
    return true;
  }
  return false;
}

bool ZatData::LoadAppTokenFromJson(std::string html) {
  size_t basePos = html.find("src=\"/app-") + 5;
  if (basePos < 6) {
    kodi::Log(ADDON_LOG_ERROR, "Unable to find app-*.js");
    return false;
  }
  size_t endPos = html.find("\"", basePos);
  std::string appJsPath = html.substr(basePos, endPos - basePos);
  std::string content = HttpGet(m_providerUrl + appJsPath, true);

  basePos = content.find("\"token-") + 1;
  if (basePos < 6) {
    kodi::Log(ADDON_LOG_ERROR, "Unable to find token-*.json in %s", appJsPath.c_str());
    return false;
  }
  endPos = content.find("\"", basePos);
  std::string tokenJsonPath = content.substr(basePos, endPos - basePos);
  std::string jsonString = HttpGet(m_providerUrl + "/" + tokenJsonPath, true);

  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc["success"].GetBool())
  {
    kodi::Log(ADDON_LOG_DEBUG, "Failed to load json from %s", tokenJsonPath.c_str());
    return false;
  }

  m_appToken = doc["session_token"].GetString();
  return true;
}

bool ZatData::SendHello(std::string uuid)
{
  kodi::Log(ADDON_LOG_DEBUG, "Send hello.");
  std::ostringstream dataStream;
  dataStream << "uuid=" << uuid << "&lang=en&app_version=3.2038.0&format=json&client_app_token="
      << m_appToken;

  std::string jsonString = HttpPost(m_providerUrl + "/zapi/v3/session/hello",
      dataStream.str(), true);

  Document doc;
  doc.Parse(jsonString.c_str());
  if (!doc.GetParseError() && doc["active"].GetBool())
  {
    kodi::Log(ADDON_LOG_DEBUG, "Hello was successful.");
    return true;
  }
  else
  {
    kodi::Log(ADDON_LOG_INFO, "Hello failed.");
    return false;
  }
}

Document ZatData::Login()
{
  kodi::Log(ADDON_LOG_DEBUG, "Try to login.");

  std::ostringstream dataStream;
  dataStream << "login=" << Utils::UrlEncode(m_username) << "&password="
      << Utils::UrlEncode(m_password) << "&format=json&remember=true";
  std::string jsonString = HttpPost(m_providerUrl + "/zapi/v3/account/login",
      dataStream.str(), true, user_agent);
  Document doc;
  doc.Parse(jsonString.c_str());
  return doc;
}

bool ZatData::ReinitSession()
{
  m_uuid = "";
  m_beakerSessionId = "";
  m_appToken = "";
  
  return InitSession(true);
}

bool ZatData::InitSession(bool isReinit)
{
  if (!LoadAppId())
  {
    return isReinit ? false : ReinitSession();
  }
  
  std::string uuid = GetUUID();

  SendHello(uuid);
  
  std::string jsonString = HttpGet(m_providerUrl + "/zapi/v3/session", true);
  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc["active"].GetBool())
  {
    kodi::Log(ADDON_LOG_ERROR, "Initialize session failed.");
    return isReinit ? false : ReinitSession();
  }
  
  if (doc["account"].IsNull())
  {
    kodi::Log(ADDON_LOG_DEBUG, "Need to login.");
    m_zattooSession = "";
    m_beakerSessionId = "";
    doc = Login();
    if (doc.GetParseError() || doc["account"].IsNull())
    {
      kodi::Log(ADDON_LOG_ERROR, "Login failed.");
      return isReinit ? false : ReinitSession();
    }
    else
    {
      kodi::Log(ADDON_LOG_DEBUG, "Login was successful.");
    }
  }

  const Value& account = doc["account"];
  const Value& nonlive = doc["nonlive"];

  m_countryCode = GetStringOrEmpty(doc, "current_country");
  m_serviceRegionCountry = GetStringOrEmpty(account, "service_country");
  m_recallEnabled = GetStringOrEmpty(nonlive, "replay_availability") == "available";
  m_recordingEnabled = nonlive["recording_number_limit"].GetInt() > 0;
  kodi::Log(ADDON_LOG_INFO, "Current country code: %s", m_countryCode.c_str());
  kodi::Log(ADDON_LOG_INFO, "Service region country: %s",
      m_serviceRegionCountry.c_str());
  kodi::Log(ADDON_LOG_INFO, "Stream type: %s", GetStreamTypeString().c_str());
  kodi::Log(ADDON_LOG_INFO, "Recall are %s",
      m_recallEnabled ? "enabled" : "disabled");
  kodi::Log(ADDON_LOG_INFO, "Recordings are %s",
      m_recordingEnabled ? "enabled" : "disabled");
  m_powerHash = GetStringOrEmpty(doc, "power_guide_hash");
  return true;
}

bool ZatData::LoadChannels()
{
  std::map<std::string, ZatChannel> allChannels;
  std::string jsonString = HttpGet(m_providerUrl + "/zapi/channels/favorites");
  Document favDoc;
  favDoc.Parse(jsonString.c_str());

  if (favDoc.GetParseError() || !favDoc["success"].GetBool())
  {
    return false;
  }
  const Value& favs = favDoc["favorites"];

  std::ostringstream urlStream;
  urlStream << m_providerUrl + "/zapi/v3/cached/"  << m_powerHash << "/channels";
  jsonString = HttpGet(urlStream.str());

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
    group.name = GetStringOrEmpty(groupItem, "name");
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
      std::string avail = GetStringOrEmpty(qualityItem, "availability");
      if (avail == "available")
      {
        ZatChannel channel;
        channel.name = GetStringOrEmpty(qualityItem, "title");
        std::string cid = GetStringOrEmpty(channelItem, "cid");
        channel.iUniqueId = GetChannelId(cid.c_str());
        channel.cid = cid;
        channel.iChannelNumber = ++channelNumber;
        channel.strLogoPath = "http://logos.zattic.com";
        channel.strLogoPath.append(
            GetStringOrEmpty(qualityItem, "logo_white_84"));
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
  
  if (m_favoritesOnly) {
    m_channelGroups.clear();
  }

  if (!favGroup.channels.empty())
    m_channelGroups.insert(m_channelGroups.end(), favGroup);

  return true;
}

int ZatData::GetChannelId(const char * strChannelName)
{
  int iId = 0;
  int c;
  while ((c = *strChannelName++))
    iId = ((iId << 5) + iId) + c; /* iId * 33 + c */
  return abs(iId);
}

PVR_ERROR ZatData::GetChannelGroupsAmount(int& amount)
{
  amount = static_cast<int>(m_channelGroups.size());
  return PVR_ERROR_NO_ERROR;
}

ZatData::ZatData(KODI_HANDLE instance, const std::string& version,
      const std::string& u, const std::string& p, bool favoritesOnly,
      bool alternativeEpgService, const STREAM_TYPE& streamType, bool enableDolby, int provider,
      const std::string& xmlTVFile, const std::string& parentalPin) :
    kodi::addon::CInstancePVRClient(instance, version),
    m_alternativeEpgService(alternativeEpgService),
    m_favoritesOnly(favoritesOnly),
    m_enableDolby(enableDolby),
    m_streamType(streamType),
    m_username(u),
    m_password(p),
    m_parentalPin(parentalPin)

{
  
  m_epgDB = new EpgDB(UserPath());
  m_recordingsDB = new RecordingsDB(UserPath());
  m_parameterDB = new ParameterDB(UserPath());
  
  kodi::Log(ADDON_LOG_INFO, "Using useragent: %s", user_agent.c_str());

  switch (provider)
  {
  case 1:
    m_providerUrl = "https://www.netplus.tv";
    break;
  case 2:
    m_providerUrl = "https://mobiltv.quickline.com";
    break;
  case 3:
    m_providerUrl = "https://tvplus.m-net.de";
    break;
  case 4:
    m_providerUrl = "https://player.waly.tv";
    break;
  case 5:
    m_providerUrl = "https://www.meinewelt.cc";
    break;
  case 6:
    m_providerUrl = "https://www.bbv-tv.net";
    break;
  case 7:
    m_providerUrl = "https://www.vtxtv.ch";
    break;
  case 8:
    m_providerUrl = "https://www.myvisiontv.ch";
    break;
  case 9:
    m_providerUrl = "https://iptv.glattvision.ch";
    break;
  case 10:
    m_providerUrl = "https://www.saktv.ch";
    break;
  case 11:
    m_providerUrl = "https://nettv.netcologne.de";
    break;
  case 12:
    m_providerUrl = "https://tvonline.ewe.de";
    break;
  case 13:
    m_providerUrl = "https://www.quantum-tv.com";
    break;
  case 14:
    m_providerUrl = "https://tv.salt.ch";
    break;
  case 15:
    m_providerUrl = "https://tvonline.swb-gruppe.de";
    break;
  case 16:
    m_providerUrl = "https://www.1und1.tv";
    break;
  default:
    m_providerUrl = "https://zattoo.com";
  }

  m_uuid = m_parameterDB->Get("uuid");
  m_zattooSession = m_parameterDB->Get("zattooSession");
  
  ReadDataJson();
  if (!xmlTVFile.empty())
  {
    m_xmlTV = new XmlTV(*this, xmlTVFile);
  }
}

ZatData::~ZatData()
{
  for (auto const &updateThread : m_updateThreads)
  {
    delete updateThread;
  }
  m_channelGroups.clear();
  if (m_xmlTV)
  {
    delete m_xmlTV;
  }
  delete m_epgDB;
}

bool ZatData::Initialize()
{
  if (!InitSession(false)) {
    return false;
  }

  for (int i = 0; i < 3; ++i)
  {
    m_updateThreads.emplace_back(new UpdateThread(*this, i, this));
  }

  return true;
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
  capabilities.SetSupportsRecordings(m_recordingEnabled);
  capabilities.SetSupportsTimers(m_recordingEnabled);

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
  connection = "connected";
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetChannelGroups(bool radio, kodi::addon::PVRChannelGroupsResultSet& results)
{
  if (radio)
    return PVR_ERROR_NOT_IMPLEMENTED;

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
  amount = static_cast<int>(m_channelsByCid.size());
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results)
{
  if (radio)
    return PVR_ERROR_NO_ERROR;

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
  if (doc.GetParseError() || !doc["success"].GetBool())
  {
    return "";
  }
  const Value& watchUrls = doc["stream"]["watch_urls"];
  std::string url = GetStringOrEmpty(doc["stream"], "url");
  for (Value::ConstValueIterator itr = watchUrls.Begin(); itr != watchUrls.End(); ++itr)
  {
    const Value& watchUrl = (*itr);
    kodi::Log(ADDON_LOG_DEBUG, "Selected url for maxrate: %d", watchUrl["maxrate"].GetInt());
    url = GetStringOrEmpty(watchUrl, "url");
    if (m_streamType == DASH_WIDEVINE) {
      std::string licenseUrl = GetStringOrEmpty(watchUrl, "license_url");
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

  std::string jsonString = HttpPost(m_providerUrl + "/zapi/watch/live/" + ownChannel->cid, dataStream.str());

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

void ZatData::GetEPGForChannelExternalService(int uniqueChannelId,
    time_t iStart, time_t iEnd)
{
  ZatChannel *zatChannel = FindChannel(uniqueChannelId);
  std::string cid = zatChannel->cid;
  std::ostringstream urlStream;
  std::string country = m_serviceRegionCountry.empty() ? m_countryCode : m_serviceRegionCountry;
  urlStream << "https://zattoo.buehlmann.net/epg/api/Epg/"
      << country << "/" << m_powerHash << "/" << cid << "/" << iStart
      << "/" << iEnd;
  std::string jsonString = HttpGetCached(urlStream.str(), 86400, user_agent);
  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError())
  {
    return;
  }
  std::lock_guard<std::mutex> lock(sendEpgToKodiMutex);
  for (Value::ConstValueIterator itr = doc.Begin(); itr != doc.End(); ++itr)
  {
    const Value& program = (*itr);
    kodi::addon::PVREPGTag tag;

    tag.SetUniqueBroadcastId(static_cast<unsigned int>(program["Id"].GetInt()));
    std::string title = GetStringOrEmpty(program, "Title");
    tag.SetTitle(title);
    tag.SetUniqueChannelId(static_cast<unsigned int>(zatChannel->iUniqueId));
    tag.SetStartTime(Utils::StringToTime(GetStringOrEmpty(program, "StartTime")));
    tag.SetEndTime(Utils::StringToTime(GetStringOrEmpty(program, "EndTime")));
    std::string description = GetStringOrEmpty(program, "Description");
    tag.SetPlotOutline(description);
    tag.SetPlot(description);
    tag.SetOriginalTitle(""); /* not supported */
    tag.SetCast(""); /* not supported */
    tag.SetDirector(""); /*SA not supported */
    tag.SetWriter(""); /* not supported */
    tag.SetYear(0); /* not supported */
    tag.SetIMDBNumber(""); /* not supported */
    std::string imageToken = GetStringOrEmpty(program, "ImageToken");
    std::string imageUrl;
    if (!imageToken.empty()) {
      imageUrl = GetImageUrl(imageToken);
      tag.SetIconPath(imageUrl);
    }
    tag.SetParentalRating(0); /* not supported */
    tag.SetStarRating(0); /* not supported */
    tag.SetSeriesNumber(EPG_TAG_INVALID_SERIES_EPISODE); /* not supported */
    tag.SetEpisodeNumber(EPG_TAG_INVALID_SERIES_EPISODE); /* not supported */
    tag.SetEpisodePartNumber(EPG_TAG_INVALID_SERIES_EPISODE); /* not supported */
    std::string subtitle = GetStringOrEmpty(program, "Subtitle");
    tag.SetEpisodeName(subtitle);
    std::string genreStr = GetStringOrEmpty(program, "Genre");
    int genre = m_categories.Category(genreStr);
    if (genre)
    {
      tag.SetGenreSubType(genre & 0x0F);
      tag.SetGenreType(genre & 0xF0);
    }
    else
    {
      tag.SetGenreType(EPG_GENRE_USE_STRING);
      tag.SetGenreSubType(0); /* not supported */
      tag.SetGenreDescription(genreStr);
    }
    kodi::addon::CInstancePVRClient::EpgEventStateChange(tag, EPG_EVENT_CREATED);
  }
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
  ZatChannel *zatChannel = FindChannel(uniqueChannelId);

  if (m_xmlTV && m_xmlTV->GetEPGForChannel(zatChannel->cid, m_channelsByCid))
  {
    return;
  }

  if (m_alternativeEpgService)
  {
    GetEPGForChannelExternalService(uniqueChannelId, iStart, iEnd);
    return;
  }

  std::map<time_t, PVRIptvEpgEntry>* channelEpgCache = LoadEPG(iStart, iEnd,
      uniqueChannelId);
  if (channelEpgCache == nullptr)
  {
    kodi::Log(ADDON_LOG_INFO, "Loading epg faild for channel '%s' from %lu to %lu",
        zatChannel->name.c_str(), iStart, iEnd);
    return;
  }
  
  std::vector<EpgDBInfo> epgDBInfos;
  
  std::lock_guard<std::mutex> lock(sendEpgToKodiMutex);
  for (auto const &entry : *channelEpgCache)
  {
    PVRIptvEpgEntry epgEntry = entry.second;

    kodi::addon::PVREPGTag tag;

    tag.SetUniqueBroadcastId(static_cast<unsigned int>(epgEntry.iBroadcastId));
    tag.SetTitle(epgEntry.strTitle);
    tag.SetUniqueChannelId(static_cast<unsigned int>(epgEntry.iChannelId));
    tag.SetStartTime(epgEntry.startTime);
    tag.SetEndTime(epgEntry.endTime);
    tag.SetPlotOutline(epgEntry.strPlot);
    tag.SetPlot(epgEntry.strPlot);
    tag.SetOriginalTitle(""); /* not supported */
    tag.SetCast(""); /* not supported */
    tag.SetDirector(""); /*SA not supported */
    tag.SetWriter(""); /* not supported */
    tag.SetYear(0); /* not supported */
    tag.SetIMDBNumber(""); /* not supported */
    tag.SetIconPath(epgEntry.strIconPath);
    tag.SetParentalRating(0); /* not supported */
    tag.SetStarRating(0); /* not supported */
    tag.SetSeriesNumber(EPG_TAG_INVALID_SERIES_EPISODE); /* not supported */
    tag.SetEpisodeNumber(EPG_TAG_INVALID_SERIES_EPISODE); /* not supported */
    tag.SetEpisodePartNumber(EPG_TAG_INVALID_SERIES_EPISODE); /* not supported */
    tag.SetEpisodeName(""); /* not supported */
    
    EpgDBInfo epgDBInfo;
    
    epgDBInfo.programId = epgEntry.iBroadcastId;
    epgDBInfo.recordUntil = epgEntry.recordUntil;
    epgDBInfo.replayUntil = epgEntry.replayUntil;
    epgDBInfo.restartUntil = epgEntry.restartUntil;
    epgDBInfos.push_back(epgDBInfo);

    int genre = m_categories.Category(epgEntry.strGenreString);
    if (genre)
    {
      tag.SetGenreSubType(genre & 0x0F);
      tag.SetGenreType(genre & 0xF0);
    }
    else
    {
      tag.SetGenreType(EPG_GENRE_USE_STRING);
      tag.SetGenreSubType(0); /* not supported */
      tag.SetGenreDescription(epgEntry.strGenreString);
    }
    kodi::addon::CInstancePVRClient::EpgEventStateChange(tag, EPG_EVENT_CREATED);
  }
  delete channelEpgCache;
  m_epgDB->InsertBatch(epgDBInfos);
}

std::map<time_t, PVRIptvEpgEntry>* ZatData::LoadEPG(time_t iStart, time_t iEnd,
    int uniqueChannelId)
{
  //Do some time magic that the start date is not to far in the past because zattoo doesnt like that
  time_t tempStart = iStart - (iStart % (3600 / 2)) - 86400;
  time_t tempEnd = tempStart + 3600 * 5; //Add 5 hours

  auto *epgCache = new std::map<time_t, PVRIptvEpgEntry>();

  while (tempEnd <= iEnd)
  {
    std::ostringstream urlStream;
    urlStream << m_providerUrl << "/zapi/v3/cached/" + m_powerHash + "/guide"
        << "?end=" << tempEnd << "&start=" << tempStart
        << "&format=json";

    std::string jsonString = HttpGetCached(urlStream.str(), 86400);

    Document doc;
    doc.Parse(jsonString.c_str());
    if (doc.GetParseError())
    {
      return nullptr;
    }

    const Value& channels = doc["channels"];

    for (Value::ConstMemberIterator iter = channels.MemberBegin(); iter != channels.MemberEnd(); ++iter){
      std::string cid = iter->name.GetString();

      int channelId = GetChannelId(cid.c_str());
      ZatChannel *channel = FindChannel(channelId);

      if (!channel || channel->iUniqueId != uniqueChannelId)
      {
        continue;
      }

      const Value& programs = iter->value;
      for (Value::ConstValueIterator itr1 = programs.Begin();
          itr1 != programs.End(); ++itr1)
      {
        const Value& program = (*itr1);

        const Type& checkType = program["t"].GetType();
        if (checkType != kStringType)
          continue;

        PVRIptvEpgEntry entry;
        entry.strTitle = GetStringOrEmpty(program, "t");
        entry.startTime = program["s"].GetInt();
        entry.endTime = program["e"].GetInt();
        entry.iBroadcastId = program["id"].GetInt();
        entry.strIconPath = GetImageUrl(GetStringOrEmpty(program, "i_t"));
        entry.iChannelId = channel->iUniqueId;
        entry.strPlot = GetStringOrEmpty(program, "et");
        entry.recordUntil = GetIntOrZero(program, "rg_u");
        entry.replayUntil = GetIntOrZero(program, "sr_u");
        entry.restartUntil = GetIntOrZero(program, "ry_u");
        
        const Value& genres = program["g"];
        for (Value::ConstValueIterator itr2 = genres.Begin();
            itr2 != genres.End(); ++itr2)
        {
          entry.strGenreString = (*itr2).GetString();
          break;
        }

        (*epgCache)[entry.startTime] = entry;
      }
    }
    tempStart = tempEnd;
    tempEnd = tempStart + 3600 * 5; //Add 5 hours
  }
  return epgCache;
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
    urlStream << m_providerUrl << "/zapi/v2/cached/program/power_details/"
        << m_powerHash << "?complete=True&program_ids=";
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

    std::string jsonString = HttpGetCached(urlStream.str(), 60 * 60 * 24 * 30);
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
        details.description = GetStringOrEmpty(program, "d");
        detailsById.insert(
            std::pair<int, ZatRecordingDetails>(program["id"].GetInt(), details));
      }
    }

  }

  return true;
}

PVR_ERROR ZatData::GetTimerTypes(std::vector<kodi::addon::PVRTimerType>& types)
{
  AddTimerType(types, 0, PVR_TIMER_TYPE_ATTRIBUTE_NONE);
  AddTimerType(types, 1, PVR_TIMER_TYPE_IS_MANUAL);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetTimers(kodi::addon::PVRTimersResultSet& results)
{
  std::string jsonString = HttpGet(m_providerUrl + "/zapi/playlist");

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

    std::string cid = GetStringOrEmpty(recording, "cid");
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
        GetStringOrEmpty(recording, "start"));
    if (startTime > current_time)
    {
      kodi::addon::PVRTimer tag;

      tag.SetClientIndex(static_cast<unsigned int>(recording["id"].GetInt()));
      tag.SetTitle(GetStringOrEmpty(recording, "title"));
      tag.SetSummary(GetStringOrEmpty(recording, "episode_title"));
      time_t endTime = Utils::StringToTime(
          GetStringOrEmpty(recording, "end").c_str());
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

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR ZatData::GetTimersAmount(int& amount)
{
  std::string jsonString = HttpGetCached(m_providerUrl + "/zapi/playlist", 60);

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
        GetStringOrEmpty(recording, "start"));
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
  else if (!Record(timer.GetEPGUid()))
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
  std::ostringstream dataStream;
  dataStream << "recording_id=" << timer.GetClientIndex() << "";

  std::string jsonString = HttpPost(m_providerUrl + "/zapi/playlist/remove",
      dataStream.str());

  Document doc;
  doc.Parse(jsonString.c_str());
  return doc.GetParseError() && doc["success"].GetBool() ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
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
  std::string jsonString = HttpGet(m_providerUrl + "/zapi/playlist");

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

    std::string cid = GetStringOrEmpty(recording, "cid");
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
        GetStringOrEmpty(recording, "start"));
    if (startTime <= current_time)
    {
      kodi::addon::PVRRecording tag;

      tag.SetSeriesNumber(PVR_RECORDING_INVALID_SERIES_EPISODE);
      tag.SetEpisodeNumber(PVR_RECORDING_INVALID_SERIES_EPISODE);

      tag.SetIsDeleted(false);

      tag.SetRecordingId(std::to_string(recording["id"].GetInt()));
      tag.SetTitle(GetStringOrEmpty(recording, "title"));
      tag.SetEpisodeName(GetStringOrEmpty(recording, "episode_title"));
      tag.SetPlot(hasDetails ? detailIterator->second.description : "");

      std::string imageToken = GetStringOrEmpty(recording, "image_token");
      std::string imageUrl = GetImageUrl(imageToken);;
      tag.SetIconPath(imageUrl);
      tag.SetChannelUid(channel.iUniqueId);
      tag.SetChannelName(channel.name);
      time_t endTime = Utils::StringToTime(
          GetStringOrEmpty(recording, "end").c_str());
      tag.SetRecordingTime(startTime);
      tag.SetDuration(static_cast<int>(endTime - startTime));

      if (genre)
      {
        tag.SetGenreSubType(genre & 0x0F);
        tag.SetGenreType(genre & 0xF0);
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
  std::string jsonString = HttpGetCached(m_providerUrl + "/zapi/playlist", 60);

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
        GetStringOrEmpty(recording, "start"));
    if (startTime <= current_time)
    {
      amount++;
    }
  }
  return PVR_ERROR_NO_ERROR;
}

std::string ZatData::GetStreamParameters() {
  std::string params = m_enableDolby ? "&enable_eac3=true" : "";
  params += "&stream_type=" + GetStreamTypeString();

  if (!m_parentalPin.empty()) {
    params += "&youth_protection_pin=" + m_parentalPin;
  }

  return params;
}

std::string ZatData::GetStreamTypeString() {
  switch (m_streamType) {
    case HLS:
      return "hls";
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
  dataStream << "recording_id=" << recording.GetRecordingId() << GetStreamParameters();

  std::string jsonString = HttpPost(m_providerUrl + "/zapi/watch", dataStream.str());

  std::string strUrl = GetStreamUrl(jsonString, properties);
  PVR_ERROR ret = PVR_ERROR_FAILED;
  if (!strUrl.empty())
  {
    SetStreamProperties(properties, strUrl);
    ret = PVR_ERROR_NO_ERROR;
  }

  return ret;
}

bool ZatData::Record(int programId)
{
  std::ostringstream dataStream;
  dataStream << "program_id=" << programId;

  std::string jsonString = HttpPost(m_providerUrl + "/zapi/playlist/program",
      dataStream.str());
  Document doc;
  doc.Parse(jsonString.c_str());
  return !doc.GetParseError() && doc["success"].GetBool();
}

PVR_ERROR ZatData::DeleteRecording(const kodi::addon::PVRRecording& recording)
{
  std::ostringstream dataStream;
  dataStream << "recording_id=" << recording.GetRecordingId() << "";

  std::string jsonString = HttpPost(m_providerUrl + "/zapi/playlist/remove",
      dataStream.str());

  Document doc;
  doc.Parse(jsonString.c_str());
  return doc.GetParseError() && doc["success"].GetBool() ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
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
  if (!m_recordingEnabled)
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

time_t ZatData::GetTimeForEpgTag(const kodi::addon::PVREPGTag& tag, const char * field)
{
  std::ostringstream urlStream;
  urlStream << "https://zattoo.com/zapi/v2/cached/program/power_details/" << m_powerHash << "?program_ids=" << tag.GetUniqueBroadcastId();
  std::string jsonString = HttpGetCached(urlStream.str(), 86400, user_agent);
  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError())
  {
    return 0;
  }
  const Value& programs = doc["programs"];
  for (Value::ConstValueIterator itr = programs.Begin(); itr != programs.End(); ++itr)
  {
    const Value& program = (*itr);
    if (program.HasMember(field)) {
      return program[field].GetInt();
    }
  }
  return 0;
}

PVR_ERROR ZatData::GetEPGTagStreamProperties(const kodi::addon::PVREPGTag& tag, std::vector<kodi::addon::PVRStreamProperty>& properties)
{
  PVR_ERROR ret = PVR_ERROR_FAILED;
  std::ostringstream dataStream;
  ZatChannel channel = m_channelsByUid[tag.GetUniqueChannelId()];

  std::string jsonString;

  kodi::Log(ADDON_LOG_DEBUG, "Get timeshift url for channel %s and program %i", channel.cid.c_str(), tag.GetUniqueBroadcastId());

  dataStream << GetStreamParameters();
  jsonString = HttpPost(m_providerUrl + "/zapi/watch/recall/" + channel.cid + "/" + std::to_string(tag.GetUniqueBroadcastId()), dataStream.str());

  std::string strUrl = GetStreamUrl(jsonString, properties);
  if (!strUrl.empty())
  {
    SetStreamProperties(properties, strUrl);
    ret = PVR_ERROR_NO_ERROR;
  }

  return ret;
}

PVR_ERROR ZatData::GetEPGTagEdl(const kodi::addon::PVREPGTag& tag, std::vector<kodi::addon::PVREDLEntry>& edl)
{
  kodi::addon::PVREDLEntry entry;
  entry.SetStart(0);
  entry.SetEnd(300000);
  entry.SetType(PVR_EDL_TYPE_COMBREAK);
  edl.emplace_back(entry);
  return PVR_ERROR_NO_ERROR;
}

std::string ZatData::GetStringOrEmpty(const Value& jsonValue, const char* fieldName)
{
  if (!jsonValue.HasMember(fieldName) || !jsonValue[fieldName].IsString())
  {
    return "";
  }
  return jsonValue[fieldName].GetString();
}

int ZatData::GetIntOrZero(const Value& jsonValue, const char* fieldName)
{
  if (!jsonValue.HasMember(fieldName) || !jsonValue[fieldName].IsInt())
  {
    return 0;
  }
  return jsonValue[fieldName].GetInt();
}

std::string ZatData::GetImageUrl(const std::string& imageToken) {
  return "https://images.zattic.com/cms/" + imageToken + "/format_640x360.jpg";
}
