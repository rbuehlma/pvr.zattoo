#include "client.h"

#include "ZatData.h"

#include <kodi/General.h>

ADDON_STATUS CZattooTVAddon::CreateInstance(int instanceType,
                                            const std::string& instanceID,
                                            KODI_HANDLE instance,
                                            const std::string& version,
                                            KODI_HANDLE& addonInstance)
{
  if (instanceType)
  {
    m_settings.Load();

    if (m_settings.GetZatUsername().empty() || m_settings.GetZatPassword().empty())
    {
      kodi::Log(ADDON_LOG_INFO, "Username or password not set.");
      kodi::QueueNotification(QUEUE_WARNING, "", kodi::GetLocalizedString(30200));
      return ADDON_STATUS_NEED_SETTINGS;
    }

    /* Connect to ARGUS TV */
    kodi::Log(ADDON_LOG_DEBUG, "Create Zat");

    std::string zatUsername = m_settings.GetZatUsername();
    std::string zatPassword = m_settings.GetZatPassword();
    bool zatFavoritesOnly = m_settings.GetZatFavoritesOnly();
    bool zatAlternativeEpgService = m_settings.GetZatAlternativeEpgService();
    bool zatAlternativeEpgServiceProvideSession = m_settings.GetZatAlternativeEpgServiceProvideSession();
    STREAM_TYPE streamType = m_settings.GetStreamType();
    bool zatEnableDolby = m_settings.GetZatEnableDolby();
    int provider = m_settings.GetProvider();
    std::string xmlTVFile = m_settings.GetXmlTVFile();
    std::string parentalPin = m_settings.GetParentalPin();

    ZatData* client = new ZatData(instance, version, zatUsername, zatPassword, zatFavoritesOnly,
        zatAlternativeEpgService && zatAlternativeEpgServiceProvideSession, streamType, zatEnableDolby, provider, xmlTVFile, parentalPin);
    addonInstance = client;
    kodi::Log(ADDON_LOG_DEBUG, "Zat created");

    if (client->Initialize() && client->LoadChannels())
    {
      return ADDON_STATUS_OK;
    }
    else
    {
      kodi::QueueNotification(QUEUE_WARNING, "", kodi::GetLocalizedString(37111));
      return ADDON_STATUS_LOST_CONNECTION;
    }
  }

  return ADDON_STATUS_UNKNOWN;
}

ADDON_STATUS CZattooTVAddon::SetSetting(const std::string& settingName,
                                        const kodi::CSettingValue& settingValue)
{
  return m_settings.SetSetting(settingName, settingValue);
}

ADDONCREATOR(CZattooTVAddon);
