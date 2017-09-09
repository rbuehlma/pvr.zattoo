#include "Curl.h"
#include "client.h"

using namespace std;
using namespace ADDON;

Curl::Curl()
{
}

Curl::~Curl()
{
}

string Curl::Post(string url, string postData, int &statusCode)
{
  void* file = XBMC->CURLCreate(url.c_str());
  if (!file)
  {
    statusCode = -1;
    return "";
  }

  XBMC->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "acceptencoding",
      "gzip");
  if (postData.size() != 0)
  {
    string base64 = Base64Encode((const unsigned char *) postData.c_str(),
        postData.size(), false);
    XBMC->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "postdata",
        base64.c_str());
  }

  if (!cookies.empty())
  {
    XBMC->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "cookie",
        cookies.c_str());
  }

  if (!XBMC->CURLOpen(file, XFILE::READ_NO_CACHE))
  {
    statusCode = 403; //Fake statusCode for now
    return "";
  }

  char *cookiesPtr = XBMC->GetFilePropertyValue(file,
      XFILE::FILE_PROPERTY_RESPONSE_HEADER, "set-cookie");
  if (cookiesPtr && *cookiesPtr)
  {
    cookies = cookiesPtr;
    std::string::size_type paramPos = cookies.find(';');
    if (paramPos != std::string::npos)
      cookies.resize(paramPos);
  }
  XBMC->FreeString(cookiesPtr);

  // read the file
  static const unsigned int CHUNKSIZE = 16384;
  char buf[CHUNKSIZE + 1];
  size_t nbRead;
  string body = "";
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
