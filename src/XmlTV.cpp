#include "XmlTV.h"
#include "tinyxml2.h"
#include <time.h>

#include "client.h"

using namespace tinyxml2;

XmlTV::XmlTV(std::string xmlFile) :
  m_xmlFile(xmlFile)
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

bool XmlTV::GetEPGForChannel(std::string cid, unsigned int uniqueChannelId)
{
  if (!XBMC->FileExists(m_xmlFile.c_str(), true))
  {
    return false;
  }

  XMLDocument doc;
  if (doc.LoadFile(m_xmlFile.c_str()) != XML_SUCCESS)
  {
    XBMC->Log(LOG_ERROR, "XMLTV: failed to parse xml-file.");
    return false;
  }
  XMLElement* tv = doc.FirstChildElement("tv");
  if (!tv)
  {
    XBMC->Log(LOG_ERROR, "XMLTV: no 'tv' section in xml-file.");
    return false;
  }
  bool result = false;
  XMLElement* programme = tv->FirstChildElement("programme");
  while (programme)
  {
    EPG_TAG tag;
    memset(&tag, 0, sizeof(EPG_TAG));

    XMLElement* title = programme->FirstChildElement("title");
    const char *start = programme->Attribute("start");
    const char *stop = programme->Attribute("stop");
    const char *channel = programme->Attribute("channel");

    if (!title || !start || !stop || !channel || strcmp(channel, cid.c_str()))
    {
      programme = programme->NextSiblingElement("programme");
      continue;
    }

    XMLElement* subTitle = programme->FirstChildElement("sub-title");
    XMLElement* category = programme->FirstChildElement("category");
    tag.startTime = StringToTime(start);
    tag.endTime = StringToTime(stop);
    tag.iUniqueBroadcastId = tag.startTime;
    tag.strTitle = title->GetText();
    tag.iUniqueChannelId = uniqueChannelId;
    if (subTitle)
    {
      const char *description = subTitle->GetText();
      tag.strPlotOutline = description;
      tag.strPlot = description;
    }
    if (category)
    {
      const char *genre = category->GetText();
      tag.iGenreType = EPG_GENRE_USE_STRING;
      tag.strGenreDescription = genre;
    }

    result = true;
    PVR->EpgEventStateChange(&tag, EPG_EVENT_CREATED);

    programme = programme->NextSiblingElement("programme");

  }
  return result;

}

time_t XmlTV::StringToTime(std::string timeString)
{
  struct tm tm
  { };
  strptime(timeString.c_str(), "%Y%m%d%H%M%S", &tm);
  time_t ret = timegm(&tm);
  return ret;
}

