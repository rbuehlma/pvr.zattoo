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
        dataStream << "uuid=888b4f54-c127-11e5-9912-ba0be0483c18&lang=en&format=xml&client_app_token=" << appToken;
        string data = dataStream.str();

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.length());

        //curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "zatCookie.txt");
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "zatCookie.txt");




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
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "zatCookie.txt");
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "zatCookie.txt");

        //Response handling
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        std::string xmlString = stream.str();

        tinyxml2::XMLDocument xml;
        xml.Parse(xmlString.c_str());

        string succ = xml.RootElement()->FirstChildElement("success")->GetText();
        if(succ == "False") {
            return false;
        }


        cout << xmlString << endl;
        powerHash = xml.RootElement()->FirstChildElement("account")->FirstChildElement("power_guide_hash")->GetText();
        cout << "Power Hash: " << powerHash << endl;
        return true;
    }

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

        XBMC->Log(LOG_DEBUG, "Loaded App token %s", appToken);

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
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "zatCookie.txt");
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "zatCookie.txt");

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

                channel.strStreamURL = "http://video3.smoothhd.com/ondemand/Turner_Sports_PGA.ism/Manifest";


                cout << channel.name << endl;


                std::string cid =channels[i]["cid"].asString(); //returns std::size_t


                channel.iUniqueId = GetChannelId(cid.c_str());
                channel.cid = cid;
                channel.iChannelNumber = ++channelNumber;

                group.channels.insert(group.channels.end(), channel);
            }

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
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "zatCookie.txt");
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "zatCookie.txt");

        //Response handling
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        std::string xmlString = stream.str();




        //Get the Power Hash
        tinyxml2::XMLDocument xml;
        xml.Parse(xmlString.c_str());
        std::string url  = xml.RootElement()->FirstChildElement("stream")->FirstChildElement("url")->GetText();
        cout << "URL " << url << endl;
        return url;

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
