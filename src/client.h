#pragma once

#include "Settings.h"

#include <kodi/AddonBase.h>
#include <unordered_map>

class ZatData;

class ATTR_DLL_LOCAL CZattooTVAddon : public kodi::addon::CAddonBase
{
public:
  CZattooTVAddon() = default;

  ADDON_STATUS CreateInstance(const kodi::addon::IInstanceInfo& instance,
                              KODI_ADDON_INSTANCE_HDL& hdl) override;

  ADDON_STATUS SetSetting(const std::string& settingName,
                          const kodi::addon::CSettingValue& settingValue) override;

private:
  CSettings m_settings;
};
