/*
 *  Copyright (C) 2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Settings.h"

bool CSettings::Load()
{
  if (!kodi::CheckSettingString("username", m_zatUsername))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'username' setting");
    return false;
  }

  if (!kodi::CheckSettingString("password", m_zatPassword))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'password' setting");
    return false;
  }

  if (!kodi::CheckSettingBoolean("favoritesonly", m_zatFavoritesOnly))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'useradio' setting, falling back to 'false' as default");
    m_zatFavoritesOnly = false;
  }

  if (!kodi::CheckSettingBoolean("enableDolby", m_zatEnableDolby))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'enableDolby' setting, falling back to 'true' as default");
    m_zatEnableDolby = true;
  }

  if (!kodi::CheckSettingBoolean("alternativeepgservice_https", m_zatAlternativeEpgService))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'alternativeepgservice_https' setting, falling back to 'false' as default");
    m_zatAlternativeEpgService = false;
  }

  if (!kodi::CheckSettingBoolean("provide_zattoo_session", m_zatAlternativeEpgServiceProvideSession))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'provide_zattoo_session' setting, falling back to 'false' as default");
    m_zatAlternativeEpgServiceProvideSession = false;
  }

  if (!kodi::CheckSettingEnum<STREAM_TYPE>("streamtype", m_streamType))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'streamtype' setting, falling back to 'DASH' as default");
    m_streamType = DASH;
  }

  if (!kodi::CheckSettingString("parentalPin", m_parentalPin))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'parentalPin' setting, falling back to 'empty' as default");
    m_parentalPin = "";
  }

  if (!kodi::CheckSettingString("xmlTVFile", m_xmlTVFile))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'xmlTVFile' setting, falling back to 'empty' as default");
    m_xmlTVFile = "";
  }

  if (!kodi::CheckSettingString("xmlTVFile", m_xmlTVFile))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'xmlTVFile' setting, falling back to 'empty' as default");
    m_xmlTVFile = "";
  }

  if (!kodi::CheckSettingInt("provider", m_provider))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'provider' setting, falling back to '0' as default");
    m_provider = 0;
  }

  return true;
}

ADDON_STATUS CSettings::SetSetting(const std::string& settingName,
                                   const kodi::CSettingValue& settingValue)
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
  else if (settingName == "alternativeepgservice_https")
  {
    kodi::Log(ADDON_LOG_DEBUG, "Changed Setting 'alternativeepgservice_https' from %u to %u", m_zatAlternativeEpgService, settingValue.GetBoolean());
    if (m_zatAlternativeEpgService != settingValue.GetBoolean())
    {
      m_zatAlternativeEpgService = settingValue.GetBoolean();
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (settingName == "provide_zattoo_session")
  {
    kodi::Log(ADDON_LOG_DEBUG, "Changed Setting 'alternativeepgservice_https' from %u to %u", m_zatAlternativeEpgServiceProvideSession, settingValue.GetBoolean());
    if (m_zatAlternativeEpgServiceProvideSession != settingValue.GetBoolean())
    {
      m_zatAlternativeEpgServiceProvideSession = settingValue.GetBoolean();
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
  else if (settingName == "xmlTVFile")
  {
    std::string tmp_sXmlTVFile;
    kodi::Log(ADDON_LOG_DEBUG, "Changed Setting 'xmlTVFile'");
    tmp_sXmlTVFile = m_xmlTVFile;
    m_xmlTVFile = settingValue.GetString();
    if (tmp_sXmlTVFile != m_xmlTVFile)
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
