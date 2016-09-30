//
// Created by johannes on 04.02.16.
//

#include "client.h"
#include "JsonParser.h"
#include "UpdateThread.h"
#include <map>
#include "../lib/cpr/include/cpr.h"

/*!
 * @brief PVR macros for string exchange
 */
#define PVR_STRCPY(dest, source) do { strncpy(dest, source, sizeof(dest)-1); dest[sizeof(dest)-1] = '\0'; } while(0)
#define PVR_STRCLR(dest) memset(dest, 0, sizeof(dest))

struct PVRIptvEpgEntry
{
    int         iBroadcastId;
    int         iChannelId;
    int         iGenreType;
    int         iGenreSubType;
    time_t      startTime;
    time_t      endTime;
    std::string strTitle;
    std::string strPlotOutline;
    std::string strPlot;
    std::string strIconPath;
    std::string strGenreString;
};

struct PVRIptvEpgChannel
{
    std::string                  strId;
    std::string                  strName;
    std::string                  strIcon;
    std::vector<PVRIptvEpgEntry> epg;
};

struct ZatChannel
{
    bool        bRadio;
    int         iUniqueId;
    int         iChannelNumber;
    int         iEncryptionSystem;
    int         iTvgShift;
    std::string name;
    std::string strLogoPath;
    std::string strStreamURL;
    std::string strTvgId;
    std::string strTvgName;
    std::string strTvgLogo;
    std::string cid;
    std::vector<PVRIptvEpgEntry> epg;
};

struct PVRZattooChannelGroup
{
    std::string name;
    std::vector<ZatChannel> channels;
};

struct PVRIptvEpgGenre
{
    int               iGenreType;
    int               iGenreSubType;
    std::string       strGenre;
};



class ZatData : public P8PLATFORM::CThread
{
public:
    ZatData(UpdateThread *updateThread, std::string username, std::string password, bool favoritesOnly);
    virtual ~ZatData();
    virtual bool Initialize();
    virtual void      GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities);
    virtual int       GetChannelsAmount(void);
    virtual PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio);
    //virtual bool      GetChannel(const PVR_CHANNEL &channel, ZatChannel &myChannel);
    virtual int       GetChannelGroupsAmount(void);
    virtual PVR_ERROR GetChannelGroups(ADDON_HANDLE handle);
    virtual PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group);
    virtual PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd);

    //    virtual void      ReaplyChannelsLogos(const char * strNewPath);
    //    virtual void      ReloadPlayList(const char * strNewPath);
    //    virtual void      ReloadEPG(const char * strNewPath);
    virtual std::string GetChannelStreamUrl(int uniqueId);
    virtual void GetRecordings(ADDON_HANDLE handle, bool future);
    virtual int GetRecordingsAmount(bool future);
    virtual std::string GetRecordingStreamUrl(string recordingId);
    virtual bool Record(int programId);
    virtual bool DeleteRecording(string recordingId);

protected:
    virtual std::string HttpReq(const cpr::Url &url, bool checkSession = true);
    virtual std::string HttpReq(const cpr::Url &url, const cpr::Payload* const postData, bool checkSession = true);

    virtual bool                 LoadEPG(time_t iStart, time_t iEnd);

    virtual ZatChannel*      FindChannel(int uniqueId);
    virtual PVRZattooChannelGroup* FindGroup(const std::string &strName);

    virtual int                  GetChannelId(const char * strChannelName);

protected:
    virtual void *Process(void);

private:
    bool                              m_bTSOverride;
    int                               m_iEPGTimeShift;
    int                               m_iLastStart;
    int                               m_iLastEnd;
    int                               channelNumber;
    std::string                       appToken;
    std::string                       powerHash;
    bool                              recallEnabled;
    bool                              recordingEnabled;
    std::string                       streamType;
    std::string                       username;
    std::string                       password;
    bool                              favoritesOnly;
    std::string                       m_strLogoPath;
    std::vector<PVRZattooChannelGroup> channelGroups;
    std::map<int, ZatChannel>         channelsByNumber;
    std::map<std::string, ZatChannel> channelsByCid;
    int64_t                           maxRecallSeconds;
    UpdateThread *updateThread;
    std::string                       pzuidCookie;
    cpr::Session                      session;

    void saveSession();

    bool renewSession();

    bool loadCookieFromFile();

    bool sendHello(bool checkLogin = false);

    bool login();

    bool initSession();

    bool loadChannels();

    int findChannelNumber(int uniqueId);

    yajl_val loadFavourites();
};
