#pragma once

#include "Settings.h"

#include <kodi/AddonBase.h>
#include <unordered_map>

class ZatData;

class ATTRIBUTE_HIDDEN CZattooTVAddon : public kodi::addon::CAddonBase
{
public:
  CZattooTVAddon() = default;

  ADDON_STATUS CreateInstance(int instanceType,
                              const std::string& instanceID,
                              KODI_HANDLE instance,
                              const std::string& version,
                              KODI_HANDLE& addonInstance) override;

  ADDON_STATUS SetSetting(const std::string& settingName,
                          const kodi::CSettingValue& settingValue) override;

private:
  CSettings m_settings;
};
