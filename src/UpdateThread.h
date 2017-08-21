#pragma once

#include <p8-platform/threads/threads.h>
#include "kodi/libXBMC_pvr.h"

class UpdateThread: public P8PLATFORM::CThread
{
public:
  UpdateThread();
  ~UpdateThread();
  void SetNextRecordingUpdate(time_t nextRecordingsUpdate);
  void* Process();

private:
  time_t nextRecordingsUpdate;
};
