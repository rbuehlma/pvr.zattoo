#ifndef SRC_SQL_EPGDB_H_
#define SRC_SQL_EPGDB_H_

#include "SQLConnection.h"
#include <list>

struct EpgDBInfo
{
  int programId = 0;
  time_t recordUntil = 0;
  time_t replayUntil = 0;
  time_t restartUntil = 0;
  time_t startTime = 0;
  time_t endTime = 0;
  bool detailsLoaded = false;
  std::string genre;
  std::string title;
  std::string subtitle;
  std::string description;
  int season = -1;
  int episode = -1;
  std::string imageToken;
  std::string cid;
};

class EpgDB : public SQLConnection
{
public:
  EpgDB(std::string folder);
  ~EpgDB();
  bool Insert(EpgDBInfo &epgDBInfo);
  bool Update(EpgDBInfo &epgDBInfo);
  EpgDBInfo Get(int programId);
  std::list<EpgDBInfo> GetWithWhere(std::string where);
private:
  bool MigrateDbIfRequired();
  bool Migrate0To1();
  bool Migrate1To2();
  bool Migrate2To3();
  void Cleanup();
  time_t m_nextCleanupDue = 0;
  sqlite3_stmt *m_prepareInsertStatement;
  sqlite3_stmt *m_prepareUpdateStatement;
};

#endif /* SRC_SQL_EPGDB_H_ */
