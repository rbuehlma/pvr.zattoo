#pragma once

#include <kodi/addon-instance/PVR.h>
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>

struct EpgQueueEntry
{
  int uniqueChannelId;
  time_t startTime;
  time_t endTime;
};

class UpdateThread
{
public:
  UpdateThread(kodi::addon::CInstancePVRClient& instance, int threadIdx, void *zat);
  ~UpdateThread();
  static void SetNextRecordingUpdate(time_t nextRecordingsUpdate);
  static void LoadEpg(int uniqueChannelId, time_t startTime, time_t endTime);
  void Process();

private:
  void *m_zat;
  int m_threadIdx;
  kodi::addon::CInstancePVRClient& m_instance;
  static std::queue<EpgQueueEntry> loadEpgQueue;
  static time_t nextRecordingsUpdate;
  std::atomic<bool> m_running = {false};
  std::thread m_thread;
  static std::mutex mutex;
};
