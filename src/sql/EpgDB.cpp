#include "EpgDB.h"

const int DB_VERSION = 1;

class ProcessEpgDBInfoRowCallback : public ProcessRowCallback {
public:
  void ProcessRow(sqlite3_stmt* stmt) {
    m_result.programId = sqlite3_column_int(stmt, 0);
    m_result.recordUntil = sqlite3_column_int(stmt, 1);
    m_result.replayUntil = sqlite3_column_int(stmt, 2);
    m_result.restartUntil = sqlite3_column_int(stmt, 3);
  }
  
  EpgDBInfo Result() {
    return m_result;
  }
  
private:
  EpgDBInfo m_result;
};

EpgDB::EpgDB(std::string folder)
: SQLConnection("EPG") {
  std::string dbPath = folder + "test.sqlite";
  Open(dbPath);
  if (!MigrateDbIfRequired()) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to migrate DB to version: %i", m_name.c_str(), DB_VERSION);
  }
  Cleanup();
}

EpgDB::~EpgDB() {
}

bool EpgDB::MigrateDbIfRequired() {
  int currentVersion = GetVersion();
  while (currentVersion < DB_VERSION) {
    switch (currentVersion) {
    case 0:
      if (!Migrate0To1()) {
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

void EpgDB::Cleanup() {
  time_t now;
  time(&now);
  if (now < m_nextCleanupDue) {
    return;
  }
  
  m_nextCleanupDue = now + 3600;
  
  if (!Execute("delete from EPG_INFO where max(RECORD_UNTIL, REPLAY_UNTIL, RESTART_UNTIL) < " + std::to_string(now))) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to clean db", m_name.c_str());
  }
}

bool EpgDB::InsertBatch(std::vector<EpgDBInfo>& epgDBInfos) {
  std::string insert = "replace into EPG_INFO VALUES";
  std::string separator = " ";
  std::vector<EpgDBInfo>::iterator it;
  for (it = epgDBInfos.begin(); it != epgDBInfos.end(); ++it)
  {
    EpgDBInfo &epgDBInfo = (*it);
    
    insert += separator + "("
        + std::to_string(epgDBInfo.programId) + ","
        + std::to_string(epgDBInfo.recordUntil) + ","
        + std::to_string(epgDBInfo.replayUntil) + ","
        + std::to_string(epgDBInfo.restartUntil) + ")";
    separator = ", ";
  }
  
  if (!Execute(insert)) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to batch insert", m_name.c_str());
  }
  return true;
}

EpgDBInfo EpgDB::Get(int programId) {
  ProcessEpgDBInfoRowCallback callback;
  if (!Query("select * from EPG_INFO where PROGRAM_ID = " + std::to_string(programId), callback)) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to get info from db.", m_name.c_str());
  }
  return callback.Result();
}

