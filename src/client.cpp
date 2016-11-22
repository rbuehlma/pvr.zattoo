#include "client.h"
#include "ZatData.h"
#include "kodi/xbmc_pvr_dll.h"
#include "kodi/libKODI_guilib.h"
#include <iostream>

using namespace ADDON;

#ifdef TARGET_WINDOWS
#define snprintf _snprintf
#endif

ADDON_STATUS m_CurStatus = ADDON_STATUS_UNKNOWN;
ZatData *zat = NULL;
bool m_bIsPlaying = false;

/* User adjustable settings are saved here.
 * Default values are defined inside client.h
 * and exported to the other source files.
 */
std::string g_strUserPath   = "";
std::string g_strClientPath = "";

CHelper_libXBMC_addon *XBMC = NULL;
CHelper_libXBMC_pvr   *PVR  = NULL;

std::string zatUsername    = "";
std::string zatPassword    = "";
bool      zatFavoritesOnly = false;
int         g_iStartNumber  = 1;
bool        g_bTSOverride   = true;
bool        g_bCacheM3U     = false;
bool        g_bCacheEPG     = false;
int         g_iEPGLogos     = 0;



extern std::string PathCombine(const std::string &strPath, const std::string &strFileName)
{
    std::string strResult = strPath;
    if (strResult.at(strResult.size() - 1) == '\\' ||
        strResult.at(strResult.size() - 1) == '/')
    {
        strResult.append(strFileName);
    }
    else
    {
        strResult.append("/");
        strResult.append(strFileName);
    }

    return strResult;
}


extern std::string GetUserFilePath(const std::string &strFileName)
{
    return PathCombine(g_strUserPath, strFileName);
}

extern "C" {

void ADDON_ReadSettings(void) {
    char buffer[1024];
    bool boolBuffer;
    XBMC->Log(LOG_DEBUG, "Read settings");
    if (XBMC->GetSetting("username", &buffer))
    {
        zatUsername = buffer;
    }
    if (XBMC->GetSetting("password", &buffer))
    {
        zatPassword = buffer;
    }
    if (XBMC->GetSetting("favoritesonly", &boolBuffer))
    {
        zatFavoritesOnly = boolBuffer;
    }
    XBMC->Log(LOG_DEBUG, "End Readsettings");
}

ADDON_STATUS ADDON_Create(void *hdl, void *props) {
    if (!hdl || !props) {
        return ADDON_STATUS_UNKNOWN;
    }

    PVR_PROPERTIES *pvrprops = (PVR_PROPERTIES *) props;

    XBMC = new CHelper_libXBMC_addon;
    XBMC->RegisterMe(hdl);

    if (!XBMC->RegisterMe(hdl)) {
        SAFE_DELETE(XBMC);
        return ADDON_STATUS_PERMANENT_FAILURE;
    }

    PVR = new CHelper_libXBMC_pvr;
    if (!PVR->RegisterMe(hdl)) {
        SAFE_DELETE(PVR);
        SAFE_DELETE(XBMC);
        return ADDON_STATUS_PERMANENT_FAILURE;
    }

    XBMC->Log(LOG_DEBUG, "%s - Creating the PVR Zattoo add-on", __FUNCTION__);

    m_CurStatus = ADDON_STATUS_NEED_SETTINGS;

    g_strClientPath = pvrprops->strClientPath;
    g_strUserPath = pvrprops->strUserPath;

    zatUsername = "";
    zatPassword = "";
    ADDON_ReadSettings();
    if (!zatUsername.empty() && !zatPassword.empty()) {
      XBMC->Log(LOG_DEBUG, "Create Zat");
      zat = new ZatData(zatUsername, zatPassword, zatFavoritesOnly);
      XBMC->Log(LOG_DEBUG, "Zat created");
      if (zat->Initialize() && zat->LoadChannels()) {
        m_CurStatus = ADDON_STATUS_OK;
      } else {
        XBMC->QueueNotification(QUEUE_ERROR, "Zattoo login failed!");
      }
    }

    return m_CurStatus;
}

ADDON_STATUS ADDON_GetStatus() {
    return m_CurStatus;
}

void ADDON_Destroy() {
  SAFE_DELETE(zat);
  m_CurStatus = ADDON_STATUS_UNKNOWN;
}

bool ADDON_HasSettings() {
    return true;
}

unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet) {
    return 0;
}

ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue) {
  string name = settingName;

  if (name == "username") {
    string username = (const char*)settingValue;
    if (username != zatUsername) {
      zatUsername = username;
      return ADDON_STATUS_NEED_RESTART;
    }
  }

  if (name == "password") {
    string password = (const char*)settingValue;
    if (password != zatPassword) {
      zatPassword = password;
      return ADDON_STATUS_NEED_RESTART;
    }
  }

  if (name == "favoritesonly") {
    bool favOnly = *(bool *)settingValue;
    if (favOnly != zatFavoritesOnly) {
      zatFavoritesOnly = favOnly;
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  return !zatUsername.empty() && !zatPassword.empty() ? ADDON_STATUS_OK : ADDON_STATUS_NEED_SETTINGS;
}

void ADDON_Stop() {
}

void ADDON_FreeSettings() {
}

/***********************************************************
 * PVR Client AddOn specific public library functions
 ***********************************************************/


void OnSystemSleep()
{
}

void OnSystemWake()
{
}

void OnPowerSavingActivated()
{
}

void OnPowerSavingDeactivated()
{
}

const char* GetPVRAPIVersion(void)
{
  static const char *strApiVersion = XBMC_PVR_API_VERSION;
  return strApiVersion;
}

const char* GetMininumPVRAPIVersion(void)
{

  static const char *strMinApiVersion = XBMC_PVR_MIN_API_VERSION;
  return strMinApiVersion;
}

const char* GetGUIAPIVersion(void)
{
    return KODI_GUILIB_API_VERSION;
}

const char* GetMininumGUIAPIVersion(void)
{
    return KODI_GUILIB_MIN_API_VERSION;
}

PVR_ERROR GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{
  pCapabilities->bSupportsEPG             = true;
  pCapabilities->bSupportsTV              = true;
  pCapabilities->bSupportsRadio           = true;
  pCapabilities->bSupportsChannelGroups   = true;
  pCapabilities->bSupportsRecordingPlayCount = true;
  pCapabilities->bSupportsLastPlayedPosition = true;

  if (zat) {
    zat->GetAddonCapabilities(pCapabilities);
  }

  return PVR_ERROR_NO_ERROR;
}

const char *GetBackendName(void)
{
  static const char *strBackendName = "Zattoo PVR Add-on";
  return strBackendName;
}

const char *GetBackendVersion(void)
{
  static std::string strBackendVersion = XBMC_PVR_API_VERSION;
  return strBackendVersion.c_str();
}

const char *GetConnectionString(void)
{
  static std::string strConnectionString = "connected";
  return strConnectionString.c_str();
}

const char *GetBackendHostname(void)
{
  return "";
}

PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed)
{
  *iTotal = 0;
  *iUsed  = 0;
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  if (zat)
    return zat->GetEPGForChannel(handle, channel, iStart, iEnd);

  return PVR_ERROR_SERVER_ERROR;
}

int GetChannelsAmount(void)
{
  if (zat)
    return zat->GetChannelsAmount();

  return 0;
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
    if(bRadio)
        return PVR_ERROR_NO_ERROR;

  if (zat)
    return zat->GetChannels(handle, bRadio);

  return PVR_ERROR_NO_ERROR;
}

bool OpenLiveStream(const PVR_CHANNEL &channel)
{

    std::string url = GetLiveStreamURL(channel);

    XBMC->Log(LOG_DEBUG, "Open Livestream URL %s", url.c_str());
    return true;



}

void CloseLiveStream(void)
{

}

int GetCurrentClientChannel(void)
{
  return -1;
}

bool SwitchChannel(const PVR_CHANNEL &channel)
{
  CloseLiveStream();

  return OpenLiveStream(channel);
}

PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES* pProperties)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

int GetChannelGroupsAmount(void)
{
  return zat->GetChannelGroupsAmount();
}

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
    if(bRadio)
        return PVR_ERROR_NO_ERROR;
    if (zat)
        return zat->GetChannelGroups(handle);

    return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{

    if (zat)
        return zat->GetChannelGroupMembers(handle, group);

    return PVR_ERROR_SERVER_ERROR;

}

PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
{
  snprintf(signalStatus.strAdapterName, sizeof(signalStatus.strAdapterName), "Zattoo Adapter 1");
  snprintf(signalStatus.strAdapterStatus, sizeof(signalStatus.strAdapterStatus), "OK");

  return PVR_ERROR_NO_ERROR;
}

const char * GetLiveStreamURL(const PVR_CHANNEL &channel)  {
    return XBMC->UnknownToUTF8(zat->GetChannelStreamUrl(channel.iUniqueId).c_str());
}

/** Recording API **/
int GetRecordingsAmount(bool deleted) {
  if (deleted) {
    return 0;
  }
  if (!zat) {
    return 0;
  }
  return zat->GetRecordingsAmount(false);
}

PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted) {
  if (deleted) {
    return PVR_ERROR_NO_ERROR;
  }
  if (!zat) {
    return PVR_ERROR_SERVER_ERROR;
  }
  zat->GetRecordings(handle, false);
  return PVR_ERROR_NO_ERROR;
}

int GetTimersAmount(void) {
  if (!zat) {
    return 0;
  }
  return zat->GetRecordingsAmount(true);
}

PVR_ERROR GetTimers(ADDON_HANDLE handle) {
  if (!zat) {
    return PVR_ERROR_SERVER_ERROR;
  }
  zat->GetRecordings(handle, true);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR AddTimer(const PVR_TIMER &timer) {
  if (!zat) {
    return PVR_ERROR_SERVER_ERROR;
  }
  if (timer.iEpgUid <= EPG_TAG_INVALID_UID) {
    return PVR_ERROR_REJECTED;
  }
  if (!zat->Record(timer.iEpgUid)) {
    return PVR_ERROR_REJECTED;
  }
  PVR->TriggerTimerUpdate();
  PVR->TriggerRecordingUpdate();
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR DeleteRecording(const PVR_RECORDING &recording) {
  if (!zat) {
    return PVR_ERROR_SERVER_ERROR;
  }
  if (!zat->DeleteRecording(recording.strRecordingId)) {
    return PVR_ERROR_REJECTED;
  }
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete) {
  if (!zat) {
    return PVR_ERROR_SERVER_ERROR;
  }
   if (!zat->DeleteRecording(to_string(timer.iClientIndex))) {
    return PVR_ERROR_REJECTED;
  } 
    
  PVR->TriggerTimerUpdate();
  return PVR_ERROR_NO_ERROR;
}

void addTimerType(PVR_TIMER_TYPE types[], int idx, int attributes) {
  types[idx].iId = idx + 1;
  types[idx].iAttributes = attributes;
  types[idx].iPrioritiesSize = 0;
  types[idx].iLifetimesSize = 0;
  types[idx].iPreventDuplicateEpisodesSize = 0;
  types[idx].iRecordingGroupSize = 0;
  types[idx].iMaxRecordingsSize = 0;
}

PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int *size) {
  addTimerType(types, 0, PVR_TIMER_TYPE_ATTRIBUTE_NONE);
  addTimerType(types, 1, PVR_TIMER_TYPE_IS_MANUAL);
  *size = 2;
  return PVR_ERROR_NO_ERROR;
}



PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count) {
  if (!zat) {
    return PVR_ERROR_FAILED;
  }
  zat->SetRecordingPlayCount(recording, count);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition) {
  if (!zat) {
    return PVR_ERROR_FAILED;
  }
  zat->SetRecordingLastPlayedPosition(recording, lastplayedposition);
  return PVR_ERROR_NO_ERROR;
}

int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording) {
  if (!zat) {
    return -1;
  }

  return zat->GetRecordingLastPlayedPosition(recording);
}



/** UNUSED API FUNCTIONS */
bool CanPauseStream(void) { return false; }
PVR_ERROR OpenDialogChannelScan(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR CallMenuHook(const PVR_MENUHOOK &menuhook, const PVR_MENUHOOK_DATA &item) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR MoveChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
bool OpenRecordedStream(const PVR_RECORDING &recording) { return false; }
void CloseRecordedStream(void) {}
int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
long long SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */) { return 0; }
long long PositionRecordedStream(void) { return -1; }
long long LengthRecordedStream(void) { return 0; }
void DemuxReset(void) {}
void DemuxFlush(void) {}
int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
long long SeekLiveStream(long long iPosition, int iWhence /* = SEEK_SET */) { return -1; }
long long PositionLiveStream(void) { return -1; }
long long LengthLiveStream(void) { return -1; }
PVR_ERROR RenameRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int*) { return PVR_ERROR_NOT_IMPLEMENTED; };
PVR_ERROR UpdateTimer(const PVR_TIMER &timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
void DemuxAbort(void) {}
DemuxPacket* DemuxRead(void) { return NULL; }
unsigned int GetChannelSwitchDelay(void) { return 0; }
bool IsTimeshifting(void) { return false; }
bool IsRealTimeStream(void) { return true; }
void PauseStream(bool bPaused) {}
bool CanSeekStream(void) { return false; }
bool SeekTime(int,bool,double*) { return false; }
void SetSpeed(int) {};
time_t GetPlayingTime() { return 0; }
time_t GetBufferTimeStart() { return 0; }
time_t GetBufferTimeEnd() { return 0; }
PVR_ERROR UndeleteRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteAllRecordingsFromTrash() { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetEPGTimeFrame(int) { return PVR_ERROR_NOT_IMPLEMENTED; }

}
