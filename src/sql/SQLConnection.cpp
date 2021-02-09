#include "SQLConnection.h"

class ProcessSingleIntRowCallback : public ProcessRowCallback {
public:
  virtual ~ProcessSingleIntRowCallback() { }
  
  void ProcessRow(sqlite3_stmt* stmt) {
    m_result = sqlite3_column_int(stmt, 0);
  }
  
  int GetResult() {
    return m_result;
  }
  
private:
  int m_result = -1;
};

class NoopRowCallback : public ProcessRowCallback {
public:
  void ProcessRow(sqlite3_stmt* stmt) {
  }
};


SQLConnection::SQLConnection(std::string name):
    m_db(nullptr),
    m_name(name) {
}

SQLConnection::~SQLConnection() {
  sqlite3_close(m_db);
}

bool SQLConnection::Open(std::string& file) {
  int rc = sqlite3_open(file.c_str(), &m_db);
  if(rc) {
    kodi::Log(ADDON_LOG_ERROR, "%s: Can't open database: %s", m_name.c_str(), sqlite3_errmsg(m_db));
    return false;
  }
  sqlite3_exec(m_db, "PRAGMA synchronous = OFF;", NULL, NULL, NULL);
  sqlite3_exec(m_db, "PRAGMA journal_mode = OFF;", NULL, NULL, NULL);
  EnsureVersionTable();
  return true;
}

bool SQLConnection::Query(std::string query, ProcessRowCallback& callback) {
  sqlite3_stmt* stmt;
  int ret = sqlite3_prepare(m_db, query.c_str(), query.length(), &stmt, NULL);
  
  if (ret != SQLITE_OK) {
    sqlite3_finalize(stmt);
    kodi::Log(ADDON_LOG_ERROR, "%s: Query failed: %s", m_name.c_str(), sqlite3_errmsg(m_db));
    return false;
  }
  
  bool err = false;
  bool done = false;
  while (!done) {
    switch (sqlite3_step(stmt)) {
    case SQLITE_ROW:
      callback.ProcessRow(stmt);
      break;
      
    case SQLITE_DONE:
      done = true;
      break;

    default:
      kodi::Log(ADDON_LOG_ERROR, "%s: Query failed.", m_name.c_str());
      err = true;
      done = true;
    }
  }
  
  sqlite3_finalize(stmt);
  return !err;
}

bool SQLConnection::Execute(std::string query) {
  NoopRowCallback callback;
  return Query(query, callback);
}

bool SQLConnection::EnsureVersionTable() {
  ProcessSingleIntRowCallback callback;
  if (!Query("SELECT count(*) FROM sqlite_master WHERE type='table' AND name='SCHEMA_VERSION'", callback)) {
    return false;
  }
  
  if (callback.GetResult() == 0) {
    kodi::Log(ADDON_LOG_INFO, "%s: SCHEMA_VERSION does not exist. Creating Table.", m_name.c_str());
    if (!Execute("create table SCHEMA_VERSION (VERSION integer NOT NULL)")) {
      return false;
    }
    return Execute("insert into SCHEMA_VERSION VALUES (0)");
  }  
  
  return true;
}

int SQLConnection::GetVersion() {
  ProcessSingleIntRowCallback callback;
  if (!Query("select VERSION from SCHEMA_VERSION", callback)) {
    kodi::Log(ADDON_LOG_INFO, "%s: Failed to get current version.", m_name.c_str());
    return -1;
  }
  int currentVersion = callback.GetResult();
  kodi::Log(ADDON_LOG_INFO, "%s: Current version: %d", m_name.c_str(), currentVersion);
  return currentVersion;
}

bool SQLConnection::SetVersion(int newVersion) {
  return Execute("update SCHEMA_VERSION set VERSION = " + std::to_string(newVersion));
}

void SQLConnection::BeginTransaction() {
  sqlite3_exec(m_db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
}

void SQLConnection::EndTransaction() {
  sqlite3_exec(m_db, "END TRANSACTION;", NULL, NULL, NULL);
}

