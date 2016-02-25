#include <iostream>
#include "ZatData.h"
#include "curl/curl.h"
#include <sstream>
#include <regex>
#include "../lib/tinyxml2/tinyxml2.h"
#include <json/json.h>
#include <functional>



using namespace ADDON;
using namespace std;


size_t write_data(char *ptr, size_t size, size_t nmemb, void *userdata) {
    std::ostringstream *stream = (std::ostringstream*)userdata;
    size_t count = size * nmemb;
    stream->write(ptr, count);
    return count;
}


void ZatData::sendHello() {
    CURL *curl = curl_easy_init();
    CURLcode res;

    if(curl) {
        std::ostringstream stream;

        curl_easy_setopt(curl, CURLOPT_URL, "http://zattoo.com/zapi/session/hello");
        //Post Data
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        ostringstream dataStream;
        dataStream << "uuid=888b4f54-c127-11e5-9912-ba0be0483c18&lang=en&format=json&client_app_token=" << appToken;
        string data = dataStream.str();

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.length());

        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookiePath.c_str());
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookiePath.c_str());




        //Response handling
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);



        std::string xml = stream.str();
        cout << xml << endl;
    }
}


void dump(const char *text,
          FILE *stream, unsigned char *ptr, size_t size)
{
    size_t i;
    size_t c;
    unsigned int width=0x10;

    XBMC->Log(LOG_DEBUG, "%s, %10.10ld bytes (0x%8.8lx)\n",
            text, (long)size, (long)size);

    for(i=0; i<size; i+= width) {
        XBMC->Log(LOG_DEBUG, "%4.4lx: ", (long)i);

        /* show hex to the left */
        for(c = 0; c < width; c++) {
            if(i+c < size)
                XBMC->Log(LOG_DEBUG, "%02x ", ptr[i+c]);
            else
                XBMC->Log(LOG_DEBUG,"   ");
        }

        /* show data on the right */
        for(c = 0; (c < width) && (i+c < size); c++) {
            char x = (ptr[i+c] >= 0x20 && ptr[i+c] < 0x80) ? ptr[i+c] : '.';
            //XBMC->Log(LOG_DEBUG,x);
        }

        fputc('\n', stream); /* newline */
    }
}

static
int my_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp)
{
    const char *text;
    (void)handle; /* prevent compiler warning */

    switch (type) {
        case CURLINFO_TEXT:
            XBMC->Log(LOG_DEBUG, "== Info: %s", data);
        default: /* in case a new one is introduced to shock us */
            return 0;

//        case CURLINFO_HEADER_OUT:
//            text = "=> Send header";
//            break;
//        case CURLINFO_DATA_OUT:
//            text = "=> Send data";
//            break;
//        case CURLINFO_SSL_DATA_OUT:
//            text = "=> Send SSL data";
//            break;
//        case CURLINFO_HEADER_IN:
//            text = "<= Recv header";
//            break;
//        case CURLINFO_DATA_IN:
//            text = "<= Recv data";
//            break;
//        case CURLINFO_SSL_DATA_IN:
//            text = "<= Recv SSL data";
//            break;
    }

    dump(text, stderr, (unsigned char *)data, size);
    return 0;
}



bool ZatData::login() {
    CURL *curl = curl_easy_init();
    CURLcode res;

    if(curl) {
        std::ostringstream stream;

        curl_easy_setopt(curl, CURLOPT_URL, "http://zattoo.com/zapi/account/login");
        //Post Data
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        ostringstream dataStream;
        dataStream << "login=" << username << "&password=" << password << "&format=json";
        string data = dataStream.str();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.length());


        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookiePath.c_str());
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookiePath.c_str());


        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);

        /* the DEBUGFUNCTION has no effect until we enable VERBOSE */
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);


        //Response handling
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);

        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        CURLcode code = curl_easy_perform(curl);
        XBMC->Log(LOG_DEBUG, "Login status: %s", curl_easy_strerror(code));
        curl_easy_cleanup(curl);

        std::string jsonString = stream.str();
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
    XBMC->QueueNotification(QUEUE_ERROR, "CURL ERROR!!");
    return false;

}



void ZatData::loadAppId() {
    appToken = "";

    CURL *curl = curl_easy_init();
    CURLcode res;

    if(curl) {
        std::ostringstream stream;

        curl_easy_setopt(curl, CURLOPT_URL, "http://zattoo.com");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);



        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);


        std::string html = stream.str();


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

}



void ZatData::loadChannels() {
/*    var promise = new Promise(function(resolve, reject) {
        request.get('http://zattoo.com/zapi/v2/cached/channels/' + user.account.power_guide_hash + "?details=true",
                    function (err,httpResponse, body) {
            resolve(body.channel_groups);
        });
    });*/


    CURL *curl = curl_easy_init();
    CURLcode res;

    if(curl) {
        std::ostringstream stream;

        ostringstream urlStream;
        urlStream << "http://zattoo.com/zapi/v2/cached/channels/" << powerHash << "?details=false&";
        string url = urlStream.str();


        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        //Post Data
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookiePath.c_str());
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookiePath.c_str());

        //Response handling
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        std::string jsonString = stream.str();




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

    cookiePath = GetUserFilePath("zatCookie.txt");


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
/*
    url: 'http://zattoo.com/zapi/watch',
            form: {
        cid: cid,
                stream_type: 'hls'
    }*/

    ZatChannel *channel = FindChannel(uniqueId);
    XBMC->QueueNotification(QUEUE_INFO, "Getting URL for channel %s", XBMC->UnknownToUTF8(channel->name.c_str()));


    CURL *curl = curl_easy_init();
    CURLcode res;

    if(curl) {
        std::ostringstream stream;

        curl_easy_setopt(curl, CURLOPT_URL, "http://zattoo.com/zapi/watch");
        //Post Data
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        ostringstream dataStream;
        dataStream << "cid=" << channel->cid << "&stream_type=hls&format=json";
        string data = dataStream.str();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.length());
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookiePath.c_str());
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookiePath.c_str());

        //Response handling
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        std::string jsonString = stream.str();

        Json::Value json;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(jsonString,json);


        cout << json << endl;

        string url = json["stream"]["url"].asString();
        return url;


/*
        //Get the Power Hash
        tinyxml2::XMLDocument xml;
        xml.Parse(xmlString.c_str());
        std::string url  = xml.RootElement()->FirstChildElement("stream")->FirstChildElement("url")->GetText();

        return url;*/

    }

    return "";
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

    //?end=1456426800&start=1456419600
    CURL *curl = curl_easy_init();
    CURLcode res;

    if(curl) {
        std::ostringstream stream;

        ostringstream urlStream;
        urlStream << "http://zattoo.com/zapi/v2/cached/program/power_guide/" << powerHash << "?end=" << iEnd << "&start=" << iStart << "&format=json";
        string url = urlStream.str();

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        //Post Data
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookiePath.c_str());
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookiePath.c_str());

        //Response handling
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        std::string jsonString = stream.str();

        Json::Value json;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(jsonString,json);



//t=titel s=start e=end

        cout << json << endl;

        Json::Value channels = json["channels"];

        //Load the channel groups and channels
        for ( int index = 0; index < channels.size(); ++index ) {

            string cid = channels[index]["cid"].asString();
            for ( int i = 0; i < channels[index]["programs"].size(); ++i ) {

                Json::Value program = channels[index]["programs"][i];
                int channelId = GetChannelId(cid.c_str());
                ZatChannel *channel = FindChannel(channelId);

                if(!channel) {
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

                if(channel)
                    channel->epg.insert(channel->epg.end(),entry);

                cout << cid << " titel=" << program["t"].asString() << endl;
            }





//            cout
//
//
//            PVRIptvEpgEntry entry;
//            entry.startTime =
        }








    }
    return true;
}
