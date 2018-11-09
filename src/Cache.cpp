#include "Cache.h"
#include "client.h"
#include "Utils.h"
#include "kodi_vfs_types.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#ifdef TARGET_WINDOWS
#include <windows.h>
#ifdef CreateDirectory
#undef CreateDirectory
#endif
#ifdef DeleteFile
#undef DeleteFile
#endif
#endif

using namespace rapidjson;
using namespace std;
using namespace ADDON;

constexpr char CACHE_DIR[] = "special://profile/addon_data/pvr.zattoo/cache/";

time_t Cache::lastCleanup = 0;

bool Cache::Read(const string& key, std::string& data)
{
  string cacheFile = CACHE_DIR + key;
  if (!XBMC->FileExists(cacheFile.c_str(), true))
  {
    return false;
  }
  string jsonString = Utils::ReadFile(cacheFile);
  if (jsonString.empty())
  {
    return false;
  }
  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError())
  {
    XBMC->Log(LOG_ERROR, "Parsing cache file [%s] failed.", cacheFile.c_str());
    return false;
  }

  if (!IsStillValid(doc))
  {
    XBMC->Log(LOG_DEBUG, "Ignoring cache file [%s] due to expiry.",
        cacheFile.c_str());
    return false;
  }

  XBMC->Log(LOG_DEBUG, "Load from cache file [%s].", cacheFile.c_str());
  data = doc["data"].GetString();
  return !data.empty();
}

void Cache::Write(const string& key, const std::string& data, time_t validUntil)
{
  if (!XBMC->DirectoryExists(CACHE_DIR))
  {
    if (!XBMC->CreateDirectory(CACHE_DIR))
    {
      XBMC->Log(LOG_ERROR, "Could not crate cache directory [%s].", CACHE_DIR);
      return;
    }
  }
  string cacheFile = CACHE_DIR + key;
  void* file;
  if (!(file = XBMC->OpenFileForWrite(cacheFile.c_str(), true)))
  {
    XBMC->Log(LOG_ERROR, "Could not write to cache file [%s].",
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
  XBMC->WriteFile(file, output, strlen(output));
  XBMC->CloseFile(file);
}

void Cache::Cleanup()
{
  time_t currTime;
  time(&currTime);
  if (lastCleanup + 60 * 60 > currTime)
  {
   return;
  }
  lastCleanup = currTime;
  if (!XBMC->DirectoryExists(CACHE_DIR))
  {
    return;
  }
  VFSDirEntry *items;
  unsigned int itemCount;
  if (!XBMC->GetDirectory(CACHE_DIR, "", &items, &itemCount))
  {
    XBMC->Log(LOG_ERROR, "Could not get cache directory.");
    return;
  }
  for (unsigned int i = 0; i < itemCount; i++)
  {
    if (items[i].folder)
    {
      continue;
    }
    char *path = items[i].path;
    string jsonString = Utils::ReadFile(path);
    if (jsonString.empty())
    {
      continue;
    }
    Document doc;
    doc.Parse(jsonString.c_str());
    if (doc.GetParseError())
    {
      XBMC->Log(LOG_ERROR, "Parsing cache file [%s] failed. -> Delete", path);
      XBMC->DeleteFile(path);
    }

    if (!IsStillValid(doc))
    {
      XBMC->Log(LOG_DEBUG, "Deleting expired cache file [%s].", path);
      if (!XBMC->DeleteFile(path))
      {
        XBMC->Log(LOG_DEBUG, "Deletion of file [%s] failed.", path);
      }
    }

  }

  XBMC->FreeDirectory(items, itemCount);
}

bool Cache::IsStillValid(const Value& cache)
{
  time_t validUntil = static_cast<time_t>(cache["validUntil"].GetUint64());
  time_t current_time;
  time(&current_time);
  return validUntil >= current_time;
}
