#include "Curl.h"
#include <kodi/Filesystem.h>
#include <utility>
#include "../Utils.h"

Curl::Curl()
= default;

Curl::~Curl()
= default;

std::string Curl::GetCookie(const std::string& name)
{
  if (m_cookies.find(name) == m_cookies.end())
  {
    return "";
  }
  return m_cookies[name];
}

void Curl::AddHeader(const std::string& name, const std::string& value)
{
  m_headers[name] = value;
}

void Curl::AddOption(const std::string& name, const std::string& value)
{
  m_options[name] = value;
}

void Curl::ResetHeaders()
{
  m_headers.clear();
}

std::string Curl::Delete(const std::string& url, int &statusCode)
{
  return Request("DELETE", url, "", statusCode);
}

std::string Curl::Get(const std::string& url, int &statusCode)
{
  return Request("GET", url, "", statusCode);
}

std::string Curl::Post(const std::string& url, const std::string& postData, int &statusCode)
{
  return Request("POST", url, postData, statusCode);
}

std::string Curl::Request(const std::string& action, const std::string& url, const std::string& postData,
    int &statusCode)
{
  kodi::vfs::CFile file;
  if (!file.CURLCreate(url))
  {
    statusCode = -1;
    return "";
  }

  file.CURLAddOption(ADDON_CURL_OPTION_PROTOCOL, "customrequest", action);
  file.CURLAddOption(ADDON_CURL_OPTION_HEADER, "acceptencoding", "gzip");
  if (!postData.empty())
  {
    std::string base64 = Base64Encode((const unsigned char *) postData.c_str(),
        postData.size(), false);
    file.CURLAddOption(ADDON_CURL_OPTION_PROTOCOL, "postdata", base64);
  }

  for (auto const &entry : m_headers)
  {
    file.CURLAddOption(ADDON_CURL_OPTION_HEADER, entry.first.c_str(), entry.second);
  }

  for (auto const &entry : m_options)
  {
    file.CURLAddOption(ADDON_CURL_OPTION_PROTOCOL, entry.first.c_str(), entry.second);
  }
  file.CURLAddOption(ADDON_CURL_OPTION_PROTOCOL, "failonerror", "false");

  if (!file.CURLOpen(ADDON_READ_NO_CACHE))
  {
    statusCode = -2;
    return "";
  }
  
  std::string proto = file.GetPropertyValue(ADDON_FILE_PROPERTY_RESPONSE_PROTOCOL, "");
  std::string::size_type posResponseCode = proto.find(' ');
  if (posResponseCode != std::string::npos)
    statusCode = atoi(proto.c_str() + (posResponseCode + 1));
  
  if (statusCode >= 400) {
    return "";
  }

  const std::vector<std::string> values = file.GetPropertyValues(ADDON_FILE_PROPERTY_RESPONSE_HEADER, "set-cookie");
  for (const auto& value : values)
  {
    std::string cookie = value;
    std::string::size_type paramPos = cookie.find(';');
    if (paramPos != std::string::npos)
      cookie.resize(paramPos);
    std::vector<std::string> parts = Utils::SplitString(cookie, '=', 2);
    if (parts.size() != 2)
    {
      continue;
    }
    m_cookies[parts[0]] = parts[1];
    kodi::Log(ADDON_LOG_DEBUG, "Got cookie: %s.", parts[0].c_str());
  }

  m_location = file.GetPropertyValue(ADDON_FILE_PROPERTY_RESPONSE_HEADER, "Location");

  // read the file
  static const unsigned int CHUNKSIZE = 16384;
  char buf[CHUNKSIZE + 1];
  ssize_t nbRead;
  std::string body;
  while ((nbRead = file.Read(buf, CHUNKSIZE)) > 0 && ~nbRead)
  {
    buf[nbRead] = 0x0;
    body += buf;
  }

  return body;
}

std::string Curl::Base64Encode(unsigned char const* in, unsigned int in_len,
    bool urlEncode)
{
  std::string ret;
  int i(3);
  unsigned char c_3[3];
  unsigned char c_4[4];

  const char *to_base64 =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  while (in_len)
  {
    i = in_len > 2 ? 3 : in_len;
    in_len -= i;
    c_3[0] = *(in++);
    c_3[1] = i > 1 ? *(in++) : 0;
    c_3[2] = i > 2 ? *(in++) : 0;

    c_4[0] = (c_3[0] & 0xfc) >> 2;
    c_4[1] = ((c_3[0] & 0x03) << 4) + ((c_3[1] & 0xf0) >> 4);
    c_4[2] = ((c_3[1] & 0x0f) << 2) + ((c_3[2] & 0xc0) >> 6);
    c_4[3] = c_3[2] & 0x3f;

    for (int j = 0; (j < i + 1); ++j)
    {
      if (urlEncode && to_base64[c_4[j]] == '+')
        ret += "%2B";
      else if (urlEncode && to_base64[c_4[j]] == '/')
        ret += "%2F";
      else
        ret += to_base64[c_4[j]];
    }
  }
  while ((i++ < 3))
    ret += urlEncode ? "%3D" : "=";
  return ret;
}
