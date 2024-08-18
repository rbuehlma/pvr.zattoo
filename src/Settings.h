/*
 *  Copyright (C) 2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <kodi/AddonBase.h>

class ATTR_DLL_LOCAL CSettings
{
public:
  CSettings() = default;

  bool Load();
  ADDON_STATUS SetSetting(const std::string& settingName, const kodi::addon::CSettingValue& settingValue);
  bool VerifySettings();

  const std::string& GetZatUsername() const { return m_zatUsername; }
  const std::string& GetZatPassword() const { return m_zatPassword; }
  bool GetZatFavoritesOnly() const { return m_zatFavoritesOnly; }
  bool GetSmartTV() const { return m_smartTV; }
  bool GetZatEnableDolby() const { return m_zatEnableDolby; }
  bool GetSkipStartOfProgramme() const { return m_skipStartOfProgramme; }
  bool GetSkipEndOfProgramme() const { return m_skipEndOfProgramme; }
  int DrmLevel() const { return m_drmLevel; }
  const std::string GetParentalPin() const { return m_parentalPin; }
  int GetProvider() const { return m_provider; }

private:
  std::string m_zatUsername;
  std::string m_zatPassword;
  bool m_zatFavoritesOnly = false;
  bool m_smartTV = false;
  bool m_zatEnableDolby = true;
  bool m_skipStartOfProgramme = true;
  bool m_skipEndOfProgramme = true;
  int m_drmLevel = 0;
  std::string m_parentalPin;
  int m_provider = 0;
};
