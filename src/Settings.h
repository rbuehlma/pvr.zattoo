/*
 *  Copyright (C) 2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <kodi/AddonBase.h>

enum STREAM_TYPE : int
{
  DASH,
  HLS,
  DASH_WIDEVINE
};

class ATTRIBUTE_HIDDEN CSettings
{
public:
  CSettings() = default;

  bool Load();
  ADDON_STATUS SetSetting(const std::string& settingName, const kodi::CSettingValue& settingValue);

  const std::string& GetZatUsername() const { return m_zatUsername; }
  const std::string& GetZatPassword() const { return m_zatPassword; }
  bool GetZatFavoritesOnly() const { return m_zatFavoritesOnly; }
  bool GetRecordedSeriesFolder() const { return m_recordedSeriesFolder; }
  bool GetZatEnableDolby() const { return m_zatEnableDolby; }
  STREAM_TYPE GetStreamType() const { return m_streamType; }
  const std::string GetParentalPin() const { return m_parentalPin; }
  int GetProvider() const { return m_provider; }

private:
  std::string m_zatUsername;
  std::string m_zatPassword;
  bool m_zatFavoritesOnly = false;
  bool m_recordedSeriesFolder = false;
  bool m_zatEnableDolby = true;
  STREAM_TYPE m_streamType = DASH;
  std::string m_parentalPin;
  int m_provider = 0;
};
