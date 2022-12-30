/*
 *  Copyright (C) 2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Settings.h"
#include <kodi/General.h>

bool CSettings::Load()
{
  if (!kodi::addon::CheckSettingString("username", m_zatUsername))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'username' setting");
    return false;
  }

  if (!kodi::addon::CheckSettingString("password", m_zatPassword))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'password' setting");
    return false;
  }

  if (!kodi::addon::CheckSettingBoolean("favoritesonly", m_zatFavoritesOnly))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'favoritesonly' setting, falling back to 'false' as default");
    m_zatFavoritesOnly = false;
  }

  if (!kodi::addon::CheckSettingBoolean("enableDolby", m_zatEnableDolby))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'enableDolby' setting, falling back to 'true' as default");
    m_zatEnableDolby = true;
  }

  if (!kodi::addon::CheckSettingBoolean("skipStart", m_skipStartOfProgramme))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'skipStart' setting, falling back to 'true' as default");
    m_skipStartOfProgramme = true;
  }

  if (!kodi::addon::CheckSettingEnum<STREAM_TYPE>("streamtype", m_streamType))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'streamtype' setting, falling back to 'DASH' as default");
    m_streamType = DASH;
  }

  if (!kodi::addon::CheckSettingString("parentalPin", m_parentalPin))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'parentalPin' setting, falling back to 'empty' as default");
    m_parentalPin = "";
  }

  if (!kodi::addon::CheckSettingInt("provider", m_provider))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'provider' setting, falling back to '0' as default");
    m_provider = 0;
  }

  return true;
}

ADDON_STATUS CSettings::SetSetting(const std::string& settingName,
                                   const kodi::addon::CSettingValue& settingValue)
{
  if (settingName == "username")
  {
    std::string tmp_sUsername;
    kodi::Log(ADDON_LOG_DEBUG, "Changed Setting 'username'");
    tmp_sUsername = m_zatUsername;
    m_zatUsername = settingValue.GetString();
    if (tmp_sUsername != m_zatUsername)
      return ADDON_STATUS_NEED_RESTART;
  }
  else if (settingName == "password")
  {
    std::string tmp_sPassword;
    kodi::Log(ADDON_LOG_DEBUG, "Changed Setting 'password'");
    tmp_sPassword = m_zatPassword;
    m_zatPassword = settingValue.GetString();
    if (tmp_sPassword != m_zatPassword)
      return ADDON_STATUS_NEED_RESTART;
  }
  else if (settingName == "favoritesonly")
  {
    kodi::Log(ADDON_LOG_DEBUG, "Changed Setting 'favoritesonly' from %u to %u", m_zatFavoritesOnly, settingValue.GetBoolean());
    if (m_zatFavoritesOnly != settingValue.GetBoolean())
    {
      m_zatFavoritesOnly = settingValue.GetBoolean();
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (settingName == "enableDolby")
  {
    kodi::Log(ADDON_LOG_DEBUG, "Changed Setting 'enableDolby' from %u to %u", m_zatEnableDolby, settingValue.GetBoolean());
    if (m_zatEnableDolby != settingValue.GetBoolean())
    {
      m_zatEnableDolby = settingValue.GetBoolean();
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (settingName == "skipStart")
  {
    kodi::Log(ADDON_LOG_DEBUG, "Changed Setting 'skipStart' from %u to %u", m_skipStartOfProgramme, settingValue.GetBoolean());
    if (m_skipStartOfProgramme != settingValue.GetBoolean())
    {
      m_skipStartOfProgramme = settingValue.GetBoolean();
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (settingName == "streamtype")
  {
    kodi::Log(ADDON_LOG_DEBUG, "Changed Setting 'streamtype' from %u to %u", m_streamType, settingValue.GetInt());
    if (m_streamType != settingValue.GetEnum<STREAM_TYPE>())
    {
      m_streamType = settingValue.GetEnum<STREAM_TYPE>();
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (settingName == "parentalPin")
  {
    std::string tmp_sParentalPin;
    kodi::Log(ADDON_LOG_DEBUG, "Changed Setting 'parentalPin'");
    tmp_sParentalPin = m_parentalPin;
    m_parentalPin = settingValue.GetString();
    if (tmp_sParentalPin != m_parentalPin)
      return ADDON_STATUS_NEED_RESTART;
  }
  else if (settingName == "provider")
  {
    kodi::Log(ADDON_LOG_DEBUG, "Changed Setting 'provider'");
    if (m_provider != settingValue.GetInt())
    {
      m_provider = settingValue.GetInt();
      return ADDON_STATUS_NEED_RESTART;
    }
  }

  return ADDON_STATUS_OK;
}

bool CSettings::VerifySettings() {
  std::string username = GetZatUsername();
  std::string password = GetZatPassword();
  if (username.empty() || password.empty()) {
    kodi::Log(ADDON_LOG_INFO, "Username or password not set.");
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::addon::GetLocalizedString(30200));

    return false;
  }
  return true;
}
