#include "EnhancedEpgProvider.h"
#include "rapidjson/document.h"
#include "../client.h"
#include "../Utils.h"

using namespace rapidjson;

EnhancedEpgProvider::EnhancedEpgProvider(
    kodi::addon::CInstancePVRClient *addon,
    EpgDB &epgDB,
    HttpClient &httpClient,
    Categories &categories,
    std::string powerHash,
    std::string countryCode
  ):
  EpgProvider(addon),
  m_epgDB(epgDB),
  m_httpClient(httpClient),
  m_categories(categories),
  m_powerHash(powerHash),
  m_countryCode(countryCode)
{
}

EnhancedEpgProvider::~EnhancedEpgProvider() { }

bool EnhancedEpgProvider::LoadEPGForChannel(ZatChannel &zatChannel, time_t iStart, time_t iEnd) {
  std::string cid = zatChannel.cid;
  std::ostringstream urlStream;
  urlStream << "https://zattoo.buehlmann.net/epg/api/Epg/"
      << m_countryCode << "/" << m_powerHash << "/" << cid << "/" << iStart
      << "/" << iEnd;
  int statusCode;
  std::string jsonString = m_httpClient.HttpGetCached(urlStream.str(), 86400, statusCode);
  Document doc;
  doc.Parse(jsonString.c_str());
  if (doc.GetParseError())
  {
    return false;
  }
  std::lock_guard<std::mutex> lock(sendEpgToKodiMutex);
  m_epgDB.BeginTransaction();
  for (Value::ConstValueIterator itr = doc.Begin(); itr != doc.End(); ++itr)
  {
    const Value& program = (*itr);
    kodi::addon::PVREPGTag tag;

    tag.SetUniqueBroadcastId(static_cast<unsigned int>(program["Id"].GetInt()));
    std::string title = Utils::JsonStringOrEmpty(program, "Title");
    tag.SetTitle(title);
    tag.SetUniqueChannelId(static_cast<unsigned int>(zatChannel.iUniqueId));
    tag.SetStartTime(Utils::StringToTime(Utils::JsonStringOrEmpty(program, "StartTime")));
    tag.SetEndTime(Utils::StringToTime(Utils::JsonStringOrEmpty(program, "EndTime")));
    std::string description = Utils::JsonStringOrEmpty(program, "Description");
    tag.SetPlotOutline(description);
    tag.SetPlot(description);
    tag.SetOriginalTitle(""); /* not supported */
    tag.SetCast(""); /* not supported */
    tag.SetDirector(""); /*SA not supported */
    tag.SetWriter(""); /* not supported */
    tag.SetYear(0); /* not supported */
    tag.SetIMDBNumber(""); /* not supported */
    std::string imageToken = Utils::JsonStringOrEmpty(program, "ImageToken");
    std::string imageUrl;
    if (!imageToken.empty()) {
      imageUrl = Utils::GetImageUrl(imageToken);
      tag.SetIconPath(imageUrl);
    }
    tag.SetParentalRating(0); /* not supported */
    tag.SetStarRating(0); /* not supported */
    tag.SetSeriesNumber(EPG_TAG_INVALID_SERIES_EPISODE); /* not supported */
    tag.SetEpisodeNumber(EPG_TAG_INVALID_SERIES_EPISODE); /* not supported */
    tag.SetEpisodePartNumber(EPG_TAG_INVALID_SERIES_EPISODE); /* not supported */
    std::string subtitle = Utils::JsonStringOrEmpty(program, "Subtitle");
    tag.SetEpisodeName(subtitle);
    std::string genreStr = Utils::JsonStringOrEmpty(program, "Genre");
    int genre = m_categories.Category(genreStr);
    if (genre)
    {
      tag.SetGenreSubType(genre & 0x0F);
      tag.SetGenreType(genre & 0xF0);
    }
    else
    {
      tag.SetGenreType(EPG_GENRE_USE_STRING);
      tag.SetGenreSubType(0); /* not supported */
      tag.SetGenreDescription(genreStr);
    }
    EpgDBInfo epgDBInfo;
    epgDBInfo.programId = program["Id"].GetInt();
    epgDBInfo.recordUntil = Utils::StringToTime(Utils::JsonStringOrEmpty(program, "RecordUntil"));
    epgDBInfo.replayUntil = Utils::StringToTime(Utils::JsonStringOrEmpty(program, "ReplayUntil"));
    epgDBInfo.restartUntil = Utils::StringToTime(Utils::JsonStringOrEmpty(program, "RestartUntil"));
    m_epgDB.Insert(epgDBInfo);
    SendEpg(tag);
  }
  m_epgDB.EndTransaction();
  return true;
}



