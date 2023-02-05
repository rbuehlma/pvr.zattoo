#include "Session.h"
#include <kodi/AddonBase.h>
#include <kodi/General.h>
#include "ZatData.h"
#include "rapidjson/document.h"

using namespace rapidjson;

Session::Session(HttpClient* httpClient, ZatData* zatData, CSettings* settings, ParameterDB *parameterDB):
  m_httpClient(httpClient),
  m_zatData(zatData),
  m_settings(settings),
  m_parameterDB(parameterDB)
{
  SetProviderUrl();
}

Session::~Session()
{
  m_running = false;
  if (m_thread.joinable())
    m_thread.join();  
}

ADDON_STATUS Session::Start()
{
  if (!m_settings->VerifySettings()) {
    return ADDON_STATUS_NEED_SETTINGS;
  }
  
  m_running = true;
  m_thread = std::thread([&] { LoginThread(); });
  return ADDON_STATUS_OK;
}

void Session::LoginThread() {
  while (m_running) {
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    if (!m_running) {
      return;
    }
    
    if (m_isConnected) {
      continue;
    }
    
    if (m_nextLoginAttempt > std::time(0)) {
      continue;
    }
    
    m_zatData->UpdateConnectionState("Zattoo Connecting", PVR_CONNECTION_STATE_CONNECTING, "");
    
    std::string username = m_settings->GetZatUsername();
    std::string password = m_settings->GetZatPassword();
    
    kodi::Log(ADDON_LOG_DEBUG, "Login Zattoo");
    if (Login(username, password))
    {
      if (!m_zatData->SessionInitialized()) {
        m_nextLoginAttempt = std::time(0) + 60;
        continue;
      }
      m_isConnected = true;
      kodi::Log(ADDON_LOG_DEBUG, "Login done");
      m_zatData->UpdateConnectionState("Zattoo connection established", PVR_CONNECTION_STATE_CONNECTED, "");
      kodi::QueueNotification(QUEUE_INFO, "", kodi::addon::GetLocalizedString(30202));      
    }
    else
    {
      kodi::Log(ADDON_LOG_ERROR, "Login failed");
      m_nextLoginAttempt = std::time(0) + 3600;
      kodi::QueueNotification(QUEUE_ERROR, "", kodi::addon::GetLocalizedString(30201));
    }
  }
}

bool Session::Login(std::string u, std::string p)
{
  
  if (!LoadAppId())
  {
    Reset();
    m_zatData->UpdateConnectionState("Failed to get appId", PVR_CONNECTION_STATE_SERVER_UNREACHABLE, kodi::addon::GetLocalizedString(30203));
    m_nextLoginAttempt = std::time(0) + 60;
    return false;
  }
  
  SendHello();
  
  int statusCode;
  std::string jsonString = m_httpClient->HttpGet(m_providerUrl + "/zapi/v3/session", statusCode);
  
  if (statusCode != 200)
  {
    Reset();
    m_zatData->UpdateConnectionState("Not reachable", PVR_CONNECTION_STATE_SERVER_UNREACHABLE, kodi::addon::GetLocalizedString(30203));
    m_nextLoginAttempt = std::time(0) + 60;
    return false;
  }

  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc["active"].GetBool())
  {
    kodi::Log(ADDON_LOG_ERROR, "Initialize session failed.");
    m_nextLoginAttempt = std::time(0) + 300;
    return false;
  }
  
  if (doc["account"].IsNull())
  {
     kodi::Log(ADDON_LOG_DEBUG, "Need to login.");
     m_httpClient->ClearSession();
     
     kodi::Log(ADDON_LOG_DEBUG, "Try to login.");

     std::ostringstream dataStream;
     dataStream << "login=" << Utils::UrlEncode(m_settings->GetZatUsername()) << "&password="
         << Utils::UrlEncode(m_settings->GetZatPassword()) << "&format=json&remember=true";
     int statusCode;
     std::string jsonString = m_httpClient->HttpPost(m_providerUrl + "/zapi/v3/account/login", dataStream.str(), statusCode);
     doc.Parse(jsonString.c_str());     
     
     if (doc.GetParseError() || !doc["active"].GetBool())
     {
       kodi::Log(ADDON_LOG_ERROR, "Login failed.");
           m_nextLoginAttempt = std::time(0) + 300;
       Reset();
       return false;
     }
     else
     {
       kodi::Log(ADDON_LOG_DEBUG, "Login was successful.");
     }
  }
  
  const Value& account = doc["account"];
  const Value& nonlive = doc["nonlive"];
  
  m_countryCode = Utils::JsonStringOrEmpty(doc, "current_country");
  m_serviceRegionCountry = Utils::JsonStringOrEmpty(account, "service_country");
  m_recallEnabled = Utils::JsonStringOrEmpty(nonlive, "replay_availability") == "available";
  m_recordingEnabled = Utils::JsonBoolOrFalse(nonlive, "recording_number_limit");
  kodi::Log(ADDON_LOG_INFO, "Current country code: %s", m_countryCode.c_str());
  kodi::Log(ADDON_LOG_INFO, "Service region country: %s",
      m_serviceRegionCountry.c_str());
  kodi::Log(ADDON_LOG_INFO, "Recall are %s",
      m_recallEnabled ? "enabled" : "disabled");
  kodi::Log(ADDON_LOG_INFO, "Recordings are %s",
      m_recordingEnabled ? "enabled" : "disabled");
  m_powerHash = Utils::JsonStringOrEmpty(doc, "power_guide_hash");
  
  
  return true;
}

bool Session::LoadAppId()
{
  if (!m_appToken.empty()) {
    return true;
  }
  
  if (!LoadAppTokenFromTokenJson("token.json")) {
    int statusCode;
    std::string html = m_httpClient->HttpGet(m_providerUrl + "/login", statusCode);
  
    if (!LoadAppTokenFromHtml(html)) {
      if (!LoadAppTokenFromJson(html)) {
        m_appToken = m_parameterDB->Get("appToken");
        return !m_appToken.empty();
      }
    }
  }
  m_parameterDB->Set("appToken", m_appToken);
  return true;
}

bool Session::LoadAppTokenFromTokenJson(std::string tokenJsonPath) {
  int statusCode;
  std::string jsonString = m_httpClient->HttpGet(m_providerUrl + "/" + tokenJsonPath, statusCode);

  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError() || !doc["success"].GetBool())
  {
    kodi::Log(ADDON_LOG_DEBUG, "Failed to load json from %s", tokenJsonPath.c_str());
    return false;
  }

  m_appToken = doc["session_token"].GetString();
  return true;
}

bool Session::LoadAppTokenFromHtml(std::string html) {
  size_t basePos = html.find("window.appToken = '") + 19;
  if (basePos > 19)
  {
    size_t endPos = html.find("'", basePos);
    m_appToken = html.substr(basePos, endPos - basePos);
    return true;
  }
  return false;
}

bool Session::LoadAppTokenFromJson(std::string html) {
  size_t basePos = html.find("src=\"/app-") + 5;
  if (basePos < 6) {
    kodi::Log(ADDON_LOG_ERROR, "Unable to find app-*.js");
    return false;
  }
  size_t endPos = html.find("\"", basePos);
  std::string appJsPath = html.substr(basePos, endPos - basePos);
  int statusCode;
  std::string content = m_httpClient->HttpGet(m_providerUrl + appJsPath, statusCode);

  basePos = content.find("\"token-") + 1;
  if (basePos < 6) {
    kodi::Log(ADDON_LOG_ERROR, "Unable to find token-*.json in %s", appJsPath.c_str());
    return false;
  }
  endPos = content.find("\"", basePos);
  std::string tokenJsonPath = content.substr(basePos, endPos - basePos);
  return LoadAppTokenFromTokenJson(tokenJsonPath);
}

bool Session::SendHello()
{
  std::string uuid = m_httpClient->GetUUID();
  kodi::Log(ADDON_LOG_DEBUG, "Send hello.");
  std::ostringstream dataStream;
  dataStream << "uuid=" << uuid << "&lang=en&app_version=3.2038.0&format=json&client_app_token="
      << m_appToken;
  int statusCode;
  std::string jsonString = m_httpClient->HttpPost(m_providerUrl + "/zapi/v3/session/hello", dataStream.str(), statusCode);

  Document doc;
  doc.Parse(jsonString.c_str());
  if (!doc.GetParseError() && doc["active"].GetBool())
  {
    kodi::Log(ADDON_LOG_DEBUG, "Hello was successful.");
    return true;
  }
  else
  {
    kodi::Log(ADDON_LOG_INFO, "Hello failed.");
    return false;
  }
}

void Session::Reset()
{
  SetProviderUrl();
  m_isConnected = false;
  m_httpClient->ClearSession();
  m_appToken = "";
  m_parameterDB->Set("appToken", m_appToken);
  m_zatData->UpdateConnectionState("Zattoo session expired", PVR_CONNECTION_STATE_CONNECTING, "");
}

void Session::ErrorStatusCode (int statusCode) {
  if (statusCode == 403)
  {
    kodi::Log(ADDON_LOG_ERROR, "Got 403. Try to re-init session.");
    Reset();
  }
}

void Session::SetProviderUrl() {
  switch (m_settings->GetProvider())
    {
    case 1:
      m_providerUrl = "https://www.netplus.tv";
      break;
    case 2:
      m_providerUrl = "https://mobiltv.quickline.com";
      break;
    case 3:
      m_providerUrl = "https://tvplus.m-net.de";
      break;
    case 4:
      m_providerUrl = "https://player.waly.tv";
      break;
    case 5:
      m_providerUrl = "https://www.meinewelt.cc";
      break;
    case 6:
      m_providerUrl = "https://www.bbv-tv.net";
      break;
    case 7:
      m_providerUrl = "https://www.vtxtv.ch";
      break;
    case 8:
      m_providerUrl = "https://www.myvisiontv.ch";
      break;
    case 9:
      m_providerUrl = "https://iptv.glattvision.ch";
      break;
    case 10:
      m_providerUrl = "https://www.saktv.ch";
      break;
    case 11:
      m_providerUrl = "https://nettv.netcologne.de";
      break;
    case 12:
      m_providerUrl = "https://tvonline.ewe.de";
      break;
    case 13:
      m_providerUrl = "https://www.quantum-tv.com";
      break;
    case 14:
      m_providerUrl = "https://tv.salt.ch";
      break;
    case 15:
      m_providerUrl = "https://tvonline.swb-gruppe.de";
      break;
    case 16:
      m_providerUrl = "https://www.1und1.tv";
      break;
    default:
      m_providerUrl = "https://zattoo.com";
    }
}
