#include <iostream>
#include <string>
#include "ZatData.h"
#include "yajl/yajl_tree.h"
#include <sstream>
#include "../lib/tinyxml2/tinyxml2.h"
#include "p8-platform/sockets/tcp.h"
#include "p8-platform/util/timeutils.h"
#include <map>
#include <time.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
 #pragma comment(lib, "ws2_32.lib")
  #include <stdio.h>
 #include <stdlib.h>
#endif


#define DEBUG

#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif



using namespace ADDON;
using namespace std;


static const string pzuid_cookie_file = "special://profile/addon_data/pvr.zattoo/pzuid_cookie";

string ZatData::HttpReq(const cpr::Url &url, bool checkSession) {
  return HttpReq(url, nullptr, checkSession);
}

string ZatData::HttpReq(const cpr::Url &url, const cpr::Payload* const postData, bool checkSession) {
  cpr::Response res;

  session.SetUrl(url);

  if (postData) {
    session.SetPayload(*postData);
    res = session.Post();
  }
  else {
    res = session.Get();
  }

  XBMC->Log(LOG_DEBUG, "Req status: %lu, Req url: %s", res.status_code, XBMC->UnknownToUTF8(res.url.c_str()));

  if (pzuidCookie.empty() && !res.cookies["pzuid"].empty()) {
    pzuidCookie = res.cookies["pzuid"];
  }

  if (res.status_code < 400 && res.status_code != 0) {
    return res.text;
  }
  else if (res.status_code == 403 && checkSession && renewSession()) {
    return HttpReq(url, postData, false);
  }
  else if (res.status_code == 0) {
    XBMC->Log(LOG_DEBUG, "Sleeping 100ms");
    usleep(100 * 1000);
    return HttpReq(url, postData);
  }

  return "";
}

void ZatData::saveSession() {
  if (!pzuidCookie.empty()) {
    void *file = XBMC->OpenFileForWrite(pzuid_cookie_file.c_str(), true);
    XBMC->WriteFile(file, pzuidCookie.c_str(), pzuidCookie.length());
    XBMC->CloseFile(file);
    XBMC->Log(LOG_DEBUG, "Saved pzuid cookie %s", XBMC->UnknownToUTF8(pzuidCookie.c_str()));

  }
}

bool ZatData::renewSession() {
  if (!sendHello(true)) {
    return login();
  }
  return true;
}

bool ZatData::loadCookieFromFile() {
  void* file;
  char buf[1025];
  size_t nbRead;

  file = XBMC->OpenFile(pzuid_cookie_file.c_str(), 0);
  if (file) {
    while ((nbRead = XBMC->ReadFile(file, buf, 1024)) > 0 && ~nbRead) {
      buf[nbRead] = 0x0;
      pzuidCookie += buf;
    }
    XBMC->CloseFile(file);

    if (!pzuidCookie.empty()) {
      session.SetCookies(cpr::Cookies{{"pzuid", pzuidCookie}});
      XBMC->Log(LOG_DEBUG, "Loaded pzuid cookie: %s", XBMC->UnknownToUTF8(pzuidCookie.c_str()));
      return true;
    }
  }

  return false;
}

bool ZatData::sendHello(bool checkLogin) {
  XBMC->Log(LOG_DEBUG, "Send hello.");

  cpr::Payload payload = cpr::Payload{{"uuid", "69ded3aa-67e0-424e-a42b-bd501e1af5fa"}, {"lang", "en"}, {"format", "json"}, {"app_tid", "3ce85a52-9fbd-499f-8693-2bda083aab81"}, {"app_version", "0.2.0"}, {"bundle_id", "pvr.zattoo"}, {"device_type", "Kodi"}};
  string jsonString = HttpReq(cpr::Url{"http://zattoo.com/zapi/session/hello"}, &payload, false);
  yajl_val json = JsonParser::parse(jsonString);

  if (json != NULL && JsonParser::getBoolean(json, 1, "success")) {
    XBMC->Log(LOG_DEBUG, "Hello was successful-");
    if (checkLogin) {
      bool result = JsonParser::getBoolean(json, 2, "session" , "loggedin");
      yajl_tree_free(json);
      return result;
    }
    return true;
  } else {
    yajl_tree_free(json);
    XBMC->Log(LOG_NOTICE, "Hello failed.");
    return false;
  }
}

bool ZatData::login() {
  XBMC->Log(LOG_DEBUG, "Try to login.");

  cpr::Payload payload = cpr::Payload{{"login", username}, {"remember", "True"}, {"password", password}, {"format", "json"}};
  string jsonString = HttpReq(cpr::Url{"http://zattoo.com/zapi/account/login"}, &payload, false);
  yajl_val json = JsonParser::parse(jsonString);

  if (json == NULL || !JsonParser::getBoolean(json, 1, "success")){
    yajl_tree_free(json);
    XBMC->Log(LOG_NOTICE, "Login failed.");
    return false;
  }

  yajl_tree_free(json);
  XBMC->Log(LOG_DEBUG, "Login was successful.");
  saveSession();
  return true;
}

bool ZatData::initSession() {

  string jsonString = HttpReq(cpr::Url{"http://zattoo.com/zapi/v2/session"});
  yajl_val json = JsonParser::parse(jsonString);
  if(json == NULL || !JsonParser::getBoolean(json, 1, "success") || !JsonParser::getBoolean(json, 2, "session" , "loggedin")) {
    yajl_tree_free(json);
    XBMC->Log(LOG_DEBUG, "Initialize session failed");
    return false;
  }

  recallEnabled = streamType == "dash" && JsonParser::getBoolean(json, 2, "session" , "recall_eligible");
  recordingEnabled = JsonParser::getBoolean(json, 2, "session" , "recording_eligible");
  if (recallEnabled) {
    maxRecallSeconds = JsonParser::getInt(json, 2, "session" , "recall_seconds");
  }
  powerHash = JsonParser::getString(json, 2, "session" , "power_guide_hash");
  yajl_tree_free(json);
  return true;
}


yajl_val ZatData::loadFavourites() {

    string jsonString = HttpReq(cpr::Url{"http://zattoo.com/zapi/channels/favorites"});
    yajl_val json = JsonParser::parse(jsonString);

    if (json == NULL || !JsonParser::getBoolean(json, 1, "success")){
      yajl_tree_free(json);
      return NULL;
    }

    return json;
}



bool ZatData::loadChannels() {

    std::map<std::string, ZatChannel> allChannels;
    yajl_val favsJson = loadFavourites();

    if (favsJson == NULL) {
        return false;
    }

    yajl_val favs = JsonParser::getArray(favsJson, 1, "favorites");

    ostringstream urlStream;
    urlStream << "http://zattoo.com/zapi/v2/cached/channels/" << powerHash << "?details=False";
    string jsonString = HttpReq(cpr::Url{urlStream.str()});

    yajl_val json = JsonParser::parse(jsonString);

    if (json == NULL || !JsonParser::getBoolean(json, 1, "success")){
        std::cout  << "Failed to parse configuration\n";
        yajl_tree_free(json);
        yajl_tree_free(favsJson);
        return false;
    }

    channelNumber = favs->u.array.len + 1;
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

ZatData::ZatData(UpdateThread *updateThread, std::string u, std::string p, bool favoritesOnly)  {
  username = u;
  password = p;
  this->updateThread = updateThread;
  this->favoritesOnly = favoritesOnly;
  m_iLastStart = 0;
  m_iLastEnd = 0;
  streamType = "hls";
}

ZatData::~ZatData() {
    channelGroups.clear();

}

bool ZatData::Initialize() {

  pzuidCookie = "";

  if (!loadCookieFromFile()) {
    sendHello();
    login();
  }
  else {
    renewSession();
  }

  if (initSession() && loadChannels()) {
    return true;
  }

  return false;
}

void ZatData::GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities) {
  pCapabilities->bSupportsRecordings      = recordingEnabled;
  pCapabilities->bSupportsTimers          = recordingEnabled;
}

void *ZatData::Process(void) {
    return NULL;
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

    cpr::Payload payload = cpr::Payload{{"cid", channel->cid}, {"stream_type", streamType}, {"format", "json"}};
    string jsonString = HttpReq(cpr::Url{"http://zattoo.com/zapi/watch"}, &payload);

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

PVR_ERROR ZatData::GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd) {

    ZatChannel *zatChannel = FindChannel(channel.iUniqueId);

    if (iStart > m_iLastStart + 60 || iEnd > m_iLastEnd + 60)
    {
        // reload EPG for new time interval only (with 1 min buffer)
        // doesn't matter is epg loaded or not we shouldn't try to load it for same interval
        m_iLastStart = iStart;
        m_iLastEnd = iEnd;

        if (!LoadEPG(iStart, iEnd)) {
          XBMC->Log(LOG_NOTICE, "Loading epg faild for channel '%s' from %lu to %lu", channel.strChannelName, iStart, iEnd);
          return PVR_ERROR_SERVER_ERROR;
        }
    }


    std::vector<PVRIptvEpgEntry>::iterator it;
    for (it = zatChannel->epg.begin(); it != zatChannel->epg.end(); ++it)
    {
        PVRIptvEpgEntry &epgEntry = (*it);

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
//        if (FindEpgGenre(myTag->strGenreString, iGenreType, iGenreSubType))
//        {
//            tag.iGenreType          = iGenreType;
//            tag.iGenreSubType       = iGenreSubType;
//            tag.strGenreDescription = NULL;
//        }
//        else
//        {
            tag.iGenreType          = EPG_GENRE_USE_STRING;
            tag.iGenreSubType       = 0;     /* not supported */
            tag.strGenreDescription = epgEntry.strGenreString.c_str();
//        }
        tag.iParentalRating     = 0;     /* not supported */
        tag.iStarRating         = 0;     /* not supported */
        tag.bNotify             = false; /* not supported */
        tag.iSeriesNumber       = 0;     /* not supported */
        tag.iEpisodeNumber      = 0;     /* not supported */
        tag.iEpisodePartNumber  = 0;     /* not supported */
        tag.strEpisodeName      = NULL;  /* not supported */
        tag.iFlags              = EPG_TAG_FLAG_UNDEFINED;

        PVR->TransferEpgEntry(handle, &tag);

    }


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
        urlStream << "http://zattoo.com/zapi/v2/cached/program/power_guide/" << powerHash << "?end=" << tempEnd << "&start=" << tempStart << "&format=json";

        string jsonString = HttpReq(cpr::Url{urlStream.str()});

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
            yajl_val programs = JsonParser::getArray(channelItem, 1, "programs");
            for (int i = 0; i < programs->u.array.len; ++i) {
                yajl_val program = programs->u.array.values[i];
                int channelId = GetChannelId(cid.c_str());
                ZatChannel *channel = FindChannel(channelId);

                if (!channel) {
                    continue;
                }

                PVRIptvEpgEntry entry;
                entry.strTitle = JsonParser::getString(program, 1, "t");
                entry.startTime = JsonParser::getInt(program, 1, "s");
                entry.endTime = JsonParser::getInt(program, 1, "e");
                entry.iBroadcastId = JsonParser::getInt(program, 1, "id");
                entry.strIconPath = JsonParser::getString(program, 1, "i_url");
                entry.iChannelId = channel->iChannelNumber;
                entry.strPlot = JsonParser::getString(program, 1, "et");

                yajl_val genres = JsonParser::getArray(program, 1, "g");
                ostringstream generesStream;
                for (int genre = 0; genre < genres->u.array.len; genre++) {
                    string genereName = YAJL_GET_STRING(genres->u.array.values[genre]);
                    generesStream << genereName << " ";
                }
                entry.strGenreString = generesStream.str();

                if (channel)
                    channel->epg.insert(channel->epg.end(), entry);
            }
        }
        yajl_tree_free(json);
        tempStart = tempEnd;
        tempEnd = tempStart + 3600*5; //Add 5 hours
    }
    return true;
}

void ZatData::GetRecordings(ADDON_HANDLE handle, bool future) {
  string jsonString = HttpReq(cpr::Url{"http://zattoo.com/zapi/playlist"});

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

      updateThread->SetNextRecordingUpdate(startTime);
    } else if (!future && startTime <= current_time) {
      PVR_RECORDING tag;
      memset(&tag, 0, sizeof(PVR_RECORDING));
      tag.bIsDeleted = false;

      PVR_STRCPY(tag.strRecordingId, to_string(JsonParser::getInt(recording, 1, "id")).c_str());
      PVR_STRCPY(tag.strTitle, JsonParser::getString(recording, 1, "title").c_str());
      PVR_STRCPY(tag.strEpisodeName, JsonParser::getString(recording, 1, "episode_title").c_str());
      PVR_STRCPY(tag.strIconPath, JsonParser::getString(recording, 1, "image_url").c_str());
      time_t endTime  = JsonParser::getTime(recording, 1, "end");
      tag.recordingTime = startTime;
      tag.iDuration = endTime -  startTime;
      PVR_STRCPY(tag.strStreamURL, GetRecordingStreamUrl(tag.strRecordingId).c_str());
      PVR->TransferRecordingEntry(handle, &tag);
    }
  }
  yajl_tree_free(json);
}

int ZatData::GetRecordingsAmount(bool future) {
  string jsonString = HttpReq(cpr::Url{"http://zattoo.com/zapi/playlist"});

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

    cpr::Payload payload = cpr::Payload{{"recording_id", recordingId}, {"stream_type", streamType}, {"format", "json"}};
    string jsonString = HttpReq(cpr::Url{"http://zattoo.com/zapi/watch"}, &payload);

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
  cpr::Payload payload = cpr::Payload{{"program_id", programId}, {"format", "json"}};
  string jsonString = HttpReq(cpr::Url{"http://zattoo.com/zapi/playlist/program"}, &payload);

  yajl_val json = JsonParser::parse(jsonString);
  bool ret = json != NULL && JsonParser::getBoolean(json, 1, "success");
  yajl_tree_free(json);
  return ret;
}

bool ZatData::DeleteRecording(string recordingId) {
  cpr::Payload payload = cpr::Payload{{"recording_id", recordingId}, {"format", "json"}};
  string jsonString = HttpReq(cpr::Url{"http://zattoo.com/zapi/playlist/remove"}, &payload);

  yajl_val json = JsonParser::parse(jsonString);
  bool ret = json != NULL && JsonParser::getBoolean(json, 1, "success");
  yajl_tree_free(json);
  return ret;
}
