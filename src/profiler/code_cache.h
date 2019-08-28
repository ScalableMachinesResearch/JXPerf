#ifndef _CODE_CACHE_H
#define _CODE_CACHE_H

#include <jvmti.h>
#include <stdint.h>
#include <iostream>
#include <assert.h>
#include <map>
#include <set>

#ifdef USE_BOOST_UNORDERED_CONTAINER
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#else
#include <unordered_map>
#include <unordered_set>
#endif

#include "lock.h"
#include "range.h"
#include "xml.h"

class Method {
public:
  Method(jmethodID method_id);
  jmethodID getMethodID(){ return _method_id;}
  std::string getMethodName(){ return _method_name;}
  const std::string &getSourceFile(){ return _src_file;}
  const std::string &getClassName(){return _class_name;} 
  bool isNative(){return _isNative;}
  void loadBCILineTable();
private:
  jmethodID _method_id;
  std::string _method_name;
  std::string _src_file;
  std::string _class_name;
  bool _isNative;
protected:
  RangeSet<uint64_t, int32_t> _bci2line_rangeset;
};


class CompiledMethod: public Method {
public:
  CompiledMethod(jmethodID method, uint32_t version, jint code_size, const void* code_addr);
  ~CompiledMethod();
  uint32_t getVersion(){ return _version;}
  bool isAddrIn(void *addr);
  void loadAddrLocationMap(const jvmtiAddrLocationMap* map, jint map_length);
  int32_t addr2line(uint64_t addr);
  inline const uint32_t getMethodVersion(){return _version;}
  // inline const uint64_t getFirstPC() {return _first_pc;}
  void getMethodBoundary(void **start_addr, void **end_addr);
private:
  uint32_t _version;
  jint _code_size;
  uint64_t _start_addr;
  // uint64_t _first_pc;

  RangeSet<uint64_t, int32_t> _addr2line_rangeset;

  friend class CompiledMethodGroup;
  friend xml::XMLObj *xml::createXMLObj<CompiledMethod>(CompiledMethod *instance);
};

//It is used to store different compiled methods with same method id.
class CompiledMethodGroup {
public:
  CompiledMethodGroup(jmethodID method_id);
  ~CompiledMethodGroup();
  CompiledMethod *addMethod(jint code_size, const void *code_addr, jint map_length, const jvmtiAddrLocationMap* map, LockScope<SpinLock> &lock_scope);
  void removeMethodByAddr(const void *code_addr);
  CompiledMethod *getMethodByVersion(uint32_t version);
private:
  jmethodID _method_id;
  uint32_t _recent_version = 0;

#ifdef USE_BOOST_UNORDERED_CONTAINER
  boost::unordered_map<uint64_t/*start_addr*/, CompiledMethod *> _address_method_map;
#else
  std::unordered_map<uint64_t/*start_addr*/, CompiledMethod *> _address_method_map;
  // std::map<uint64_t/*start_addr*/, CompiledMethod *> _address_method_map;
#endif
};


class InterpretMethod: public Method {
public:
  InterpretMethod(jmethodID method_id):Method(method_id){}
  uint32_t getVersion() {return 0;}
  xml::XMLObj *createXMLObj();
};


// it is used to store the methods which are never added but shown in context;
class MethodCache {
public:
  inline bool hasMethod(jmethodID method){
    LockScope<SpinLock> lock_scope(&_lock);
    if (_method_set.count(method) > 0) return true;
    else return false;
  }
  inline void addMethod(jmethodID method){
    // LockScope<SpinLock> lock_scope(&_lock);
    _method_set.insert(method);
  }
  inline void removeMethod(jmethodID method){
    // LockScope<SpinLock> lock_scope(&_lock);
    _method_set.erase(method);
  }
  void performActionAll(void (*cb)(jmethodID, void *data), void *data){
    LockScope<SpinLock> lock_scope(&_lock);
    for (auto &elem: _method_set){
      cb(elem, data);
    }
  }
private:
#ifdef USE_BOOST_UNORDERED_CONTAINER
  boost::unordered_set<jmethodID> _method_set;
#else
  std::unordered_set<jmethodID> _method_set;
#endif
  SpinLock _lock;
};


class CodeCacheManager {
public:
  CodeCacheManager();
  ~CodeCacheManager();
  CompiledMethod *addMethod(jmethodID method_id, jint code_size, const void *code_addr, jint map_length, const jvmtiAddrLocationMap *map);
  CompiledMethod *addMethodAndRemoveFromUncompiledSet(jmethodID method_id, jint code_size, const void *code_addr, jint map_length, const jvmtiAddrLocationMap *map);
  void removeMethod(jmethodID method_id, const void* code_addr);
  CompiledMethod *getMethod(uint64_t addr);
  CompiledMethod *getMethod(jmethodID method_id); //return the most recent version
  CompiledMethod *getMethod(const uint64_t addr, jmethodID &method_id, uint32_t &version);
  void checkAndMoveMethodToUncompiledSet(jmethodID method_id); 
  bool findMethodByAddr(const uint64_t addr, /*output*/ jmethodID &method_id, /*output*/ uint32_t &version ); //return true if successful   

private:
  RangeSet<uint64_t/* start_addr */, CompiledMethod *> _method_range_set;
#ifdef USE_BOOST_UNORDERED_CONTAINER
  boost::unordered_map<jmethodID, CompiledMethodGroup *> _method_id_map;
#else
  std::unordered_map<jmethodID, CompiledMethodGroup *> _method_id_map;
#endif
   /* NOTE: We need to provide an interface of accepting the method ID. 
  After all, the ASGCT only provides the method id without any address info. */
  SpinLock _lock;
};

#endif
