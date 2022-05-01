#include <kodi/AddonBase.h>
#include "Cache.h"
#include <kodi/Filesystem.h>
#include "../Utils.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#ifdef TARGET_WINDOWS
#include "../windows.h"
#ifdef CreateDirectory
#undef CreateDirectory
#endif
#ifdef DeleteFile
#undef DeleteFile
#endif
#endif

using namespace rapidjson;

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
  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError())
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
  data = doc["data"].GetString();
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

  Document d;
  d.SetObject();
  d.AddMember("validUntil", static_cast<uint64_t>(validUntil), d.GetAllocator());
  Value value;
  value.SetString(data.c_str(), static_cast<SizeType>(data.length()), d.GetAllocator());
  d.AddMember("data", value, d.GetAllocator());

  StringBuffer buffer;
  Writer<StringBuffer> writer(buffer);
  d.Accept(writer);
  const char* output = buffer.GetString();
  file.Write(output, strlen(output));
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
    Document doc;
    doc.Parse(jsonString.c_str());
    if (doc.GetParseError())
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

bool Cache::IsStillValid(const Value& cache)
{
  time_t validUntil = static_cast<time_t>(cache["validUntil"].GetUint64());
  time_t current_time;
  time(&current_time);
  return validUntil >= current_time;
}
