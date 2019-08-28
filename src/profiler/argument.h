#ifndef _ARGUMENT_H
#define _ARGUMENT_H

#include <vector>
#include <string>

class Argument {
public:
  Argument(const char* arg);
  ~Argument();

  const std::vector<std::string> &getPerfEventList(){ return _perf_event_list;};

private:

  //std::vector<std::string> _event_list; // string format: EVENT_NAME@SAMPLING_RATE
  std::vector<std::string> _perf_event_list;

};

#endif
