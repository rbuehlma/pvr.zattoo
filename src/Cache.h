#include <string>
#include "rapidjson/document.h"

using namespace rapidjson;

class Cache
{
public:
  static bool Read(std::string key, std::string& data);
  static void Write(std::string key, const std::string &data,
      time_t validUntil);
  static void Cleanup();
private:
  static bool IsStillValid(const Value& cache);
  static time_t lastCleanup;
};
