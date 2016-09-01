#include <iostream>
#include <string>
#include "ZatData.h"
#include <sstream>
#include <regex>
#include "../lib/tinyxml2/tinyxml2.h"
#include "p8-platform/sockets/tcp.h"

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

static const char *to_base64 =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ\
abcdefghijklmnopqrstuvwxyz\
0123456789+/";

std::string ZatData::Base64Encode(unsigned char const* in, unsigned int in_len, bool urlEncode)
{
  std::string ret;
  int i(3);
  unsigned char c_3[3];
  unsigned char c_4[4];

  while (in_len) {
    i = in_len > 2 ? 3 : in_len;
    in_len -= i;
    c_3[0] = *(in++);
    c_3[1] = i > 1 ? *(in++) : 0;
    c_3[2] = i > 2 ? *(in++) : 0;

    c_4[0] = (c_3[0] & 0xfc) >> 2;
    c_4[1] = ((c_3[0] & 0x03) << 4) + ((c_3[1] & 0xf0) >> 4);
    c_4[2] = ((c_3[1] & 0x0f) << 2) + ((c_3[2] & 0xc0) >> 6);
    c_4[3] = c_3[2] & 0x3f;

    for (int j = 0; (j < i + 1); ++j)
    {
      if (urlEncode && to_base64[c_4[j]] == '+')
        ret += "%2B";
      else if (urlEncode && to_base64[c_4[j]] == '/')
        ret += "%2F";
      else
        ret += to_base64[c_4[j]];
    }
  }
  while ((i++ < 3))
    ret += urlEncode ? "%3D" : "=";
  return ret;
}

string ZatData::HttpGet(string url) {
  return HttpPost(url, "");
}

string ZatData::HttpPost(string url, string postData) {
  // open the file
  void* file = XBMC->CURLCreate(url.c_str());
  if (!file)
    return NULL;
  XBMC->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "acceptencoding", "gzip");
  if (postData.size() != 0) {
    string base64 = Base64Encode((const unsigned char *)postData.c_str(), postData.size(), false);
    XBMC->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "postdata", base64.c_str());
  }
  XBMC->CURLOpen(file, 0);

  // read the file
  static const unsigned int CHUNKSIZE = 16384;
  char buf[CHUNKSIZE + 1];
  size_t nbRead;
  string body = "";
  while ((nbRead = XBMC->ReadFile(file, buf, CHUNKSIZE)) > 0 && ~nbRead) {
    buf[nbRead] = 0x0;
    body += buf;
  }

  XBMC->CloseFile(file);

  return body;
}

void ZatData::loadAppId() {

    string html = HttpGet("http://zattoo.com");


    appToken = "";

    std::smatch m;
    std::regex e ("appToken.*\\'(.*)\\'");

    std::string token = "";

    if (std::regex_search(html, m, e)) {
        token = m[1];
    }

    appToken = token;

    XBMC->Log(LOG_DEBUG, "Loaded App token %s", XBMC->UnknownToUTF8(appToken.c_str()));
}

void ZatData::sendHello() {

    ostringstream dataStream;
    dataStream << "uuid=888b4f54-c127-11e5-9912-ba0be0483c18&lang=en&format=json&client_app_token=" << appToken;

    string jsonString = HttpPost("http://zattoo.com/zapi/session/hello", dataStream.str());
}

bool ZatData::login() {


    ostringstream dataStream;
    dataStream << "login=" << username << "&password=" << password << "&format=json";
    string jsonString = HttpPost("http://zattoo.com/zapi/account/login", dataStream.str());

    yajl_val json = JsonParser::parse(jsonString);

    if (json == NULL){
        return false;
    }

    if(!JsonParser::getBoolean(json, 1, "success")) {
        return false;
    }

    powerHash = JsonParser::getString(json, 2, "account" , "power_guide_hash");

    jsonString = HttpGet("http://zattoo.com/zapi/v2/session");
    json = JsonParser::parse(jsonString);
    if(!JsonParser::getBoolean(json, 1, "success")) {
        return false;
    }

    recallEnabled = JsonParser::getBoolean(json, 2, "session" , "recall_eligible");
    recordingEnabled = JsonParser::getBoolean(json, 2, "session" , "recording_eligible");

    return true;
}


yajl_val ZatData::loadFavourites() {

    string jsonString = HttpGet("http://zattoo.com/zapi/channels/favorites");
    yajl_val json = JsonParser::parse(jsonString);

    return JsonParser::getArray(json, 1, "favorites");
}



void ZatData::loadChannels() {


    yajl_val favs = loadFavourites();

    ostringstream urlStream;
    urlStream << "http://zattoo.com/zapi/v2/cached/channels/" << powerHash << "?details=False";
    string jsonString = HttpGet(urlStream.str());

    yajl_val json = JsonParser::parse(jsonString);
    
    if (json == NULL){
        std::cout  << "Failed to parse configuration\n";
        return;
    }

    channelNumber = 1;
    yajl_val groups = JsonParser::getArray(json, 1, "channel_groups");

    PVRZattooChannelGroup favGroup;
    favGroup.name = "Favoriten";
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
                    //Yeah thats bad performance
                    for (int fav = 0; fav < favs->u.array.len; fav++) {
						yajl_val favItem = favs->u.array.values[fav];
						string favCid = YAJL_GET_STRING(favItem);
                        if (favCid == cid) {
                            favGroup.channels.insert(favGroup.channels.end(), channel);
                        }
                    }
                    break;
                }   
            }
        }
        if (group.channels.size() > 0)
            channelGroups.insert(channelGroups.end(),group);
    }

    if (favGroup.channels.size() > 0)
        channelGroups.insert(channelGroups.end(),favGroup);
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

ZatData::ZatData(std::string u, std::string p)  {
    username = u;
    password = p;
    m_iLastStart    = 0;
    m_iLastEnd      = 0;

    //httpResponse response = getRequest("zattoo.com/deinemama");
    //cout << response.body;

    this->loadAppId();
    this->sendHello();
    if(this->login()) {
        this->loadChannels();
    }
    else {
        XBMC->QueueNotification(QUEUE_ERROR, "Zattoo Login fehlgeschlagen!");
    }

}

ZatData::~ZatData() {
    channelGroups.clear();
    
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
    return channelNumber-1;
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


            strncpy(kodiChannel.strIconPath, channel.strLogoPath.c_str(), sizeof(kodiChannel.strIconPath) - 1);


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
    dataStream << "cid=" << channel->cid << "&stream_type=hls&format=json";

    string jsonString = HttpPost("http://zattoo.com/zapi/watch", dataStream.str());

    yajl_val json = JsonParser::parse(jsonString);
    string url = JsonParser::getString(json, 2, "stream", "url");
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

    if (iStart > m_iLastStart || iEnd > m_iLastEnd)
    {
        // reload EPG for new time interval only
        LoadEPG(iStart, iEnd);
        {
            // doesn't matter is epg loaded or not we shouldn't try to load it for same interval
            m_iLastStart = iStart;
            m_iLastEnd = iEnd;
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

    while(tempEnd < iEnd) {
        ostringstream urlStream;
        urlStream << "http://zattoo.com/zapi/v2/cached/program/power_guide/" << powerHash << "?end=" << tempEnd << "&start=" << tempStart << "&format=json";

        string jsonString = HttpGet(urlStream.str());

        yajl_val json = JsonParser::parse(jsonString);

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

        tempStart = tempEnd;
        tempEnd = tempStart + 3600*5; //Add 5 hours
    }





    return true;
}

void ZatData::GetRecordings(ADDON_HANDLE handle, bool future) {
  string jsonString = HttpGet("http://zattoo.com/zapi/playlist");

  yajl_val json = JsonParser::parse(jsonString);
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
      PVR->TransferTimerEntry(handle, &tag);
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
}

int ZatData::GetRecordingsAmount(bool future) {
  string jsonString = HttpGet("http://zattoo.com/zapi/playlist");

  time_t current_time;
  time(&current_time);

  yajl_val json = JsonParser::parse(jsonString);
  yajl_val recordings = JsonParser::getArray(json, 1, "recordings");
  int count = 0;
  for ( int index = 0; index < recordings->u.array.len; ++index ) {
    yajl_val recording = recordings->u.array.values[index];
    time_t startTime  = JsonParser::getTime(recording, 1, "start");
    if (future == startTime > current_time) {
      count++;
    }
  }
  return count;
}

std::string ZatData::GetRecordingStreamUrl(string recordingId) {
    ostringstream dataStream;
    dataStream << "recording_id=" << recordingId <<"&stream_type=hls";

    string jsonString = HttpPost("http://zattoo.com/zapi/watch", dataStream.str());

    yajl_val json = JsonParser::parse(jsonString);
    string url = JsonParser::getString(json, 2, "stream", "url");
    return url;

}

bool ZatData::Record(int programId) {
  ostringstream dataStream;
  dataStream << "program_id=" << programId;

  string jsonString = HttpPost("http://zattoo.com/zapi/playlist/program", dataStream.str());
  yajl_val json = JsonParser::parse(jsonString);
  return JsonParser::getBoolean(json, 1, "success");
}

bool ZatData::DeleteRecording(string recordingId) {
  ostringstream dataStream;
  dataStream << "recording_id=" << recordingId <<"";

  string jsonString = HttpPost("http://zattoo.com/zapi/playlist/remove", dataStream.str());

  yajl_val json = JsonParser::parse(jsonString);
  return JsonParser::getBoolean(json, 1, "success");
}

