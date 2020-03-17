#include <iostream>
#include <string>
#include "ZatData.h"
#include <sstream>
#include "p8-platform/sockets/tcp.h"
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

#ifdef TARGET_ANDROID
#include "to_string.h"
#endif

using namespace ADDON;
using namespace rapidjson;

constexpr char app_token_file[] = "special://temp/zattoo_app_token";
const char data_file[] = "special://profile/addon_data/pvr.zattoo/data.json";
const unsigned int EPG_TAG_FLAG_SELECTIVE_REPLAY = 0x00400000;
static const std::string user_agent = std::string("Kodi/")
    + std::string(STR(KODI_VERSION)) + std::string(" pvr.zattoo/")
    + std::string(STR(ZATTOO_VERSION)) + std::string(" (Kodi PVR addon)");
P8PLATFORM::CMutex ZatData::sendEpgToKodiMutex;

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
    curl.AddOption("cookie", "beaker.session.id=" + m_beakerSessionId);
  }

  if (!m_pzuid.empty())
  {
    curl.AddOption("Cookie", "pzuid=" + m_pzuid);
  }

  if (!userAgent.empty())
  {
    curl.AddHeader("User-Agent", userAgent);
  }

  std::string content = HttpRequestToCurl(curl, action, url, postData, statusCode);

  if (statusCode == 403 && !isInit)
  {
    XBMC->Log(LOG_ERROR, "Open URL failed. Try to re-init session.");
    if (!InitSession())
    {
      XBMC->Log(LOG_ERROR, "Re-init of session. Failed.");
      return "";
    }
    return HttpRequestToCurl(curl, action, url, postData, statusCode);
  }
  std::string sessionId = curl.GetCookie("beaker.session.id");
  if (!sessionId.empty() && m_beakerSessionId != sessionId)
  {
    XBMC->Log(LOG_DEBUG, "Got new beaker.session.id: %s..",
        sessionId.substr(0, 5).c_str());
    m_beakerSessionId = sessionId;
  }

  std::string pzuid = curl.GetCookie("pzuid");
  if (!pzuid.empty() && m_pzuid != pzuid)
  {
    XBMC->Log(LOG_DEBUG, "Got new pzuid: %s..", pzuid.substr(0, 5).c_str());
    m_pzuid = pzuid;
    WriteDataJson();
  }

  return content;
}

std::string ZatData::HttpRequestToCurl(Curl &curl, const std::string& action,
    const std::string& url, const std::string& postData, int &statusCode)
{
  XBMC->Log(LOG_DEBUG, "Http-Request: %s %s.", action.c_str(), url.c_str());
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
  if (!XBMC->FileExists(data_file, true))
  {
    return true;
  }
  std::string jsonString = Utils::ReadFile(data_file);
  if (jsonString.empty())
  {
    XBMC->Log(LOG_ERROR, "Loading data.json failed.");
    return false;
  }

  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError())
  {
    XBMC->Log(LOG_ERROR, "Parsing data.json failed.");
    return false;
  }

  const Value& recordings = doc["recordings"];
  for (Value::ConstValueIterator itr = recordings.Begin();
      itr != recordings.End(); ++itr)
  {
    const Value& recording = (*itr);
    auto *recData = new ZatRecordingData();
    recData->recordingId = GetStringOrEmpty(recording, "recordingId");
    recData->playCount = recording["playCount"].GetInt();
    recData->lastPlayedPosition = recording["lastPlayedPosition"].GetInt();
    recData->stillValid = false;
    m_recordingsData[recData->recordingId] = recData;
  }

  m_recordingsLoaded = false;

  if (doc.HasMember("pzuid"))
  {
    m_pzuid = GetStringOrEmpty(doc, "pzuid");
    XBMC->Log(LOG_DEBUG, "Loaded pzuid: %s..", m_pzuid.substr(0, 5).c_str());
  }

  if (doc.HasMember("uuid"))
  {
    m_uuid = GetStringOrEmpty(doc, "uuid");
    XBMC->Log(LOG_DEBUG, "Loaded uuid: %s", m_uuid.c_str());
  }

  XBMC->Log(LOG_DEBUG, "Loaded data.json.");
  return true;
}

bool ZatData::WriteDataJson()
{
  void* file;
  if (!(file = XBMC->OpenFileForWrite(data_file, true)))
  {
    XBMC->Log(LOG_ERROR, "Save data.json failed.");
    return false;
  }

  Document d;
  d.SetObject();

  Value a(kArrayType);
  Document::AllocatorType& allocator = d.GetAllocator();
  for (auto const& item : m_recordingsData)
  {
    if (m_recordingsLoaded && !item.second->stillValid)
    {
      continue;
    }

    Value r;
    r.SetObject();
    Value recordingId;
    recordingId.SetString(item.second->recordingId.c_str(),
        item.second->recordingId.length(), allocator);
    r.AddMember("recordingId", recordingId, allocator);
    r.AddMember("playCount", item.second->playCount, allocator);
    r.AddMember("lastPlayedPosition", item.second->lastPlayedPosition,
        allocator);
    a.PushBack(r, allocator);
  }
  d.AddMember("recordings", a, allocator);

  if (!m_pzuid.empty())
  {
    Value pzuidValue;
    pzuidValue.SetString(m_pzuid.c_str(), m_pzuid.length(), allocator);
    d.AddMember("pzuid", pzuidValue, allocator);
  }

  if (!m_uuid.empty())
  {
    Value uuidValue;
    uuidValue.SetString(m_uuid.c_str(), m_uuid.length(), allocator);
    d.AddMember("uuid", uuidValue, allocator);
  }

  StringBuffer buffer;
  Writer<StringBuffer> writer(buffer);
  d.Accept(writer);
  const char* output = buffer.GetString();
  XBMC->WriteFile(file, output, strlen(output));
  XBMC->CloseFile(file);
  return true;
}

std::string ZatData::GetUUID()
{
  if (!m_uuid.empty())
  {
    return m_uuid;
  }

  m_uuid = GenerateUUID();
  WriteDataJson();
  return m_uuid;
}

std::string ZatData::GenerateUUID()
{
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> uni(0, 15);
  std::ostringstream uuid;

  uuid << std::hex;

  for (int i = 0; i < 32; i++)
  {
    if (i == 8 || i == 12 || i == 16 || i == 20)
    {
      uuid << "-";
    }
    if (i == 12)
    {
      uuid << 4;
    }
    else if (i == 16)
    {
      uuid << ((uni(rng) % 4) + 8);
    }
    else
    {
      uuid << uni(rng);
    }
  }
  return uuid.str();
}

bool ZatData::LoadAppId()
{
  std::string html = HttpGet(m_providerUrl, true);

  m_appToken = "";
  //There seems to be a problem with old gcc and osx with regex. Do it the dirty way:
  size_t basePos = html.find("window.appToken = '") + 19;
  if (basePos > 19)
  {
    size_t endPos = html.find("'", basePos);
    m_appToken = html.substr(basePos, endPos - basePos);

    void* file;
    if (!(file = XBMC->OpenFileForWrite(app_token_file, true)))
    {
      XBMC->Log(LOG_ERROR, "Could not save app taken to %s", app_token_file);
    }
    else
    {
      XBMC->WriteFile(file, m_appToken.c_str(), m_appToken.length());
      XBMC->CloseFile(file);
    }
  }

  if (m_appToken.empty() && XBMC->FileExists(app_token_file, true))
  {
    XBMC->Log(LOG_NOTICE,
        "Could not get app token from page. Try to load from file.");
    m_appToken = Utils::ReadFile(app_token_file);
  }

  if (m_appToken.empty())
  {
    XBMC->Log(LOG_ERROR, "Unable to get app token.");
    return false;
  }

  XBMC->Log(LOG_DEBUG, "Loaded app token %s", m_appToken.c_str());
  return true;

}

bool ZatData::SendHello(std::string uuid)
{
  XBMC->Log(LOG_DEBUG, "Send hello.");
  std::ostringstream dataStream;
  dataStream << "uuid=" << uuid << "&lang=en&format=json&client_app_token="
      << m_appToken;

  std::string jsonString = HttpPost(m_providerUrl + "/zapi/session/hello",
      dataStream.str(), true);

  Document doc;
  doc.Parse(jsonString.c_str());
  if (!doc.GetParseError() && doc["success"].GetBool())
  {
    XBMC->Log(LOG_DEBUG, "Hello was successful.");
    return true;
  }
  else
  {
    XBMC->Log(LOG_NOTICE, "Hello failed.");
    return false;
  }
}

Document ZatData::Login()
{
  XBMC->Log(LOG_DEBUG, "Try to login.");

  std::ostringstream dataStream;
  dataStream << "login=" << Utils::UrlEncode(m_username) << "&password="
      << Utils::UrlEncode(m_password) << "&format=json&remember=true";
  std::string jsonString = HttpPost(m_providerUrl + "/zapi/v2/account/login",
      dataStream.str(), true, user_agent);

  Document doc;
  doc.Parse(jsonString.c_str());
  return doc;
}

bool ZatData::InitSession()
{
  std::string jsonString = HttpGet(m_providerUrl + "/zapi/v2/session", true);
  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc["success"].GetBool())
  {
    XBMC->Log(LOG_ERROR, "Initialize session failed.");
    return false;
  }

  if (!doc["session"]["loggedin"].GetBool())
  {
    XBMC->Log(LOG_DEBUG, "Need to login.");
    m_pzuid = "";
    m_beakerSessionId = "";
    WriteDataJson();
    doc = Login();
    if (doc.GetParseError() || !doc["success"].GetBool()
        || !doc["session"]["loggedin"].GetBool())
    {
      XBMC->Log(LOG_ERROR, "Login failed.");
      return false;
    }
    else
    {
      XBMC->Log(LOG_DEBUG, "Login was successful.");
    }
  }

  const Value& session = doc["session"];

  m_countryCode = GetStringOrEmpty(session, "aliased_country_code");
  m_serviceRegionCountry = GetStringOrEmpty(session, "service_region_country");
  m_recallEnabled = session["recall_eligible"].GetBool();
  m_selectiveRecallEnabled =
      session.HasMember("selective_recall_eligible") ?
          session["selective_recall_eligible"].GetBool() : false;
  m_recordingEnabled = session["recording_eligible"].GetBool();
  XBMC->Log(LOG_NOTICE, "Country code: %s", m_countryCode.c_str());
  XBMC->Log(LOG_NOTICE, "Service region country: %s",
      m_serviceRegionCountry.c_str());
  XBMC->Log(LOG_NOTICE, "Stream type: %s", GetStreamTypeString().c_str());
  if (m_recallEnabled)
  {
    m_maxRecallSeconds = session["recall_seconds"].GetInt();
    XBMC->Log(LOG_NOTICE, "Recall is enabled for %d seconds.",
        m_maxRecallSeconds);
  }
  else
  {
    XBMC->Log(LOG_NOTICE, "Recall is disabled");
  }
  XBMC->Log(LOG_NOTICE, "Selective recall is %s",
      m_selectiveRecallEnabled ? "enabled" : "disabled");
  XBMC->Log(LOG_NOTICE, "Recordings are %s",
      m_recordingEnabled ? "enabled" : "disabled");
  m_powerHash = GetStringOrEmpty(session, "power_guide_hash");
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
  urlStream << m_providerUrl + "/zapi/v2/cached/channels/" << m_powerHash
      << "?details=False";
  jsonString = HttpGet(urlStream.str());

  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc["success"].GetBool())
  {
    XBMC->Log(LOG_ERROR, "Failed to load channels");
    return false;
  }

  int channelNumber = favs.Size();
  const Value& groups = doc["channel_groups"];

  //Load the channel groups and channels
  for (Value::ConstValueIterator itr = groups.Begin(); itr != groups.End();
      ++itr)
  {
    PVRZattooChannelGroup group;
    const Value& groupItem = (*itr);
    group.name = GetStringOrEmpty(groupItem, "name");
    const Value& channels = groupItem["channels"];
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
          channel.selectiveRecallSeconds =
              channelItem.HasMember("selective_recall_seconds") ?
                  channelItem["selective_recall_seconds"].GetInt() : 0;
          channel.recordingEnabled =
              channelItem.HasMember("recording") ?
                  channelItem["recording"].GetBool() : false;
          group.channels.insert(group.channels.end(), channel);
          allChannels[cid] = channel;
          m_channelsByCid[channel.cid] = channel;
          m_channelsByUid[channel.iUniqueId] = channel;
          break;
        }
      }
    }
    if (!m_favoritesOnly && !group.channels.empty())
      m_channelGroups.insert(m_channelGroups.end(), group);
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

int ZatData::GetChannelGroupsAmount()
{
  return static_cast<int>(m_channelGroups.size());
}

ZatData::ZatData(const std::string& u, const std::string& p, bool favoritesOnly,
    bool alternativeEpgService, const STREAM_TYPE& streamType,  bool enableDolby, int provider,
    const std::string& xmlTVFile, const std::string& parentalPin) :
    m_alternativeEpgService(alternativeEpgService),
    m_favoritesOnly(favoritesOnly),
    m_enableDolby(enableDolby),
    m_streamType(streamType),
    m_username(u),
    m_password(p),
    m_parentalPin(parentalPin)
{
  XBMC->Log(LOG_NOTICE, "Using useragent: %s", user_agent.c_str());

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

  ReadDataJson();
  if (!xmlTVFile.empty())
  {
    m_xmlTV = new XmlTV(xmlTVFile);
  }
}

ZatData::~ZatData()
{
  for (auto const &updateThread : m_updateThreads)
  {
    updateThread->StopThread(200);
    delete updateThread;
  }
  for (auto const& item : m_recordingsData)
  {
    delete item.second;
  }
  m_channelGroups.clear();
  if (m_xmlTV)
  {
    delete m_xmlTV;
  }
}

bool ZatData::Initialize()
{
  if (!LoadAppId())
  {
    return false;
  }

  std::string uuid = GetUUID();

  SendHello(uuid);
  //Ignore if hello fails

  if (!InitSession()) {
    return false;
  }
  
  for (int i = 0; i < 3; ++i)
  {
    m_updateThreads.emplace_back(new UpdateThread(i, this));
  }

  return true;
}

void ZatData::GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{
  pCapabilities->bSupportsRecordings = m_recordingEnabled;
  pCapabilities->bSupportsTimers = m_recordingEnabled;
}

PVR_ERROR ZatData::GetChannelGroups(ADDON_HANDLE handle)
{
  std::vector<PVRZattooChannelGroup>::iterator it;
  for (it = m_channelGroups.begin(); it != m_channelGroups.end(); ++it)
  {
    PVR_CHANNEL_GROUP xbmcGroup;
    memset(&xbmcGroup, 0, sizeof(PVR_CHANNEL_GROUP));
    xbmcGroup.iPosition = 0; /* not supported  */
    xbmcGroup.bIsRadio = false; /* is radio group */
    strncpy(xbmcGroup.strGroupName, it->name.c_str(),
        sizeof(xbmcGroup.strGroupName) - 1);

    PVR->TransferChannelGroup(handle, &xbmcGroup);
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

PVR_ERROR ZatData::GetChannelGroupMembers(ADDON_HANDLE handle,
    const PVR_CHANNEL_GROUP &group)
{
  PVRZattooChannelGroup *myGroup;
  if ((myGroup = FindGroup(group.strGroupName)) != nullptr)
  {
    std::vector<ZatChannel>::iterator it;
    for (it = myGroup->channels.begin(); it != myGroup->channels.end(); ++it)
    {
      ZatChannel &channel = (*it);
      PVR_CHANNEL_GROUP_MEMBER xbmcGroupMember;
      memset(&xbmcGroupMember, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));

      strncpy(xbmcGroupMember.strGroupName, group.strGroupName,
          sizeof(xbmcGroupMember.strGroupName) - 1);
      xbmcGroupMember.iChannelUniqueId =
          static_cast<unsigned int>(channel.iUniqueId);
      xbmcGroupMember.iChannelNumber =
          static_cast<unsigned int>(channel.iChannelNumber);

      PVR->TransferChannelGroupMember(handle, &xbmcGroupMember);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

int ZatData::GetChannelsAmount()
{
  return static_cast<int>(m_channelsByCid.size());
}

PVR_ERROR ZatData::GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  std::vector<PVRZattooChannelGroup>::iterator it;
  for (it = m_channelGroups.begin(); it != m_channelGroups.end(); ++it)
  {
    std::vector<ZatChannel>::iterator it2;
    for (it2 = it->channels.begin(); it2 != it->channels.end(); ++it2)
    {
      ZatChannel &channel = (*it2);

      PVR_CHANNEL kodiChannel;
      memset(&kodiChannel, 0, sizeof(PVR_CHANNEL));

      kodiChannel.iUniqueId = static_cast<unsigned int>(channel.iUniqueId);
      kodiChannel.bIsRadio = false;
      kodiChannel.iChannelNumber =
          static_cast<unsigned int>(channel.iChannelNumber);
      strncpy(kodiChannel.strChannelName, channel.name.c_str(),
          sizeof(kodiChannel.strChannelName) - 1);
      kodiChannel.iEncryptionSystem = 0;

      std::ostringstream iconStream;
      iconStream
          << "special://home/addons/pvr.zattoo/resources/media/channel_logo/"
          << channel.cid << ".png";
      std::string iconPath = iconStream.str();
      if (!XBMC->FileExists(iconPath.c_str(), true))
      {
        std::ostringstream iconStreamSystem;
        iconStreamSystem
            << "special://xbmc/addons/pvr.zattoo/resources/media/channel_logo/"
            << channel.cid << ".png";
        iconPath = iconStreamSystem.str();
        if (!XBMC->FileExists(iconPath.c_str(), true))
        {
          XBMC->Log(LOG_INFO,
              "No logo found for channel '%s'. Fallback to Zattoo-Logo.",
              channel.cid.c_str());
          iconPath = channel.strLogoPath;
        }
      }

      strncpy(kodiChannel.strIconPath, iconPath.c_str(),
          sizeof(kodiChannel.strIconPath) - 1);

      kodiChannel.bIsHidden = false;

      PVR->TransferChannelEntry(handle, &kodiChannel);

    }
  }
  return PVR_ERROR_NO_ERROR;
}

std::string ZatData::GetStreamUrl(std::string& jsonString, std::map<std::string, std::string>& additionalPropertiesOut) {
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
    XBMC->Log(LOG_DEBUG, "Selected url for maxrate: %d", watchUrl["maxrate"].GetInt());
    url = GetStringOrEmpty(watchUrl, "url");
    if (m_streamType == DASH_WIDEVINE) {
      std::string licenseUrl = GetStringOrEmpty(watchUrl, "license_url");
      additionalPropertiesOut["inputstream.adaptive.license_key"] = licenseUrl + "||A{SSM}|";
      additionalPropertiesOut["inputstream.adaptive.license_type"] = "com.widevine.alpha";
    }
    break;
  }
  XBMC->Log(LOG_DEBUG, "Got url: %s", url.c_str());
  return url;
}

std::string ZatData::GetChannelStreamUrl(int uniqueId, std::map<std::string, std::string> &additionalPropertiesOut)
{

  ZatChannel *channel = FindChannel(uniqueId);
  XBMC->Log(LOG_DEBUG, "Get live url for channel %s", channel->cid.c_str());

  std::ostringstream dataStream;
  dataStream << GetStreamParameters() << "&format=json";

  if (m_recallEnabled)
  {
    dataStream << "&timeshift=" << m_maxRecallSeconds;
  }

  std::string jsonString = HttpPost(m_providerUrl + "/zapi/watch/live/" + channel->cid, dataStream.str());
  
  return GetStreamUrl(jsonString, additionalPropertiesOut);
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
  if (!sendEpgToKodiMutex.Lock()) {
    XBMC->Log(LOG_NOTICE, "Failed to lock sendEpgToKodiMutex.");
    return;
  }
  for (Value::ConstValueIterator itr = doc.Begin(); itr != doc.End(); ++itr)
  {
    const Value& program = (*itr);
    EPG_TAG tag;
    memset(&tag, 0, sizeof(EPG_TAG));

    tag.iUniqueBroadcastId = static_cast<unsigned int>(program["Id"].GetInt());
    std::string title = GetStringOrEmpty(program, "Title");
    tag.strTitle = title.c_str();
    tag.iUniqueChannelId = static_cast<unsigned int>(zatChannel->iUniqueId);
    tag.startTime = Utils::StringToTime(GetStringOrEmpty(program, "StartTime"));
    tag.endTime = Utils::StringToTime(GetStringOrEmpty(program, "EndTime"));
    std::string description = GetStringOrEmpty(program, "Description");
    tag.strPlotOutline = description.c_str();
    tag.strPlot = description.c_str();
    tag.strOriginalTitle = nullptr; /* not supported */
    tag.strCast = nullptr; /* not supported */
    tag.strDirector = nullptr; /*SA not supported */
    tag.strWriter = nullptr; /* not supported */
    tag.iYear = 0; /* not supported */
    tag.strIMDBNumber = nullptr; /* not supported */
    std::string imageToken = GetStringOrEmpty(program, "ImageToken");
    std::string imageUrl;
    if (!imageToken.empty()) {
      imageUrl = GetImageUrl(imageToken);
      tag.strIconPath = imageUrl.c_str();
    }
    tag.iParentalRating = 0; /* not supported */
    tag.iStarRating = 0; /* not supported */
    tag.iSeriesNumber = EPG_TAG_INVALID_SERIES_EPISODE; /* not supported */
    tag.iEpisodeNumber = EPG_TAG_INVALID_SERIES_EPISODE; /* not supported */
    tag.iEpisodePartNumber = EPG_TAG_INVALID_SERIES_EPISODE; /* not supported */
    std::string subtitle = GetStringOrEmpty(program, "Subtitle");
    tag.strEpisodeName = subtitle.c_str();
    tag.iFlags = program["SelectiveReplayAllowed"].GetBool() ? EPG_TAG_FLAG_SELECTIVE_REPLAY : EPG_TAG_FLAG_UNDEFINED;
    std::string genreStr = GetStringOrEmpty(program, "Genre");
    int genre = m_categories.Category(genreStr);
    if (genre)
    {
      tag.iGenreSubType = genre & 0x0F;
      tag.iGenreType = genre & 0xF0;
    }
    else
    {
      tag.iGenreType = EPG_GENRE_USE_STRING;
      tag.iGenreSubType = 0; /* not supported */
      tag.strGenreDescription = genreStr.c_str();
    }
    PVR->EpgEventStateChange(&tag, EPG_EVENT_CREATED);
  }
  sendEpgToKodiMutex.Unlock();

}

void ZatData::GetEPGForChannel(int iChannelUid, time_t iStart,
    time_t iEnd)
{
  // Aligning the start- and end times improves caching
  time_t aligendStart = iStart - (iStart % 86400);
  time_t alignedEnd = iEnd - (iEnd % 86400) + 86400;

  UpdateThread::LoadEpg(iChannelUid, aligendStart, alignedEnd);
}

void ZatData::GetEPGForChannelAsync(int uniqueChannelId, time_t iStart,
    time_t iEnd)
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
    XBMC->Log(LOG_NOTICE, "Loading epg faild for channel '%s' from %lu to %lu",
        zatChannel->name.c_str(), iStart, iEnd);
    return;
  }
  if (!sendEpgToKodiMutex.Lock()) {
    XBMC->Log(LOG_NOTICE, "Failed to lock sendEpgToKodiMutex.");
    return;
  }
  for (auto const &entry : *channelEpgCache)
  {
    PVRIptvEpgEntry epgEntry = entry.second;

    EPG_TAG tag;
    memset(&tag, 0, sizeof(EPG_TAG));

    tag.iUniqueBroadcastId = static_cast<unsigned int>(epgEntry.iBroadcastId);
    tag.strTitle = epgEntry.strTitle.c_str();
    tag.iUniqueChannelId = static_cast<unsigned int>(epgEntry.iChannelId);
    tag.startTime = epgEntry.startTime;
    tag.endTime = epgEntry.endTime;
    tag.strPlotOutline = epgEntry.strPlot.c_str();
    tag.strPlot = epgEntry.strPlot.c_str();
    tag.strOriginalTitle = nullptr; /* not supported */
    tag.strCast = nullptr; /* not supported */
    tag.strDirector = nullptr; /*SA not supported */
    tag.strWriter = nullptr; /* not supported */
    tag.iYear = 0; /* not supported */
    tag.strIMDBNumber = nullptr; /* not supported */
    tag.strIconPath = epgEntry.strIconPath.c_str();
    tag.iParentalRating = 0; /* not supported */
    tag.iStarRating = 0; /* not supported */
    tag.iSeriesNumber = EPG_TAG_INVALID_SERIES_EPISODE; /* not supported */
    tag.iEpisodeNumber = EPG_TAG_INVALID_SERIES_EPISODE; /* not supported */
    tag.iEpisodePartNumber = EPG_TAG_INVALID_SERIES_EPISODE; /* not supported */
    tag.strEpisodeName = nullptr; /* not supported */
    tag.iFlags = epgEntry.selectiveReplay ? EPG_TAG_FLAG_SELECTIVE_REPLAY : EPG_TAG_FLAG_UNDEFINED;

    int genre = m_categories.Category(epgEntry.strGenreString);
    if (genre)
    {
      tag.iGenreSubType = genre & 0x0F;
      tag.iGenreType = genre & 0xF0;
    }
    else
    {
      tag.iGenreType = EPG_GENRE_USE_STRING;
      tag.iGenreSubType = 0; /* not supported */
      tag.strGenreDescription = epgEntry.strGenreString.c_str();
    }
    PVR->EpgEventStateChange(&tag, EPG_EVENT_CREATED);
  }
  sendEpgToKodiMutex.Unlock();
  delete channelEpgCache;
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
    urlStream << m_providerUrl << "/zapi/v2/cached/program/power_guide/"
        << m_powerHash << "?end=" << tempEnd << "&start=" << tempStart
        << "&format=json";

    std::string jsonString = HttpGetCached(urlStream.str(), 86400);

    Document doc;
    doc.Parse(jsonString.c_str());
    if (doc.GetParseError() || !doc["success"].GetBool())
    {
      return nullptr;
    }

    const Value& channels = doc["channels"];

    //Load the channel groups and channels
    for (Value::ConstValueIterator itr = channels.Begin();
        itr != channels.End(); ++itr)
    {
      const Value& channelItem = (*itr);
      std::string cid = GetStringOrEmpty(channelItem, "cid");

      int channelId = GetChannelId(cid.c_str());
      ZatChannel *channel = FindChannel(channelId);

      if (!channel || channel->iUniqueId != uniqueChannelId)
      {
        continue;
      }

      const Value& programs = channelItem["programs"];
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
        entry.selectiveReplay = program["r_e"].GetBool();

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

void ZatData::SetRecordingPlayCount(const PVR_RECORDING &recording, int count)
{
  std::string recordingId = recording.strRecordingId;
  ZatRecordingData *recData;
  if (m_recordingsData.find(recordingId) != m_recordingsData.end())
  {
    recData = m_recordingsData[recordingId];
    recData->playCount = count;
  }
  else
  {
    recData = new ZatRecordingData();
    recData->playCount = count;
    recData->recordingId = recordingId;
    recData->lastPlayedPosition = 0;
    recData->stillValid = true;
    m_recordingsData[recordingId] = recData;
  }

  WriteDataJson();
}

void ZatData::SetRecordingLastPlayedPosition(const PVR_RECORDING &recording,
    int lastplayedposition)
{
  std::string recordingId = recording.strRecordingId;
  ZatRecordingData *recData;
  if (m_recordingsData.find(recordingId) != m_recordingsData.end())
  {
    recData = m_recordingsData[recordingId];
    recData->lastPlayedPosition = lastplayedposition;
  }
  else
  {
    recData = new ZatRecordingData();
    recData->playCount = 0;
    recData->recordingId = recordingId;
    recData->lastPlayedPosition = lastplayedposition;
    recData->stillValid = true;
    m_recordingsData[recordingId] = recData;
  }

  WriteDataJson();
}

int ZatData::GetRecordingLastPlayedPosition(const PVR_RECORDING &recording)
{
  if (m_recordingsData.find(recording.strRecordingId) != m_recordingsData.end())
  {
    ZatRecordingData* recData = m_recordingsData[recording.strRecordingId];
    return recData->lastPlayedPosition;
  }
  return 0;
}

void ZatData::GetRecordings(ADDON_HANDLE handle, bool future)
{
  std::string jsonString = HttpGet(m_providerUrl + "/zapi/playlist");

  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc["success"].GetBool())
  {
    return;
  }

  const Value& recordings = doc["recordings"];

  time_t current_time;
  time(&current_time);

  std::string idList;

  std::map<int, ZatRecordingDetails> detailsById;
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

    jsonString = HttpGetCached(urlStream.str(), 60 * 60 * 24 * 30);
    Document detailDoc;
    detailDoc.Parse(jsonString.c_str());
    if (detailDoc.GetParseError() || !detailDoc["success"].GetBool())
    {
      XBMC->Log(LOG_ERROR, "Failed to load details for recordings.");
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

  for (Value::ConstValueIterator itr = recordings.Begin();
      itr != recordings.End(); ++itr)
  {
    const Value& recording = (*itr);
    int programId = recording["program_id"].GetInt();

    std::string cid = GetStringOrEmpty(recording, "cid");
    auto iterator = m_channelsByCid.find(cid);
    if (iterator == m_channelsByCid.end())
    {
      XBMC->Log(LOG_ERROR, "Channel %s not found for recording: %i",
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
    if (future && (startTime > current_time))
    {
      PVR_TIMER tag;
      memset(&tag, 0, sizeof(PVR_TIMER));

      tag.iClientIndex = static_cast<unsigned int>(recording["id"].GetInt());
      PVR_STRCPY(tag.strTitle, GetStringOrEmpty(recording, "title").c_str());
      PVR_STRCPY(tag.strSummary,
          GetStringOrEmpty(recording, "episode_title").c_str());
      time_t endTime = Utils::StringToTime(
          GetStringOrEmpty(recording, "end").c_str());
      tag.startTime = startTime;
      tag.endTime = endTime;
      tag.state = PVR_TIMER_STATE_SCHEDULED;
      tag.iTimerType = 1;
      tag.iEpgUid = static_cast<unsigned int>(recording["program_id"].GetInt());
      tag.iClientChannelUid = channel.iUniqueId;
      if (genre)
      {
        tag.iGenreSubType = genre & 0x0F;
        tag.iGenreType = genre & 0xF0;
      }
      PVR->TransferTimerEntry(handle, &tag);
      UpdateThread::SetNextRecordingUpdate(startTime);
    }
    else if (!future && (startTime <= current_time))
    {
      PVR_RECORDING tag;
      memset(&tag, 0, sizeof(PVR_RECORDING));
      tag.iSeriesNumber = PVR_RECORDING_INVALID_SERIES_EPISODE;
      tag.iEpisodeNumber = PVR_RECORDING_INVALID_SERIES_EPISODE;

      tag.bIsDeleted = false;

      PVR_STRCPY(tag.strRecordingId,
          std::to_string(recording["id"].GetInt()).c_str());
      PVR_STRCPY(tag.strTitle, GetStringOrEmpty(recording, "title").c_str());
      PVR_STRCPY(tag.strEpisodeName,
          GetStringOrEmpty(recording, "episode_title").c_str());
      PVR_STRCPY(tag.strPlot,
          hasDetails ? detailIterator->second.description.c_str() : "");
      
      std::string imageToken = GetStringOrEmpty(recording, "image_token");
      std::string imageUrl = GetImageUrl(imageToken);;
      PVR_STRCPY(tag.strIconPath, imageUrl.c_str());
      tag.iChannelUid = channel.iUniqueId;
      PVR_STRCPY(tag.strChannelName, channel.name.c_str());
      time_t endTime = Utils::StringToTime(
          GetStringOrEmpty(recording, "end").c_str());
      tag.recordingTime = startTime;
      tag.iDuration = static_cast<int>(endTime - startTime);

      if (genre)
      {
        tag.iGenreSubType = genre & 0x0F;
        tag.iGenreType = genre & 0xF0;
      }

      if (m_recordingsData.find(tag.strRecordingId) != m_recordingsData.end())
      {
        ZatRecordingData* recData = m_recordingsData[tag.strRecordingId];
        tag.iPlayCount = recData->playCount;
        tag.iLastPlayedPosition = recData->lastPlayedPosition;
        recData->stillValid = true;
      }

      PVR->TransferRecordingEntry(handle, &tag);
      m_recordingsLoaded = true;
    }
  }
}

int ZatData::GetRecordingsAmount(bool future)
{
  std::string jsonString = HttpGetCached(m_providerUrl + "/zapi/playlist", 60);

  time_t current_time;
  time(&current_time);

  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc["success"].GetBool())
  {
    return 0;
  }

  const Value& recordings = doc["recordings"];

  int count = 0;
  for (Value::ConstValueIterator itr = recordings.Begin();
      itr != recordings.End(); ++itr)
  {
    const Value& recording = (*itr);
    time_t startTime = Utils::StringToTime(
        GetStringOrEmpty(recording, "start"));
    if (future == (startTime > current_time))
    {
      count++;
    }
  }
  return count;
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

std::string ZatData::GetRecordingStreamUrl(const std::string& recordingId, std::map<std::string, std::string> &additionalPropertiesOut)
{
  XBMC->Log(LOG_DEBUG, "Get url for recording %s", recordingId.c_str());

  std::ostringstream dataStream;
  dataStream << "recording_id=" << recordingId << GetStreamParameters();

  std::string jsonString = HttpPost(m_providerUrl + "/zapi/watch", dataStream.str());

  return GetStreamUrl(jsonString, additionalPropertiesOut);
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

bool ZatData::DeleteRecording(const std::string& recordingId)
{
  std::ostringstream dataStream;
  dataStream << "recording_id=" << recordingId << "";

  std::string jsonString = HttpPost(m_providerUrl + "/zapi/playlist/remove",
      dataStream.str());

  Document doc;
  doc.Parse(jsonString.c_str());
  return !doc.GetParseError() && doc["success"].GetBool();
}

bool ZatData::IsPlayable(const EPG_TAG *tag)
{
  time_t current_time;
  time(&current_time);
  if (tag->startTime > current_time)
  {
    return false;
  }
  int recallSeconds = GetRecallSeconds(tag);
  if (recallSeconds == 0)
  {
    return false;
  }
  if (current_time < tag->endTime)
  {
    return true;
  }
  return (current_time - tag->endTime) < recallSeconds;
}

int ZatData::GetRecallSeconds(const EPG_TAG *tag)
{
  if (m_recallEnabled)
  {
    return static_cast<int>(m_maxRecallSeconds);
  }
  if (m_selectiveRecallEnabled)
  {
    if ((tag->iFlags & EPG_TAG_FLAG_SELECTIVE_REPLAY) == 0) {
      return 0;
    }
    ZatChannel channel = m_channelsByUid[tag->iUniqueChannelId];
    return channel.selectiveRecallSeconds;
  }
  return 0;
}

bool ZatData::IsRecordable(const EPG_TAG *tag)
{
  if (!m_recordingEnabled)
  {
    return false;
  }
  ZatChannel channel = m_channelsByUid[tag->iUniqueChannelId];
  if (!channel.recordingEnabled)
  {
    return false;
  }
  int recallSeconds = GetRecallSeconds(tag);
  time_t current_time;
  time(&current_time);
  if (recallSeconds == 0)
  {
    return current_time < tag->startTime;
  }
  return ((current_time - tag->endTime) < recallSeconds);
}

std::string ZatData::GetEpgTagUrl(const EPG_TAG *tag, std::map<std::string, std::string> &additionalPropertiesOut)
{
  std::ostringstream dataStream;
  ZatChannel channel = m_channelsByUid[tag->iUniqueChannelId];

  std::string jsonString;

  XBMC->Log(LOG_DEBUG, "Get timeshift url for channel %s and program %i", channel.cid.c_str(), tag->iUniqueBroadcastId);
  
  if (m_recallEnabled)
  {
    dataStream << GetStreamParameters();
    jsonString = HttpPost(m_providerUrl + "/zapi/watch/recall/" + channel.cid + "/" + std::to_string(tag->iUniqueBroadcastId), dataStream.str());
  }
  else if (m_selectiveRecallEnabled)
  {
    dataStream << "https_watch_urls=True" << GetStreamParameters();
    jsonString = HttpPost(
        m_providerUrl + "/zapi/watch/selective_recall/" + channel.cid + "/"
            + std::to_string(tag->iUniqueBroadcastId), dataStream.str());
  }
  else
  {
    return "";
  }

  return GetStreamUrl(jsonString, additionalPropertiesOut);
}

std::string ZatData::GetStringOrEmpty(const Value& jsonValue, const char* fieldName)
{
  if (!jsonValue.HasMember(fieldName) || !jsonValue[fieldName].IsString())
  {
    return "";
  }
  return jsonValue[fieldName].GetString();
}

std::string ZatData::GetImageUrl(const std::string& imageToken) {
  return "https://images.zattic.com/cms/" + imageToken + "/format_640x360.jpg";
}
