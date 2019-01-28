#ifndef Templates_h
#define Templates_h

#include <string>
#include <sstream>

template<typename T> std::string to_string(const T& n)
{
  std::ostringstream stm;
  stm << n;
  return stm.str();
}

#endif  // Templates_h
