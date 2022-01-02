#include "client.h"

#include "ZatData.h"

#include <kodi/General.h>

ADDON_STATUS CZattooTVAddon::CreateInstance(const kodi::addon::IInstanceInfo& instance,
                                            KODI_ADDON_INSTANCE_HDL& hdl)
{
  if (instance.IsType(ADDON_INSTANCE_PVR))
  {
    m_settings.Load();

    if (m_settings.GetZatUsername().empty() || m_settings.GetZatPassword().empty())
    {
      kodi::Log(ADDON_LOG_INFO, "Username or password not set.");
      kodi::QueueNotification(QUEUE_WARNING, "", kodi::addon::GetLocalizedString(30200));
      return ADDON_STATUS_NEED_SETTINGS;
    }

    kodi::Log(ADDON_LOG_DEBUG, "Create Zattoo");

    std::string zatUsername = m_settings.GetZatUsername();
    std::string zatPassword = m_settings.GetZatPassword();
    bool zatFavoritesOnly = m_settings.GetZatFavoritesOnly();
    STREAM_TYPE streamType = m_settings.GetStreamType();
    bool zatEnableDolby = m_settings.GetZatEnableDolby();
    int provider = m_settings.GetProvider();
    std::string parentalPin = m_settings.GetParentalPin();

    ZatData* client = new ZatData(instance, zatUsername, zatPassword, zatFavoritesOnly, streamType, zatEnableDolby, provider, parentalPin);
    hdl = client;
    kodi::Log(ADDON_LOG_DEBUG, "Zattoo created");

    if (client->Initialize() && client->LoadChannels())
    {
      return ADDON_STATUS_OK;
    }
    else
    {
      kodi::QueueNotification(QUEUE_WARNING, "", kodi::addon::GetLocalizedString(37111));
      return ADDON_STATUS_LOST_CONNECTION;
    }
  }

  return ADDON_STATUS_UNKNOWN;
}

ADDON_STATUS CZattooTVAddon::SetSetting(const std::string& settingName,
                                        const kodi::addon::CSettingValue& settingValue)
{
  return m_settings.SetSetting(settingName, settingValue);
}

ADDONCREATOR(CZattooTVAddon);
