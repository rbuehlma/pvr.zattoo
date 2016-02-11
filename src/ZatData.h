//
// Created by johannes on 04.02.16.
//

#include "client.h"


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
    ZatData(std::string username, std::string password);
    virtual ~ZatData();

    virtual int       GetChannelsAmount(void);
    virtual PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio);
    //virtual bool      GetChannel(const PVR_CHANNEL &channel, ZatChannel &myChannel);
    virtual int       GetChannelGroupsAmount(void);
    virtual PVR_ERROR GetChannelGroups(ADDON_HANDLE handle);
    virtual PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group);
    //    virtual PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd);
    //    virtual void      ReaplyChannelsLogos(const char * strNewPath);
    //    virtual void      ReloadPlayList(const char * strNewPath);
    //    virtual void      ReloadEPG(const char * strNewPath);
    virtual std::string GetChannelStreamUrl(int uniqueId);
protected:

    virtual void loadAppId();




//
//    virtual bool                 LoadPlayList(void);
//    virtual bool                 LoadEPG(time_t iStart, time_t iEnd);
//    virtual bool                 LoadGenres(void);



    virtual ZatChannel*      FindChannel(int uniqueId);
    virtual PVRZattooChannelGroup* FindGroup(const std::string &strName);


//    virtual PVRIptvEpgChannel*   FindEpg(const std::string &strId);
//    virtual PVRIptvEpgChannel*   FindEpgForChannel(ZatChannel &channel);
//    virtual bool                 FindEpgGenre(const std::string& strGenre, int& iType, int& iSubType);
//    virtual int                  ParseDateTime(std::string& strDate, bool iDateFormat = true);
//    virtual bool                 GzipInflate( const std::string &compressedBytes, std::string &uncompressedBytes);
//    virtual int                  GetCachedFileContents(const std::string &strCachedName, const std::string &strFilePath,
//                                                       std::string &strContent, const bool bUseCache = false);
//    virtual void                 ApplyChannelsLogos();
//    virtual void                 ApplyChannelsLogosFromEPG();
//    virtual std::string          ReadMarkerValue(std::string &strLine, const char * strMarkerName);
    virtual int                  GetChannelId(const char * strChannelName);

protected:
    virtual void *Process(void);

private:
    bool                              m_bTSOverride;
    int                               m_iEPGTimeShift;
    int                               m_iLastStart;
    int                               channelNumber;
    std::string                       appToken;
    std::string                       powerHash;
    std::string                       username;
    std::string                       password;
    std::string                       m_strLogoPath;
    std::vector<PVRZattooChannelGroup> channelGroups;

    void sendHello();

    bool login();

    void loadChannels();
};