#include "UpdateThread.h"
#include <time.h>
#include "client.h"

using namespace ADDON;

const time_t maximumUpdateInterval = 600;

UpdateThread::UpdateThread() :
    CThread()
{
  time(&nextRecordingsUpdate);
  nextRecordingsUpdate += maximumUpdateInterval;
  CreateThread(false);
}

UpdateThread::~UpdateThread()
{

}

void UpdateThread::SetNextRecordingUpdate(time_t nextRecordingsUpdate)
{
  if (nextRecordingsUpdate < this->nextRecordingsUpdate)
  {
    this->nextRecordingsUpdate = nextRecordingsUpdate;
  }
}

void* UpdateThread::Process()
{
  XBMC->Log(LOG_DEBUG, "Update thread started.");
  while (!IsStopped())
  {
    Sleep(100);
    time_t currentTime;
    time(&currentTime);
    if (currentTime < nextRecordingsUpdate)
    {
      continue;
    }
    nextRecordingsUpdate = currentTime + maximumUpdateInterval;
    PVR->TriggerTimerUpdate();
    PVR->TriggerRecordingUpdate();
    XBMC->Log(LOG_DEBUG, "Update thread triggered update.");
  }
  XBMC->Log(LOG_DEBUG, "Update thread stopped.");
  return 0;
}

