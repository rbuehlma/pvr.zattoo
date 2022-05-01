#include <kodi/AddonBase.h>
#include "RecordingsDB.h"

const int DB_VERSION = 1;

class ProcessRecordingDBInfoRowCallback : public ProcessRowCallback {
public:
  virtual ~ProcessRecordingDBInfoRowCallback() { }
  
  void ProcessRow(sqlite3_stmt* stmt) {
    m_result.recordingId = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    m_result.playCount = sqlite3_column_int(stmt, 1);
    m_result.lastPlayedPosition = sqlite3_column_int(stmt, 2);
    m_result.lastSeen = sqlite3_column_int(stmt, 3);
  }
  
  RecordingDBInfo Result() {
    return m_result;
  }
  
private:
  RecordingDBInfo m_result;
};

RecordingsDB::RecordingsDB(std::string folder)
: SQLConnection("REC-DB") {
  std::string dbPath = folder + "recordings.sqlite";
  Open(dbPath);
  if (!MigrateDbIfRequired()) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to migrate DB to version: %i", m_name.c_str(), DB_VERSION);
  }
}

RecordingsDB::~RecordingsDB() {
}

bool RecordingsDB::MigrateDbIfRequired() {
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
    }
    currentVersion = GetVersion();
  }
  return true;
}

bool RecordingsDB::Migrate0To1() {
  kodi::Log(ADDON_LOG_INFO, "%s: Migrate to version 1.", m_name.c_str());
  std::string migrationScript = "";
  migrationScript += "create table RECORDING_INFO (";
  migrationScript += " RECORDING_ID text not null primary key,";
  migrationScript += " PLAY_COUNT integer not null,";
  migrationScript += " LAST_PLAYED_POSITION integer not null,";
  migrationScript += " LAST_SEEN integer not null";
  migrationScript += ")";
  if (!Execute(migrationScript)) {
    return false;
  }
  return SetVersion(1);
}

void RecordingsDB::Cleanup() {
  time_t now;
  time(&now);
  
  if (!Execute("delete from RECORDING_INFO where LAST_SEEN < " + std::to_string(now - 3600))) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to clean db", m_name.c_str());
  }
}


bool RecordingsDB::Set(RecordingDBInfo& recordingDBInfo) {
  time(&recordingDBInfo.lastSeen);
  std::string insert = "replace into RECORDING_INFO VALUES ";
  insert += "('" + recordingDBInfo.recordingId + "'," + std::to_string(recordingDBInfo.playCount) + "," + std::to_string(recordingDBInfo.lastPlayedPosition) + "," + std::to_string(recordingDBInfo.lastSeen) + ")";
  
  if (!Execute(insert)) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to insert", m_name.c_str());
    return false;
  }
  return true;
}

RecordingDBInfo RecordingsDB::Get(std::string recordingId) {
  ProcessRecordingDBInfoRowCallback callback;
  if (!Query("select * from RECORDING_INFO where RECORDING_ID = '" + recordingId + "'", callback)) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to get info from db.", m_name.c_str());
  }
  RecordingDBInfo ret = callback.Result();
  ret.recordingId = recordingId;
  return ret;
}

