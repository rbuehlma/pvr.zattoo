#include "XmlTV.h"
#ifdef TARGET_WINDOWS
#include "windows.h"
#endif

#include <sstream>
#include <algorithm>
#include <tinyxml2.h>
#include <time.h>

#include <kodi/Filesystem.h>
#include <kodi/General.h>

using namespace tinyxml2;

const time_t minimumUpdateDelay = 600;

XmlTV::XmlTV(kodi::addon::CInstancePVRClient& instance, std::string xmlFile) :
  m_xmlFile(xmlFile), m_lastUpdate(0), m_instance(instance)
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

XmlTV::~XmlTV()
{
  m_xmlFile = "";
}

bool XmlTV::GetEPGForChannel(const std::string &cid, std::map<std::string, ZatChannel> &channelsByCid)
{
  if (!m_xmlFile.empty() && !kodi::vfs::FileExists(m_xmlFile, true))
  {
    return false;
  }

  std::unique_lock<std::mutex> lock(m_mutex);

  if (!isUpdateDue()) {
    lock.unlock();
    return m_loadedChannels.find(cid) != m_loadedChannels.end();
  }

  m_loadedChannels.clear();

  std::string xml;
  kodi::vfs::CFile file;
  int bufferLen = 1024 * 1024;
  char *buffer = new char[bufferLen];
  if (!file.OpenFile(m_xmlFile, 0))
  {
    kodi::Log(ADDON_LOG_ERROR, "XMLTV: failed to open xml-file.");
    lock.unlock();
    return false;
  }

  while (int dataRead = file.Read(buffer, bufferLen - 1) > 0) {
    buffer[dataRead] = 0;
    xml += buffer;
  }
  file.Close();
  delete[] buffer;

  XMLDocument doc;
  if (doc.Parse(xml.c_str(), xml.length()) != XML_SUCCESS)
  {
    kodi::Log(ADDON_LOG_ERROR, "XMLTV: failed to parse xml-file.");
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

    auto channelIterator = channelsByCid.find(channel);
    if (channelIterator == channelsByCid.end()) {
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

    m_instance.EpgEventStateChange(tag, EPG_EVENT_CREATED);

    programme = programme->NextSiblingElement("programme");

  }
  return m_loadedChannels.find(cid) != m_loadedChannels.end();
}

bool XmlTV::isUpdateDue() {
  time_t currentTime;
  time(&currentTime);
  if (m_lastUpdate + minimumUpdateDelay > currentTime) {
    return false;
  }
  m_lastUpdate = currentTime;
  return true;
}

time_t XmlTV::StringToTime(const std::string &timeString)
{
  struct tm tm
  { };
  strptime(timeString.c_str(), "%Y%m%d%H%M%S", &tm);
  time_t ret = timegm(&tm);
  return ret;
}

