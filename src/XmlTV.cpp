#include <sstream>
#include <algorithm>
#include "XmlTV.h"
#include <tinyxml2.h>
#include <time.h>

#include "client.h"

using namespace tinyxml2;
using namespace ADDON;

const time_t minimumUpdateDelay = 600;

XmlTV::XmlTV(std::string xmlFile) :
  m_xmlFile(xmlFile), m_lastUpdate(0)
{
  if (!XBMC->FileExists(m_xmlFile.c_str(), true))
  {
    XBMC->Log(LOG_DEBUG,
        "XMLTV: Xml file for additional guide data not found: %s",
        m_xmlFile.c_str());
  }
  else
  {
    XBMC->Log(LOG_DEBUG, "XMLTV: Using xml file for additional guide data: %s",
        m_xmlFile.c_str());
  }
}

XmlTV::~XmlTV()
{
  m_xmlFile = "";
}

bool XmlTV::GetEPGForChannel(const std::string &cid, std::map<std::string, ZatChannel> &channelsByCid)
{
  if (!m_xmlFile.empty() && !XBMC->FileExists(m_xmlFile.c_str(), true))
  {
    return false;
  }
  
  if (!m_mutex.Lock())
  {
    XBMC->Log(LOG_ERROR,
        "XmlTV::GetEPGForChannel : Could not lock mutex");
    return false;
  }
  
  if (!isUpdateDue()) {
    m_mutex.Unlock();
    return m_loadedChannels.find(cid) != m_loadedChannels.end();
  }
  
  m_loadedChannels.clear();

  XMLDocument doc;
  if (doc.LoadFile(m_xmlFile.c_str()) != XML_SUCCESS)
  {
    XBMC->Log(LOG_ERROR, "XMLTV: failed to parse xml-file.");
    m_mutex.Unlock();
    return false;
  }
  XMLElement* tv = doc.FirstChildElement("tv");
  if (!tv)
  {
    XBMC->Log(LOG_ERROR, "XMLTV: no 'tv' section in xml-file.");
    m_mutex.Unlock();
    return false;
  }
  XMLElement* programme = tv->FirstChildElement("programme");
  while (programme)
  {
    EPG_TAG tag;
    memset(&tag, 0, sizeof(EPG_TAG));
    tag.iSeriesNumber = EPG_TAG_INVALID_SERIES_EPISODE;
    tag.iEpisodeNumber = EPG_TAG_INVALID_SERIES_EPISODE;
    tag.iEpisodePartNumber = EPG_TAG_INVALID_SERIES_EPISODE;

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
    tag.startTime = StringToTime(start);
    tag.endTime = StringToTime(stop);
    tag.iUniqueBroadcastId = tag.startTime;
    tag.strTitle = title->GetText();
    tag.iUniqueChannelId = zatChannel.iUniqueId;
    if (subTitle)
    {
      const char *sub = subTitle->GetText();
      tag.strEpisodeName = sub;
    }
    if (description)
    {
      const char *desc = description->GetText();
      tag.strPlot = desc;
      tag.strPlotOutline = desc;
    }
    if (category)
    {
      const char *genre = category->GetText();
      tag.iGenreType = EPG_GENRE_USE_STRING;
      tag.strGenreDescription = genre;
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
        tag.iSeriesNumber = number + 1;
        if (ss >> number) {
          tag.iEpisodeNumber = number + 1;
        }
      }
    }
    
    XMLElement* ratingItem = programme->FirstChildElement("rating");
    if (ratingItem != nullptr) {
      XMLElement* valuteItem = ratingItem->FirstChildElement("value");
      if (valuteItem != nullptr) {
        std::string ratingString = valuteItem->GetText();
        XBMC->Log(LOG_ERROR, "Rating: %s", ratingString.c_str());
        try {
          tag.iParentalRating  = std::stoi(ratingString.c_str());
        } catch (std::invalid_argument& e) {
          // ignore
        }
      }
    }
    
    XMLElement* iconItem = programme->FirstChildElement("icon");
    if (iconItem != nullptr) {
      tag.strIconPath = iconItem->Attribute("src", 0);
    }
    
    XMLElement* starRatingItem = programme->FirstChildElement("star-rating");
    if (starRatingItem != nullptr) {
      XMLElement* valuteItem = starRatingItem->FirstChildElement("value");
      if (valuteItem != nullptr) {
        std::string ratingString = valuteItem->GetText();
        tag.iStarRating  = std::stoi(ratingString);
      }
    }

    PVR->EpgEventStateChange(&tag, EPG_EVENT_CREATED);

    programme = programme->NextSiblingElement("programme");

  }
  m_mutex.Unlock();
  return true;

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

