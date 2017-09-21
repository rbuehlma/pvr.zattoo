#include "client.h"
#include "ZatData.h"
#include "kodi/xbmc_pvr_dll.h"
#include "kodi/libKODI_guilib.h"

using namespace ADDON;

#ifdef TARGET_WINDOWS
#define snprintf _snprintf
#endif

#ifdef TARGET_ANDROID
#include "to_string.h"
#endif

ADDON_STATUS m_CurStatus = ADDON_STATUS_UNKNOWN;
ZatData *zat = NULL;
time_t g_pvrZattooTimeShift;

/* User adjustable settings are saved here.
 * Default values are defined inside client.h
 * and exported to the other source files.
 */
std::string g_strUserPath = "";
std::string g_strClientPath = "";

CHelper_libXBMC_addon *XBMC = NULL;
CHelper_libXBMC_pvr *PVR = NULL;

std::string zatUsername = "";
std::string zatPassword = "";
bool zatFavoritesOnly = false;
bool zatAlternativeEpgService = false;
bool streamType = 0;
int provider = 0;

extern "C"
{

void ADDON_ReadSettings(void)
{
  char buffer[1024];
  bool boolBuffer;
  int intBuffer;
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
  if (XBMC->GetSetting("alternativeepgservice", &boolBuffer))
  {
    zatAlternativeEpgService = boolBuffer;
  }
  if (XBMC->GetSetting("streamtype", &intBuffer))
  {
    streamType = intBuffer;
  }
  if (XBMC->GetSetting("provider", &intBuffer))
  {
    provider = intBuffer;
  }
  XBMC->Log(LOG_DEBUG, "End Readsettings");
}

ADDON_STATUS ADDON_Create(void *hdl, void *props)
{
  if (!hdl || !props)
  {
    return ADDON_STATUS_UNKNOWN;
  }

  g_pvrZattooTimeShift = 0;

  PVR_PROPERTIES *pvrprops = (PVR_PROPERTIES *) props;

  XBMC = new CHelper_libXBMC_addon;
  XBMC->RegisterMe(hdl);

  if (!XBMC->RegisterMe(hdl))
  {
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  PVR = new CHelper_libXBMC_pvr;
  if (!PVR->RegisterMe(hdl))
  {
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
  if (!zatUsername.empty() && !zatPassword.empty())
  {
    XBMC->Log(LOG_DEBUG, "Create Zat");
    zat = new ZatData(zatUsername, zatPassword, zatFavoritesOnly,
        zatAlternativeEpgService, streamType ? "hls" : "dash", provider);
    XBMC->Log(LOG_DEBUG, "Zat created");
    if (zat->Initialize() && zat->LoadChannels())
    {
      m_CurStatus = ADDON_STATUS_OK;
    }
    else
    {
      XBMC->QueueNotification(QUEUE_ERROR, XBMC->GetLocalizedString(37111));
    }
  }

  return m_CurStatus;
}

ADDON_STATUS ADDON_GetStatus()
{
  return m_CurStatus;
}

void ADDON_Destroy()
{
  SAFE_DELETE(zat);
  m_CurStatus = ADDON_STATUS_UNKNOWN;
}

ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
{
  string name = settingName;

  if (name == "username")
  {
    string username = (const char*) settingValue;
    if (username != zatUsername)
    {
      zatUsername = username;
      return ADDON_STATUS_NEED_RESTART;
    }
  }

  if (name == "password")
  {
    string password = (const char*) settingValue;
    if (password != zatPassword)
    {
      zatPassword = password;
      return ADDON_STATUS_NEED_RESTART;
    }
  }

  if (name == "favoritesonly")
  {
    bool favOnly = *(bool *) settingValue;
    if (favOnly != zatFavoritesOnly)
    {
      zatFavoritesOnly = favOnly;
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  if (name == "streamtype")
  {
    int type = *(int *) settingValue;
    if (type != streamType)
    {
      streamType = type;
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  if (name == "provider")
  {
    int prov = *(int *) settingValue;
    if (provider != prov)
    {
      provider = prov;
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  return ADDON_STATUS_OK;
}

void ADDON_Stop()
{
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

PVR_ERROR GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{
  pCapabilities->bSupportsEPG = true;
  pCapabilities->bSupportsTV = true;
  pCapabilities->bSupportsRadio = true;
  pCapabilities->bSupportsChannelGroups = true;
  pCapabilities->bSupportsRecordingPlayCount = true;
  pCapabilities->bSupportsLastPlayedPosition = true;
  pCapabilities->bSupportsRecordingsRename = true;
  pCapabilities->bSupportsRecordingsLifetimeChange = false;
  pCapabilities->bSupportsDescrambleInfo = false;

  if (zat)
  {
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
  static std::string strBackendVersion = STR(IPTV_VERSION);
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

PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel,
    time_t iStart, time_t iEnd)
{
  if (zat)
  {
    zat->GetEPGForChannel(channel, iStart, iEnd);
    return PVR_ERROR_NO_ERROR;
  }

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
  if (bRadio)
    return PVR_ERROR_NO_ERROR;

  if (zat)
    return zat->GetChannels(handle, bRadio);

  return PVR_ERROR_NO_ERROR;
}

bool OpenLiveStream(const PVR_CHANNEL &channel)
{
  return false;
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
  if (bRadio)
    return PVR_ERROR_NO_ERROR;
  if (zat)
    return zat->GetChannelGroups(handle);

  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle,
    const PVR_CHANNEL_GROUP &group)
{

  if (zat)
    return zat->GetChannelGroupMembers(handle, group);

  return PVR_ERROR_SERVER_ERROR;

}

void setStreamProperties(PVR_NAMED_VALUE* properties,
    unsigned int* propertiesCount, std::string url)
{
  strncpy(properties[0].strName, PVR_STREAM_PROPERTY_STREAMURL,
      sizeof(properties[0].strName));
  strncpy(properties[0].strValue, url.c_str(), sizeof(properties[0].strValue));
  strncpy(properties[1].strName, PVR_STREAM_PROPERTY_INPUTSTREAMADDON,
      sizeof(properties[1].strName));
  strncpy(properties[1].strValue, "inputstream.adaptive",
      sizeof(properties[1].strValue));
  strncpy(properties[2].strName, "inputstream.adaptive.manifest_type",
      sizeof(properties[2].strName));
  strncpy(properties[2].strValue, streamType ? "hls" : "mpd",
      sizeof(properties[2].strValue));
  *propertiesCount = 3;
}

PVR_ERROR GetChannelStreamProperties(const PVR_CHANNEL* channel,
    PVR_NAMED_VALUE* properties, unsigned int* propertiesCount)
{
  std::string strUrl = zat->GetChannelStreamUrl(channel->iUniqueId);
  if (strUrl.empty())
  {
    return PVR_ERROR_FAILED;
  }
  setStreamProperties(properties, propertiesCount, strUrl);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR GetRecordingStreamProperties(const PVR_RECORDING* recording,
    PVR_NAMED_VALUE* properties, unsigned int* propertiesCount)
{
  std::string strUrl = zat->GetRecordingStreamUrl(recording->strRecordingId);
  ;
  if (strUrl.empty())
  {
    return PVR_ERROR_FAILED;
  }
  setStreamProperties(properties, propertiesCount, strUrl);
  return PVR_ERROR_NO_ERROR;
}

/** Recording API **/
int GetRecordingsAmount(bool deleted)
{
  if (deleted)
  {
    return 0;
  }
  if (!zat)
  {
    return 0;
  }
  return zat->GetRecordingsAmount(false);
}

PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted)
{
  if (deleted)
  {
    return PVR_ERROR_NO_ERROR;
  }
  if (!zat)
  {
    return PVR_ERROR_SERVER_ERROR;
  }
  zat->GetRecordings(handle, false);
  return PVR_ERROR_NO_ERROR;
}

int GetTimersAmount(void)
{
  if (!zat)
  {
    return 0;
  }
  return zat->GetRecordingsAmount(true);
}

PVR_ERROR GetTimers(ADDON_HANDLE handle)
{
  if (!zat)
  {
    return PVR_ERROR_SERVER_ERROR;
  }
  zat->GetRecordings(handle, true);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR AddTimer(const PVR_TIMER &timer)
{
  if (!zat)
  {
    return PVR_ERROR_SERVER_ERROR;
  }
  if (timer.iEpgUid <= EPG_TAG_INVALID_UID)
  {
    return PVR_ERROR_REJECTED;
  }
  if (!zat->Record(timer.iEpgUid))
  {
    return PVR_ERROR_REJECTED;
  }
  PVR->TriggerTimerUpdate();
  PVR->TriggerRecordingUpdate();
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR DeleteRecording(const PVR_RECORDING &recording)
{
  if (!zat)
  {
    return PVR_ERROR_SERVER_ERROR;
  }
  if (!zat->DeleteRecording(recording.strRecordingId))
  {
    return PVR_ERROR_REJECTED;
  }
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete)
{
  if (!zat)
  {
    return PVR_ERROR_SERVER_ERROR;
  }
  if (!zat->DeleteRecording(to_string(timer.iClientIndex)))
  {
    return PVR_ERROR_REJECTED;
  }
  PVR->TriggerTimerUpdate();
  PVR->TriggerRecordingUpdate();
  return PVR_ERROR_NO_ERROR;
}

void addTimerType(PVR_TIMER_TYPE types[], int idx, int attributes)
{
  types[idx].iId = idx + 1;
  types[idx].iAttributes = attributes;
  types[idx].iPrioritiesSize = 0;
  types[idx].iLifetimesSize = 0;
  types[idx].iPreventDuplicateEpisodesSize = 0;
  types[idx].iRecordingGroupSize = 0;
  types[idx].iMaxRecordingsSize = 0;
}

PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int *size)
{
  addTimerType(types, 0, PVR_TIMER_TYPE_ATTRIBUTE_NONE);
  addTimerType(types, 1, PVR_TIMER_TYPE_IS_MANUAL);
  *size = 2;
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR IsEPGTagRecordable(const EPG_TAG* tag, bool* bIsRecordable)
{
  if (!zat)
  {
    return PVR_ERROR_FAILED;
  }
  *bIsRecordable = zat->IsRecordable(tag);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR IsEPGTagPlayable(const EPG_TAG* tag, bool* bIsPlayable)
{
  if (!zat)
  {
    return PVR_ERROR_FAILED;
  }
  *bIsPlayable = zat->IsPlayable(tag);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR GetEPGTagStreamProperties(const EPG_TAG* tag,
    PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount)
{
  std::string strUrl = zat->GetEpgTagUrl(tag);
  if (strUrl.empty())
  {
    return PVR_ERROR_FAILED;
  }
  setStreamProperties(properties, iPropertiesCount, strUrl);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count)
{
  if (!zat)
  {
    return PVR_ERROR_FAILED;
  }
  zat->SetRecordingPlayCount(recording, count);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording,
    int lastplayedposition)
{
  if (!zat)
  {
    return PVR_ERROR_FAILED;
  }
  zat->SetRecordingLastPlayedPosition(recording, lastplayedposition);
  return PVR_ERROR_NO_ERROR;
}

int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording)
{
  if (!zat)
  {
    return -1;
  }

  return zat->GetRecordingLastPlayedPosition(recording);
}

time_t GetPlayingTime()
{
  time_t current_time;
  time(&current_time);
  return current_time - g_pvrZattooTimeShift;
}

/** UNUSED API FUNCTIONS */
bool CanPauseStream(void)
{
  return true;
}
PVR_ERROR OpenDialogChannelScan(void)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR CallMenuHook(const PVR_MENUHOOK &menuhook,
    const PVR_MENUHOOK_DATA &item)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR DeleteChannel(const PVR_CHANNEL &channel)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR RenameChannel(const PVR_CHANNEL &channel)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR MoveChannel(const PVR_CHANNEL &channel)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL &channel)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL &channel)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
bool OpenRecordedStream(const PVR_RECORDING &recording)
{
  return false;
}
void CloseRecordedStream(void)
{
}
int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
  return 0;
}
long long SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */)
{
  return 0;
}
long long PositionRecordedStream(void)
{
  return -1;
}
long long LengthRecordedStream(void)
{
  return 0;
}
void DemuxReset(void)
{
}
void DemuxFlush(void)
{
}
int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
  return 0;
}
long long SeekLiveStream(long long iPosition, int iWhence /* = SEEK_SET */)
{
  return -1;
}
long long PositionLiveStream(void)
{
  return -1;
}
long long LengthLiveStream(void)
{
  return -1;
}
PVR_ERROR RenameRecording(const PVR_RECORDING &recording)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int*)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
;
PVR_ERROR UpdateTimer(const PVR_TIMER &timer)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
void DemuxAbort(void)
{
}
DemuxPacket* DemuxRead(void)
{
  return NULL;
}
unsigned int GetChannelSwitchDelay(void)
{
  return 0;
}
bool IsTimeshifting(void)
{
  return false;
}
bool IsRealTimeStream(void)
{
  return true;
}
void PauseStream(bool bPaused)
{
}
bool CanSeekStream(void)
{
  return true;
}
bool SeekTime(double, bool, double*)
{
  return false;
}
void SetSpeed(int)
{
}
;
time_t GetBufferTimeStart()
{
  return 0;
}
time_t GetBufferTimeEnd()
{
  return 0;
}
PVR_ERROR UndeleteRecording(const PVR_RECORDING& recording)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR DeleteAllRecordingsFromTrash()
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR SetEPGTimeFrame(int)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR GetDescrambleInfo(PVR_DESCRAMBLE_INFO*)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR SetRecordingLifetime(const PVR_RECORDING*)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR GetStreamTimes(PVR_STREAM_TIMES *times)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
;
PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
}
