#include <kodi/AddonBase.h>
#include "EpgDB.h"

const int DB_VERSION = 3;

class ProcessEpgDBInfoRowCallback : public ProcessRowCallback {
public:
  virtual ~ProcessEpgDBInfoRowCallback() { }

  void ProcessRow(sqlite3_stmt* stmt) {
    EpgDBInfo dbInfo;
    dbInfo.programId = sqlite3_column_int(stmt, 0);
    dbInfo.recordUntil = sqlite3_column_int(stmt, 1);
    dbInfo.replayUntil = sqlite3_column_int(stmt, 2);
    dbInfo.restartUntil = sqlite3_column_int(stmt, 3);
    dbInfo.startTime = sqlite3_column_int(stmt, 4);
    dbInfo.endTime = sqlite3_column_int(stmt, 5);
    dbInfo.detailsLoaded = sqlite3_column_int(stmt, 6) > 0;
    dbInfo.genre = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7)));
    dbInfo.title = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)));
    dbInfo.subtitle = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9)));
    dbInfo.description = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10)));
    dbInfo.season = sqlite3_column_int(stmt, 11);
    dbInfo.episode = sqlite3_column_int(stmt, 12);
    dbInfo.imageToken = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13)));
    dbInfo.cid = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14)));
    
    m_result.push_back(dbInfo);
  }
  
  EpgDBInfo SingleResult() {
    if (m_result.size() < 1) {
      EpgDBInfo dbInfo;
      return dbInfo;
    }
    return m_result.front();
  }
  
  std::list<EpgDBInfo> Result() {
    return m_result;
  }
  
private:
  std::list<EpgDBInfo> m_result;
};

EpgDB::EpgDB(std::string folder)
: SQLConnection("EPG-DB") {
  std::string dbPath = folder + "epg.sqlite";
  Open(dbPath);
  if (!MigrateDbIfRequired()) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to migrate DB to version: %i", m_name.c_str(), DB_VERSION);
  }
  Cleanup();
  
  std::string str = "insert into EPG_INFO values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
  int ret = sqlite3_prepare_v2(m_db, str.c_str(), str.size() + 1, &m_prepareInsertStatement, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to prepare insert statement.", m_name.c_str());
  }

  str =  "update EPG_INFO set RECORD_UNTIL = ?, REPLAY_UNTIL = ?, RESTART_UNTIL = ?, START_TIME = ?, END_TIME = ?, ";
  str += "DETAILS_LOADED = ?, GENRE = ?, TITLE = ?, SUBTITLE = ?, DESCRIPTION = ?, SEASON = ?, EPISODE = ?, ";
  str += "IMAGE_TOKEN = ?, CID = ? where PROGRAM_ID = ?";
  ret = sqlite3_prepare_v2(m_db, str.c_str(), str.size() + 1, &m_prepareUpdateStatement, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to prepare update statement.", m_name.c_str());
  }
}

EpgDB::~EpgDB() {
  sqlite3_finalize(m_prepareInsertStatement);
  sqlite3_finalize(m_prepareUpdateStatement);
}

bool EpgDB::MigrateDbIfRequired() {
  int currentVersion = GetVersion();
  while (currentVersion < DB_VERSION) {
    if (currentVersion < 0) {
      return false;
    }
    switch (currentVersion) {
    case 0:
      if (!Migrate0To1()) {
        return false;
      }
      break;
    case 1:
      if (!Migrate1To2()) {
        return false;
      }
      break;
    case 2:
      if (!Migrate2To3()) {
        return false;
      }
      break;
    }
    currentVersion = GetVersion();
  }
  return true;
}

bool EpgDB::Migrate0To1() {
  kodi::Log(ADDON_LOG_INFO, "%s: Migrate to version 1.", m_name.c_str());
  std::string migrationScript = "";
  migrationScript += "create table EPG_INFO (";
  migrationScript += " PROGRAM_ID integer not null primary key,";
  migrationScript += " RECORD_UNTIL integer not null,";
  migrationScript += " REPLAY_UNTIL integer not null,";
  migrationScript += " RESTART_UNTIL integer not null";
  migrationScript += ")";
  if (!Execute(migrationScript)) {
    return false;
  }
  return SetVersion(1);
}

bool EpgDB::Migrate1To2() {
  kodi::Log(ADDON_LOG_INFO, "%s: Migrate to version 2.", m_name.c_str());
  if (!Execute("alter table EPG_INFO add column START_TIME integer not null default 0;")) {
    return false;
  }
  if (!Execute("alter table EPG_INFO add column END_TIME integer not null default 0;")) {
    return false;
  }
  if (!Execute("alter table EPG_INFO add column DETAILS_LOADED integer not null default 0;")) {
    return false;
  }
  if (!Execute("alter table EPG_INFO add column GENRE text;")) {
    return false;
  }
  if (!Execute("alter table EPG_INFO add column TITLE text;")) {
    return false;
  }
  if (!Execute("alter table EPG_INFO add column SUBTITLE text;")) {
    return false;
  }
  if (!Execute("alter table EPG_INFO add column DESCRIPTION text;")) {
    return false;
  }
  if (!Execute("alter table EPG_INFO add column SEASON integer;")) {
    return false;
  }
  if (!Execute("alter table EPG_INFO add column EPISODE integer;")) {
    return false;
  }
  if (!Execute("alter table EPG_INFO add column IMAGE_TOKEN text;")) {
    return false;
  }
  if (!Execute("alter table EPG_INFO add column CID text;")) {
    return false;
  }
  
  return SetVersion(2);
}

bool EpgDB::Migrate2To3() {
  kodi::Log(ADDON_LOG_INFO, "%s: Migrate to version 3.", m_name.c_str());
  if (!Execute("update EPG_INFO set DETAILS_LOADED = 0;")) {
    return false;
  }
  return SetVersion(3);
}

void EpgDB::Cleanup() {
  time_t now;
  time(&now);
  if (now < m_nextCleanupDue) {
    return;
  }
  
  m_nextCleanupDue = now + 3600;
  
  if (!Execute("delete from EPG_INFO where END_TIME < " + std::to_string(now - 60 * 60 * 24 * 7))) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to clean db", m_name.c_str());
  }
}

bool EpgDB::Insert(EpgDBInfo &epgDBInfo) {
  int ret = sqlite3_bind_int(m_prepareInsertStatement, 1, epgDBInfo.programId);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 1.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareInsertStatement, 2, epgDBInfo.recordUntil);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 2.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareInsertStatement, 3, epgDBInfo.replayUntil);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 3.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareInsertStatement, 4, epgDBInfo.restartUntil);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 4.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareInsertStatement, 5, epgDBInfo.startTime);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 5.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareInsertStatement, 6, epgDBInfo.endTime);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 6.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareInsertStatement, 7, epgDBInfo.detailsLoaded);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 7.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_text(m_prepareInsertStatement, 8, epgDBInfo.genre.c_str(), -1, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 8.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_text(m_prepareInsertStatement, 9, epgDBInfo.title.c_str(), -1, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 9.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_text(m_prepareInsertStatement, 10, epgDBInfo.subtitle.c_str(), -1, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 10.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_text(m_prepareInsertStatement, 11, epgDBInfo.description.c_str(), -1, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 11.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareInsertStatement, 12, epgDBInfo.season);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 12.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareInsertStatement, 13, epgDBInfo.episode);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 13.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_text(m_prepareInsertStatement, 14, epgDBInfo.imageToken.c_str(), -1, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 14.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_text(m_prepareInsertStatement, 15, epgDBInfo.cid.c_str(), -1, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 15.", m_name.c_str());
    return false;
  }

  ret = sqlite3_step(m_prepareInsertStatement);

  sqlite3_reset(m_prepareInsertStatement);
  
  return ret == SQLITE_DONE;
}

bool EpgDB::Update(EpgDBInfo &epgDBInfo) {
  int ret = sqlite3_bind_int(m_prepareUpdateStatement, 15, epgDBInfo.programId);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 15.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareUpdateStatement, 1, epgDBInfo.recordUntil);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 1.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareUpdateStatement, 2, epgDBInfo.replayUntil);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 2.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareUpdateStatement, 3, epgDBInfo.restartUntil);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 3.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareUpdateStatement, 4, epgDBInfo.startTime);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 4.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareUpdateStatement, 5, epgDBInfo.endTime);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 5.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareUpdateStatement, 6, epgDBInfo.detailsLoaded);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 6.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_text(m_prepareUpdateStatement, 7, epgDBInfo.genre.c_str(), -1, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 7.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_text(m_prepareUpdateStatement, 8, epgDBInfo.title.c_str(), -1, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 8.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_text(m_prepareUpdateStatement, 9, epgDBInfo.subtitle.c_str(), -1, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 9.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_text(m_prepareUpdateStatement, 10, epgDBInfo.description.c_str(), -1, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 10.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareUpdateStatement, 11, epgDBInfo.season);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 11.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_int(m_prepareUpdateStatement, 12, epgDBInfo.episode);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 12.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_text(m_prepareUpdateStatement, 13, epgDBInfo.imageToken.c_str(), -1, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 13.", m_name.c_str());
    return false;
  }
  ret = sqlite3_bind_text(m_prepareUpdateStatement, 14, epgDBInfo.cid.c_str(), -1, nullptr);
  if (ret != SQLITE_OK) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed bind value 14.", m_name.c_str());
    return false;
  }

  ret = sqlite3_step(m_prepareUpdateStatement);

  sqlite3_reset(m_prepareUpdateStatement);
  
  return ret == SQLITE_DONE;
}

EpgDBInfo EpgDB::Get(int programId) {
  ProcessEpgDBInfoRowCallback callback;
  if (!Query("select * from EPG_INFO where PROGRAM_ID = " + std::to_string(programId), callback)) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to get info from db.", m_name.c_str());
  }
  return callback.SingleResult();
}

std::list<EpgDBInfo> EpgDB::GetWithWhere(std::string where) {
  ProcessEpgDBInfoRowCallback callback;
  if (!Query("select * from EPG_INFO where " + where, callback)) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to get info from db using where part.", m_name.c_str());
  }
  return callback.Result();
}

