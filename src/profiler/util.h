#ifndef _UTIL_H
#define _UTIL_H
#include <sstream>
#include <string>

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

std::string formate_speical_char(std::string ori) {
  std::string new_str = ori;
  std::replace(new_str.begin(), new_str.end(), '<', '-');
  std::replace(new_str.begin(), new_str.end(), '>', '-');
  return new_str;
}

#endif
