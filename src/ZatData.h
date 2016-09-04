//
// Created by johannes on 04.02.16.
//

#include "client.h"
#include "JsonParser.h"
#include "UpdateThread.h"
#include <map>

/*!
 * @brief PVR macros for string exchange
 */
#define PVR_STRCPY(dest, source) do { strncpy(dest, source, sizeof(dest)-1); dest[sizeof(dest)-1] = '\0'; } while(0)
#define PVR_STRCLR(dest) memset(dest, 0, sizeof(dest))

#define JS_STR(G, STR) do {                                             \
        string _s = (STR);                                         \
        yajl_gen_string(G, (const unsigned char*)_s.c_str(), _s.length());      \
} while (0)

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

struct ZatRecordingData
{
  std::string recordingId;
  int playCount;
  int lastPlayedPosition;
  bool stillValid;
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
    ZatData(std::string username, std::string password, bool favoritesOnly);
    virtual ~ZatData();
    virtual bool Initialize();
    virtual bool LoadChannels();
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
    virtual void SetRecordingPlayCount(const PVR_RECORDING &recording, int count);
    virtual void SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition);
    virtual int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording);
    virtual bool IsPlayable(const EPG_TAG &tag);
    virtual std::string GetEpgTagUrl(const EPG_TAG &tag);

protected:
    virtual std::string Base64Encode(unsigned char const* in, unsigned int in_len, bool urlEncode);
    virtual std::string HttpGet(string url, bool isInit = false);
    virtual std::string HttpPost(string url, string postData, bool isInit = false);

    virtual bool                 LoadEPG(time_t iStart, time_t iEnd);

    virtual ZatChannel*      FindChannel(int uniqueId);
    virtual PVRZattooChannelGroup* FindGroup(const std::string &strName);

    virtual int                  GetChannelId(const char * strChannelName);

protected:
    virtual void *Process(void);

private:
    int                               m_iLastStart;
    int                               m_iLastEnd;
    std::string                       appToken;
    std::string                       powerHash;
    bool                              recallEnabled;
    bool                              recordingEnabled;
    std::string                       streamType;
    std::string                       username;
    std::string                       password;
    bool                              favoritesOnly;
    std::vector<PVRZattooChannelGroup> channelGroups;
    std::map<int, ZatChannel>         channelsByNumber;
    std::map<std::string, ZatChannel> channelsByCid;
    std::map<std::string, ZatRecordingData*> recordingsData;
    int64_t                           maxRecallSeconds;
    UpdateThread *updateThread;
    std::string uuid;
    std::string cookie;

    bool loadAppId();

    bool readDataJson();

    bool writeDataJson();

    string getUUID();

    string generateUUID();

    bool sendHello(string uuid);

    bool login();

    bool initSession();

    int findChannelNumber(int uniqueId);

    yajl_val loadFavourites();
};
