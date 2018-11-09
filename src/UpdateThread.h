#pragma once

#include <p8-platform/threads/threads.h>
#include <p8-platform/threads/mutex.h>
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
  UpdateThread(int threadIdx, void *zat);
  ~UpdateThread() override;
  static void SetNextRecordingUpdate(time_t nextRecordingsUpdate);
  static void LoadEpg(int uniqueChannelId, time_t startTime, time_t endTime);
  void* Process() override;

private:
  void *zat;
  int threadIdx;
  static std::queue<EpgQueueEntry> loadEpgQueue;
  static time_t nextRecordingsUpdate;
  static P8PLATFORM::CMutex mutex;
};
