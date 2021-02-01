#pragma once

#include <string>
#include "sqlite/sqlite3.h"
#include "../client.h"

class ProcessRowCallback {
public:
  virtual void ProcessRow(sqlite3_stmt* stmt) = 0;
};
 
class SQLConnection
{
protected:
  SQLConnection(std::string name);
  ~SQLConnection();
  bool Open(std::string& file);
  bool Query(std::string query, ProcessRowCallback& callback);
  bool Execute(std::string query);
  int GetVersion();
  bool SetVersion(int newVersion);  
  std::string m_name;
  
private:
  bool EnsureVersionTable();
  sqlite3* m_db;
};
