#pragma once

#include <sstream>
#include <string>
#include <vector>
#include "yajl/yajl_tree.h"

class Utils
{
public:
  static std::string GetFilePath(std::string strPath, bool bUserPath = true);
  static std::string UrlEncode(const std::string &string);
  static double StringToDouble(const std::string &value);
  static int StringToInt(const std::string &value);
  static int GetIntFromJsonValue(yajl_val &value, int defaultValue = 0);
  static double GetDoubleFromJsonValue(yajl_val &value,
      double defaultValue = 0);
};
