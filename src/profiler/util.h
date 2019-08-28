#ifndef _UTIL_H
#define _UTIL_H
#include <sstream>

template<typename T>
std::string compose_str(T t){
  std::ostringstream ss;
  ss << t;
  return ss.str();
}

template<typename T, typename... Rest>
std::string compose_str(T t, Rest ... rest){
  std::ostringstream ss;
  ss << t;
  return ss.str() + compose_str(rest...);
}

#endif
