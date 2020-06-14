#ifndef GMTIME_H
#define GMTIME_H
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#pragma comment(lib, "ws2_32.lib")
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
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

#include <time.h>
#include <iomanip>
#include <sstream>

static inline char* strptime(const char* s,
                          const char* f,
                          struct tm* tm) {
  // Isn't the C++ standard lib nice? std::get_time is defined such that its
  // format parameters are the exact same as strptime. Of course, we have to
  // create a string stream first, and imbue it with the current C locale, and
  // we also have to make sure we return the right things if it fails, or
  // if it succeeds, but this is still far simpler an implementation than any
  // of the versions in any of the C standard libraries.
  std::istringstream input(s);
  input.imbue(std::locale(setlocale(LC_ALL, nullptr)));
  input >> std::get_time(tm, f);
  if (input.fail()) {
    return nullptr;
  }
  return (char*)(s + input.tellg());
}

#endif
#endif

