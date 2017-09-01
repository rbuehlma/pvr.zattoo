#pragma once

#include <p8-platform/threads/threads.h>
#include "kodi/libXBMC_pvr.h"
#include <queue>

struct EpgQueueEntry
{
  int uniqueChannelId;
  time_t startTime;
  time_t endTime;
};

class UpdateThread: public P8PLATFORM::CThread
{
public:
  UpdateThread(void *zat);
  ~UpdateThread();
  void SetNextRecordingUpdate(time_t nextRecordingsUpdate);
  void LoadEpg(int uniqueChannelId, time_t startTime, time_t endTime);
  void* Process();

private:
  time_t nextRecordingsUpdate;
  void *zat;
  std::queue<EpgQueueEntry> loadEpgQueue;
};
