#include "Utils.h"

#include <algorithm>
#include <iomanip>
#include <iterator>
#include <sstream>

#include "p8-platform/os.h"

#include "client.h"

using namespace ADDON;

std::string Utils::GetFilePath(std::string strPath, bool bUserPath)
{
  return (bUserPath ? g_strUserPath : g_strClientPath) + PATH_SEPARATOR_CHAR
      + strPath;
}

// http://stackoverflow.com/a/17708801
std::string Utils::UrlEncode(const std::string &value)
{
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;

  for (std::string::const_iterator i = value.begin(), n = value.end(); i != n;
      ++i)
  {
    std::string::value_type c = (*i);

    // Keep alphanumeric and other accepted characters intact
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
    {
      escaped << c;
      continue;
    }

    // Any other characters are percent-encoded
    escaped << '%' << std::setw(2) << int((unsigned char) c);
  }

  return escaped.str();
}

double Utils::StringToDouble(const std::string &value)
{
  std::istringstream iss(value);
  double result;

  iss >> result;

  return result;
}

int Utils::StringToInt(const std::string &value)
{
  return (int) StringToDouble(value);
}

std::vector<std::string> Utils::SplitString(const std::string &str,
    const char &delim, int maxParts)
{
  typedef std::string::const_iterator iter;
  iter beg = str.begin();
  std::vector < std::string > tokens;

  while (beg != str.end())
  {
    if (maxParts == 1)
    {
      tokens.push_back(std::string(beg, str.end()));
      break;
    }
    maxParts--;
    iter temp = find(beg, str.end(), delim);
    if (beg != str.end())
      tokens.push_back(std::string(beg, temp));
    beg = temp;
    while ((beg != str.end()) && (*beg == delim))
      beg++;
  }

  return tokens;
}

std::string Utils::ReadFile(const std::string path)
{
  void* file;
  file = XBMC->CURLCreate(path.c_str());
  if (!file || !XBMC->CURLOpen(file, 0))
  {
    XBMC->Log(LOG_ERROR, "Failed to open file [%s].", path.c_str());
    return "";
  }

  char buf[1025];
  size_t nbRead;
  std::string content;
  while ((nbRead = XBMC->ReadFile(file, buf, 1024)) > 0)
  {
    buf[nbRead] = 0;
    content.append(buf);
  }
  XBMC->CloseFile(file);
  return content;

}
