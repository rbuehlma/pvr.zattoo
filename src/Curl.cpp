#include "Curl.h"
#include <utility>
#include "client.h"
#include "Utils.h"

using namespace ADDON;

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
  void* file = XBMC->CURLCreate(url.c_str());
  if (!file)
  {
    statusCode = -1;
    return "";
  }

  XBMC->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "customrequest",
      action.c_str());

  XBMC->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "acceptencoding",
      "gzip");
  if (!postData.empty())
  {
    std::string base64 = Base64Encode((const unsigned char *) postData.c_str(),
        postData.size(), false);
    XBMC->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "postdata",
        base64.c_str());
  }

  for (auto const &entry : m_headers)
  {
    XBMC->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, entry.first.c_str(),
        entry.second.c_str());
  }

  for (auto const &entry : m_options)
  {
    XBMC->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, entry.first.c_str(),
        entry.second.c_str());
  }

  if (!XBMC->CURLOpen(file, XFILE::READ_NO_CACHE))
  {
    statusCode = 403; //Fake statusCode for now
    return "";
  }
  int numValues;
  char **cookiesPtr = XBMC->GetFilePropertyValues(file,
      XFILE::FILE_PROPERTY_RESPONSE_HEADER, "set-cookie", &numValues);

  for (int i = 0; i < numValues; i++)
  {
    char *cookiePtr = cookiesPtr[i];
    if (cookiePtr && *cookiePtr)
    {
      std::string cookie = cookiePtr;
      std::string::size_type paramPos = cookie.find(';');
      if (paramPos != std::string::npos)
        cookie.resize(paramPos);
      std::vector<std::string> parts = Utils::SplitString(cookie, '=', 2);
      if (parts.size() != 2)
      {
        continue;
      }
      m_cookies[parts[0]] = parts[1];
      XBMC->Log(LOG_DEBUG, "Got cookie: %s.", parts[0].c_str());
    }
  }
  XBMC->FreeStringArray(cookiesPtr, numValues);
  
  char *tmp = XBMC->GetFilePropertyValue(file,
      XFILE::FILE_PROPERTY_RESPONSE_HEADER, "Location");
  m_location = tmp != nullptr ? tmp : "";
  
  XBMC->FreeString(tmp);


  // read the file
  static const unsigned int CHUNKSIZE = 16384;
  char buf[CHUNKSIZE + 1];
  ssize_t nbRead;
  std::string body;
  while ((nbRead = XBMC->ReadFile(file, buf, CHUNKSIZE)) > 0 && ~nbRead)
  {
    buf[nbRead] = 0x0;
    body += buf;
  }

  XBMC->CloseFile(file);
  statusCode = 200;
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
