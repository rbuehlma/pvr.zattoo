#ifndef SRC_SQL_RECORDINGSDB_H_
#define SRC_SQL_RECORDINGSDB_H_

#include "SQLConnection.h"

struct RecordingDBInfo
{
  std::string recordingId;
  int playCount = 0;
  int lastPlayedPosition = 0;
  time_t lastSeen = 0;
};

class RecordingsDB : public SQLConnection
{
public:
  RecordingsDB(std::string folder);
  ~RecordingsDB();
  bool Set(RecordingDBInfo& recordingDBInfo);
  RecordingDBInfo Get(std::string recordingsId);
  void Cleanup();
private:
  bool MigrateDbIfRequired();
  bool Migrate0To1();
};

#endif /* SRC_SQL_RECORDINGSDB_H_ */
