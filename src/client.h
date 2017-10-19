#ifndef CLIENT_H
#define CLIENT_H

#include "kodi/libXBMC_addon.h"
#include "kodi/libXBMC_pvr.h"
#include "p8-platform/threads/threads.h"
#include "p8-platform/util/util.h"

extern std::string g_strUserPath;
extern std::string g_strClientPath;
extern ADDON::CHelper_libXBMC_addon *XBMC;
extern CHelper_libXBMC_pvr *PVR;

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#pragma comment(lib, "ws2_32.lib")
#include <stdio.h>
#include <stdlib.h>
#define timegm _mkgmtime

#define gmtime_r __gmtime_r
static inline struct tm *gmtime_r(const time_t *clock, struct tm *result)
{
  struct tm *data;
  if (!clock || !result)
    return NULL;
  data = gmtime(clock);
  if (!data)
    return NULL;
  memcpy(result, data, sizeof(*result));
  return result;
}

#endif
#endif

