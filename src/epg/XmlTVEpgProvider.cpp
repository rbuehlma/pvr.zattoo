#include "XmlTVEpgProvider.h"
#ifdef TARGET_WINDOWS
#include "../windows.h"
#endif

#include <sstream>
#include <algorithm>
#include <tinyxml2.h>
#include <time.h>

#include <kodi/Filesystem.h>
#include <kodi/General.h>

using namespace tinyxml2;

const time_t minimumUpdateDelay = 600;
const std::string TEMP_FILE_NAME = "xmltv_tmp_epg.xml";

XmlTVEpgProvider::XmlTVEpgProvider(kodi::addon::CInstancePVRClient *addon, std::string xmlFile, std::map<std::string, ZatChannel>& channelsByCid, EpgProvider &fallbackProvider) :
  m_xmlFile(xmlFile), m_lastUpdate(0), m_addon(addon), m_channelsByCid(channelsByCid), m_fallbackProvider(fallbackProvider),
  EpgProvider(addon)
{
  if (!kodi::vfs::FileExists(m_xmlFile, true))
  {
    kodi::Log(ADDON_LOG_ERROR,
        "XMLTV: Xml file for additional guide data not found: %s",
        m_xmlFile.c_str());
  }
  else
  {
    kodi::Log(ADDON_LOG_DEBUG, "XMLTV: Using xml file for additional guide data: %s",
        m_xmlFile.c_str());
  }
}

XmlTVEpgProvider::~XmlTVEpgProvider()
{
  m_xmlFile = "";
}

bool XmlTVEpgProvider::LoadEPGForChannel(ZatChannel &zatChannel, time_t iStart, time_t iEnd)
{
  bool dataFromXML = LoadEPGFromFile(zatChannel);
  if (dataFromXML) {
    return true;
  }
  return m_fallbackProvider.LoadEPGForChannel(zatChannel, iStart, iEnd);
}


bool XmlTVEpgProvider::LoadEPGFromFile(ZatChannel &zatChannel)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  if (!isUpdateDue()) {
    lock.unlock();
    return m_loadedChannels.find(zatChannel.cid) != m_loadedChannels.end();
  }

  m_loadedChannels.clear();
  
  std::string tmpFile = m_addon->UserPath() + TEMP_FILE_NAME;
  if (!copyFileToTempFileIfNewer(tmpFile)) {
    return m_loadedChannels.find(zatChannel.cid) != m_loadedChannels.end();
  }

  kodi::vfs::CFile file;

  XMLDocument doc;
  if (doc.LoadFile(tmpFile.c_str()) != XML_SUCCESS)
  {
    kodi::Log(ADDON_LOG_ERROR, "XMLTV: failed to open xml-file.");
    lock.unlock();
    return false;
  }
  XMLElement* tv = doc.FirstChildElement("tv");
  if (!tv)
  {
    kodi::Log(ADDON_LOG_ERROR, "XMLTV: no 'tv' section in xml-file.");
    lock.unlock();
    return false;
  }
  XMLElement* programme = tv->FirstChildElement("programme");
  while (programme)
  {
    kodi::addon::PVREPGTag tag;
    tag.SetSeriesNumber(EPG_TAG_INVALID_SERIES_EPISODE);
    tag.SetEpisodeNumber(EPG_TAG_INVALID_SERIES_EPISODE);
    tag.SetEpisodePartNumber(EPG_TAG_INVALID_SERIES_EPISODE);

    XMLElement* title = programme->FirstChildElement("title");
    const char *start = programme->Attribute("start");
    const char *stop = programme->Attribute("stop");
    const char *channel = programme->Attribute("channel");

    if (!title || !start || !stop || !channel)
    {
      programme = programme->NextSiblingElement("programme");
      continue;
    }

    m_loadedChannels.insert(channel);

    auto channelIterator = m_channelsByCid.find(channel);
    if (channelIterator == m_channelsByCid.end()) {
      programme = programme->NextSiblingElement("programme");
      continue;
    }
    ZatChannel zatChannel = channelIterator->second;

    XMLElement* subTitle = programme->FirstChildElement("sub-title");
    XMLElement* description = programme->FirstChildElement("desc");
    XMLElement* category = programme->FirstChildElement("category");
    tag.SetStartTime(StringToTime(start));
    tag.SetEndTime(StringToTime(stop));
    tag.SetUniqueBroadcastId(tag.GetStartTime());
    tag.SetTitle(title->GetText());
    tag.SetUniqueChannelId(zatChannel.iUniqueId);
    if (subTitle)
    {
      const char *sub = subTitle->GetText();
      tag.SetEpisodeName(sub);
    }
    if (description)
    {
      const char *desc = description->GetText();
      tag.SetPlot(desc);
      tag.SetPlotOutline(desc);
    }
    if (category)
    {
      const char *genre = category->GetText();
      tag.SetGenreType(EPG_GENRE_USE_STRING);
      tag.SetGenreDescription(genre);
    }

    // episode is deliverd in format "1 . 2 .". Try to parse that format
    XMLElement* episodeItem = programme->FirstChildElement("episode-num");
    if (episodeItem != nullptr) {
      std::string episode = episodeItem->GetText();
      episode.erase (std::remove(episode.begin(), episode.end(), ' '), episode.end());
      std::replace(episode.begin(), episode.end(), '.', ' ');
      std::stringstream ss(episode);
      int number;
      if (ss >> number) {
        tag.SetSeriesNumber(number + 1);
        if (ss >> number) {
          tag.SetEpisodeNumber(number + 1);
        }
      }
    }

    XMLElement* ratingItem = programme->FirstChildElement("rating");
    if (ratingItem != nullptr) {
      XMLElement* valuteItem = ratingItem->FirstChildElement("value");
      if (valuteItem != nullptr) {
        std::string ratingString = valuteItem->GetText();
        kodi::Log(ADDON_LOG_ERROR, "Rating: %s", ratingString.c_str());
        try {
          tag.SetParentalRating(std::stoi(ratingString.c_str()));
        } catch (std::invalid_argument& e) {
          // ignore
        }
      }
    }

    XMLElement* iconItem = programme->FirstChildElement("icon");
    if (iconItem != nullptr) {
      tag.SetIconPath(iconItem->Attribute("src", 0));
    }

    XMLElement* starRatingItem = programme->FirstChildElement("star-rating");
    if (starRatingItem != nullptr) {
      XMLElement* valuteItem = starRatingItem->FirstChildElement("value");
      if (valuteItem != nullptr) {
        std::string ratingString = valuteItem->GetText();
        tag.SetStarRating(std::stoi(ratingString));
      }
    }

    m_addon->EpgEventStateChange(tag, EPG_EVENT_CREATED);

    programme = programme->NextSiblingElement("programme");

  }
  return m_loadedChannels.find(zatChannel.cid) != m_loadedChannels.end();
}

bool XmlTVEpgProvider::isUpdateDue() {
  time_t currentTime;
  time(&currentTime);
  if (m_lastUpdate + minimumUpdateDelay > currentTime) {
    return false;
  }
  m_lastUpdate = currentTime;
  return true;
}

time_t XmlTVEpgProvider::StringToTime(const std::string &timeString)
{
  struct tm tm
  { };
  strptime(timeString.c_str(), "%Y%m%d%H%M%S", &tm);
  time_t ret = timegm(&tm);
  return ret;
}

bool XmlTVEpgProvider::copyFileToTempFileIfNewer(std::string &tempFile) {
  if (!m_xmlFile.empty() && !kodi::vfs::FileExists(m_xmlFile, true))
  {
    return false;
  }
  kodi::vfs::FileStatus fileStatus;
  kodi::vfs::StatFile(m_xmlFile, fileStatus);
  if (fileStatus.GetModificationTime() == m_lastFileModification) {
    return false;
  }

  m_lastFileModification = fileStatus.GetModificationTime();
  kodi::Log(ADDON_LOG_INFO, "XMLTV file %s has been updated. Loading new data.", m_xmlFile.c_str());
  return kodi::vfs::CopyFile(m_xmlFile, tempFile);
}

