#include "client.h"
#include "ZatData.h"
#include "kodi/xbmc_pvr_dll.h"
#include "kodi/libKODI_guilib.h"
#include <chrono>
#include <thread>
#include <utility>

using namespace ADDON;

#ifdef TARGET_WINDOWS
#define snprintf _snprintf
#endif

#ifdef TARGET_ANDROID
#include "to_string.h"
#endif

ADDON_STATUS m_CurStatus = ADDON_STATUS_UNKNOWN;
ZatData *zat = nullptr;

/* User adjustable settings are saved here.
 * Default values are defined inside client.h
 * and exported to the other source files.
 */
std::string g_strUserPath;
std::string g_strClientPath;

CHelper_libXBMC_addon *XBMC = nullptr;
CHelper_libXBMC_pvr *PVR = nullptr;

std::string zatUsername;
std::string zatPassword;
bool zatFavoritesOnly = false;
bool zatAlternativeEpgService = false;
bool zatAlternativeEpgServiceProvideSession = false;
bool streamType = false;
std::string xmlTVFile;
int provider = 0;
int runningRequests = 0;

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
  if (XBMC->GetSetting("alternativeepgservice_https", &boolBuffer))
  {
    zatAlternativeEpgService = boolBuffer;
  }
  if (XBMC->GetSetting("provide_zattoo_session", &boolBuffer))
  {
    zatAlternativeEpgServiceProvideSession = boolBuffer;
  }
  if (XBMC->GetSetting("streamtype", &intBuffer))
  {
    streamType = static_cast<bool>(intBuffer);
  }
  if (XBMC->GetSetting("xmlTVFile", &buffer))
  {
    xmlTVFile = buffer;
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

    auto *pvrprops = static_cast<PVR_PROPERTIES*>(props);

  XBMC = new CHelper_libXBMC_addon;
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
        zatAlternativeEpgService && zatAlternativeEpgServiceProvideSession, streamType ? "hls" : "dash", provider, xmlTVFile);
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
  ZatData* oldZat = zat;
  zat = nullptr;
  int waitCount = 10;
  while (runningRequests > 0 && waitCount > 0)
  {
    XBMC->Log(LOG_NOTICE, "Wait for %d requests to finish for %d seconds.", runningRequests, waitCount);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    waitCount--;
  }
  SAFE_DELETE(oldZat);
  SAFE_DELETE(PVR);
  SAFE_DELETE(XBMC);
  m_CurStatus = ADDON_STATUS_UNKNOWN;
}

ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
{
  std::string name = settingName;

  if (name == "username")
  {
    std::string username = static_cast<const char*>(settingValue);
    if (username != zatUsername)
    {
      zatUsername = username;
      return ADDON_STATUS_NEED_RESTART;
    }
  }

  if (name == "password")
  {
    std::string password = static_cast<const char*>(settingValue);
    if (password != zatPassword)
    {
      zatPassword = password;
      return ADDON_STATUS_NEED_RESTART;
    }
  }

  if (name == "favoritesonly")
  {
    bool favOnly = *static_cast<const bool*>(settingValue);
    if (favOnly != zatFavoritesOnly)
    {
      zatFavoritesOnly = favOnly;
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  if (name == "streamtype")
  {
    int type = *static_cast<const int*>(settingValue);
    if (type != streamType)
    {
      streamType = static_cast<bool>(type);
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  if (name == "provider")
  {
    int prov = *static_cast<const int*>(settingValue);
    if (provider != prov)
    {
      provider = prov;
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  
  if (name == "xmlTVFile")
  {
    std::string xmlTVFileProp = static_cast<const char*>(settingValue);
    if (xmlTVFile != xmlTVFileProp)
    {
      xmlTVFile = xmlTVFileProp;
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
  runningRequests++;
  pCapabilities->bSupportsEPG = true;
  pCapabilities->bSupportsEPGEdl = true;
  pCapabilities->bSupportsTV = true;
  pCapabilities->bSupportsRadio = true;
  pCapabilities->bSupportsChannelGroups = true;
  pCapabilities->bSupportsRecordingPlayCount = true;
  pCapabilities->bSupportsLastPlayedPosition = true;
  pCapabilities->bSupportsRecordingsRename = true;
  pCapabilities->bSupportsRecordingsLifetimeChange = false;
  pCapabilities->bSupportsDescrambleInfo = false;
  pCapabilities->bSupportsRecordingEdl = true;

  if (zat)
  {
    zat->GetAddonCapabilities(pCapabilities);
  }
  runningRequests--;
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
  runningRequests++;
  PVR_ERROR ret = PVR_ERROR_SERVER_ERROR;
  if (zat)
  {
    zat->GetEPGForChannel(channel, iStart, iEnd);
    ret = PVR_ERROR_NO_ERROR;
  }
  runningRequests--;
  return ret;
}

int GetChannelsAmount(void)
{
  runningRequests++;
  int ret = 0;
  if (zat)
    ret = zat->GetChannelsAmount();
  runningRequests--;
  return ret;
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  PVR_ERROR ret = PVR_ERROR_NO_ERROR;
  if (bRadio)
    return ret;
  runningRequests++;
  if (zat)
    ret = zat->GetChannels(handle, bRadio);
  runningRequests--;
  return ret;
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
  runningRequests++;
  int ret = zat->GetChannelGroupsAmount();
  runningRequests--;
  return ret;
}

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  if (bRadio)
    return PVR_ERROR_NO_ERROR;
  PVR_ERROR ret = PVR_ERROR_SERVER_ERROR;
  runningRequests++;
  if (zat)
    ret = zat->GetChannelGroups(handle);

  runningRequests--;
  return ret;
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle,
    const PVR_CHANNEL_GROUP &group)
{
  runningRequests++;
  PVR_ERROR ret = PVR_ERROR_SERVER_ERROR;
  if (zat)
    ret = zat->GetChannelGroupMembers(handle, group);
  runningRequests--;
  return ret;
}

void setStreamProperty(PVR_NAMED_VALUE* properties, unsigned int* propertiesCount, const std::string &name, const std::string &value)
{
  strncpy(properties[*propertiesCount].strName, name.c_str(), sizeof(properties[*propertiesCount].strName));
  strncpy(properties[*propertiesCount].strValue, value.c_str(), sizeof(properties[*propertiesCount].strValue));  
  *propertiesCount = (*propertiesCount) + 1;
}

void setStreamProperties(PVR_NAMED_VALUE* properties, unsigned int* propertiesCount, const std::string& url)
{
  setStreamProperty(properties, propertiesCount, PVR_STREAM_PROPERTY_STREAMURL, url);
  setStreamProperty(properties, propertiesCount, PVR_STREAM_PROPERTY_INPUTSTREAMADDON, "inputstream.adaptive");
  setStreamProperty(properties, propertiesCount, "inputstream.adaptive.manifest_type", streamType ? "hls" : "mpd");
  setStreamProperty(properties, propertiesCount, PVR_STREAM_PROPERTY_MIMETYPE, streamType ? "application/x-mpegURL" : "application/xml+dash");

  if (!streamType)
  {
    setStreamProperty(properties, propertiesCount, "inputstream.adaptive.manifest_update_parameter", "full");
  }
}

PVR_ERROR GetChannelStreamProperties(const PVR_CHANNEL* channel,
    PVR_NAMED_VALUE* properties, unsigned int* propertiesCount)
{
  runningRequests++;
	  std::string strUrl = zat->GetChannelStreamUrl(channel->iUniqueId);
  PVR_ERROR ret = PVR_ERROR_FAILED;
  if (!strUrl.empty())
  {
    *propertiesCount = 0;
    setStreamProperties(properties, propertiesCount, strUrl);
    setStreamProperty(properties, propertiesCount, PVR_STREAM_PROPERTY_ISREALTIMESTREAM, "true");
    ret = PVR_ERROR_NO_ERROR;
  }
  runningRequests--;
  return ret;
}

PVR_ERROR GetRecordingStreamProperties(const PVR_RECORDING* recording,
    PVR_NAMED_VALUE* properties, unsigned int* propertiesCount)
{
  runningRequests++;
  std::string strUrl = zat->GetRecordingStreamUrl(recording->strRecordingId);
  PVR_ERROR ret = PVR_ERROR_FAILED;
  if (!strUrl.empty())
  {
    *propertiesCount = 0;
    setStreamProperties(properties, propertiesCount, strUrl);
    ret = PVR_ERROR_NO_ERROR;
  }
  runningRequests--;
  return ret;
}

/** Recording API **/
int GetRecordingsAmount(bool deleted)
{
  if (deleted)
  {
    return 0;
  }
  runningRequests++;
  int ret = 0;
  if (zat)
  {
    ret = zat->GetRecordingsAmount(false);
  }
  runningRequests--;
  return ret;
}

PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted)
{
  if (deleted)
  {
    return PVR_ERROR_NO_ERROR;
  }
  runningRequests++;
  PVR_ERROR ret = PVR_ERROR_SERVER_ERROR;
  if (zat)
  {
    zat->GetRecordings(handle, false);
    ret = PVR_ERROR_NO_ERROR;
  }
  runningRequests--;
  return ret;
}

int GetTimersAmount(void)
{
  runningRequests++;
  int ret = 0;
  if (zat)
  {
    ret = zat->GetRecordingsAmount(true);
  }
  runningRequests--;
  return ret;
}

PVR_ERROR GetTimers(ADDON_HANDLE handle)
{
  runningRequests++;
  PVR_ERROR ret = PVR_ERROR_SERVER_ERROR;
  if (zat)
  {
    zat->GetRecordings(handle, true);
    ret = PVR_ERROR_NO_ERROR;
  }
  runningRequests--;
  return ret;
}

PVR_ERROR AddTimer(const PVR_TIMER &timer)
{
  runningRequests++;
  PVR_ERROR ret = PVR_ERROR_NO_ERROR;
  if (!zat)
  {
    ret = PVR_ERROR_SERVER_ERROR;
  }
  else if (timer.iEpgUid <= EPG_TAG_INVALID_UID)
  {
    ret = PVR_ERROR_REJECTED;
  }
  else if (!zat->Record(timer.iEpgUid))
  {
    ret = PVR_ERROR_REJECTED;
  }
  else
  {
    PVR->TriggerTimerUpdate();
    PVR->TriggerRecordingUpdate();
  }
  runningRequests--;
  return ret;
}

PVR_ERROR DeleteRecording(const PVR_RECORDING &recording)
{
  runningRequests++;
  PVR_ERROR ret = PVR_ERROR_NO_ERROR;
  if (!zat)
  {
    ret = PVR_ERROR_SERVER_ERROR;
  }
  else if (!zat->DeleteRecording(recording.strRecordingId))
  {
    ret = PVR_ERROR_REJECTED;
  }
  runningRequests--;
  return ret;
}

PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete)
{
  runningRequests++;
  PVR_ERROR ret = PVR_ERROR_NO_ERROR;
  if (!zat)
  {
    ret = PVR_ERROR_SERVER_ERROR;
  }
  else if (!zat->DeleteRecording(std::to_string(timer.iClientIndex)))
  {
    ret = PVR_ERROR_REJECTED;
  }
  else
  {
    PVR->TriggerTimerUpdate();
    PVR->TriggerRecordingUpdate();
  }
  runningRequests--;
  return ret;
}

void addTimerType(PVR_TIMER_TYPE types[], int idx, int attributes)
{
  types[idx].iId = static_cast<unsigned int>(idx + 1);
  types[idx].iAttributes = static_cast<unsigned int>(attributes);
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
  runningRequests++;
  PVR_ERROR ret = PVR_ERROR_FAILED;
  if (zat)
  {
    *bIsRecordable = zat->IsRecordable(tag);
    ret = PVR_ERROR_NO_ERROR;
  }
  runningRequests--;
  return ret;
}

PVR_ERROR IsEPGTagPlayable(const EPG_TAG* tag, bool* bIsPlayable)
{
  runningRequests++;
  PVR_ERROR ret = PVR_ERROR_FAILED;
  if (zat)
  {
    *bIsPlayable = zat->IsPlayable(tag);
    ret = PVR_ERROR_NO_ERROR;
  }

  runningRequests--;
  return ret;
}

PVR_ERROR GetEPGTagStreamProperties(const EPG_TAG* tag,
    PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount)
{
  runningRequests++;
  PVR_ERROR ret = PVR_ERROR_FAILED;
  std::string strUrl = zat->GetEpgTagUrl(tag);
  if (!strUrl.empty())
  {
    *iPropertiesCount = 0;
    setStreamProperties(properties, iPropertiesCount, strUrl);
    ret = PVR_ERROR_NO_ERROR;
  }
  runningRequests--;
  return ret;
}

PVR_ERROR GetEPGTagEdl(const EPG_TAG* epgTag, PVR_EDL_ENTRY edl[], int *size)
{
  edl[0].start=0;
  edl[0].end = 300000;
  edl[0].type = PVR_EDL_TYPE_COMBREAK;
  *size = 1;
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count)
{
  runningRequests++;
  PVR_ERROR ret = PVR_ERROR_FAILED;
  if (zat)
  {
    zat->SetRecordingPlayCount(recording, count);
    ret = PVR_ERROR_NO_ERROR;
  }
  runningRequests--;
  return ret;
}

PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording,
    int lastplayedposition)
{
  runningRequests++;
  PVR_ERROR ret = PVR_ERROR_FAILED;
  if (zat)
  {
    zat->SetRecordingLastPlayedPosition(recording, lastplayedposition);
    ret = PVR_ERROR_NO_ERROR;
  }
  runningRequests--;
  return ret;
}

int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording)
{
  runningRequests++;
  int ret = -1;
  if (zat)
  {
    ret = zat->GetRecordingLastPlayedPosition(recording);
  }
  runningRequests--;
  return ret;
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
long long LengthLiveStream(void)
{
  return -1;
}
PVR_ERROR RenameRecording(const PVR_RECORDING &recording)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR GetRecordingEdl(const PVR_RECORDING& recording, PVR_EDL_ENTRY edl[], int *size)
{
  edl[0].start=0;
  edl[0].end = 300000;
  edl[0].type = PVR_EDL_TYPE_COMBREAK;
  *size = 1;
  return PVR_ERROR_NO_ERROR;
}
PVR_ERROR UpdateTimer(const PVR_TIMER &timer)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
void DemuxAbort(void)
{
}
DemuxPacket* DemuxRead(void)
{
  return nullptr;
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
PVR_ERROR GetStreamReadChunkSize(int* chunksize)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

}
