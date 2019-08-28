#include "xml.h"

namespace xml {

XMLObj::XMLObj(std::string tag_name){
  _tag_name = tag_name;
}
XMLObj::~XMLObj(){
  for(auto &elem:_children_map){
    if(elem.second) {
      delete elem.second;
      elem.second = nullptr;
    }
    // _children_map.clear();
  }
}

void XMLObj::addAttr(std::string key, std::string value) {
  _attr_map[key] = "\"" + value + "\"";
}

void XMLObj::addChild(uint32_t id, XMLObj *child) {
  _children_map[id] = child;
}

std::string XMLObj::getXMLStr(uint32_t indent_num) {
  std::string indent(indent_num, ' ');

  std::ostringstream os;
  os << indent << "<" << _tag_name;
  for (auto &elem : _attr_map){
    os << " " << elem.first << "=" << elem.second;
  }
  os << ">" << std::endl;
  for (auto &elem : _children_map){
    os << elem.second->getXMLStr(indent_num+2);
  }
  os << indent << "</" <<_tag_name << ">"  << std::endl;
  return os.str();
}

bool XMLObj::hasAttr(std::string key, std::string value) {
#ifdef USE_BOOST_UNORDERED_CONTAINER
    boost::unordered_map<std::string, std::string>::const_iterator it = _attr_map.find(key);
#else
    std::unordered_map<std::string, std::string>::const_iterator it = _attr_map.find(key);
#endif
    if (it == _attr_map.end()) return false;
    else if (it->second != value) return false;
    return true;
}

}
