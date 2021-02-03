#pragma once

#include <string>
#include "sqlite/sqlite3.h"
#include "../client.h"

class ProcessRowCallback {
public:
  virtual ~ProcessRowCallback() = 0;
  virtual void ProcessRow(sqlite3_stmt* stmt) = 0;
};
 
class SQLConnection
{
public:
  void BeginTransaction();
  void EndTransaction();
protected:
  SQLConnection(std::string name);
  ~SQLConnection();
  bool Open(std::string& file);
  bool Query(std::string query, ProcessRowCallback& callback);
  bool Execute(std::string query);
  int GetVersion();
  bool SetVersion(int newVersion);  
  sqlite3* m_db;
  std::string m_name;
  
private:
  bool EnsureVersionTable();
};
