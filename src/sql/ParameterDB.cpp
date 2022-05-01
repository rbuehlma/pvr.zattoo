#include <kodi/AddonBase.h>
#include "ParameterDB.h"

const int DB_VERSION = 1;

class ProcessParameterRowCallback : public ProcessRowCallback {
public:
  virtual ~ProcessParameterRowCallback() { }
  
  void ProcessRow(sqlite3_stmt* stmt) {
    m_result = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
  }
  
  std::string Result() {
    return m_result;
  }
  
private:
  std::string m_result = "";
};

ParameterDB::ParameterDB(std::string folder)
: SQLConnection("PARAMS-DB") {
  std::string dbPath = folder + "parameter.sqlite";
  Open(dbPath);
  if (!MigrateDbIfRequired()) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to migrate DB to version: %i", m_name.c_str(), DB_VERSION);
  }
}

ParameterDB::~ParameterDB() {
}

bool ParameterDB::MigrateDbIfRequired() {
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

bool ParameterDB::Migrate0To1() {
  kodi::Log(ADDON_LOG_INFO, "%s: Migrate to version 1.", m_name.c_str());
  std::string migrationScript = "";
  migrationScript += "create table PARAMETER (";
  migrationScript += " KEY text not null primary key,";
  migrationScript += " VALUE text not null";
  migrationScript += ")";
  if (!Execute(migrationScript)) {
    return false;
  }
  return SetVersion(1);
}

bool ParameterDB::Set(std::string key, std::string value) {
  std::string insert = "replace into PARAMETER VALUES ";
  insert += "('" + key + "','" + value + "')";
  
  if (!Execute(insert)) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to insert", m_name.c_str());
    return false;
  }
  return true;
}

std::string ParameterDB::Get(std::string key) {
  ProcessParameterRowCallback callback;
  if (!Query("select VALUE from PARAMETER where KEY = '" + key + "'", callback)) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to get parameter from db.", m_name.c_str());
  }
  return callback.Result();
}

