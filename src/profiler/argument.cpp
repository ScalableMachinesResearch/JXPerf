#include "argument.h"
#include <iostream>

namespace {

static void Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters = " ") {
    /* This function is copied from http://oopweb.com/CPP/Documents/CPPHOWTO/Volume/C++Programming-HOWTO-7.html */
    // Skip delimiters at beginning.
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    std::string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (std::string::npos != pos || std::string::npos != lastPos)
    {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}


}


Argument::Argument(const char *arg) {
  //the delimiter is ","
  std::vector<std::string> tokens;
  std::string str = arg;
  Tokenize(str, tokens, ",");
  /*
  for(auto &elem : tokens){
    std::cout << elem << std::endl;
  }
  */
  _perf_event_list = tokens;
}

Argument::~Argument() {
}
