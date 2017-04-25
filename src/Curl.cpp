#include "Curl.h"
#include "client.h"

using namespace std;
using namespace ADDON;

static const string setCookie = "Set-Cookie: ";
static const string beakerSesssionId = "beaker.session.id=";

string Curl::cookie = "";

Curl::Curl()
{
}

Curl::~Curl() {
}

size_t Curl::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

size_t Curl::HeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata)
{
  std::string header(buffer, 0, nitems);
  if (strncmp(header.c_str(), (setCookie + beakerSesssionId).c_str(), (setCookie + beakerSesssionId).size()) == 0) {
    cookie = header.substr(setCookie.size(), string::npos);
    cookie = cookie.substr(0, cookie.find(";", 0));
  }
  return nitems * size;
}

string Curl::Post(string url, string postData, int &statusCode) {
  CURLcode res;
  CURL *curl;
  string readBuffer;
  curl = curl_easy_init();
  if (!curl) {
      XBMC->Log(LOG_ERROR, "curl_easy_init failed.");
      return "";
  }
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  if (!postData.empty()) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
  }
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Curl::WriteCallback);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, Curl::HeaderCallback);
  if (!cookie.empty()) {
    curl_easy_setopt(curl, CURLOPT_COOKIE, cookie.c_str());
  }

  res = curl_easy_perform(curl);
  if(res != CURLE_OK) {
    XBMC->Log(LOG_ERROR, "Http request failed");
    curl_easy_cleanup(curl);
    return "";
  }
  curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &statusCode);
  if (statusCode != 200) {
    XBMC->Log(LOG_ERROR, "HTTP failed with status code %i.", statusCode);
    curl_easy_cleanup(curl);
    return "";
  }
  curl_easy_cleanup(curl);
  return readBuffer;
}
