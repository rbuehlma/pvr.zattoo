#include "UpdateThread.h"
#include <time.h>
#include "client.h"
#include "ZatData.h"
#include "Cache.h"

using namespace ADDON;

const time_t maximumUpdateInterval = 600;

std::queue<EpgQueueEntry> UpdateThread::loadEpgQueue;
time_t UpdateThread::nextRecordingsUpdate;
P8PLATFORM::CMutex UpdateThread::mutex;

UpdateThread::UpdateThread(int threadIdx, void *zat) :
    CThread()
{
  this->threadIdx = threadIdx;
  this->zat = zat;
  time(&UpdateThread::nextRecordingsUpdate);
  UpdateThread::nextRecordingsUpdate += maximumUpdateInterval;
  CreateThread(false);
}

UpdateThread::~UpdateThread()
{

}

void UpdateThread::SetNextRecordingUpdate(time_t nextRecordingsUpdate)
{
  if (nextRecordingsUpdate < UpdateThread::nextRecordingsUpdate)
  {
    if (!mutex.Lock())
    {
      XBMC->Log(LOG_ERROR,
          "UpdateThread::SetNextRecordingUpdate : Could not lock mutex.");
      return;
    }
    if (nextRecordingsUpdate < UpdateThread::nextRecordingsUpdate)
    {
      UpdateThread::nextRecordingsUpdate = nextRecordingsUpdate;
    }
    mutex.Unlock();
  }
}

void UpdateThread::LoadEpg(int uniqueChannelId, time_t startTime,
    time_t endTime)
{
  EpgQueueEntry entry;
  entry.uniqueChannelId = uniqueChannelId;
  entry.startTime = startTime;
  entry.endTime = endTime;
  if (!mutex.Lock())
  {
    XBMC->Log(LOG_ERROR, "UpdateThread::LoadEpg : Could not lock mutex.");
    return;
  }
  loadEpgQueue.push(entry);
  mutex.Unlock();
}

void* UpdateThread::Process()
{
  XBMC->Log(LOG_DEBUG, "Update thread started.");
  while (!IsStopped())
  {
    Sleep(100);
    if (IsStopped())
    {
      continue;
    }
    
    if (this->threadIdx == 0) {
      Cache::Cleanup();
    }

    while (!loadEpgQueue.empty())
    {
      if (!mutex.Lock())
      {
        XBMC->Log(LOG_ERROR,
            "UpdateThread::Process : Could not lock mutex for epg queue");
        break;
      }
      if (!loadEpgQueue.empty())
      {
        EpgQueueEntry entry = loadEpgQueue.front();
        loadEpgQueue.pop();
        mutex.Unlock();
        ((ZatData*) zat)->GetEPGForChannelAsync(entry.uniqueChannelId,
            entry.startTime, entry.endTime);
      }
      else
      {
        mutex.Unlock();
      }
    }
    
    time_t currentTime;
    time(&currentTime);

    if (((ZatData *)zat)->RecordingEnabled() && currentTime >= UpdateThread::nextRecordingsUpdate)
    {
      if (!mutex.Lock())
      {
        XBMC->Log(LOG_ERROR,
            "UpdateThread::Process : Could not lock mutex for recordings update");
        continue;
      }
      if (currentTime >= UpdateThread::nextRecordingsUpdate)
      {
        UpdateThread::nextRecordingsUpdate = currentTime
            + maximumUpdateInterval;
        mutex.Unlock();
        PVR->TriggerTimerUpdate();
        PVR->TriggerRecordingUpdate();
        XBMC->Log(LOG_DEBUG, "Update thread triggered update.");
      }
    }
  }

  XBMC->Log(LOG_DEBUG, "Update thread stopped.");
  return 0;
}

