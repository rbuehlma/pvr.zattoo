#ifndef SRC_SQL_EPGDB_H_
#define SRC_SQL_EPGDB_H_

#include "SQLConnection.h"

struct EpgDBInfo
{
  int programId = 0;
  time_t recordUntil = 0;
  time_t replayUntil = 0;
  time_t restartUntil = 0;
};

class EpgDB : public SQLConnection
{
public:
  EpgDB(std::string folder);
  ~EpgDB();
  bool InsertBatch(std::vector<EpgDBInfo>& epgDBInfos);
  EpgDBInfo Get(int programId);
private:
  bool MigrateDbIfRequired();
  bool Migrate0To1();
  void Cleanup();
  time_t m_nextCleanupDue = 0;
};

#endif /* SRC_SQL_EPGDB_H_ */
