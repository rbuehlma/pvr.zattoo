#include <iostream>
#include <string>
#include "ZatData.h"
#include "yajl/yajl_tree.h"
#include "yajl/yajl_gen.h"
#include <sstream>
#include "../lib/tinyxml2/tinyxml2.h"
#include "p8-platform/sockets/tcp.h"
#include <map>
#include <time.h>
#include <random>
#include "Utils.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
 #pragma comment(lib, "ws2_32.lib")
  #include <stdio.h>
 #include <stdlib.h>
#endif

#ifdef TARGET_ANDROID
#include "to_string.h"
#endif

#define DEBUG

#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif



using namespace ADDON;
using namespace std;

static const string data_file = "special://profile/addon_data/pvr.zattoo/data.json";
static const string zattooServer = "https://zattoo.com";

string ZatData::HttpGet(string url, bool isInit) {
  return HttpPost(url, "", isInit);
}

string ZatData::HttpPost(string url, string postData, bool isInit) {
  int statusCode;
  XBMC->Log(LOG_DEBUG, "Http-Request: %s.", url.c_str());
  string content = curl->Post(url, postData, statusCode);
  
  if (statusCode == 403 && !isInit) {
    delete curl;
    curl = new Curl();
    XBMC->Log(LOG_ERROR, "Open URL failed. Try to re-init session.");
    if (!initSession()) {
      XBMC->Log(LOG_ERROR, "Re-init of session. Failed.");
      return "";
    }
    return curl->Post(url, postData, statusCode);
  }
  return content;
}

bool ZatData::readDataJson() {
  void* file;
  char buf[256];
  size_t nbRead;
  string jsonString;
  if (!XBMC->FileExists(data_file.c_str(), true)) {
    return true;
  }
  file = XBMC->CURLCreate(data_file.c_str());
  if (!file || !XBMC->CURLOpen(file, 0)) {
    XBMC->Log(LOG_ERROR, "Loading data.json failed.");
    return false;
  }
  while ((nbRead = XBMC->ReadFile(file, buf, 255)) > 0) {
    buf[nbRead] = 0;
    jsonString.append(buf);
  }
  XBMC->CloseFile(file);

  yajl_val json = JsonParser::parse(jsonString);
  if (json == NULL) {
    XBMC->Log(LOG_ERROR, "Parsing data.json failed.");
    return false;
  }

  yajl_val recordings = JsonParser::getArray(json, 1, "recordings");
  for ( int index = 0; index < recordings->u.array.len; ++index ) {
    yajl_val recording = recordings->u.array.values[index];
    ZatRecordingData *recData = new ZatRecordingData();
    recData->recordingId = JsonParser::getString(recording, 1, "recordingId");
    recData->playCount = JsonParser::getInt(recording, 1, "playCount");
    recData->lastPlayedPosition = JsonParser::getInt(recording, 1, "lastPlayedPosition");
    recData->stillValid = false;
    recordingsData[recData->recordingId] = recData;
  }

  yajl_tree_free(json);
  XBMC->Log(LOG_DEBUG, "Loaded data.json.");
  return true;
}

bool ZatData::writeDataJson() {
  void* file;
  string jsonString;
  yajl_gen g;
  if (!(file = XBMC->OpenFileForWrite(data_file.c_str(), true))) {
    XBMC->Log(LOG_ERROR, "Save data.json failed.");
    return false;
  }

  g = yajl_gen_alloc(NULL);
  yajl_gen_config(g, yajl_gen_beautify, 1);
  yajl_gen_config(g, yajl_gen_validate_utf8, 1);
  const unsigned char * buf;
  size_t len;
  yajl_gen_map_open(g);
  JS_STR(g, "recordings");
  yajl_gen_array_open(g);
  for (auto const& item : recordingsData) {
    if (!item.second->stillValid) {
      continue;
    }
    yajl_gen_map_open(g);
    JS_STR(g, "recordingId");
    JS_STR(g, item.second->recordingId);
    JS_STR(g, "playCount");
    yajl_gen_integer(g, item.second->playCount);
    JS_STR(g, "lastPlayedPosition");
    yajl_gen_integer(g, item.second->lastPlayedPosition);
    yajl_gen_map_close(g);
  }
  yajl_gen_array_close(g);
  yajl_gen_map_close(g);
  while (yajl_gen_status_ok == yajl_gen_get_buf(g, &buf, &len) && len > 0) {
    XBMC->WriteFile(file, buf, len);
    yajl_gen_clear(g);
  }
  XBMC->CloseFile(file);
  yajl_gen_free(g);
  return true;
}

string ZatData::getUUID() {
  void* file;
  char buf[37];
  size_t nbRead;

  if (!uuid.empty()) {
    return uuid;
  }

  uuid = generateUUID();
  return uuid;
}

string ZatData::generateUUID() {
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> uni(0,15);
  ostringstream uuid;

  uuid << std::hex;

  for (int i = 0; i<32; i++) {
    if (i == 8 || i == 12 || i == 16 || i == 20) {
      uuid << "-";
    }
    if (i == 12) {
      uuid << 4;
    } else if (i == 16) {
      uuid << ((uni(rng) % 4) + 8);
    } else {
      uuid << uni(rng);
    }
  }
  return uuid.str();
}

bool ZatData::loadAppId() {
  string html = HttpGet(zattooServer, true);

  appToken = "";
  //There seems to be a problem with old gcc and osx with regex. Do it the dirty way:
  int basePos = html.find("window.appToken = '") + 19;
  if (basePos > 19) {
    int endPos = html.find("'", basePos);
    appToken = html.substr(basePos, endPos-basePos);
  }

  if(appToken.empty()) {
    XBMC->Log(LOG_ERROR, "Error getting app token");
    return false;
  }

  XBMC->Log(LOG_DEBUG, "Loaded App token %s", appToken.c_str());
  return true;

}

bool ZatData::sendHello(string uuid) {
  XBMC->Log(LOG_DEBUG, "Send hello.");
  ostringstream dataStream;
  dataStream << "uuid=" << uuid << "&lang=en&format=json&client_app_token=" << appToken;

  string jsonString = HttpPost(zattooServer + "/zapi/session/hello", dataStream.str(), true);

  yajl_val json = JsonParser::parse(jsonString);

  if (json != NULL && JsonParser::getBoolean(json, 1, "success")) {
    yajl_tree_free(json);
    XBMC->Log(LOG_DEBUG, "Hello was successful.");
    return true;
  } else {
    yajl_tree_free(json);
    XBMC->Log(LOG_ERROR, "Hello failed.");
    return false;
  }
}

bool ZatData::login() {
  XBMC->Log(LOG_DEBUG, "Try to login.");

  ostringstream dataStream;
  dataStream << "login=" << Utils::UrlEncode(username) << "&password=" << Utils::UrlEncode(password) << "&format=json";
  string jsonString = HttpPost(zattooServer + "/zapi/account/login", dataStream.str(), true);

  yajl_val json = JsonParser::parse(jsonString);

  if (json == NULL || !JsonParser::getBoolean(json, 1, "success")){
    yajl_tree_free(json);
    XBMC->Log(LOG_ERROR, "Login failed.");
    return false;
  }

  yajl_tree_free(json);
  XBMC->Log(LOG_DEBUG, "Login was successful.");
  return true;
}

bool ZatData::initSession() {
  string jsonString = HttpGet(zattooServer + "/zapi/v2/session", true);
  yajl_val json = JsonParser::parse(jsonString);
  if(json == NULL || !JsonParser::getBoolean(json, 1, "success")) {
    yajl_tree_free(json);
    XBMC->Log(LOG_ERROR, "Initialize session failed.");
    return false;
  }

  if (!JsonParser::getBoolean(json, 2, "session" , "loggedin")) {
    yajl_tree_free(json);
    XBMC->Log(LOG_DEBUG, "Need to login.");

    string uuid = getUUID();
    if (uuid.empty()) {
      return false;
    }

    sendHello(uuid);
    //Ignore if hello failes

    if (!login()) {
      return false;
    }
    jsonString = HttpGet(zattooServer + "/zapi/v2/session", true);
    json = JsonParser::parse(jsonString);
    if (json == NULL || !JsonParser::getBoolean(json, 1, "success") || !JsonParser::getBoolean(json, 2, "session" , "loggedin")) {
      yajl_tree_free(json);
      XBMC->Log(LOG_ERROR, "Initialize session failed.");
      return false;
    }
  }

  countryCode = JsonParser::getString(json, 2, "session" , "aliased_country_code");
  recallEnabled = streamType == "dash" && JsonParser::getBoolean(json, 2, "session" , "recall_eligible");
  recordingEnabled = JsonParser::getBoolean(json, 2, "session" , "recording_eligible");
  if (recallEnabled) {
    maxRecallSeconds = JsonParser::getInt(json, 2, "session" , "recall_seconds");
  }
  if (recordingEnabled && updateThread == NULL) {
    updateThread = new UpdateThread();
  }
  powerHash = JsonParser::getString(json, 2, "session" , "power_guide_hash");
  yajl_tree_free(json);
  return true;
}


yajl_val ZatData::loadFavourites() {

    string jsonString = HttpGet(zattooServer + "/zapi/channels/favorites");
    yajl_val json = JsonParser::parse(jsonString);
    
    if (json == NULL || !JsonParser::getBoolean(json, 1, "success")){
      yajl_tree_free(json);
      return NULL;
    }

    return json;
}


bool ZatData::LoadChannels() {
    std::map<std::string, ZatChannel> allChannels;
    yajl_val favsJson = loadFavourites();
    
    if (favsJson == NULL) {
        return false;
    }

    yajl_val favs = JsonParser::getArray(favsJson, 1, "favorites");

    ostringstream urlStream;
    urlStream << zattooServer + "/zapi/v2/cached/channels/" << powerHash << "?details=False";
    string jsonString = HttpGet(urlStream.str());

    yajl_val json = JsonParser::parse(jsonString);
    
    if (json == NULL || !JsonParser::getBoolean(json, 1, "success")){
        std::cout  << "Failed to parse configuration\n";
        yajl_tree_free(json);
        yajl_tree_free(favsJson);
        return false;
    }

    int channelNumber = favs->u.array.len + 1;
    yajl_val groups = JsonParser::getArray(json, 1, "channel_groups");

    //Load the channel groups and channels
    for ( int index = 0; index < groups->u.array.len; ++index ) {
        PVRZattooChannelGroup group;
        yajl_val groupItem = groups->u.array.values[index];
        group.name = JsonParser::getString(groupItem, 1, "name");
        yajl_val channels = JsonParser::getArray(groupItem, 1, "channels");
        for(int i = 0; i < channels->u.array.len; ++i) {
            yajl_val channelItem = channels->u.array.values[i];
            yajl_val qualities = JsonParser::getArray(channelItem, 1, "qualities");
            for(int q = 0; q < qualities->u.array.len; ++q) {
                yajl_val qualityItem = qualities->u.array.values[q];
                std::string avail = JsonParser::getString(qualityItem, 1, "availability");
                if(avail == "available") {
                    ZatChannel channel;
                    channel.name = JsonParser::getString(qualityItem, 1, "title");
                    channel.strStreamURL = "";
                    std::string cid = JsonParser::getString(channelItem, 1, "cid");
                    channel.iUniqueId = GetChannelId(cid.c_str());
                    channel.cid = cid;
                    channel.iChannelNumber = ++channelNumber;
                    channel.strLogoPath = "http://logos.zattic.com";
                    channel.strLogoPath.append(JsonParser::getString(qualityItem, 1, "logo_white_84"));
                    group.channels.insert(group.channels.end(), channel);
                    allChannels[cid] = channel;
                    channelsByNumber[channel.iChannelNumber] = channel;
                    channelsByCid[channel.cid] = channel;
                    break;
                }
            }
        }
        if (!favoritesOnly && group.channels.size() > 0)
            channelGroups.insert(channelGroups.end(),group);
    }

    PVRZattooChannelGroup favGroup;
    favGroup.name = "Favoriten";

    for (int fav = 0; fav < favs->u.array.len; fav++) {
      yajl_val favItem = favs->u.array.values[fav];
      string favCid = YAJL_GET_STRING(favItem);
      if (allChannels.find(favCid) != allChannels.end()) {
        ZatChannel channel = allChannels[favCid];
        channel.iChannelNumber = favGroup.channels.size() + 1;
        favGroup.channels.insert(favGroup.channels.end(), channel);
        channelsByNumber[channel.iChannelNumber] = channel;
        channelsByCid[channel.cid] = channel;
      }
    }

    if (favGroup.channels.size() > 0)
        channelGroups.insert(channelGroups.end(),favGroup);

    yajl_tree_free(json);
    yajl_tree_free(favsJson);
    return true;
}

int ZatData::GetChannelId(const char * strChannelName)
{
    int iId = 0;
    int c;
    while (c = *strChannelName++)
        iId = ((iId << 5) + iId) + c; /* iId * 33 + c */
    return abs(iId);
}


int ZatData::GetChannelGroupsAmount() {
    return channelGroups.size();
}

ZatData::ZatData(std::string u, std::string p, bool favoritesOnly, bool alternativeEpgService) :
  maxRecallSeconds(0),
  recallEnabled(false),
  countryCode(""),
  recordingEnabled(false),
  updateThread(NULL),
  uuid("")
{
  curl = new Curl();
  username = u;
  password = p;
  this->alternativeEpgService = alternativeEpgService;
  this->favoritesOnly = favoritesOnly;
  m_iLastStart = 0;
  m_iLastEnd = 0;
  streamType = "hls";
}

ZatData::~ZatData() {
  if (updateThread != NULL) {
    updateThread->StopThread(1000);
    delete updateThread;
  }
  for (auto const& item : recordingsData) {
    delete item.second;
  }
  channelGroups.clear();
  delete curl;
}

bool ZatData::Initialize() {

  if (!loadAppId()) {
    XBMC->Log(LOG_ERROR, "Could not get an app id.");
    return false;
  }

  if (!initSession()) {
    return false;
  }

  readDataJson();

  return true;
}

void ZatData::GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities) {
  pCapabilities->bSupportsRecordings      = recordingEnabled;
  pCapabilities->bSupportsTimers          = recordingEnabled;
}

PVR_ERROR ZatData::GetChannelGroups(ADDON_HANDLE handle) {
    std::vector<PVRZattooChannelGroup>::iterator it;
    for (it = channelGroups.begin(); it != channelGroups.end(); ++it)
    {
            PVR_CHANNEL_GROUP xbmcGroup;
            memset(&xbmcGroup, 0, sizeof(PVR_CHANNEL_GROUP));
            xbmcGroup.iPosition = 0;      /* not supported  */
            xbmcGroup.bIsRadio  = false; /* is radio group */
            strncpy(xbmcGroup.strGroupName, it->name.c_str(), sizeof(xbmcGroup.strGroupName) - 1);

            PVR->TransferChannelGroup(handle, &xbmcGroup);
    }
    return PVR_ERROR_NO_ERROR;
}

PVRZattooChannelGroup *ZatData::FindGroup(const std::string &strName) {
    std::vector<PVRZattooChannelGroup>::iterator it;
    for(it = channelGroups.begin(); it < channelGroups.end(); ++it)
    {
        if (it->name == strName)
            return &*it;
    }

    return NULL;
}

PVR_ERROR ZatData::GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group) {
    PVRZattooChannelGroup *myGroup;
    if ((myGroup = FindGroup(group.strGroupName)) != NULL)
    {
        std::vector<ZatChannel>::iterator it;
        for (it = myGroup->channels.begin(); it != myGroup->channels.end(); ++it)
        {
            ZatChannel &channel = (*it);
            PVR_CHANNEL_GROUP_MEMBER xbmcGroupMember;
            memset(&xbmcGroupMember, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));

            strncpy(xbmcGroupMember.strGroupName, group.strGroupName, sizeof(xbmcGroupMember.strGroupName) - 1);
            xbmcGroupMember.iChannelUniqueId = channel.iUniqueId;
            xbmcGroupMember.iChannelNumber   = channel.iChannelNumber;

            PVR->TransferChannelGroupMember(handle, &xbmcGroupMember);
        }
    }

    return PVR_ERROR_NO_ERROR;
}

int ZatData::GetChannelsAmount(void) {
    return channelsByCid.size();
}

PVR_ERROR ZatData::GetChannels(ADDON_HANDLE handle, bool bRadio) {
    std::vector<PVRZattooChannelGroup>::iterator it;
    for (it = channelGroups.begin(); it != channelGroups.end(); ++it)
    {
        std::vector<ZatChannel>::iterator it2;
        for (it2 = it->channels.begin(); it2 != it->channels.end(); ++it2)
        {
            ZatChannel &channel = (*it2);

            PVR_CHANNEL kodiChannel;
            memset(&kodiChannel, 0, sizeof(PVR_CHANNEL));

            kodiChannel.iUniqueId         = channel.iUniqueId;
            kodiChannel.bIsRadio          = false;
            kodiChannel.iChannelNumber    = channel.iChannelNumber;
            strncpy(kodiChannel.strChannelName, channel.name.c_str(), sizeof(kodiChannel.strChannelName) - 1);
            kodiChannel.iEncryptionSystem = 0;

            ostringstream iconStream;
            iconStream << "special://home/addons/pvr.zattoo/resources/media/channel_logo/" << channel.cid << ".png";
            std::string iconPath = iconStream.str();
            if (!XBMC->FileExists(iconPath.c_str(), true)) {
              XBMC->Log(LOG_INFO, "No logo found for channel '%s'. Fallback to Zattoo-Logo.", channel.cid.c_str());
              iconPath = channel.strLogoPath;
            }

            strncpy(kodiChannel.strIconPath, iconPath.c_str(), sizeof(kodiChannel.strIconPath) - 1);


            kodiChannel.bIsHidden         = false;




            // self referencing so GetLiveStreamURL() gets triggered
            std::string streamURL;
            streamURL = ("pvr://stream/tv/zattoo.ts");
            strncpy(kodiChannel.strStreamURL, streamURL.c_str(), sizeof(kodiChannel.strStreamURL) - 1);
            //


            PVR->TransferChannelEntry(handle, &kodiChannel);


        }
    }
    return PVR_ERROR_NO_ERROR;
}


std::string ZatData::GetChannelStreamUrl(int uniqueId) {

    ZatChannel *channel = FindChannel(uniqueId);
    //XBMC->QueueNotification(QUEUE_INFO, "Getting URL for channel %s", XBMC->UnknownToUTF8(channel->name.c_str()));

    ostringstream dataStream;
    dataStream << "cid=" << channel->cid << "&stream_type=" << streamType << "&format=json";

    string jsonString = HttpPost(zattooServer + "/zapi/watch", dataStream.str());

    yajl_val json = JsonParser::parse(jsonString);
    if (json == NULL || !JsonParser::getBoolean(json, 1, "success")){
      yajl_tree_free(json);
      return "";
    }
    string url = JsonParser::getString(json, 2, "stream", "url");
    yajl_tree_free(json);
    return url;

}


ZatChannel *ZatData::FindChannel(int uniqueId) {
    std::vector<PVRZattooChannelGroup>::iterator it;
    for (it = channelGroups.begin(); it != channelGroups.end(); ++it)
    {
        std::vector<ZatChannel>::iterator it2;
        for (it2 = it->channels.begin(); it2 != it->channels.end(); ++it2)
        {
            ZatChannel &channel = (*it2);
            if(channel.iUniqueId == uniqueId) {
                return &channel;
            }
        }
    }
    return NULL;
}


int ZatData::findChannelNumber(int uniqueId) {
    ZatChannel *channel = FindChannel(uniqueId);
    return 0;
}

PVR_ERROR ZatData::GetEPGForChannelExternalService(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd) {
  ZatChannel *zatChannel = FindChannel(channel.iUniqueId);
  string cid = zatChannel->cid;
  ostringstream urlStream;
  urlStream << "http://zattoo.buehlmann.net/epg/api/Epg/" << countryCode << "/"
            << powerHash << "/" << cid << "/" << iStart << "/" << iEnd;
  string jsonString = HttpGet(urlStream.str());
  yajl_val json = JsonParser::parse(jsonString);
  if (json == NULL){
    yajl_tree_free(json);
    return PVR_ERROR_SERVER_ERROR;
  }

  for (int i = 0; i < json->u.array.len; ++i) {
    yajl_val program = json->u.array.values[i];
    EPG_TAG tag;
    memset(&tag, 0, sizeof(EPG_TAG));

    tag.iUniqueBroadcastId  = JsonParser::getInt(program, 1, "Id");
    string title = JsonParser::getString(program, 1, "Title");
    tag.strTitle            = title.c_str();
    tag.iChannelNumber      = zatChannel->iChannelNumber;
    tag.startTime           = JsonParser::getTime(program, 1, "StartTime");
    tag.endTime             = JsonParser::getTime(program, 1, "EndTime");
    string description = JsonParser::getString(program, 1, "Description");
    tag.strPlotOutline      = description.c_str();
    tag.strPlot             = description.c_str();
    tag.strOriginalTitle    = NULL;  /* not supported */
    tag.strCast             = NULL;  /* not supported */
    tag.strDirector         = NULL;  /*SA not supported */
    tag.strWriter           = NULL;  /* not supported */
    tag.iYear               = 0;     /* not supported */
    tag.strIMDBNumber       = NULL;  /* not supported */
    string imageUrl = JsonParser::getString(program, 1, "ImageUrl");
    tag.strIconPath         = imageUrl.c_str();
    tag.iParentalRating     = 0;     /* not supported */
    tag.iStarRating         = 0;     /* not supported */
    tag.bNotify             = false; /* not supported */
    tag.iSeriesNumber       = 0;     /* not supported */
    tag.iEpisodeNumber      = 0;     /* not supported */
    tag.iEpisodePartNumber  = 0;     /* not supported */
    string subTitle = JsonParser::getString(program, 1, "Subtitle");
    tag.strEpisodeName      = subTitle.c_str();  /* not supported */
    tag.iFlags              = EPG_TAG_FLAG_UNDEFINED;

    string genreStr = JsonParser::getString(program, 1, "Genre");
    int genre = categories.Category(genreStr.c_str());
    if (genre) {
      tag.iGenreSubType = genre&0x0F;
      tag.iGenreType = genre&0xF0;
    } else {
      tag.iGenreType          = EPG_GENRE_USE_STRING;
      tag.iGenreSubType       = 0;     /* not supported */
      tag.strGenreDescription = genreStr.c_str();
    }

    PVR->TransferEpgEntry(handle, &tag);
  }

  yajl_tree_free(json);

  return PVR_ERROR_NO_ERROR;
}

string ZatData::timeToIsoString(time_t t) {
  char buf[sizeof "2011-10-08T07:07:09Z"];
  strftime(buf, sizeof buf, "%FT%TZ", gmtime(&t));
  return buf;
}

PVR_ERROR ZatData::GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd) {

  if (this->alternativeEpgService) {
    return GetEPGForChannelExternalService(handle, channel, iStart, iEnd);
  }

    ZatChannel *zatChannel = FindChannel(channel.iUniqueId);

    if (iStart > m_iLastStart || iEnd > m_iLastEnd)
    {
        // reload EPG for new time interval only
        // doesn't matter is epg loaded or not we shouldn't try to load it for same interval
        m_iLastStart = iStart;
        m_iLastEnd = iEnd;

        if (!LoadEPG(iStart, iEnd)) {
          XBMC->Log(LOG_NOTICE, "Loading epg faild for channel '%s' from %lu to %lu", channel.strChannelName, iStart, iEnd);
          return PVR_ERROR_SERVER_ERROR;
        }
    }

    std::map<time_t, PVRIptvEpgEntry>* channelEpgCache = epgCache[zatChannel->cid];
    if (channelEpgCache == NULL) {
        return PVR_ERROR_NO_ERROR;
    }
    epgCache.erase(zatChannel->cid);
    for (auto const &entry : *channelEpgCache)
    {
        PVRIptvEpgEntry epgEntry = entry.second;

        EPG_TAG tag;
        memset(&tag, 0, sizeof(EPG_TAG));

        tag.iUniqueBroadcastId  = epgEntry.iBroadcastId;
        tag.strTitle            = epgEntry.strTitle.c_str();
        tag.iChannelNumber      = epgEntry.iChannelId;
        tag.startTime           = epgEntry.startTime;
        tag.endTime             = epgEntry.endTime;
        tag.strPlotOutline      = epgEntry.strPlot.c_str();//epgEntry.strPlotOutline.c_str();
        tag.strPlot             = epgEntry.strPlot.c_str();
        tag.strOriginalTitle    = NULL;  /* not supported */
        tag.strCast             = NULL;  /* not supported */
        tag.strDirector         = NULL;  /*SA not supported */
        tag.strWriter           = NULL;  /* not supported */
        tag.iYear               = 0;     /* not supported */
        tag.strIMDBNumber       = NULL;  /* not supported */
        tag.strIconPath         = epgEntry.strIconPath.c_str();
        tag.iParentalRating     = 0;     /* not supported */
        tag.iStarRating         = 0;     /* not supported */
        tag.bNotify             = false; /* not supported */
        tag.iSeriesNumber       = 0;     /* not supported */
        tag.iEpisodeNumber      = 0;     /* not supported */
        tag.iEpisodePartNumber  = 0;     /* not supported */
        tag.strEpisodeName      = NULL;  /* not supported */
        tag.iFlags              = EPG_TAG_FLAG_UNDEFINED;

        int genre = categories.Category(epgEntry.strGenreString.c_str());
        if (genre) {
          tag.iGenreSubType = genre&0x0F;
          tag.iGenreType = genre&0xF0;
        } else {
          tag.iGenreType          = EPG_GENRE_USE_STRING;
          tag.iGenreSubType       = 0;     /* not supported */
          tag.strGenreDescription = epgEntry.strGenreString.c_str();
        }

        PVR->TransferEpgEntry(handle, &tag);
    }
    delete channelEpgCache;

    return PVR_ERROR_NO_ERROR;
}


bool ZatData::LoadEPG(time_t iStart, time_t iEnd) {


//    iStart -= (iStart % (3600/2)) - 86400; // Do s
//    iEnd = iStart + 3600*3;


    //Do some time magic that the start date is not to far in the past because zattoo doesnt like that
    time_t tempStart = iStart - (iStart % (3600/2)) - 86400;
    time_t tempEnd = tempStart + 3600*5; //Add 5 hours

    while(tempEnd <= iEnd) {
        ostringstream urlStream;
        urlStream << zattooServer + "/zapi/v2/cached/program/power_guide/" << powerHash << "?end=" << tempEnd << "&start=" << tempStart << "&format=json";

        string jsonString = HttpGet(urlStream.str());

        yajl_val json = JsonParser::parse(jsonString);
        
        if (json == NULL || !JsonParser::getBoolean(json, 1, "success")){
          yajl_tree_free(json);
          return false;
        }

        yajl_val channels = JsonParser::getArray(json, 1, "channels");

        //Load the channel groups and channels
        for ( int index = 0; index < channels->u.array.len; ++index ) {
            yajl_val channelItem = channels->u.array.values[index];
            string cid = JsonParser::getString(channelItem, 1, "cid");

            int channelId = GetChannelId(cid.c_str());
            ZatChannel *channel = FindChannel(channelId);

            if (!channel) {
              continue;
            }

            yajl_val programs = JsonParser::getArray(channelItem, 1, "programs");
            for (int i = 0; i < programs->u.array.len; ++i) {
                yajl_val program = programs->u.array.values[i];

                PVRIptvEpgEntry entry;
                entry.strTitle = JsonParser::getString(program, 1, "t");
                entry.startTime = JsonParser::getInt(program, 1, "s");
                entry.endTime = JsonParser::getInt(program, 1, "e");
                entry.iBroadcastId = JsonParser::getInt(program, 1, "id");
                entry.strIconPath = JsonParser::getString(program, 1, "i_url");
                entry.iChannelId = channel->iChannelNumber;
                entry.strPlot = JsonParser::getString(program, 1, "et");

                yajl_val genres = JsonParser::getArray(program, 1, "g");
                for (int genre = 0; genre < genres->u.array.len; genre++) {
                    entry.strGenreString = YAJL_GET_STRING(genres->u.array.values[genre]);
                    break;
                }

                if (epgCache[cid] == NULL) {
                    epgCache[cid] = new map<time_t, PVRIptvEpgEntry>();
                }
                (*epgCache[cid])[entry.startTime] = entry;
            }
        }
        yajl_tree_free(json);
        tempStart = tempEnd;
        tempEnd = tempStart + 3600*5; //Add 5 hours
    }
    return true;
}

void ZatData::SetRecordingPlayCount(const PVR_RECORDING &recording, int count) {
  string recordingId = recording.strRecordingId;
  ZatRecordingData *recData;
  if (recordingsData.find(recordingId) != recordingsData.end()) {
    recData = recordingsData[recordingId];
    recData->playCount = count;
  } else {
    recData = new ZatRecordingData();
    recData->playCount = count;
    recData->recordingId = recordingId;
    recData->lastPlayedPosition = 0;
    recData->stillValid = true;
    recordingsData[recordingId] = recData;
  }

  writeDataJson();
}

void ZatData::SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition) {
  string recordingId = recording.strRecordingId;
  ZatRecordingData *recData;
  if (recordingsData.find(recordingId) != recordingsData.end()) {
    recData = recordingsData[recordingId];
    recData->lastPlayedPosition = lastplayedposition;
  } else {
    recData = new ZatRecordingData();
    recData->playCount = 0;
    recData->recordingId = recordingId;
    recData->lastPlayedPosition = lastplayedposition;
    recData->stillValid = true;
    recordingsData[recordingId] = recData;
  }

  writeDataJson();
}

int ZatData::GetRecordingLastPlayedPosition(const PVR_RECORDING &recording) {
  if (recordingsData.find(recording.strRecordingId) != recordingsData.end()) {
    ZatRecordingData* recData = recordingsData[recording.strRecordingId];
    return recData->lastPlayedPosition;
  }
  return 0;
}

void ZatData::GetRecordings(ADDON_HANDLE handle, bool future) {
  string jsonString = HttpGet(zattooServer + "/zapi/playlist");

  yajl_val json = JsonParser::parse(jsonString);
  
  if (json == NULL || !JsonParser::getBoolean(json, 1, "success")){
    yajl_tree_free(json);
    return;
  }
  
  yajl_val recordings = JsonParser::getArray(json, 1, "recordings");

  time_t current_time;
  time(&current_time);

  for ( int index = 0; index < recordings->u.array.len; ++index ) {
    yajl_val recording = recordings->u.array.values[index];
    int programId = JsonParser::getInt(recording, 1, "program_id");
    ostringstream urlStream;
    urlStream << zattooServer + "/zapi/program/details?program_id=" << programId;
    jsonString = HttpGet(urlStream.str());
    yajl_val detailJson = JsonParser::parse(jsonString);

    if (detailJson == NULL || !JsonParser::getBoolean(detailJson, 1, "success")){
      yajl_tree_free(detailJson);
      continue;
    }

    //genre
    yajl_val genres = JsonParser::getArray(detailJson, 2, "program", "genres");
    //Only get the first genre
    int genre = 0;
    if (genres->u.array.len > 0) {
      string genreName = YAJL_GET_STRING(genres->u.array.values[0]);
      genre = categories.Category(genreName);
    }

    time_t startTime  = JsonParser::getTime(recording, 1, "start");
    if (future && startTime > current_time) {
      PVR_TIMER tag;
      memset(&tag, 0, sizeof(PVR_TIMER));

      tag.iClientIndex = JsonParser::getInt(recording, 1, "id");
      PVR_STRCPY(tag.strTitle, JsonParser::getString(recording, 1, "title").c_str());
      PVR_STRCPY(tag.strSummary, JsonParser::getString(recording, 1, "episode_title").c_str());
      time_t endTime  = JsonParser::getTime(recording, 1, "end");
      tag.startTime = startTime;
      tag.endTime = endTime;
      tag.state = PVR_TIMER_STATE_SCHEDULED;
      tag.iTimerType = 1;
      tag.iEpgUid = JsonParser::getInt(recording, 1, "program_id");
      ZatChannel channel = channelsByCid[JsonParser::getString(recording, 1, "cid").c_str()];
      tag.iClientChannelUid = channel.iUniqueId;
      PVR->TransferTimerEntry(handle, &tag);
      if (updateThread != NULL) {
        updateThread->SetNextRecordingUpdate(startTime);
      }
      if (genre) {
        tag.iGenreSubType = genre&0x0F;
        tag.iGenreType = genre&0xF0;
      }

    } else if (!future && startTime <= current_time) {
      PVR_RECORDING tag;
      memset(&tag, 0, sizeof(PVR_RECORDING));
      tag.bIsDeleted = false;

      PVR_STRCPY(tag.strRecordingId, to_string(JsonParser::getInt(recording, 1, "id")).c_str());
      PVR_STRCPY(tag.strTitle, JsonParser::getString(recording, 1, "title").c_str());
      PVR_STRCPY(tag.strEpisodeName, JsonParser::getString(recording, 1, "episode_title").c_str());
      PVR_STRCPY(tag.strPlot, JsonParser::getString(detailJson, 2, "program", "description").c_str());
      PVR_STRCPY(tag.strIconPath, JsonParser::getString(recording, 1, "image_url").c_str());
      ZatChannel channel = channelsByCid[JsonParser::getString(recording, 1, "cid").c_str()];
      tag.iChannelUid = channel.iUniqueId;
      PVR_STRCPY(tag.strChannelName, channel.name.c_str());
      time_t endTime  = JsonParser::getTime(recording, 1, "end");
      tag.recordingTime = startTime;
      tag.iDuration = endTime -  startTime;
      PVR_STRCPY(tag.strStreamURL, GetRecordingStreamUrl(tag.strRecordingId).c_str());

      if (genre) {
        tag.iGenreSubType = genre&0x0F;
        tag.iGenreType = genre&0xF0;
      }

      if (recordingsData.find(tag.strRecordingId) != recordingsData.end()) {
        ZatRecordingData* recData = recordingsData[tag.strRecordingId];
        tag.iPlayCount = recData->playCount;
        tag.iLastPlayedPosition = recData->lastPlayedPosition;
        recData->stillValid = true;
      }

      PVR->TransferRecordingEntry(handle, &tag);
    }
    yajl_tree_free(detailJson);
  }
  yajl_tree_free(json);
}

int ZatData::GetRecordingsAmount(bool future) {
  string jsonString = HttpGet(zattooServer + "/zapi/playlist");

  time_t current_time;
  time(&current_time);

  yajl_val json = JsonParser::parse(jsonString);
  
  if (json == NULL || !JsonParser::getBoolean(json, 1, "success")){
    yajl_tree_free(json);
    return 0;
  }
  
  yajl_val recordings = JsonParser::getArray(json, 1, "recordings");

  int count = 0;
  for ( int index = 0; index < recordings->u.array.len; ++index ) {
    yajl_val recording = recordings->u.array.values[index];
    time_t startTime  = JsonParser::getTime(recording, 1, "start");
    if (future == startTime > current_time) {
      count++;
    }
  }
  yajl_tree_free(json);
  return count;
}

std::string ZatData::GetRecordingStreamUrl(string recordingId) {
    ostringstream dataStream;
    dataStream << "recording_id=" << recordingId <<"&stream_type=" << streamType;

    string jsonString = HttpPost(zattooServer + "/zapi/watch", dataStream.str());

    yajl_val json = JsonParser::parse(jsonString);
    
    if (json == NULL || !JsonParser::getBoolean(json, 1, "success")){
      yajl_tree_free(json);
      return "";
    }
    
    string url = JsonParser::getString(json, 2, "stream", "url");
    yajl_tree_free(json);
    return url;

}

bool ZatData::Record(int programId) {
  ostringstream dataStream;
  dataStream << "program_id=" << programId;

  string jsonString = HttpPost(zattooServer + "/zapi/playlist/program", dataStream.str());
  yajl_val json = JsonParser::parse(jsonString);
  bool ret = json != NULL && JsonParser::getBoolean(json, 1, "success");
  yajl_tree_free(json);
  return ret;
}

bool ZatData::DeleteRecording(string recordingId) {
  ostringstream dataStream;
  dataStream << "recording_id=" << recordingId <<"";

  string jsonString = HttpPost(zattooServer + "/zapi/playlist/remove", dataStream.str());

  yajl_val json = JsonParser::parse(jsonString);
  bool ret = json != NULL && JsonParser::getBoolean(json, 1, "success");
  yajl_tree_free(json);
  return ret;
}

bool ZatData::IsPlayable(const EPG_TAG &tag) {
  if (!recallEnabled) {
    return false;
  }
  time_t current_time;
  time(&current_time);
  return ((current_time - tag.endTime) < maxRecallSeconds) && (tag.startTime < current_time);
}

string ZatData::GetEpgTagUrl(const EPG_TAG &tag) {
  ostringstream dataStream;
  ZatChannel channel = channelsByNumber[tag.iChannelNumber];
  char timeStart[sizeof "2011-10-08T07:07:09Z"];
  strftime(timeStart, sizeof timeStart, "%FT%TZ", gmtime(&tag.startTime));
  char timeEnd[sizeof "2011-10-08T07:07:09Z"];
  strftime(timeEnd, sizeof timeEnd, "%FT%TZ", gmtime(&tag.endTime));

  dataStream << "cid=" << channel.cid << "&start=" << timeStart << "&end=" << timeEnd << "&stream_type=" << streamType;

  string jsonString = HttpPost(zattooServer + "/zapi/watch", dataStream.str());

  yajl_val json = JsonParser::parse(jsonString);
  string url = JsonParser::getString(json, 2, "stream", "url");
  yajl_tree_free(json);
  return url;
}
