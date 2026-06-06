#include <kodi/AddonBase.h>
#include "Cache.h"
#include <kodi/Filesystem.h>
#include "../Utils.h"

#ifdef TARGET_WINDOWS
#include "../windows.h"
#ifdef CreateDirectory
#undef CreateDirectory
#endif
#ifdef DeleteFile
#undef DeleteFile
#endif
#endif

using json = nlohmann::json;

constexpr char CACHE_DIR[] = "special://profile/addon_data/pvr.zattoo/cache/";

time_t Cache::m_lastCleanup = 0;

bool Cache::Read(const std::string& key, std::string& data)
{
  std::string cacheFile = CACHE_DIR + key;
  if (!kodi::vfs::FileExists(cacheFile, true))
  {
    return false;
  }
  std::string jsonString = Utils::ReadFile(cacheFile);
  if (jsonString.empty())
  {
    return false;
  }
  json doc = json::parse(jsonString, nullptr, false);
  if (doc.is_discarded())
  {
    if (kodi::vfs::FileExists(cacheFile, true))
    {
      kodi::Log(ADDON_LOG_ERROR, "Parsing cache file [%s] failed.", cacheFile.c_str());
    }
    return false;
  }

  if (!IsStillValid(doc))
  {
    kodi::Log(ADDON_LOG_DEBUG, "Ignoring cache file [%s] due to expiry.",
        cacheFile.c_str());
    return false;
  }

  kodi::Log(ADDON_LOG_DEBUG, "Load from cache file [%s].", cacheFile.c_str());
  data = doc["data"].get<std::string>();
  return !data.empty();
}

void Cache::Write(const std::string& key, const std::string& data, time_t validUntil)
{
  if (!kodi::vfs::DirectoryExists(CACHE_DIR))
  {
    if (!kodi::vfs::CreateDirectory(CACHE_DIR))
    {
      kodi::Log(ADDON_LOG_ERROR, "Could not crate cache directory [%s].", CACHE_DIR);
      return;
    }
  }
  std::string cacheFile = CACHE_DIR + key;
  kodi::vfs::CFile file;
  if (!file.OpenFileForWrite(cacheFile, true))
  {
    kodi::Log(ADDON_LOG_ERROR, "Could not write to cache file [%s].",
        cacheFile.c_str());
    return;
  }

  json d;
  d["validUntil"] = static_cast<uint64_t>(validUntil);
  d["data"] = data;

  std::string output = d.dump();
  file.Write(output.c_str(), output.size());
}

void Cache::Cleanup()
{
  time_t currTime;
  time(&currTime);
  if (m_lastCleanup + 60 * 60 > currTime)
  {
   return;
  }
  m_lastCleanup = currTime;
  if (!kodi::vfs::DirectoryExists(CACHE_DIR))
  {
    return;
  }
  std::vector<kodi::vfs::CDirEntry> items;
  if (!kodi::vfs::GetDirectory(CACHE_DIR, "", items))
  {
    kodi::Log(ADDON_LOG_ERROR, "Could not get cache directory.");
    return;
  }
  for (const auto& item : items)
  {
    if (item.IsFolder())
    {
      continue;
    }
    std::string path = item.Path();
    std::string jsonString = Utils::ReadFile(path);
    if (jsonString.empty())
    {
      continue;
    }
    json doc = json::parse(jsonString, nullptr, false);
    if (doc.is_discarded())
    {
      kodi::Log(ADDON_LOG_ERROR, "Parsing cache file [%s] failed. -> Delete", path.c_str());
      kodi::vfs::DeleteFile(path);
    }

    if (!IsStillValid(doc))
    {
      kodi::Log(ADDON_LOG_DEBUG, "Deleting expired cache file [%s].", path.c_str());
      if (!kodi::vfs::DeleteFile(path))
      {
        kodi::Log(ADDON_LOG_DEBUG, "Deletion of file [%s] failed.", path.c_str());
      }
    }
  }
}

bool Cache::IsStillValid(const json& cache)
{
  time_t validUntil = static_cast<time_t>(cache["validUntil"].get<uint64_t>());
  time_t current_time;
  time(&current_time);
  return validUntil >= current_time;
}
