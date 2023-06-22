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
  bool GetZatEnableDolby() const { return m_zatEnableDolby; }
  bool GetSkipStartOfProgramme() const { return m_skipStartOfProgramme; }
  bool ForceEnableWidevineL2() const { return m_forceEnableWidevineL2; }
  const std::string GetParentalPin() const { return m_parentalPin; }
  int GetProvider() const { return m_provider; }

private:
  std::string m_zatUsername;
  std::string m_zatPassword;
  bool m_zatFavoritesOnly = false;
  bool m_zatEnableDolby = true;
  bool m_skipStartOfProgramme = true;
  bool m_forceEnableWidevineL2 = false;
  std::string m_parentalPin;
  int m_provider = 0;
};
