#ifndef _XML_H
#define _XML_H

#include <sstream>
#include <string>
#include <map>

#ifdef USE_BOOST_UNORDERED_CONTAINER
#include <boost/unordered_map.hpp>
#else
#include <unordered_map>
#endif

namespace xml {

#define XML_FILE_HEADER "<?xml version=\"1.0\" encoding=\"utf-8\"?>"

class XMLObj {
public:
  XMLObj(std::string tag_name);
  ~XMLObj();
 
  void addAttr(std::string key, std::string value);
 
  void addChild(uint32_t id, XMLObj *child);
  std::string getXMLStr(uint32_t indent_num=0);
  bool hasAttr(std::string key, std::string value);
private:
  std::string _tag_name;
  std::map<uint32_t, XMLObj *> _children_map; // the id should be unique
  
#ifdef USE_BOOST_UNORDERED_CONTAINER
  boost::unordered_map<std::string, std::string> _attr_map;
#else
  std::unordered_map<std::string, std::string> _attr_map;
#endif  
};

template<typename T>
XMLObj *createXMLObj(T *instance);

}
#endif
