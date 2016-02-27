#include <iostream>
#include "ZatData.h"
#include <sstream>
#include <regex>
#include "../lib/tinyxml2/tinyxml2.h"
#include <json/json.h>
#include "platform/sockets/tcp.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
 #pragma comment(lib, "ws2_32.lib")
#endif


using namespace ADDON;
using namespace std;


void ZatData::sendHello() {

    ostringstream dataStream;
    dataStream << "uuid=888b4f54-c127-11e5-9912-ba0be0483c18&lang=en&format=json&client_app_token=" << appToken;
    string data = dataStream.str();

    httpResponse resp = postRequest("/zapi/session/hello", data);
}

bool ZatData::login() {


    ostringstream dataStream;
    dataStream << "login=" << username << "&password=" << password << "&format=json";

    httpResponse response = postRequest("/zapi/account/login", dataStream.str());

    std::string jsonString = response.body;
    XBMC->Log(LOG_DEBUG, "Login result: %s", XBMC->UnknownToUTF8(jsonString.c_str()));




    Json::Value json;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(jsonString,json);

    if (!parsingSuccessful){
        // report to the user the failure and their locations in the document.
        std::cout  << "Failed to parse login\n"
        << reader.getFormatedErrorMessages();
        return false;
    }

    string succ = json["success"].asString();
    if(succ == "False") {
        return false;
    }
    powerHash = json["account"]["power_guide_hash"].asString();
    cout << "Power Hash: " << powerHash << endl;
    return true;
}



void ZatData::loadAppId() {
    appToken = "";

    httpResponse resp = getRequest("/");

    std::string html = resp.body;

    std::smatch m;
    std::regex e ("appToken.*\\'(.*)\\'");

    std::string token = "";

    if (std::regex_search(html, m, e)) {
        token = m[1];
    }

    cout << token << endl;

    appToken = token;

    XBMC->Log(LOG_DEBUG, "Loaded App token %s", XBMC->UnknownToUTF8(appToken.c_str()));

}



void ZatData::loadChannels() {



    ostringstream urlStream;
    urlStream << "/zapi/v2/cached/channels/" << powerHash << "?details=false";
    string url = urlStream.str();

    httpResponse response = getRequest(url);



    std::string jsonString = response.body;

    Json::Value json;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(jsonString,json);



    if (!parsingSuccessful){
        // report to the user the failure and their locations in the document.
        std::cout  << "Failed to parse configuration\n"
        << reader.getFormatedErrorMessages();
        return;
    }


    channelNumber = 1;

    Json::Value groups = json["channel_groups"];

    //Load the channel groups and channels
    for ( int index = 0; index < groups.size(); ++index ) {
        PVRZattooChannelGroup group;
        group.name = groups[index]["name"].asString();



        Json::Value channels = groups[index]["channels"];

        for(int i = 0; i < channels.size(); ++i) {
            ZatChannel channel;

            channel.name = channels[i]["title"].asString();

            channel.strStreamURL = "";
            cout << channel.name << endl;


            std::string cid =channels[i]["cid"].asString(); //returns std::size_t


            channel.iUniqueId = GetChannelId(cid.c_str());
            channel.cid = cid;
            channel.iChannelNumber = ++channelNumber;


            channel.strLogoPath = "http://logos.zattic.com";

            Json::Value qualities = channels[i]["qualities"];

            int index = 0;
            channel.strLogoPath.append(qualities[index]["logo_white_84"].asString());


            if(qualities[index]["availability"].asString() == "available") {
                group.channels.insert(group.channels.end(), channel);
            }
        }
        if(group.channels.size() > 0)
            channelGroups.insert(channelGroups.end(),group);
    }


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

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

httpResponse ZatData::postRequest(std::string url, std::string params) {
    PLATFORM::CTcpSocket *socket = new PLATFORM::CTcpSocket("zattoo.com",80);
    socket->Open(1000);

    ostringstream dataStream;
    dataStream << "POST " << url << " HTTP/1.1\r\n";
    string data = dataStream.str();
    socket->Write(&data[0], data.size());

    char line[256];
    sprintf(line, "Host: zattoo.com\r\n");
    socket->Write(line, strlen(line));

    sprintf(line, "Connection: close\r\n");
    socket->Write(line, strlen(line));

    dataStream.str( std::string() );
    dataStream.clear();
    dataStream << "Content-Length: " << params.size() << "\r\n";
    data = dataStream.str();
    socket->Write(&data[0], data.size());

    dataStream.str( std::string() );
    dataStream.clear();
    dataStream << "Cookie:" << cookie << "\r\n";
    data = dataStream.str();
    socket->Write(&data[0], data.size());


    dataStream.str( std::string() );
    dataStream.clear();
    dataStream << "Content-Type: application/x-www-form-urlencoded\r\n";
    data = dataStream.str();
    socket->Write(&data[0], data.size());

    sprintf(line, "\r\n");
    socket->Write(line, strlen(line));

    dataStream.str( std::string() );
    dataStream.clear();
    dataStream << params;
    data = dataStream.str();
    socket->Write(&data[0], data.size());


    char buf[BUFSIZ];
    ostringstream stream;

    while(socket->Read(buf, sizeof buf, 0) > 0) {
        stream << buf;
        buf[BUFSIZ] = '\0';
        buf[BUFSIZ-1] = '\0';
        buf[BUFSIZ-2] = '\0';
        buf[BUFSIZ-3] = '\0';
        buf[BUFSIZ-4] = '\0';

        memset(buf, 0, BUFSIZ);
    };
    socket->Close();



    //cout << stream.str() << endl;


    string streamStr = stream.str();



    //strstr(streamStr.c_str(),)
    std::size_t found = streamStr.find("\r\n\r\n");

    string header = streamStr.substr (0,found);
    string body = streamStr.substr(found+1,streamStr.size());


    cout << "HEADER:" << endl;
    cout << header << endl;


    //Try to find cookie
    std::smatch m;
    std::regex e ("Set-Cookie:(.*)\r\n");
    std::string token = "";
    if (std::regex_search(header, m, e)) {
        cookie = m[1];
    }

    cout << "BODY:" << endl;
    cout << body << endl;


    httpResponse response;
    response.body = body;



    return response;
}


httpResponse ZatData::getRequest(std::string url) {
    PLATFORM::CTcpSocket *socket = new PLATFORM::CTcpSocket("zattoo.com",80);
    socket->Open(1000);

    ostringstream dataStream;
    dataStream << "GET " << url << " HTTP/1.1\r\n";
    string data = dataStream.str();
    socket->Write(&data[0], data.size());

    socket->Write(&data[0], data.size());


    char line[256];
    sprintf(line, "Host: zattoo.com\r\n");
    socket->Write(line, strlen(line));

    dataStream.str( std::string() );
    dataStream.clear();
    dataStream << "Cookie:" << cookie << "\r\n";
    data = dataStream.str();
    socket->Write(&data[0], data.size());

    sprintf(line, "Connection: close\r\n");
    socket->Write(line, strlen(line));

    sprintf(line, "\r\n");
    socket->Write(line, strlen(line));


    char buf[BUFSIZ];

    ostringstream stream;



    while(socket->Read(buf, sizeof buf, 0) > 0) {
        stream << buf;
        buf[BUFSIZ] = '\0';
        buf[BUFSIZ-1] = '\0';
        buf[BUFSIZ-2] = '\0';
        buf[BUFSIZ-3] = '\0';

        buf[BUFSIZ-4] = '\0';

        memset(buf, 0, BUFSIZ);
    };
    socket->Close();



    //cout << stream.str() << endl;


    string streamStr = stream.str();



    //strstr(streamStr.c_str(),)
    std::size_t found = streamStr.find("\r\n\r\n");

    string header = streamStr.substr (0,found);
    string body = streamStr.substr(found+1,streamStr.size());


    cout << "HEADER:" << endl;
    cout << header << endl;


    //Try to find cookie
    std::smatch m;
    std::regex e ("Set-Cookie:(.*)\r\n");
    std::string token = "";
    if (std::regex_search(header, m, e)) {
        cookie = m[1];
    }

    cout << "BODY:" << endl;
    cout << body << endl;


    httpResponse response;
    response.body = body;



    return response;
}



ZatData::ZatData(std::string u, std::string p)  {
    username = u;
    password = p;
    m_iLastStart    = 0;
    m_iLastEnd      = 0;
    cookie = "";

    cookiePath = GetUserFilePath("zatCookie.txt");


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
    XBMC->QueueNotification(QUEUE_INFO, "Getting URL for channel %s", XBMC->UnknownToUTF8(channel->name.c_str()));

    ostringstream dataStream;
    dataStream << "cid=" << channel->cid << "&stream_type=hls&format=json";
    string data = dataStream.str();


    httpResponse response = postRequest("/zapi/watch", dataStream.str());

    std::string jsonString = response.body;


    Json::Value json;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(jsonString,json);

    cout << json << endl;

    string url = json["stream"]["url"].asString();
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
        tag.strPlotOutline      = "";//epgEntry.strPlotOutline.c_str();
        tag.strPlot             = "";//epgEntry.strPlot.c_str();
        tag.strOriginalTitle    = NULL;  /* not supported */
        tag.strCast             = NULL;  /* not supported */
        tag.strDirector         = NULL;  /* not supported */
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
            tag.strGenreDescription = "Wurst";
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


    iStart -= (iStart % (3600/2)) - 86400;
    iEnd = iStart + 3600*3;


    ostringstream urlStream;
    urlStream << "/zapi/v2/cached/program/power_guide/" << powerHash << "?end=" << iEnd << "&start=" << iStart << "&format=json";

    httpResponse response = getRequest(urlStream.str());



    std::string jsonString = response.body;

    Json::Value json;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(jsonString,json);

    cout << json << endl;

    Json::Value channels = json["channels"];

    //Load the channel groups and channels
    for ( int index = 0; index < channels.size(); ++index ) {

        string cid = channels[index]["cid"].asString();
        for (int i = 0; i < channels[index]["programs"].size(); ++i) {

            Json::Value program = channels[index]["programs"][i];
            int channelId = GetChannelId(cid.c_str());
            ZatChannel *channel = FindChannel(channelId);

            if (!channel) {
                continue;
            }

            PVRIptvEpgEntry entry;
            entry.strTitle = program["t"].asString();
            entry.startTime = program["s"].asInt();
            entry.endTime = program["e"].asInt();
            entry.iBroadcastId = program["id"].asInt();
            entry.strIconPath = program["i_url"].asString();
            entry.iChannelId = channel->iChannelNumber;
            entry.strPlot = program["et"].asString();

            if (channel)
                channel->epg.insert(channel->epg.end(), entry);

            cout << cid << " titel=" << program["t"].asString() << endl;
        }
    }
    return true;
}

