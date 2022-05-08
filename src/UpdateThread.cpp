#include "UpdateThread.h"
#include <ctime>
#include <kodi/AddonBase.h>
#include "ZatData.h"
#include "http/Cache.h"

const time_t maximumUpdateInterval = 600;

std::queue<EpgQueueEntry> UpdateThread::loadEpgQueue;
time_t UpdateThread::nextRecordingsUpdate;
std::mutex UpdateThread::mutex;

UpdateThread::UpdateThread(kodi::addon::CInstancePVRClient& instance, int threadIdx, void *zat) :
    m_zat(zat),
    m_threadIdx(threadIdx),
    m_instance(instance)
{
  time(&UpdateThread::nextRecordingsUpdate);
  UpdateThread::nextRecordingsUpdate += maximumUpdateInterval;
  m_running = true;
  m_thread = std::thread([&] { Process(); });
}

UpdateThread::~UpdateThread()
{
  m_running = false;
  if (m_thread.joinable())
    m_thread.join();
}

void UpdateThread::SetNextRecordingUpdate(time_t nextRecordingsUpdate)
{
  if (nextRecordingsUpdate < UpdateThread::nextRecordingsUpdate)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (nextRecordingsUpdate < UpdateThread::nextRecordingsUpdate)
    {
      UpdateThread::nextRecordingsUpdate = nextRecordingsUpdate;
    }
  }
}

void UpdateThread::LoadEpg(int uniqueChannelId, time_t startTime,
    time_t endTime)
{
  EpgQueueEntry entry{};
  entry.uniqueChannelId = uniqueChannelId;
  entry.startTime = startTime;
  entry.endTime = endTime;
  std::lock_guard<std::mutex> lock(mutex);
  loadEpgQueue.push(entry);
}

void UpdateThread::Process()
{
  kodi::Log(ADDON_LOG_DEBUG, "Update thread started.");
  while (m_running)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!m_running)
    {
      continue;
    }

    if (m_threadIdx == 0) {
      Cache::Cleanup();
    }

    while (!loadEpgQueue.empty() && m_running)
    {
      std::unique_lock<std::mutex> lock(mutex);
      if (!loadEpgQueue.empty())
      {
        EpgQueueEntry entry = loadEpgQueue.front();
        loadEpgQueue.pop();
        lock.unlock();
        (static_cast<ZatData*>(m_zat))->GetEPGForChannelAsync(entry.uniqueChannelId,
            entry.startTime, entry.endTime);
      }
    }

    time_t currentTime;
    time(&currentTime);

    if (static_cast<ZatData*>(m_zat)->RecordingEnabled() && currentTime >= UpdateThread::nextRecordingsUpdate)
    {
      std::unique_lock<std::mutex> lock(mutex);
      if (currentTime >= UpdateThread::nextRecordingsUpdate)
      {
        UpdateThread::nextRecordingsUpdate = currentTime
            + maximumUpdateInterval;
        lock.unlock();
        m_instance.TriggerTimerUpdate();
        m_instance.TriggerRecordingUpdate();
        kodi::Log(ADDON_LOG_DEBUG, "Update thread triggered update.");
      }
    }
  }

  kodi::Log(ADDON_LOG_DEBUG, "Update thread stopped.");
}

